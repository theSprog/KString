#pragma once
#include <cstdint>
#include <cstring>
#include <ostream>
#include <stdexcept>
#include <unordered_set>
#include "base.hpp"
#include "utf8.hpp"

namespace kstring {
class KChar {
    CodePoint cp_; // Unicode CodePoint

  public:
    // 默认构造（0）
    KChar() : cp_(0) {}

    bool is_surrogate() const {
        return cp_ >= 0xD800 && cp_ <= 0xDFFF;
    }

    bool is_noncharacter() const {
        return (cp_ & 0xFFFE) == 0xFFFE && cp_ <= 0x10FFFF;
    }

    bool is_valid() const {
        return cp_ <= 0x10FFFF && ! is_surrogate();
    }

    // 从 Unicode code point 构造
    explicit KChar(CodePoint cp) : cp_(cp) {
        if (! is_valid()) throw std::invalid_argument("Invalid Unicode code point");
    }

    // 从 UTF-8 单字符构造，如 KChar("😁")
    explicit KChar(const char* bytes) : cp_(0) {
        if (! bytes) throw std::invalid_argument("Null pointer passed to KChar");

        // 拷贝最多前 4 个字节（UTF-8 单字符最大长度）
        ByteVec tmp;
        for (int i = 0; i < 4 && bytes[i]; ++i) tmp.push_back(static_cast<uint8_t>(bytes[i]));

        utf8::UTF8Decoded decode_result = utf8::decode(tmp, 0);
        if (! decode_result.ok) {
            throw std::invalid_argument("Invalid UTF-8 character passed to KChar");
        }

        // 必须是完整单字符
        if (bytes[decode_result.next_pos] != '\0') {
            throw std::invalid_argument("Too many bytes: KChar must be a single UTF-8 character");
        }

        *this = KChar(decode_result.cp);
    }

    CodePoint value() const {
        return cp_;
    }

    // 比较操作符（可选）
    bool operator==(const KChar& other) const {
        return cp_ == other.cp_;
    }

    bool operator!=(const KChar& other) const {
        return cp_ != other.cp_;
    }

    // ASCII 判定
    bool is_ascii() const {
        return cp_ <= 0x7F;
    }

    bool is_digit() const {
        return cp_ >= '0' && cp_ <= '9';
    }

    bool is_upper() const {
        return cp_ >= 'A' && cp_ <= 'Z';
    }

    bool is_lower() const {
        return cp_ >= 'a' && cp_ <= 'z';
    }

    bool is_alpha() const {
        return is_upper() || is_lower();
    }

    bool is_alnum() const {
        return is_alpha() || is_digit();
    }

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
        return unicode_spaces.count(cp_) > 0;
    }

    bool is_printable() const {
        return (cp_ >= 0x20 && cp_ <= 0x7E); // basic printable ASCII
    }

    KChar to_upper() const {
        if (is_lower()) return KChar(cp_ - 32);
        return *this;
    }

    KChar to_lower() const {
        if (is_upper()) return KChar(cp_ + 32);
        return *this;
    }

    char to_char() const {
        if (! is_ascii()) throw std::runtime_error("KChar is not ASCII; cannot convert to char");

        return static_cast<char>(cp_);
    }

    std::string to_utf8string() const {
        if (! is_valid()) throw std::runtime_error("Invalid Unicode code point in KChar");

        utf8::UTF8Encoded enc = utf8::encode(cp_);
        return std::string(enc.begin(), enc.end());
    }

    ByteVec to_bytes() {
        utf8::UTF8Encoded enc = utf8::encode(cp_);
        return ByteVec(enc.bytes, enc.bytes + enc.len);
    }

    std::size_t utf8_size() const {
        return utf8::utf8_size(cp_);
    }

    std::string debug_hex() const {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "U+%04X", cp_);
        return std::string(buf);
    }

    friend std::ostream& operator<<(std::ostream& os, const KChar& kchar) {
        utf8::UTF8Encoded encoded = utf8::encode(kchar.cp_);
        os << std::string(encoded.begin(), encoded.end());
        return os;
    }
};
} // namespace kstring


