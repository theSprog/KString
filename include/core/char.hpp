#pragma once
#include <cstdint>
#include <cstring>
#include <ostream>
#include <stdexcept>
#include <unordered_set>
#include "../internal/utf8.hpp"

namespace KString {
class KChar {
    internal::utf8::CodePoint value_; // Unicode CodePoint

  public:
    const static internal::utf8::CodePoint Ill = 0xFFFD;

    // 默认构造（0）
    KChar() : value_(0) {}

    bool is_surrogate() const {
        return value_ >= 0xD800 && value_ <= 0xDFFF;
    }

    bool is_noncharacter() const {
        return (value_ & 0xFFFE) == 0xFFFE && value_ <= 0x10FFFF;
    }

    bool is_valid() const {
        return value_ <= 0x10FFFF && ! is_surrogate();
    }

    // 从 Unicode code point 构造
    explicit KChar(internal::utf8::CodePoint cp) : value_(cp) {
        if (! is_valid()) throw std::invalid_argument("Invalid Unicode code point");
    }

    // 从 UTF-8 单字符构造，如 KChar("😁")
    explicit KChar(const char* bytes) : value_(0) {
        if (! bytes) throw std::invalid_argument("Null pointer passed to KChar");

        // 拷贝最多前 4 个字节（UTF-8 单字符最大长度）
        internal::utf8::ByteVec tmp;
        for (int i = 0; i < 4 && bytes[i]; ++i) tmp.push_back(static_cast<uint8_t>(bytes[i]));

        internal::utf8::UTF8Decoded decode_result = internal::utf8::decode(tmp, 0);
        if (! decode_result.ok) {
            throw std::invalid_argument("Invalid UTF-8 character passed to KChar");
        }

        // 必须是完整单字符
        if (bytes[decode_result.next_pos] != '\0') {
            throw std::invalid_argument("Too many bytes: KChar must be a single UTF-8 character");
        }

        *this = KChar(decode_result.cp);
    }

    internal::utf8::CodePoint value() const {
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

    // bool is_space() const {
    //     // ASCII 常见空白符: space, \t, \n, \r, \v, \f
    //     return value_ == ' ' || value_ == '\t' || value_ == '\n' || value_ == '\r' || value_ == '\v' || value_ == '\f';
    // }

    bool is_whitespace() const {
        static const std::unordered_set<uint32_t> unicode_spaces = {// ASCII 空白
                                                                    0x0020,
                                                                    0x0009,
                                                                    0x000A,
                                                                    0x000B,
                                                                    0x000C,
                                                                    0x000D,
                                                                    // unicode 空白
                                                                    0x00A0,
                                                                    0x1680,
                                                                    0x2000,
                                                                    0x2001,
                                                                    0x2002,
                                                                    0x2003,
                                                                    0x2004,
                                                                    0x2005,
                                                                    0x2006,
                                                                    0x2007,
                                                                    0x2008,
                                                                    0x2009,
                                                                    0x200A,
                                                                    0x2028,
                                                                    0x2029,
                                                                    0x202F,
                                                                    0x205F,
                                                                    0x3000};
        return unicode_spaces.count(value_) > 0;
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

    char to_char() const {
        if (! is_ascii()) throw std::runtime_error("KChar is not ASCII; cannot convert to char");

        return static_cast<char>(value_);
    }

    std::string to_utf8string() const {
        if (! is_valid()) throw std::runtime_error("Invalid Unicode code point in KChar");

        internal::utf8::UTF8Encoded enc = internal::utf8::encode(value_);
        return std::string(enc.begin(), enc.end());
    }

    internal::utf8::ByteVec to_bytes() {
        internal::utf8::UTF8Encoded enc = internal::utf8::encode(value_);
        return internal::utf8::ByteVec(enc.bytes, enc.bytes + enc.len);
    }

    std::size_t utf8_size() const {
        return internal::utf8::codepoint_utf8_size(value_);
    }

    std::string debug_hex() const {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "U+%04X", value_);
        return std::string(buf);
    }

    friend std::ostream& operator<<(std::ostream& os, const KChar& kchar) {
        internal::utf8::UTF8Encoded encoded = internal::utf8::encode(kchar.value_);
        os << std::string(encoded.begin(), encoded.end());
        return os;
    }
};
} // namespace KString


