#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <doctest/doctest.h>
#include "../../include/internal/utf8.hpp" // 替换为你的实际头文件路径

using namespace internal::utf8;

TEST_CASE("lead_utf8_length") {
    CHECK(lead_utf8_length(0x00) == 1);
    CHECK(lead_utf8_length(0xC2) == 2);
    CHECK(lead_utf8_length(0xE0) == 3);
    CHECK(lead_utf8_length(0xF0) == 4);
    CHECK(lead_utf8_length(0xFF) == 0);
}

TEST_CASE("codepoint_utf8_size - all branches") {
    CHECK(codepoint_utf8_size(0x00) == 1);     // ASCII
    CHECK(codepoint_utf8_size(0x07FF) == 2);   // two-byte upper limit
    CHECK(codepoint_utf8_size(0x0800) == 3);   // start of 3-byte
    CHECK(codepoint_utf8_size(0xFFFF) == 3);   // max of 3-byte
    CHECK(codepoint_utf8_size(0x10000) == 4);  // start of 4-byte
    CHECK(codepoint_utf8_size(0x10FFFF) == 4); // max Unicode
    CHECK(codepoint_utf8_size(0x110000) == 0); // invalid > 0x10FFFF
}

TEST_CASE("is_valid_range") {
    ByteVec valid = {0xE4, 0xBD, 0xA0}; // "你"
    CHECK(is_valid_range(valid, 0, 3));

    ByteVec overlong = {0xC0, 0xAF}; // overlong '/'
    CHECK_FALSE(is_valid_range(overlong, 0, 2));

    ByteVec surrogate = {0xED, 0xA0, 0x80}; // surrogate
    CHECK_FALSE(is_valid_range(surrogate, 0, 3));
}

TEST_CASE("first_invalid and is_valid") {
    ByteVec valid = {'h', 'e', 0xE4, 0xBD, 0xA0}; // "he你"
    CHECK(first_invalid(valid) == valid.size());
    CHECK(is_valid(valid));

    valid.push_back(0xFF);
    CHECK(first_invalid(valid) == valid.size() - 1);
    CHECK_FALSE(is_valid(valid));
}

TEST_CASE("encode and decode") {
    CodePoint cp = 0x4F60; // '你'
    UTF8Encoded encoded = encode(cp);
    CHECK(encoded.len == 3);
    CHECK(encoded.bytes[0] == 0xE4);

    ByteVec data(encoded.begin(), encoded.end());
    CodePoint decoded;
    std::size_t next;
    CHECK(decode(data, 0, decoded, next));
    CHECK(decoded == cp);
    CHECK(next == 3);

    UTF8Encoded enc2 = encode(0x07FF); // 2 bytes
    CHECK(enc2.len == 2);
    CHECK(enc2.bytes[0] == (0xC0 | (0x07FF >> 6)));

    UTF8Encoded enc4 = encode(0x1F600); // 😀 U+1F600
    CHECK(enc4.len == 4);
    CHECK(enc4.bytes[0] == (0xF0 | (0x1F600 >> 18)));
}

TEST_CASE("decode detail") {
    ByteVec data = {static_cast<Byte>(0xC3), static_cast<Byte>(0xA9)}; // é
    CodePoint cp;
    std::size_t next;
    CHECK(decode(data, 0, cp, next));
    CHECK(cp == 0xE9); // 'é'
    CHECK(next == 2);

    ByteVec data2 = {static_cast<Byte>(0xFF)}; // invalid
    CodePoint cp2;
    std::size_t next2;
    CHECK_FALSE(decode(data2, 0, cp2, next2));

    ByteVec data3 = {static_cast<Byte>(0xE0), static_cast<Byte>(0x20), static_cast<Byte>(0x20)}; // second byte invalid
    CodePoint cp3;
    std::size_t next3;
    CHECK_FALSE(decode(data3, 0, cp3, next3));
}

TEST_CASE("char_count") {
    ByteVec data = {'a', 0xE4, 0xBD, 0xA0, 0xF0, 0x9F, 0x98, 0x81}; // a你😁
    CHECK(char_count(data) == 3);
}

TEST_CASE("find") {
    ByteVec data = {'a', 0xE4, 0xBD, 0xA0}; // a你
    CHECK(find(data, 'a') == 0);
    CHECK(find(data, 0x4F60) == 1);
    CHECK(find(data, 'x') == static_cast<std::size_t>(-1));
}

TEST_CASE("replace_at") {
    ByteVec data = {'a', 0xE4, 0xBD, 0xA0}; // a你
    replace_at(data, 0, 0x4F60);            // 'a' -> '你'
    CHECK(data.size() == 6);
    CHECK(find(data, 0x4F60) == 0);

    ByteVec data2 = {'x'};
    replace_at(data2, 0, 'y');
    CHECK(data2.size() == 1);
    CHECK(data2[0] == 'y');

    ByteVec data3 = {static_cast<Byte>(0xE4), static_cast<Byte>(0xBD), static_cast<Byte>(0xA0)}; // 你
    replace_at(data3, 0, 'A');                                                                   // 3-byte -> 1-byte
    CHECK(data3.size() == 1);
    CHECK(data3[0] == 'A');
}

TEST_CASE("replace_first") {
    ByteVec data = {'h', 0xE4, 0xBD, 0xA0}; // h你
    replace_first(data, 'h', 0x4F60);
    CHECK(data.size() > 1);
    CHECK(find(data, 0x4F60) == 0);

    ByteVec data2 = {'a', 'b', 'c'};
    replace_first(data2, 'x', 'y'); // not found
    CHECK(data2 == ByteVec({'a', 'b', 'c'}));
}

TEST_CASE("replace_all") {
    ByteVec data = {'a', 'b', 'a', 'c'};
    replace_all(data, 'a', 'z');
    CHECK(data[0] == 'z');
    CHECK(data[2] == 'z');
}

TEST_CASE("replace_all - diff > 0 (1-byte to 4-byte)") {
    ByteVec data = {'A', 'B', 'C'};
    replace_all(data, 'B', 0x1F600); // 😀
    CHECK(data.size() > 3);          // 变长
    std::size_t next;
    CodePoint cp;
    CHECK(decode(data, 0, cp, next));
    CHECK(cp == 'A');
    CHECK(decode(data, next, cp, next)); // 😀
    CHECK(cp == 0x1F600);                // 😀
    CHECK(decode(data, next, cp, next));
    CHECK(cp == 'C');
}

TEST_CASE("replace_all - diff < 0 (4-byte to 1-byte)") {
    ByteVec data = {static_cast<Byte>(0xF0),
                    static_cast<Byte>(0x9F),
                    static_cast<Byte>(0x98),
                    static_cast<Byte>(0x81), // 😁
                    'X'};
    replace_all(data, 0x1F601, 'Z'); // 😁 → Z

    CHECK(data.size() == 2); // 1 byte Z + 1 byte X
    CHECK(data[0] == 'Z');
    CHECK(data[1] == 'X');
}

TEST_CASE("decode_one: valid UTF-8 single characters") {
    CodePoint cp;

    SUBCASE("ASCII - 'A'") {
        ByteVec data = {'A'};
        CHECK(decode_one(data, 0, cp));
        CHECK(cp == 'A');
    }

    SUBCASE("2-byte character - U+00A9 (©)") {
        ByteVec data = {0xC2, 0xA9}; // ©
        CHECK(decode_one(data, 0, cp));
        CHECK(cp == 0x00A9);
    }

    SUBCASE("3-byte character - U+20AC (€)") {
        ByteVec data = {0xE2, 0x82, 0xAC}; // €
        CHECK(decode_one(data, 0, cp));
        CHECK(cp == 0x20AC);
    }

    SUBCASE("4-byte character - U+1F601 (😁)") {
        ByteVec data = {0xF0, 0x9F, 0x98, 0x81}; // 😁
        CHECK(decode_one(data, 0, cp));
        CHECK(cp == 0x1F601);
    }
}

TEST_CASE("decode_one: invalid sequences") {
    CodePoint cp;

    SUBCASE("Invalid continuation byte") {
        ByteVec data = {0xE2, 0x28, 0xA1}; // Invalid continuation
        CHECK_FALSE(decode_one(data, 0, cp));
    }

    SUBCASE("Overlong encoding of ASCII 'A'") {
        ByteVec data = {0xC1, 0x81}; // Overlong encoding of U+0041
        CHECK_FALSE(decode_one(data, 0, cp));
    }

    SUBCASE("Truncated input") {
        ByteVec data = {0xF0}; // 4-byte start but missing rest
        CHECK_FALSE(decode_one(data, 0, cp));
    }

    SUBCASE("Surrogate range (invalid in UTF-8)") {
        ByteVec data = {0xED, 0xA0, 0x80}; // U+D800
        CHECK_FALSE(decode_one(data, 0, cp));
    }
}

TEST_CASE("is_ascii") {
    ByteVec ascii = {'h', 'e', 'l', 'l', 'o'};
    ByteVec non_ascii = {0xE4, 0xBD, 0xA0};
    CHECK(is_ascii(ascii));
    CHECK_FALSE(is_ascii(non_ascii));
}

TEST_CASE("debug_codepoint") {
    std::string result = debug_codepoint(0x4F60);
    bool valid_result = (result == "\xE4\xBD\xA0") || (result == "\\xE4\\xBD\\xA0");
    CHECK(valid_result);
}

