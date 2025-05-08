#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <doctest/doctest.h>
#include "../../include/kstr.hpp"

using kstring::KChar;
using kstring::KStr;
using utf8::ByteSpan;

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

    std::vector<utf8::CodePoint> codepoints;
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

TEST_CASE("KStr iter_chars_rev() yields correct KChar reverse sequence") {
    KStr s("你好abc"); // UTF-8: 3 bytes + 3 bytes + 1+1+1

    std::vector<utf8::CodePoint> codepoints;
    for (KChar ch : s.iter_chars_rev()) {
        codepoints.push_back(ch.value());
    }

    REQUIRE(codepoints.size() == 5);
    CHECK(codepoints[0] == 'c');
    CHECK(codepoints[1] == 'b');
    CHECK(codepoints[2] == 'a');
    CHECK(codepoints[3] == 0x597d); // 好
    CHECK(codepoints[4] == 0x4f60); // 你

    auto iter = s.iter_chars_rev();
    auto bg = iter.begin();
    ++bg;
    CHECK(*bg == KChar('b'));
    ++bg;
    ++bg;
    ++bg;
    CHECK(*bg == KChar(0x4f60));
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
        CHECK_EQ(s2.char_at(0), KChar(kstring::Ill_CODEPOINT));
        CHECK_EQ(s2.char_at(2), KChar(kstring::Ill_CODEPOINT));

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
        CHECK(s.find(("xyz")) == kstring::knpos);
    }

    SUBCASE("rfind ascii") {
        CHECK(s.rfind(("a")) == 7);
        CHECK(s.rfind(("abc")) == 7);
        CHECK(s.rfind(("xyz")) == kstring::knpos);
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
        CHECK(KStr("a").find(("abc")) == kstring::knpos);
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
    KStr s("你好世界ab");
    CHECK(s.subrange(1, 4) == "好世界");
    CHECK(s.subrange(0, 0) == ""); // 空范围
    CHECK(s.subrange(0, 1) == "你");
    CHECK(s.subrange(5, 6) == "b");   // 最后一个字符
    CHECK(s.subrange(4, 10) == "ab"); // end 越界
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
    KStr s("你a好b\xff");
    auto vec = s.char_indices();
    REQUIRE(vec.size() == 5);
    CHECK(vec[0].ch.value() == 0x4F60); // 你
    CHECK(vec[1].ch.value() == 'a');
    CHECK(vec[2].ch.value() == 0x597D); // 好
    CHECK(vec[3].ch.value() == 'b');
    CHECK(vec[4].ch.value() == kstring::Ill_CODEPOINT);
}

TEST_CASE("KStr split family basic functionality and corner cases") {
    KStr s("a,b,c,,d");

    SUBCASE("split normal") {
        auto parts = s.split(",");
        REQUIRE(parts.size() == 5);
        CHECK(parts[0] == "a");
        CHECK(parts[1] == "b");
        CHECK(parts[2] == "c");
        CHECK(parts[3] == "");
        CHECK(parts[4] == "d");
    }

    SUBCASE("rsplit normal") {
        auto parts = s.rsplit(",");
        REQUIRE(parts.size() == 5);
        CHECK(parts[4] == "a");
        CHECK(parts[3] == "b");
        CHECK(parts[2] == "c");
        CHECK(parts[1] == "");
        CHECK(parts[0] == "d");
    }

    SUBCASE("split_count limits splits") {
        auto parts = s.split_count(",", 2);
        REQUIRE(parts.size() == 3);
        CHECK(parts[0] == "a");
        CHECK(parts[1] == "b");
        CHECK(parts[2] == "c,,d");
    }

    SUBCASE("rsplit_count limits splits") {
        auto parts = s.rsplit_count(",", 2);
        REQUIRE(parts.size() == 3);
        CHECK(parts[2] == "a,b,c");
        CHECK(parts[1] == "");
        CHECK(parts[0] == "d");
    }

    SUBCASE("split_once finds first") {
        auto parts = s.split_once(",");
        CHECK(parts.first == "a");
        CHECK(parts.second == "b,c,,d");
    }

    SUBCASE("rsplit_once finds last") {
        auto parts = s.rsplit_once(",");
        CHECK(parts.second == "a,b,c,");
        CHECK(parts.first == "d");
    }

    SUBCASE("split with no delimiter present") {
        auto parts = KStr("abc").split("-");
        REQUIRE(parts.size() == 1);
        CHECK(parts[0] == "abc");
    }

    SUBCASE("rsplit with no delimiter present") {
        auto parts = KStr("abc").rsplit("-");
        REQUIRE(parts.size() == 1);
        CHECK(parts[0] == "abc");
    }

    SUBCASE("split_once with no delimiter") {
        auto parts = KStr("abc").split_once("-");
        CHECK(parts.first == "abc");
        CHECK(parts.second == "");
    }

    SUBCASE("rsplit_once with no delimiter") {
        auto parts = KStr("abc").rsplit_once("-");
        CHECK(parts.first == "");
        CHECK(parts.second == "abc");
    }

    SUBCASE("split with empty string") {
        auto parts = KStr("").split(",");
        REQUIRE(parts.size() == 1);
        CHECK(parts[0] == "");
    }

    SUBCASE("split with empty delimiter should throw") {
        CHECK_THROWS_AS(KStr("abc").split_count("", 1), std::invalid_argument);
        CHECK_THROWS_AS(KStr("abc").rsplit_count("", 1), std::invalid_argument);
    }

    SUBCASE("split_count with max_splits = 0 returns whole") {
        auto parts = KStr("a,b,c").split_count(",", 0);
        REQUIRE(parts.size() == 1);
        CHECK(parts[0] == "a,b,c");
    }

    SUBCASE("rsplit_count with max_splits = 0 returns whole") {
        auto parts = KStr("a,b,c").rsplit_count(",", 0);
        REQUIRE(parts.size() == 1);
        CHECK(parts[0] == "a,b,c");
    }

    SUBCASE("split/rsplit UTF-8 multi-byte character as delimiter") {
        KStr utf_str("你哈你哈你");
        KStr delim("哈");

        auto parts = utf_str.split(delim);
        REQUIRE(parts.size() == 3);
        CHECK(parts[0] == "你");
        CHECK(parts[1] == "你");
        CHECK(parts[2] == "你");

        auto rparts = utf_str.rsplit(delim);
        REQUIRE(rparts.size() == 3);
        CHECK(rparts[0] == "你");
        CHECK(rparts[1] == "你");
        CHECK(rparts[2] == "你");
    }
}

TEST_CASE("KStr split_whitespace basic functionality") {
    KStr s("  你好  world\t\n你好\tabc ");
    auto parts = s.split_whitespace();
    REQUIRE(parts.size() == 4);
    CHECK(parts[0] == "你好");
    CHECK(parts[1] == "world");
    CHECK(parts[2] == "你好");
    CHECK(parts[3] == "abc");
    std::stringstream oss;
    oss << parts[0] << parts[1];
    CHECK(oss.str() == "你好world");
}

TEST_CASE("KStr split_whitespace corner cases") {
    KStr empty("");
    auto res1 = empty.split_whitespace();
    CHECK(res1.empty());

    KStr all_ws("  \t\r\n ");
    auto res2 = all_ws.split_whitespace();
    CHECK(res2.empty());

    KStr no_ws("纯文本");
    auto res3 = no_ws.split_whitespace();
    REQUIRE(res3.size() == 1);
    CHECK(res3[0] == "纯文本");

    std::vector<uint8_t> raw = {'a', 0xff, '1', ' ', '3', 0xff, 'b'};
    KStr bad(ByteSpan(raw.data(), raw.size()));
    auto res4 = bad.split_whitespace();
    REQUIRE(res4.size() == 2);
    CHECK(res4[0] == std::string({'a', static_cast<char>(0xff), '1'}));
    CHECK(res4[1] == std::string({'3', static_cast<char>(0xff), 'b'}));
}

TEST_CASE("KStr lines handles \\n \\r \\r\\n and trailing newlines") {
    KStr s("line1\nline2\rline3\r\nline4");
    auto lines = s.lines();
    REQUIRE(lines.size() == 4);
    CHECK(lines[0] == "line1");
    CHECK(lines[1] == "line2");
    CHECK(lines[2] == "line3");
    CHECK(lines[3] == "line4");
}

TEST_CASE("KStr lines trailing line ending results in empty string") {
    KStr s("a\nb\n");
    auto lines = s.lines();
    REQUIRE(lines.size() == 3);
    CHECK(lines[0] == "a");
    CHECK(lines[1] == "b");
    CHECK(lines[2] == "");
}

TEST_CASE("KStr lines empty string returns single empty line") {
    KStr s("");
    auto lines = s.lines();
    REQUIRE(lines.size() == 0);
}

TEST_CASE("KStr lines with only newlines") {
    KStr s("\n\r\n\r"); // [1]\n[2]\r\n[3]\r[4]
    auto lines = s.lines();
    REQUIRE(lines.size() == 4);
    CHECK(lines[0] == "");
    CHECK(lines[1] == "");
    CHECK(lines[2] == "");
    CHECK(lines[3] == "");

    KStr s2("\n");
    auto lines2 = s2.lines();
    REQUIRE(lines2.size() == 2);
    CHECK(lines2[0] == "");
    CHECK(lines2[1] == "");
}

TEST_CASE("KStr match/match_indices basic and edge cases") {
    KStr s("a123bb4567cc89");

    auto is_digit = [](KChar ch) { return ch.is_digit(); };

    SUBCASE("match - continuous digit runs") {
        auto parts = s.match(is_digit);
        REQUIRE(parts.size() == 3);
        CHECK(parts[0] == "123");
        CHECK(parts[1] == "4567");
        CHECK(parts[2] == "89");
    }

    SUBCASE("match_indices - reports correct indices and substrings") {
        auto result = s.match_indices(is_digit);
        REQUIRE(result.size() == 3);
        CHECK(result[0].first == 1);
        CHECK(result[0].second == "123");
        CHECK(result[1].first == 6);
        CHECK(result[1].second == "4567");
        CHECK(result[2].first == 12);
        CHECK(result[2].second == "89");
    }

    SUBCASE("match - no match") {
        KStr s2("abcXYZ");
        auto parts = s2.match(is_digit);
        CHECK(parts.empty());
    }

    SUBCASE("match - full match") {
        KStr s3("123456");
        auto parts = s3.match(is_digit);
        REQUIRE(parts.size() == 1);
        CHECK(parts[0] == "123456");
    }

    SUBCASE("match - illegal UTF-8 ignored") {
        std::vector<uint8_t> raw = {'a', 0xff, '1', '2', '3', 0xff, 'b'};
        KStr s4(ByteSpan(raw.data(), raw.size()));
        auto parts = s4.match(is_digit);
        REQUIRE(parts.size() == 1);
        CHECK(parts[0].byte_size() == 3);
        CHECK(parts[0] == "123");
    }

    SUBCASE("basic ASCII digit match") {
        KStr s("ab123cd45e6fg");
        auto digits = s.match(is_digit);
        REQUIRE(digits.size() == 3);
        CHECK(digits[0] == "123");
        CHECK(digits[1] == "45");
        CHECK(digits[2] == "6");

        auto indices = s.match_indices(is_digit);
        REQUIRE(indices.size() == 3);
        CHECK(indices[0].first == 2);
        CHECK(indices[0].second == "123");
        CHECK(indices[1].first == 7);
        CHECK(indices[1].second == "45");
        CHECK(indices[2].first == 10);
        CHECK(indices[2].second == "6");
    }

    SUBCASE("unicode mixed digits") {
        KStr s("你123好456世界789");
        auto digits = s.match(is_digit);
        REQUIRE(digits.size() == 3);
        CHECK(digits[0] == "123");
        CHECK(digits[1] == "456");
        CHECK(digits[2] == "789");

        auto indices = s.match_indices(is_digit);
        REQUIRE(indices.size() == 3);
        CHECK(indices[0].first == 1);  // after "你"
        CHECK(indices[1].first == 5);  // after "好"
        CHECK(indices[2].first == 10); // after "界"
    }

    SUBCASE("no match found") {
        KStr s("abcdef");
        CHECK(s.match(is_digit).empty());
        CHECK(s.match_indices(is_digit).empty());
    }

    SUBCASE("everything matches") {
        KStr s("0123456789");
        auto all = s.match(is_digit);
        REQUIRE(all.size() == 1);
        CHECK(all[0] == "0123456789");

        auto indices = s.match_indices(is_digit);
        REQUIRE(indices.size() == 1);
        CHECK(indices[0].first == 0);
        CHECK(indices[0].second == "0123456789");
    }

    SUBCASE("empty string") {
        KStr s("");
        CHECK(s.match(is_digit).empty());
        CHECK(s.match_indices(is_digit).empty());
    }

    SUBCASE("interleaved invalid UTF-8 bytes") {
        std::vector<uint8_t> raw = {'a', 0xff, '1', '2', '3', 0xff, 'b'};
        KStr s(ByteSpan(raw.data(), raw.size()));

        auto digits = s.match(is_digit);
        REQUIRE(digits.size() == 1);
        CHECK(digits[0] == "123");

        auto indices = s.match_indices(is_digit);
        REQUIRE(indices.size() == 1);
        CHECK(indices[0].first == 2); // index after 'a'(0), 0xff(1), then '1'(2)
        CHECK(indices[0].second == "123");
    }
}

TEST_CASE("KStr trim functions") {
    SUBCASE("empty string") {
        KStr s("");
        CHECK(s.trim() == "");
        CHECK(s.trim_start() == "");
        CHECK(s.trim_end() == "");
    }

    SUBCASE("all ASCII whitespace") {
        KStr s(" \t\n\r ");
        CHECK(s.trim() == "");
        CHECK(s.trim_start() == "");
        CHECK(s.trim_end() == "");
    }

    SUBCASE("no whitespace") {
        KStr s("abc123");
        CHECK(s.trim() == "abc123");
        CHECK(s.trim_start() == "abc123");
        CHECK(s.trim_end() == "abc123");
    }

    SUBCASE("whitespace at front") {
        KStr s(" \t\nabc");
        CHECK(s.trim_start() == "abc");
        CHECK(s.trim_end() == " \t\nabc");
        CHECK(s.trim() == "abc");
    }

    SUBCASE("whitespace at end") {
        KStr s("abc  \n");
        CHECK(s.trim_end() == "abc");
        CHECK(s.trim_start() == "abc  \n");
        CHECK(s.trim() == "abc");
    }

    SUBCASE("whitespace at both ends") {
        KStr s(" \tabc\n ");
        CHECK(s.trim() == "abc");
    }

    SUBCASE("internal whitespace not trimmed") {
        KStr s("ab\t \n cd");
        CHECK(s.trim() == "ab\t \n cd");
    }

    SUBCASE("UTF-8 whitespace - U+3000 IDEOGRAPHIC SPACE") {
        // UTF-8 encoding of U+3000 is 0xE3 0x80 0x80
        KStr s("\u3000abc\u3000");
        CHECK(s.trim() == "abc");
    }

    SUBCASE("trim_matches - remove dashes") {
        KStr s("---abc---");
        auto trimmed = s.trim_matches([](KChar c) { return c.to_char() == '-'; });
        CHECK(trimmed == "abc");
    }

    SUBCASE("trim_start_matches only") {
        KStr s("***data**");
        auto trimmed = s.trim_start_matches([](KChar c) { return c.to_char() == '*'; });
        CHECK(trimmed == "data**");
    }

    SUBCASE("trim_end_matches only") {
        KStr s("==hello==");
        auto trimmed = s.trim_end_matches([](KChar c) { return c.to_char() == '='; });
        CHECK(trimmed == "==hello");
    }

    SUBCASE("trim_matches with UTF-8 predicate") {
        // Remove fullwidth space (U+3000)
        KStr s("\u3000\u3000hello\u3000");
        auto trimmed = s.trim_matches([](KChar c) { return c.value() == 0x3000; });
        CHECK(trimmed == "hello");
    }

    SUBCASE("trim_matches with illegal UTF-8 in middle") {
        std::vector<uint8_t> raw = {'-', '-', 'x', 0xff, 'y', '-', '-'};
        KStr s(ByteSpan(raw.data(), raw.size()));
        auto trimmed = s.trim_matches([](KChar c) { return c.value() == '-'; });
        CHECK(trimmed.as_bytes().size() > 0);
        CHECK(trimmed.as_bytes()[0] == 'x'); // 不 trim 中间非法字节
    }
}

TEST_CASE("KStr::strip_prefix and strip_suffix") {
    SUBCASE("empty string and empty prefix/suffix") {
        KStr s("");
        CHECK(s.strip_prefix("") == "");
        CHECK(s.strip_suffix("") == "");
    }

    SUBCASE("strip_prefix - match") {
        KStr s("hello world");
        CHECK(s.strip_prefix("hello") == " world");
        CHECK(s.strip_prefix("hello ") == "world");
    }

    SUBCASE("strip_prefix - no match") {
        KStr s("hello world");
        CHECK(s.strip_prefix("world") == s);  // no change
        CHECK(s.strip_prefix("heLLo") == s);  // case sensitive
        CHECK(s.strip_prefix("helloo") == s); // longer than prefix
    }

    SUBCASE("strip_suffix - match") {
        KStr s("hello world");
        CHECK(s.strip_suffix("world") == "hello ");
        CHECK(s.strip_suffix(" world") == "hello");
    }

    SUBCASE("strip_suffix - no match") {
        KStr s("hello world");
        CHECK(s.strip_suffix("hello") == s);
        CHECK(s.strip_suffix("WORLD") == s);
        CHECK(s.strip_suffix("worldd") == s);
    }

    SUBCASE("strip_prefix and strip_suffix do not repeat") {
        KStr s("xxxabcxxx");
        CHECK(s.strip_prefix("xxx") == "abcxxx");                     // 只裁一次
        CHECK(s.strip_suffix("xxx") == "xxxabc");                     // 只裁一次
        CHECK(s.strip_prefix("xxx").strip_prefix("xxx") == "abcxxx"); // 仍保留后缀
    }

    SUBCASE("UTF-8 prefix/suffix match") {
        KStr s("你好世界");
        CHECK(s.strip_prefix("你好") == "世界");
        CHECK(s.strip_suffix("世界") == "你好");
    }

    SUBCASE("UTF-8 prefix/suffix mismatch") {
        KStr s("你好世界");
        CHECK(s.strip_prefix("你a") == s);
        CHECK(s.strip_suffix("界a") == s);
    }

    SUBCASE("illegal UTF-8 in prefix/suffix match") {
        std::vector<uint8_t> raw = {0xff, 0xfe, 'a', 'b', 'c', 0xff};
        KStr s(ByteSpan(raw.data(), raw.size()));
        KStr prefix(ByteSpan(raw.data(), 2)); // 0xff 0xfe
        KStr suffix(ByteSpan(&raw[5], 1));    // 0xff
        KStr middle(ByteSpan(&raw[2], 3));    // "abc"

        CHECK(s.strip_prefix(prefix).strip_suffix(suffix) == middle);
    }

    SUBCASE("partial match fails") {
        KStr s("abcdef");
        CHECK(s.strip_prefix("abcdeff") == s); // suffix longer than string
        CHECK(s.strip_suffix("bcdeff") == s);  // not matching end
    }
}
