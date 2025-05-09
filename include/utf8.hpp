#pragma once
#include <cstddef>
#include <string>
#include <vector>
#include <cstdint>
#include <iostream>
#include "base.hpp"

namespace utf8 {
using kstring::Byte;
using kstring::ByteSpan;
using kstring::ByteVec;
using kstring::CodePoint;

// 非法码点用 0xFFFD(0xEF 0xBF 0xBD) 替代
struct UTF8Encoded {
    const uint8_t* begin() const {
        return bytes;
    }

    const uint8_t* end() const {
        return bytes + len;
    }

    uint8_t bytes[4];
    std::size_t len;

    friend std::ostream& operator<<(std::ostream& os, const UTF8Encoded& d) {
        os << "UTF8Encoded{len=" << d.len << ", bytes=[";
        for (std::size_t i = 0; i < d.len; ++i) {
            if (i > 0) os << ' ';
            os << "0x" << std::hex << std::uppercase << static_cast<int>(d.bytes[i]);
        }
        os << "]}";
        return os;
    }
};

struct UTF8Decoded {
    CodePoint codepoint;
    bool ok;
    size_t next_pos;

    UTF8Decoded() : codepoint(0), ok(false), next_pos(0) {}

    // success
    UTF8Decoded(CodePoint _cp, size_t _next_pos) : codepoint(_cp), ok(true), next_pos(_next_pos) {}

    UTF8Decoded(CodePoint _cp, bool _ok, size_t _next_pos) : codepoint(_cp), ok(_ok), next_pos(_next_pos) {}

    friend std::ostream& operator<<(std::ostream& os, const UTF8Decoded& d) {
        if (d.ok) {
            os << "UTF8Decoded{cp=U+" << std::hex << std::uppercase << d.codepoint << ", next_pos=" << std::dec
               << d.next_pos << ", ok=true}";
        } else {
            os << "UTF8Decoded{<invalid>}";
        }
        return os;
    }

    static UTF8Decoded ill(size_t _next_pos) {
        return UTF8Decoded(kstring::ILL_CODEPOINT, false, _next_pos);
    }
};

} // namespace utf8

namespace utf8 {
// 获取当前字节起始的 UTF-8 编码的总长度（若非法则返回 0）
std::size_t lead_utf8_length(Byte lead);

// 判断某个位置的 len 个字节是否是合法的 UTF-8 编码
bool is_valid_range(const ByteSpan& data, std::size_t pos, std::size_t len);

// 获取第一个非法位置（返回 knpos 表示全部合法）
size_t first_invalid(const ByteSpan& data);
std::size_t count_valid_bytes(const ByteSpan& data);
std::pair<std::size_t, std::size_t> count_valid_bytes_chars(const ByteSpan& data);

// 是否是合法 UTF-8 编码
bool is_valid(const ByteSpan& data);

// 获取 code point 所需 UTF-8 长度（1~4 字节）, 非法则返回 0 长度
std::size_t utf8_size(CodePoint cp);

bool is_surrogate_codepoint(CodePoint cp);
bool is_valid_codepoint(CodePoint cp);
bool is_noncharacter(CodePoint cp);
bool is_overflow_codepoint(CodePoint cp);

// 编码一个 code point 为 UTF-8 序列
UTF8Encoded encode(CodePoint cp);

// 尝试解码 data[pos...] 开头的字符（失败时返回 false）, next_pos 存放解码后应处的 pos
UTF8Decoded decode_one(const ByteSpan& data, std::size_t pos);

UTF8Decoded decode_one_prev(const ByteSpan& data, std::size_t pos);
std::vector<CodePoint> decode_all(const ByteSpan& data);
ByteVec encode_all(const std::vector<CodePoint>& code_vec);

// decode range [start, end)
std::vector<CodePoint> decode_range(const ByteSpan& data, size_t start, size_t end);
// 查找字符数（不是字节数）
std::size_t char_count(const ByteSpan& data);

// 查找首次出现的 code point（按字符计数）
std::size_t find_codepoint(const ByteSpan& data, CodePoint target_codepoint);

void replace_at(ByteVec& data, size_t index, CodePoint cp_new);
void replace_all(ByteVec& data, CodePoint cp_old, CodePoint cp_new);

void replace_first(ByteVec& data, CodePoint cp_old, CodePoint cp_new);

// 是否是 ASCII 纯文本
bool is_all_ascii(const ByteSpan& data);
std::string codepoint_to_string(utf8::CodePoint cp);
} // namespace utf8

