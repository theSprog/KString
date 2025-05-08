#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "../../include/kchar.hpp"

using KString::KChar;

TEST_CASE("KChar constructor and value") {
    KChar a('A');
    CHECK(a.value() == 0x41);
    KChar b(0x03A3);
    CHECK(b.value() == 0x03A3);
    KChar emoji("😁");
    CHECK(emoji.value() == 0x1F601);

    CHECK_THROWS_AS(KChar(nullptr), std::invalid_argument);
    CHECK_THROWS_AS(KChar(0x110000), std::invalid_argument);
    CHECK_THROWS_AS(KChar("\xC0\x80"), std::invalid_argument); // Overlong encoding
}

TEST_CASE("KChar::is_ascii and related checks") {
    KChar a('A');
    CHECK(a.is_ascii());
    CHECK(a.is_alpha());
    CHECK(a.is_upper());
    CHECK(! a.is_lower());
    CHECK(! a.is_digit());
    CHECK(a.is_alnum());
    CHECK(! a.is_whitespace());
    CHECK(a.is_printable());

    KChar d('0');
    CHECK(d.is_digit());
    CHECK(d.is_alnum());

    KChar s(' ');
    CHECK(s.is_whitespace());

    KChar lf('\n');
    CHECK(lf.is_whitespace());

    KChar euro(0x20AC); // €
    CHECK(! euro.is_ascii());
    CHECK(! euro.is_alpha());
    CHECK(! euro.is_printable());
}

TEST_CASE("KChar::to_ascii_char") {
    KChar a('A');
    CHECK(a.to_char() == 'A');

    KChar non_ascii(0x20AC);
    CHECK_THROWS_AS(non_ascii.to_char(), std::runtime_error);
}

TEST_CASE("KChar::to_upper / to_lower") {
    KChar a('a');
    CHECK(a.to_upper().value() == 'A');
    KChar A('A');
    CHECK(A.to_lower().value() == 'a');

    KChar z('Z');
    CHECK(z.to_upper().value() == 'Z'); // already upper
    CHECK(z.to_lower().value() == 'z');

    KChar non_ascii(0x20AC);
    CHECK(non_ascii.to_upper().value() == 0x20AC);
}

TEST_CASE("KChar::to_utf8string") {
    KChar a('A');
    CHECK(a.to_utf8string() == "A");

    KChar euro(0x20AC);
    CHECK(euro.to_utf8string() == "\xE2\x82\xAC"); // encoded € (U+20AC)

    KChar emoji("😁");
    CHECK(emoji.to_utf8string() == "😁");
}

TEST_CASE("KChar::utf8_size and debug_hex") {
    CHECK(KChar('A').utf8_size() == 1);
    CHECK(KChar(0x7FF).utf8_size() == 2);
    CHECK(KChar(0x20AC).utf8_size() == 3);
    CHECK(KChar(0x1F601).utf8_size() == 4);

    CHECK(KChar('A').debug_hex() == "U+0041");
    CHECK(KChar(0x1F601).debug_hex() == "U+1F601");
}

TEST_CASE("KChar::is_valid") {
    CHECK(KChar('A').is_valid());
}

TEST_CASE("to_lower fall-through branch") {
    KChar non_upper('1');
    auto result = non_upper.to_lower();
    CHECK(result == non_upper); // 分支覆盖：return *this;
}

TEST_CASE("print printable char") {
    KChar printable("😁");
    std::ostringstream oss;
    oss << printable;
    CHECK(oss.str() == "😁");
}

TEST_CASE("Rejects valid UTF-8 prefix with trailing data") {
    // 😁 = 4 bytes, 后面还有 'x'
    const char* aaaa = "😁x";
    CHECK_THROWS_AS(KChar tmp(aaaa), std::invalid_argument);
}


