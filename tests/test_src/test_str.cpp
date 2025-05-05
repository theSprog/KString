#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <stdexcept>
#include <doctest/doctest.h>
#include "../../include/core/str.hpp"

using KString::KChar;
using KString::KStr;

TEST_CASE("KStr constructors and basic accessors") {
    SUBCASE("Default constructor yields empty string") {
        KStr s;
        CHECK(s.empty());
        CHECK(s.byte_size() == 0);
        CHECK(s.char_size() == 0);
    }

    SUBCASE("Construct from C string") {
        KStr s = "hello";
        CHECK(! s.empty());
        CHECK(s.byte_size() == 5);
        CHECK(s.char_size() == 5);
        CHECK(s.as_bytes().data()[0] == 'h');
    }

    SUBCASE("Construct from char pointer + size") {
        const char* raw = "world!!";
        KStr s(raw, 5); // only take "world"
        CHECK(! s.empty());
        CHECK(s.byte_size() == 5);
        CHECK(s.char_size() == 5);
    }

    SUBCASE("Construct from uint8_t pointer + size") {
        const uint8_t bytes[] = {0xe4, 0xbd, 0xa0, 0xe5, 0xa5, 0xbd}; // "你好"
        KStr s(bytes, sizeof(bytes));
        CHECK(s.byte_size() == 6);
        CHECK(s.char_size() == 2); // 2 Unicode chars
    }

    SUBCASE("Construct from ByteSpan") {
        const char* raw = "abc";
        KStr s(raw);
        CHECK(s.byte_size() == 3);
        CHECK(s.char_size() == 3);
    }
}

TEST_CASE("KStr iter_chars() yields correct KChar sequence") {
    KStr s("你好abc"); // UTF-8: 3 bytes + 3 bytes + 1+1+1

    std::vector<internal::utf8::CodePoint> codepoints;
    for (KChar ch : s.iter_chars()) {
        codepoints.push_back(ch.value());
    }

    REQUIRE(codepoints.size() == 5);
    CHECK(codepoints[0] == 0x4f60); // 你
    CHECK(codepoints[1] == 0x597d); // 好
    CHECK(codepoints[2] == 'a');
    CHECK(codepoints[3] == 'b');
    CHECK(codepoints[4] == 'c');

    auto iter = s.iter_chars();
    auto bg = iter.begin();
    ++bg;
    CHECK(*bg == KChar(0x597d));
    ++bg;
    ++bg;
    ++bg;
    CHECK(*bg == KChar('c'));
    ++bg;
    CHECK(bg == iter.end());
}

TEST_CASE("KStr char_at, byte_at, operator[]") {
    KStr s("你a好b"); // UTF-8: [你:3][a:1][好:3][b:1]

    SUBCASE("byte_at returns correct individual byte") {
        CHECK(s.byte_at(0) == 0xe4); // '你'
        CHECK(s.byte_at(3) == 'a');
        CHECK(s.byte_at(4) == 0xe5); // '好'
        CHECK(s.byte_at(7) == 'b');
    }

    SUBCASE("char_at returns correct unicode characters") {
        CHECK(s.char_at(0).value() == 0x4f60); // '你'
        CHECK(s.char_at(1).value() == 'a');
        CHECK(s.char_at(2).value() == 0x597d); // '好'
        CHECK(s.char_at(3).value() == 'b');
    }

    SUBCASE("operator[] returns correct ByteSpan per character") {
        auto span = s[0];
        CHECK(span.size() == 3);
        CHECK(span.data()[0] == 0xe4); // '你'
        CHECK(span.data()[1] == 0xbd);
        CHECK(span.data()[2] == 0xa0);

        auto span2 = s[2]; // '好'
        CHECK(span2.size() == 3);
        CHECK(span2.data()[0] == 0xe5);
    }

    SUBCASE("char_at throws if out of bounds") {
        CHECK_THROWS_AS(s[100], std::out_of_range);
        CHECK_THROWS_AS(s.char_at(4), std::out_of_range);
    }

    SUBCASE("byte_at throws if out of bounds") {
        CHECK_THROWS_AS(s.byte_at(100), std::out_of_range);
    }

    SUBCASE("operator[] throws if decoding fails") {
        std::vector<uint8_t> raw = {0xe4, 0xff};
        KStr s2(raw);
        CHECK_THROWS_AS(s2[0], std::runtime_error);
    }

    SUBCASE("char_at decoding fails") {
        std::vector<uint8_t> raw = {0xe4, 0xff, 0xff, 0xff};
        KStr s2(raw);
        CHECK_EQ(s2.char_at(0), KChar(KString::KChar::Ill));
        CHECK_EQ(s2.char_at(2), KChar(KString::KChar::Ill));

        std::vector<uint8_t> raw2 = {'a', 0xe4, 0xff, 0xff, 0xff, 'b'};
    }
}

TEST_CASE("KStr find/rfind/contains") {
    KStr s = ("你好abc你好abc");

    SUBCASE("find simple ascii") {
        CHECK(s.find(("a")) == 2);
        CHECK(s.find(("ab")) == 2);
        CHECK(s.find(("abc")) == 2);
    }

    SUBCASE("find unicode") {
        CHECK(s.find(("你")) == 0);
        CHECK(s.find(("好a")) == 1);
        CHECK(s.find(("你好abc")) == 0);
        CHECK(s.find(("abc你")) == 2);
    }

    SUBCASE("find not found") {
        CHECK(s.find(("xyz")) == KStr::knpos);
    }

    SUBCASE("rfind ascii") {
        CHECK(s.rfind(("a")) == 7);
        CHECK(s.rfind(("abc")) == 7);
    }

    SUBCASE("rfind unicode") {
        CHECK(s.rfind(("你")) == 5);
        CHECK(s.rfind(("你好")) == 5);
    }

    SUBCASE("contains") {
        CHECK(s.contains(("abc")));
        CHECK(s.contains(("你")));
        CHECK(! s.contains(("xyz")));
    }

    SUBCASE("empty pattern") {
        CHECK(s.find(("")) == 0);
        CHECK(s.rfind(("")) == s.char_size());
        CHECK(s.contains(("")));
    }

    SUBCASE("pattern too long") {
        CHECK(KStr("a").find(("abc")) == KStr::knpos);
    }
}

TEST_CASE("test byte_offset_to_char_index") {
    KStr s = "你a好"; // UTF-8 字节数 = 3 + 1 + 3 = 7
    CHECK_EQ(s.count_chars_before(0), 0);
    CHECK_EQ(s.count_chars_before(1), 1);
    CHECK_EQ(s.count_chars_before(2), 2);
    CHECK_EQ(s.count_chars_before(3), 1);
    CHECK_EQ(s.count_chars_before(4), 2);
    CHECK_EQ(s.count_chars_before(5), 3);
    CHECK_EQ(s.count_chars_before(6), 4);
    CHECK_EQ(s.count_chars_before(7), 3);

    CHECK_THROWS_AS(s.count_chars_before(8), std::out_of_range);
}

TEST_CASE("starts_with / ends_with") {
    KStr s("你好世界abc");
    CHECK(s.starts_with(KStr("你好")));
    CHECK(s.ends_with(KStr("abc")));
    CHECK_FALSE(s.starts_with(KStr("世界")));
    CHECK_FALSE(s.ends_with(KStr("你好")));
}

TEST_CASE("substr") {
    KStr s("你好世界abcd");
    CHECK(s.substr(0, 2) == "你好");
    CHECK(s.substr(2, 2) == "世界");
    CHECK(s.substr(4, 4) != "abc");
    CHECK(s.substr(4, 4) == "abcd");

    CHECK(s.substr(0, 100) == s);                         // count 超过实际长度
    CHECK_THROWS_AS(s.substr(100, 1), std::out_of_range); // start 越界
}

TEST_CASE("subrange") {
    KStr s("你好世界abc");
    CHECK(s.subrange(1, 4) == "好世界");
    CHECK(s.subrange(0, 0) == ""); // 空范围
    CHECK(s.subrange(0, 1) == "你");
    CHECK(s.subrange(5, 6) == "b");   // 最后一个字符
    CHECK_THROWS(s.subrange(10, 11)); // 超出边界
    CHECK_THROWS_AS(s.subrange(4, 1), std::out_of_range);
}

TEST_CASE("split_at and split_exclusive_at basic behavior (C++11)") {
    KStr s("你好世界abc");

    SUBCASE("middle split") {
        std::pair<KStr, KStr> parts = s.split_at(3);
        CHECK(parts.first == "你好世");
        CHECK(parts.second == "界abc");

        std::pair<KStr, KStr> parts2 = s.split_exclusive_at(3);
        CHECK(parts2.first == "你好世");
        CHECK(parts2.second == "abc");
    }

    SUBCASE("split at 0") {
        std::pair<KStr, KStr> parts = s.split_at(0);
        CHECK(parts.first == "");
        CHECK(parts.second == "你好世界abc");

        std::pair<KStr, KStr> parts2 = s.split_exclusive_at(0);
        CHECK(parts2.first == "");
        CHECK(parts2.second == "好世界abc");
    }

    SUBCASE("split at end") {
        std::size_t n = s.char_size();
        CHECK_THROWS_AS(s.split_at(n), std::out_of_range);
        CHECK_THROWS_AS(s.split_exclusive_at(n), std::out_of_range);
    }

    SUBCASE("split at 1") {
        std::pair<KStr, KStr> parts = s.split_at(1);
        CHECK(parts.first == "你");
        CHECK(parts.second == "好世界abc");

        std::pair<KStr, KStr> parts2 = s.split_exclusive_at(1);
        CHECK(parts2.first == "你");
        CHECK(parts2.second == "世界abc");
    }

    SUBCASE("single character") {
        KStr one("你");
        std::pair<KStr, KStr> parts1 = one.split_at(0);
        CHECK(parts1.first == "");
        CHECK(parts1.second == "你");

        std::pair<KStr, KStr> parts2 = one.split_exclusive_at(0);
        CHECK(parts2.first == "");
        CHECK(parts2.second == "");

        CHECK_THROWS_AS(one.split_at(1), std::out_of_range);
        CHECK_THROWS_AS(one.split_exclusive_at(1), std::out_of_range);
    }

    SUBCASE("index out of range throws") {
        CHECK_THROWS_AS(s.split_at(100), std::out_of_range);
        CHECK_THROWS_AS(s.split_exclusive_at(100), std::out_of_range);
    }
}

TEST_CASE("char_indices and char_index_to_byte_offset") {
    KStr s("你a好b"); // 字节序列: [E4 BD A0] [61] [E5 A5 BD] [62]

    auto indices = s.char_indices();
    REQUIRE(indices.size() == 4);

    CHECK(indices[0].ch == KChar(0x4F60)); // 你
    CHECK(indices[0].byte_offset == 0);    // byte offset 0
    CHECK(indices[0].char_index == 0);     // char index 0

    CHECK(indices[1].ch == KChar('a'));
    CHECK(indices[1].byte_offset == 3); // byte offset 3
    CHECK(indices[1].char_index == 1);  // char index 1

    CHECK(indices[2].ch == KChar(0x597D)); // 好
    CHECK(indices[2].byte_offset == 4);    // byte offset 4
    CHECK(indices[2].char_index == 2);     // char index 2

    CHECK(indices[3].ch == KChar('b'));
    CHECK(indices[3].byte_offset == 7); // byte offset 7
    CHECK(indices[3].char_index == 3);  // char index 3

    CHECK(s.char_index_to_byte_offset(0) == 0);                         // "你"
    CHECK(s.char_index_to_byte_offset(1) == 3);                         // "a"
    CHECK(s.char_index_to_byte_offset(2) == 4);                         // "好"
    CHECK(s.char_index_to_byte_offset(3) == 7);                         // "b"
    CHECK_THROWS_AS(s.char_index_to_byte_offset(4), std::out_of_range); // 越界
}

TEST_CASE("char_indices") {
    KStr s("你a好b");
    auto pairs = s.char_indices();
    REQUIRE(pairs.size() == 4);
    CHECK(pairs[0].ch.value() == 0x4F60); // 你
    CHECK(pairs[1].ch.value() == 'a');
    CHECK(pairs[2].ch.value() == 0x597D); // 好
    CHECK(pairs[3].ch.value() == 'b');
}
