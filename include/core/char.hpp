#pragma once
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include "../internal/utf8.hpp"

namespace KString {
using internal::utf8::ByteVec;
using internal::utf8::CodePoint;

class KChar {
    CodePoint value_; // Unicode CodePoint

  public:
    // 默认构造（0）
    KChar() : value_(0) {}

    bool is_valid() const {
        return value_ <= 0x10FFFF && ! (value_ >= 0xD800 && value_ <= 0xDFFF);
    }

    // 从 Unicode code point 构造
    explicit KChar(CodePoint cp) : value_(cp) {
        if (! is_valid()) throw std::invalid_argument("Invalid Unicode code point");
    }

    // 从 UTF-8 单字符构造，如 KChar("😁")
    explicit KChar(const char* utf8) : value_(0) {
        if (! utf8) throw std::invalid_argument("Null pointer passed to KChar");

        // 拷贝最多前 4 个字节（UTF-8 单字符最大长度）
        ByteVec tmp;
        for (int i = 0; i < 4 && utf8[i]; ++i) tmp.push_back(static_cast<uint8_t>(utf8[i]));

        CodePoint cp;
        std::size_t next;
        if (! internal::utf8::decode(tmp, 0, cp, next)) {
            throw std::invalid_argument("Invalid UTF-8 character passed to KChar");
        }

        // 必须是完整单字符
        if (utf8[next] != '\0') {
            throw std::invalid_argument("Too many bytes: KChar must be a single UTF-8 character");
        }

        *this = KChar(cp);
    }

    CodePoint value() const {
        return value_;
    }

    // 比较操作符（可选）
    bool operator==(const KChar& other) const {
        return value_ == other.value_;
    }

    bool operator!=(const KChar& other) const {
        return value_ != other.value_;
    }

    // ASCII 判定
    bool is_ascii() const {
        return value_ <= 0x7F;
    }

    bool is_digit() const {
        return value_ >= '0' && value_ <= '9';
    }

    bool is_upper() const {
        return value_ >= 'A' && value_ <= 'Z';
    }

    bool is_lower() const {
        return value_ >= 'a' && value_ <= 'z';
    }

    bool is_alpha() const {
        return is_upper() || is_lower();
    }

    bool is_alnum() const {
        return is_alpha() || is_digit();
    }

    bool is_space() const {
        // ASCII 常见空白符: space, \t, \n, \r, \v, \f
        return value_ == ' ' || value_ == '\t' || value_ == '\n' || value_ == '\r' || value_ == '\v' || value_ == '\f';
    }

    bool is_printable() const {
        return (value_ >= 0x20 && value_ <= 0x7E); // basic printable ASCII
    }

    KChar to_upper() const {
        if (is_lower()) return KChar(value_ - 32);
        return *this;
    }

    KChar to_lower() const {
        if (is_upper()) return KChar(value_ + 32);
        return *this;
    }

    char to_ascii_char() const {
        if (! is_ascii()) throw std::runtime_error("KChar is not ASCII; cannot convert to char");

        return static_cast<char>(value_);
    }

    std::string to_utf8string() const {
        if (! is_valid()) throw std::runtime_error("Invalid Unicode code point in KChar");

        internal::utf8::UTF8Encoded enc = internal::utf8::encode(value_);
        return std::string(enc.begin(), enc.end());
    }

    std::size_t utf8_size() const {
        return internal::utf8::codepoint_utf8_size(value_);
    }

    std::string debug_hex() const {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "U+%04X", value_);
        return std::string(buf);
    }
};
} // namespace KString


