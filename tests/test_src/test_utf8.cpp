#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <doctest/doctest.h>
#include "../../include/utf8.hpp"
#include "base.hpp"

using namespace utf8;

TEST_CASE("lead_utf8_length") {
    CHECK(lead_utf8_length(0x00) == 1);
    CHECK(lead_utf8_length(0xC2) == 2);
    CHECK(lead_utf8_length(0xE0) == 3);
    CHECK(lead_utf8_length(0xF0) == 4);
    CHECK(lead_utf8_length(0xFF) == 0);
}

TEST_CASE("codepoint_utf8_size - all branches") {
    CHECK(utf8_size(0x00) == 1);     // ASCII
    CHECK(utf8_size(0x07FF) == 2);   // two-byte upper limit
    CHECK(utf8_size(0x0800) == 3);   // start of 3-byte
    CHECK(utf8_size(0xFFFF) == 3);   // max of 3-byte
    CHECK(utf8_size(0x10000) == 4);  // start of 4-byte
    CHECK(utf8_size(0x10FFFF) == 4); // max Unicode
    CHECK(utf8_size(0x110000) == 0); // invalid > 0x10FFFF
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
    CHECK(first_invalid(valid) == -1);
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
    UTF8Decoded decode_result = decode_one(data, 0);
    CHECK(decode_result.ok);
    CHECK(decode_result.codepoint == cp);
    CHECK(decode_result.next_pos == 3);

    UTF8Encoded enc2 = encode(0x07FF); // 2 bytes
    CHECK(enc2.len == 2);
    CHECK(enc2.bytes[0] == (0xC0 | (0x07FF >> 6)));

    UTF8Encoded enc4 = encode(0x1F600); // 😀 U+1F600
    CHECK(enc4.len == 4);
    CHECK(enc4.bytes[0] == (0xF0 | (0x1F600 >> 18)));
}

TEST_CASE("decode_prev") {
    const std::vector<uint8_t> valid = {
        'a', // 0
        0xe4,
        0xbd,
        0xa0, // 1~3 '你'
        0xe5,
        0xa5,
        0xbd, // 4~6 '好'
        0xf0,
        0x9f,
        0x98,
        0x80, // 7~10 😀
        'b'   // 11
    };

    SUBCASE("basic backward decoding") {
        auto span = ByteSpan(valid.data(), valid.size());

        auto dec1 = decode_one_prev(span, 1);
        REQUIRE(dec1.ok);
        CHECK(dec1.codepoint == 'a');
        CHECK(dec1.next_pos == 0);

        auto dec2 = decode_one_prev(span, 4);
        REQUIRE(dec2.ok);
        CHECK(dec2.codepoint == 0x4F60); // '你'
        CHECK(dec2.next_pos == 1);

        auto dec3 = decode_one_prev(span, 7);
        REQUIRE(dec3.ok);
        CHECK(dec3.codepoint == 0x597D); // '好'
        CHECK(dec3.next_pos == 4);

        auto dec4 = decode_one_prev(span, 11);
        REQUIRE(dec4.ok);
        CHECK(dec4.codepoint == 0x1F600); // 😀
        CHECK(dec4.next_pos == 7);

        auto dec5 = decode_one_prev(span, 12);
        REQUIRE(dec5.ok);
        CHECK(dec5.codepoint == 'b');
        CHECK(dec5.next_pos == 11);
    }

    SUBCASE("invalid: pos == 0") {
        std::vector<uint8_t> data = {'x'};
        auto dec = decode_one_prev(ByteSpan(data.data(), data.size()), 0);
        CHECK(! dec.ok);
    }

    SUBCASE("invalid continuation byte only") {
        std::vector<uint8_t> raw = {0xa0}; // continuation without leading
        auto dec = decode_one_prev(ByteSpan(raw.data(), raw.size()), 1);
        CHECK(! dec.ok);
    }

    SUBCASE("truncated multibyte char") {
        std::vector<uint8_t> raw = {0xe4, 0xbd}; // incomplete 3-byte char
        auto dec = decode_one_prev(ByteSpan(raw.data(), raw.size()), 2);
        CHECK(! dec.ok);
    }

    SUBCASE("multiple continuation bytes without lead") {
        std::vector<uint8_t> raw = {0x80, 0x80, 0x80};
        auto dec = decode_one_prev(ByteSpan(raw.data(), raw.size()), 3);
        CHECK(! dec.ok);
    }

    SUBCASE("recovery from malformed followed by good char") {
        std::vector<uint8_t> raw = {0x80, 0xe4, 0xbd, 0xa0};
        auto dec = decode_one_prev(ByteSpan(raw.data(), raw.size()), 4);
        REQUIRE(dec.ok);
        CHECK(dec.codepoint == 0x4F60); // '你'
        CHECK(dec.next_pos == 1);
    }

    SUBCASE("ASCII only") {
        std::string ascii = "abc";
        auto span = ByteSpan(reinterpret_cast<const uint8_t*>(ascii.data()), ascii.size());
        auto dec = decode_one_prev(span, 3);
        REQUIRE(dec.ok);
        CHECK(dec.codepoint == 'c');
        CHECK(dec.next_pos == 2);
    }

    SUBCASE("2-byte valid + ASCII + 2 invalid bytes") {
        std::vector<uint8_t> raw = {
            0xc3,
            0xa9, // é (U+00E9), valid 2-byte UTF-8
            'x',  // ASCII
            0xff,
            0xff // invalid continuation-like garbage
        };
        auto span = ByteSpan(raw.data(), raw.size());

        auto dec1 = decode_one_prev(span, 5);
        REQUIRE(! dec1.ok);
        auto dec2 = decode_one_prev(span, dec1.next_pos);
        REQUIRE(! dec2.ok);

        auto dec3 = decode_one_prev(span, dec2.next_pos);
        REQUIRE(dec3.ok);
        CHECK(dec3.codepoint == 'x');
        CHECK(dec3.next_pos == 2);

        // decode_prev at pos=2 -> é
        auto dec4 = decode_one_prev(span, dec3.next_pos);
        REQUIRE(dec4.ok);
        CHECK(dec4.codepoint == 0x00E9);
        CHECK(dec4.next_pos == 0);
    }
}

TEST_CASE("decode detail") {
    ByteVec data = {static_cast<Byte>(0xC3), static_cast<Byte>(0xA9)}; // é
    UTF8Decoded decode_result = decode_one(data, 0);
    CHECK(decode_result.ok);
    CHECK(decode_result.codepoint == 0xE9); // 'é'
    CHECK(decode_result.next_pos == 2);

    ByteVec data2 = {static_cast<Byte>(0xFF)}; // invalid
    UTF8Decoded decode_result2 = decode_one(data2, 0);
    CHECK_FALSE(decode_result2.ok);

    ByteVec data3 = {static_cast<Byte>(0xE0), static_cast<Byte>(0x20), static_cast<Byte>(0x20)}; // second byte invalid
    UTF8Decoded decode_result3 = decode_one(data3, 0);
    CHECK_FALSE(decode_result3.ok);
}

TEST_CASE("char_count") {
    ByteVec data = {'a', 0xE4, 0xBD, 0xA0, 0xF0, 0x9F, 0x98, 0x81}; // a你😁
    CHECK(char_count(data) == 3);
}

TEST_CASE("find") {
    ByteVec data = {'a', 0xE4, 0xBD, 0xA0}; // a你
    CHECK(find_codepoint(data, 'a') == 0);
    CHECK(find_codepoint(data, 0x4F60) == 1);
    CHECK(find_codepoint(data, 'x') == kstring::knpos);
}

TEST_CASE("replace_at") {
    ByteVec data = {'a', 0xE4, 0xBD, 0xA0}; // a你
    replace_at(data, 0, 0x4F60);            // 'a' -> '你'
    CHECK(data.size() == 6);
    CHECK(find_codepoint(data, 0x4F60) == 0);

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
    CHECK(find_codepoint(data, 0x4F60) == 0);

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
    UTF8Decoded decode_result;
    decode_result = decode_one(data, 0);
    CHECK(decode_result.ok);
    CHECK(decode_result.codepoint == 'A');
    decode_result = decode_one(data, decode_result.next_pos);
    CHECK(decode_result.ok);                   // 😀
    CHECK(decode_result.codepoint == 0x1F600); // 😀
    decode_result = decode_one(data, decode_result.next_pos);
    CHECK(decode_result.ok);
    CHECK(decode_result.codepoint == 'C');
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

    SUBCASE("ASCII - 'A'") {
        ByteVec data = {'A'};
        UTF8Decoded decode_result = decode_one(data, 0);
        CHECK(decode_result.ok);
        CHECK(decode_result.codepoint == 'A');
    }

    SUBCASE("2-byte character - U+00A9 (©)") {
        ByteVec data = {0xC2, 0xA9}; // ©
        UTF8Decoded decode_result = decode_one(data, 0);
        CHECK(decode_result.ok);
        CHECK(decode_result.codepoint == 0x00A9);
    }

    SUBCASE("3-byte character - U+20AC (€)") {
        ByteVec data = {0xE2, 0x82, 0xAC}; // €
        UTF8Decoded decode_result = decode_one(data, 0);
        CHECK(decode_result.ok);
        CHECK(decode_result.codepoint == 0x20AC);
    }

    SUBCASE("4-byte character - U+1F601 (😁)") {
        ByteVec data = {0xF0, 0x9F, 0x98, 0x81}; // 😁
        UTF8Decoded decode_result = decode_one(data, 0);
        CHECK(decode_result.ok);
        CHECK(decode_result.codepoint == 0x1F601);
    }
}

TEST_CASE("is_ascii") {
    ByteVec ascii = {'h', 'e', 'l', 'l', 'o'};
    ByteVec non_ascii = {0xE4, 0xBD, 0xA0};
    CHECK(is_all_ascii(ascii));
    CHECK_FALSE(is_all_ascii(non_ascii));
}

TEST_CASE("debug_codepoint") {
    std::string result = codepoint_to_string(0x4F60);
    bool valid_result = (result == "\xE4\xBD\xA0") || (result == "\\xE4\\xBD\\xA0");
    CHECK(valid_result);
}

TEST_CASE("decode_all - basic ascii and utf8") {
    ByteVec data = {'h', 'e', static_cast<Byte>(0xE4), static_cast<Byte>(0xBD), static_cast<Byte>(0xA0)}; // he你
    auto result = decode_all(data);
    REQUIRE(result.size() == 3);
    CHECK(result[0] == 'h');
    CHECK(result[1] == 'e');
    CHECK(result[2] == 0x4F60); // 你
}

TEST_CASE("decode_all - illegal bytes in middle") {
    ByteVec data = {'A', 0xFF, 'B'};
    auto result = decode_all(data);
    REQUIRE(result.size() == 3);
    CHECK(result[0] == 'A');
    CHECK(result[1] == kstring::ILL_CODEPOINT); // 0xFF is illegal lead
    CHECK(result[2] == 'B');
}

TEST_CASE("decode_all - continuation byte alone") {
    ByteVec data = {static_cast<Byte>(0x80)}; // continuation without leader
    auto result = decode_all(data);
    REQUIRE(result.size() == 1);
    CHECK(result[0] == kstring::ILL_CODEPOINT);
}

TEST_CASE("decode_all - overlong encoding") {
    ByteVec data = {static_cast<Byte>(0xC0), static_cast<Byte>(0xAF)}; // overlong '/'
    auto result = decode_all(data);
    REQUIRE(result.size() == 1); // 一个非法字节(overlang)
    CHECK(result[0] == kstring::ILL_CODEPOINT);
}

TEST_CASE("decode_all - empty input") {
    ByteVec data;
    auto result = decode_all(data);
    CHECK(result.empty());
}

TEST_CASE("decode_range - normal range") {
    ByteVec data = {'x', static_cast<Byte>(0xE4), static_cast<Byte>(0xBD), static_cast<Byte>(0xA0), 'y'}; // x你y
    auto result = decode_range(data, 0, data.size());
    REQUIRE(result.size() == 3);
    CHECK(result[0] == 'x');
    CHECK(result[1] == 0x4F60);
    CHECK(result[2] == 'y');
}

TEST_CASE("decode_range - normal range") {
    ByteVec data = {'x', static_cast<Byte>(0xFF), 'y'}; // x<?>y
    auto result = decode_range(data, 0, data.size());
    REQUIRE(result.size() == 3);
    CHECK(result[0] == 'x');
    CHECK(result[1] == kstring::ILL_CODEPOINT);
    CHECK(result[2] == 'y');
}

TEST_CASE("decode_range - truncated at utf8 boundary") {
    ByteVec data = {'a',
                    static_cast<Byte>(0xF0),
                    static_cast<Byte>(0x9F),
                    static_cast<Byte>(0x98),
                    static_cast<Byte>(0x81)}; // a😁
    auto result = decode_range(data, 0, 4);   // only a + first 3 bytes of 😁
    REQUIRE(result.size() == 2);
    CHECK(result[0] == 'a');
    CHECK(result[1] == kstring::ILL_CODEPOINT); // 😁 被截断了

    auto result2 = decode_range(data, 0, 5); // a + 😁
    REQUIRE(result2.size() == 2);
    CHECK(result2[0] == 'a');
    CHECK(codepoint_to_string(result2[1]) == "😁"); // 😁 未被截断
}

TEST_CASE("decode_range - start > end") {
    ByteVec data = {'a', 'b', 'c'};
    auto result = decode_range(data, 3, 2); // invalid range
    CHECK(result.empty());
}

TEST_CASE("decode_range - end beyond data") {
    ByteVec data = {'A', 'B'};
    auto result = decode_range(data, 0, 100);
    REQUIRE(result.size() == 2);
    CHECK(result[0] == 'A');
    CHECK(result[1] == 'B');
}

TEST_CASE("decode_range - start beyond data") {
    ByteVec data = {'A'};
    auto result = decode_range(data, 100, 200);
    CHECK(result.empty());
}

TEST_CASE("print UTF8Encoded & UTF8Decoded") {
    UTF8Encoded printable = encode(0x4F60);
    std::ostringstream oss;
    oss << printable;
    CHECK(oss.str() == "UTF8Encoded{len=3, bytes=[0xE4 0xBD 0xA0]}");

    UTF8Decoded ok(0x4F60, 3); // '你'
    UTF8Decoded err(0, false, 0);
    std::ostringstream oss1, oss2;
    oss1 << ok;
    oss2 << err;
    CHECK(oss1.str() == "UTF8Decoded{cp=U+4F60, next_pos=3, ok=true}");
    CHECK(oss2.str() == "UTF8Decoded{<invalid>}");
}

TEST_CASE("decode_all normal completely testing") {
    // warning: to_bytes 存在悬垂指针风险。是否安全，完全取决于调用者如何使用 ByteSpan
    // initializer_list 作为 lambda 参数传入的，它的生命周期会延续到 lambda 执行完毕
    auto to_bytes = [](std::initializer_list<uint8_t> list) { return ByteSpan(list.begin(), list.size()); };

    SUBCASE("Empty input") {
        auto result = decode_all(to_bytes({}));
        CHECK(result.empty());
    }

    SUBCASE("Valid ASCII") {
        auto result = decode_all(to_bytes({'H', 'e', 'l', 'l', 'o'}));
        REQUIRE(result.size() == 5);
        for (int i = 0; i < 5; ++i) {
            CHECK(result[i] == static_cast<uint8_t>("Hello"[i]));
        }
    }

    SUBCASE("Valid multi-byte: © € 😀") {
        // ©(U+00A9), €(U+20AC), 😀(U+1F600)
        auto result = decode_all(to_bytes({
            0xC2,
            0xA9, // ©
            0xE2,
            0x82,
            0xAC, // €
            0xF0,
            0x9F,
            0x98,
            0x80 // 😀
        }));
        REQUIRE(result.size() == 3);
        CHECK(result[0] == 0xA9);
        CHECK(result[1] == 0x20AC);
        CHECK(result[2] == 0x1F600);
    }

    SUBCASE("Invalid UTF-8: overlong encoding of ASCII") {
        // U+0000 encoded with 2 bytes: 0xC0 0x80 — invalid
        auto result = decode_all(ByteSpan({0xC0, 0x80}));
        REQUIRE(result.size() == 1);
        CHECK(result[0] == kstring::ILL_CODEPOINT); // invalid
    }

    SUBCASE("Invalid continuation byte only") {
        // 0x80 without a leading byte
        auto result = decode_all(ByteSpan({0x80}));
        REQUIRE(result.size() == 1);
        CHECK(result[0] == kstring::ILL_CODEPOINT);
    }

    SUBCASE("Truncated 3-byte character") {
        // 0xE2, 0x82 — should be followed by 0xAC (Euro sign)
        auto result = decode_all(to_bytes({0xE2, 0x82}));
        REQUIRE(result.size() == 1);
        CHECK(result[0] == kstring::ILL_CODEPOINT); // 0xE2 0x82 (0xAC)
    }

    SUBCASE("Mixed valid and invalid sequences") {
        auto result = decode_all(to_bytes({
            'A', // valid
            0xE2,
            0x82,
            0xAC, // €
            0xFF, // invalid byte
            'B',
            0xF0,
            0x9F,
            0x98,
            0x80, // 😀
            0xC0,
            0xAF // invalid overlong
        }));
        REQUIRE(result.size() == 6);
        CHECK(result[0] == 'A');
        CHECK(result[1] == 0x20AC);
        CHECK(result[2] == kstring::ILL_CODEPOINT);
        CHECK(result[3] == 'B');
        CHECK(result[4] == 0x1F600);
        CHECK(result[5] == kstring::ILL_CODEPOINT);
    }

    SUBCASE("Valid + continuation noise") {
        auto result = decode_all(to_bytes({
            0xE2,
            0x82,
            0xAC, // €
            0x80, // invalid continuation
            0xF0,
            0x9F,
            0x98,
            0x80 // 😀
        }));
        REQUIRE(result.size() == 3);
        CHECK(result[0] == 0x20AC);
        CHECK(result[1] == kstring::ILL_CODEPOINT); // 0x80 is invalid alone
        CHECK(result[2] == 0x1F600);
    }
}

TEST_CASE("decode illegal completely testing") {
    auto to_bytes = [](std::initializer_list<uint8_t> list) { return ByteSpan(list.begin(), list.size()); };

    SUBCASE("Illegal: overlong encoding of ASCII") {
        auto result = decode_all(ByteSpan({
            0xC1,
            0x81 // U+0041 ('A') encoded in 2 bytes (overlong)
        }));
        REQUIRE(result.size() == 1);
        CHECK(result[0] == kstring::ILL_CODEPOINT); // overlong
    }

    SUBCASE("Illegal: surrogate code point (U+D800)") {
        // U+D800 = 0xED 0xA0 0x80 is not valid in UTF-8
        auto result = decode_all(to_bytes({0xED, 0xA0, 0x80}));
        REQUIRE(result.size() == 1);
        CHECK(result[0] == kstring::ILL_CODEPOINT);
    }

    SUBCASE("Illegal: lone continuation byte") {
        auto result = decode_all(to_bytes({0x80, 0xBF}));
        REQUIRE(result.size() == 2);
        CHECK(result[0] == kstring::ILL_CODEPOINT);
        CHECK(result[1] == kstring::ILL_CODEPOINT);
    }

    SUBCASE("Illegal: lone leading byte") {
        auto result = decode_all(to_bytes({0xE2}));
        REQUIRE(result.size() == 1);
        CHECK(result[0] == kstring::ILL_CODEPOINT);
    }

    SUBCASE("Illegal: truncated 4-byte sequence") {
        auto result = decode_all(to_bytes({
            0xF0,
            0x9F,
            0x98 // Should be 4 bytes but not
        }));
        REQUIRE(result.size() == 1); // 单个被截断的序列
        CHECK(result[0] == kstring::ILL_CODEPOINT);
    }

    SUBCASE("Illegal: invalid codepoint > U+10FFFF") {
        // F8-FF are not legal UTF-8 leading bytes
        auto result = decode_all(to_bytes({
            0xF8,
            0x88,
            0x80,
            0x80,
            0x80 // 5-byte sequence
        }));
        for (const auto& d : result) CHECK(d == kstring::ILL_CODEPOINT);
    }

    SUBCASE("Mixed valid + illegal (ASCII + overlong + valid)") {
        auto result = decode_all(to_bytes({'X',
                                           0xC0,
                                           0x80, // overlong for null
                                           'Y',
                                           0xED,
                                           0xA0,
                                           0x80, // surrogate
                                           'Z'}));
        REQUIRE(result.size() == 5);
        CHECK(result[0] == 'X');
        CHECK(result[1] == kstring::ILL_CODEPOINT); // 0xC0 0x80
        CHECK(result[2] == 'Y');
        CHECK(result[3] == kstring::ILL_CODEPOINT); // 0xED
        CHECK(result[4] == 'Z');
    }

    SUBCASE("Garbage bytes mixed in valid UTF-8 stream") {
        auto result = decode_all(to_bytes({
            0xE4,
            0xBD,
            0xA0, // 你 (U+4F60)
            0xFF,
            0xFE, // invalid
            0xE5,
            0xA5,
            0xBD // 好 (U+597D)
        }));
        REQUIRE(result.size() == 4);
        CHECK(result[0] == 0x4F60);
        CHECK(result[1] == kstring::ILL_CODEPOINT); // 0xFF
        CHECK(result[2] == kstring::ILL_CODEPOINT); // 0xFE
        CHECK(result[3] == 0x597D);
    }
}
