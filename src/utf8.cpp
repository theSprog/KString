#include <cstddef>
#include <cstring>
#include <string>
#include <vector>
#include <cstdint>
#include "utf8.hpp"
#include "base.hpp"
#include "third_party/simdutf.h"

namespace utf8 {
// 获取当前字节起始的 UTF-8 编码的总长度（若非法则返回 0）
std::size_t lead_utf8_length(Byte lead) {
    if ((lead & 0b10000000) == 0b00000000) return 1;
    if ((lead & 0b11100000) == 0b11000000) return 2;
    if ((lead & 0b11110000) == 0b11100000) return 3;
    if ((lead & 0b11111000) == 0b11110000) return 4;
    return 0;
}

// 获取第一个非法位置（返回 -1 表示全部合法）
std::size_t first_invalid(const ByteSpan& data) {
    auto result = simdutf::validate_utf8_with_errors(reinterpret_cast<const char*>(data.data()), data.size());

    if (result.is_ok()) {
        return kstring::knpos; // 全部合法
    } else {
        return result.count; // 返回第一个非法字节位置
    }
}

// 判断某个位置开始的 len 个字节是否是合法的 UTF-8 编码
bool is_valid_range(const ByteSpan& data, std::size_t pos, std::size_t len) {
    if (pos + len > data.size()) return false;
    return is_valid(data.subspan(pos, len));
}

// 是否是合法 UTF-8 编码
bool is_valid(const ByteSpan& data) {
    return first_invalid(data) == kstring::knpos;
}

// 获取 code point 所需 UTF-8 长度（1~4 字节）, 非法则返回 0 长度
std::size_t utf8_size(CodePoint cp) {
    if (cp <= 0x7F) return 1;
    if (cp <= 0x7FF) return 2;
    if (cp <= 0xFFFF) return 3;
    if (cp <= 0x10FFFF) return 4;
    return 0; // invalid codepoint
}

// 编码一个 code point 为 UTF-8 序列
UTF8Encoded encode(CodePoint cp) {
    UTF8Encoded out;
    if (cp <= 0x7F) {
        out.bytes[0] = static_cast<uint8_t>(cp);
        out.len = 1;
    } else if (cp <= 0x7FF) {
        out.bytes[0] = static_cast<uint8_t>(0xC0 | (cp >> 6));
        out.bytes[1] = static_cast<uint8_t>(0x80 | (cp & 0x3F));
        out.len = 2;
    } else if (cp <= 0xFFFF) {
        out.bytes[0] = static_cast<uint8_t>(0xE0 | (cp >> 12));
        out.bytes[1] = static_cast<uint8_t>(0x80 | ((cp >> 6) & 0x3F));
        out.bytes[2] = static_cast<uint8_t>(0x80 | (cp & 0x3F));
        out.len = 3;
    } else {
        out.bytes[0] = static_cast<uint8_t>(0xF0 | (cp >> 18));
        out.bytes[1] = static_cast<uint8_t>(0x80 | ((cp >> 12) & 0x3F));
        out.bytes[2] = static_cast<uint8_t>(0x80 | ((cp >> 6) & 0x3F));
        out.bytes[3] = static_cast<uint8_t>(0x80 | (cp & 0x3F));
        out.len = 4;
    }
    return out;
}

// 尝试解码 data[pos...] 开头的字符（失败时返回 false）, next_pos 存放解码后应处的 pos
UTF8Decoded decode_one(const ByteSpan& data, std::size_t pos) {
    if (pos >= data.size()) return UTF8Decoded(0, false, data.size());

    Byte lead = data[pos];
    std::size_t len = 0;
    if ((lead & 0b10000000) == 0) {
        len = 1;
    } else if ((lead & 0b11100000) == 0b11000000) {
        len = 2;
    } else if ((lead & 0b11110000) == 0b11100000) {
        len = 3;
    } else if ((lead & 0b11111000) == 0b11110000) {
        len = 4;
    } else {
        return UTF8Decoded(0, false, pos + 1);
    }

    if (pos + len > data.size()) return UTF8Decoded(0, false, pos + 1);

    // 检查 continuation 字节
    for (std::size_t i = 1; i < len; ++i) {
        if ((data[pos + i] & 0b11000000) != 0b10000000) return UTF8Decoded(0, false, pos + 1);
    }

    CodePoint cp = 0;
    if (len == 1) {
        cp = CodePoint(lead);
    } else if (len == 2) {
        cp = CodePoint(((lead & 0b00011111) << 6) | (data[pos + 1] & 0b00111111));
    } else if (len == 3) {
        cp =
            CodePoint(((lead & 0b00001111) << 12) | ((data[pos + 1] & 0b00111111) << 6) | (data[pos + 2] & 0b00111111));
    } else if (len == 4) {
        cp = CodePoint(((lead & 0b00000111) << 18) | ((data[pos + 1] & 0b00111111) << 12) |
                       ((data[pos + 2] & 0b00111111) << 6) | (data[pos + 3] & 0b00111111));
    }

    // 再次验证（例如禁止 overlong 或 surrogate）
    if (utf8_size(cp) != len) return UTF8Decoded(0, false, pos + 1);
    if (cp >= 0xD800 && cp <= 0xDFFF) return UTF8Decoded(0, false, pos + 1);
    if (cp > 0x10FFFF) return UTF8Decoded(0, false, pos + 1);

    return UTF8Decoded(cp, pos + len);
}

UTF8Decoded decode_one_prev(const ByteSpan& data, std::size_t pos) {
    if (pos == 0) return UTF8Decoded(0, false, 0);

    // 最多看 4 字节
    std::size_t start = (pos >= 4) ? pos - 4 : 0;

    for (std::size_t p = pos - 1; p >= start; --p) {
        // 试图从 p decode 成合法字符
        auto dec = utf8::decode_one(data, p);
        if (dec.ok) {
            if (dec.next_pos == pos)
                return UTF8Decoded(dec.cp, p);         // 正好 decode 到 [p, pos)
            else                                       //
                return UTF8Decoded(0, false, pos - 1); // 先迭代非法字符
        }
        // decode 失败, 继续往前探测
        if (p == 0) break; // 避免 size_t underflow
    }

    // fallback: treat last byte (pos - 1) as illegal, 一个非法字节 = 一个字符
    return UTF8Decoded(0, false, pos - 1);
}

std::vector<UTF8Decoded> decode_all(const ByteSpan& data) {
    std::vector<UTF8Decoded> results;
    std::size_t pos = 0;

    const char* input = reinterpret_cast<const char*>(data.data());
    size_t size = data.size();

    std::vector<char32_t> buf(size); // 最坏情况下 UTF-8 每字节一个字符

    while (pos < size) {
        auto result = simdutf::convert_utf8_to_utf32_with_errors(input + pos, size - pos, buf.data());

        if (result.is_ok()) { // 全部成功
            std::size_t local_pos = pos;
            for (size_t i = 0; i < result.count; ++i) {
                CodePoint cp = buf[i];
                std::size_t len = utf8_size(cp); // 你已有的函数
                results.emplace_back(cp, local_pos + len);
                local_pos += len;
            }
            break;
        } else { // 部分成功
            // 先解码错误位置之前的数据
            size_t valid_bytes = result.count;
            if (valid_bytes > 0) {
                size_t valid_chars = simdutf::convert_utf8_to_utf32(input + pos, valid_bytes, buf.data());

                std::size_t local_pos = pos;
                for (size_t i = 0; i < valid_chars; ++i) {
                    CodePoint cp = buf[i];
                    std::size_t len = utf8_size(cp);
                    results.emplace_back(cp, local_pos);
                    local_pos += len;
                }
                pos += valid_bytes;
            }

            // 处理错误字符
            results.emplace_back(CodePoint(0), false, pos + 1);
            pos += 1; // 只前进一个字节
        }
    }

    return results;
}

// decode range [start, end)
std::vector<UTF8Decoded> decode_range(const ByteSpan& data, size_t start, size_t end) {
    std::vector<UTF8Decoded> results;

    if (start >= end) return results;
    if (start > data.size()) start = data.size();
    if (end > data.size()) end = data.size();

    return decode_all(data.subspan(start, end - start));
}

// 查找字符数（不是字节数）
std::size_t char_count(const ByteSpan& data) {
    std::size_t pos = 0;
    std::size_t count = 0;
    const char* ptr = reinterpret_cast<const char*>(data.data());
    size_t size = data.size();

    std::vector<char32_t> buf(size); // 最多每字节一个字符

    while (pos < size) {
        auto result = simdutf::convert_utf8_to_utf32_with_errors(ptr + pos, size - pos, buf.data());

        if (result.is_ok()) {
            // 成功部分
            count += result.count;
            break;
        } else {
            // 部分成功：result.count 是错误位置的偏移量
            // 我们需要确定这个偏移量内有多少个完整字符

            if (result.count > 0) {
                // 处理错误位置之前的有效部分
                size_t valid_chars = simdutf::convert_utf8_to_utf32(ptr + pos, result.count, buf.data());
                count += valid_chars;

                // 更新位置到错误处
                pos += result.count;
            }

            pos += 1;   // 跳过错误字节
            count += 1; // 错误字节也被视为一个字符
        }
    }

    return count;
}

// 查找首次出现的 code point（按字符计数）
std::size_t find_codepoint(const ByteSpan& data, CodePoint target_cp) {
    if (target_cp <= 0X7F) {
        const void* result = std::memchr(data.data(), static_cast<uint8_t>(target_cp), data.size());
        if (! result) return kstring::knpos;

        std::size_t byte_offset = reinterpret_cast<const Byte*>(result) - data.data();
        return char_count(ByteSpan(data.data(), byte_offset));
    }

    std::size_t index = 0;
    for (std::size_t pos = 0; pos < data.size();) {
        UTF8Decoded decode_result = decode_one(data, pos);
        if (! decode_result.ok) break;
        if (decode_result.cp == target_cp) return index;
        pos = decode_result.next_pos;
        ++index;
    }
    return kstring::knpos;
}

void replace_at(ByteVec& data, size_t index, CodePoint cp_new) {
    UTF8Decoded decode_result = decode_one(data, index);
    if (! decode_result.ok) return;
    std::size_t next_pos = decode_result.next_pos;

    std::size_t old_len = next_pos - index;
    UTF8Encoded encoded = encode(cp_new);
    std::size_t new_len = encoded.len;

    if (old_len == new_len) {
        // 快路径：直接写入
        std::copy(encoded.begin(), encoded.end(), data.begin() + index);
        return;
    }

    // 需要滑动后续字节
    std::ptrdiff_t diff = static_cast<std::ptrdiff_t>(new_len) - static_cast<std::ptrdiff_t>(old_len);
    std::size_t tail_begin = next_pos;
    std::size_t tail_len = data.size() - tail_begin;

    if (diff > 0) {
        // 扩容 + 后移 tail
        data.resize(data.size() + diff);
        std::move_backward(data.begin() + tail_begin, data.begin() + tail_begin + tail_len, data.end());
    } else if (diff < 0) {
        // 前移 tail（提前完成，等长后再 resize）
        std::move(data.begin() + tail_begin,
                  data.end(),
                  data.begin() + tail_begin + diff // diff < 0
        );
        data.resize(data.size() + diff); // 实际是缩短
    }

    // 写入新的 codepoint 编码
    std::copy(encoded.begin(), encoded.end(), data.begin() + index);
}

void replace_all(ByteVec& data, CodePoint cp_old, CodePoint cp_new) {
    std::size_t pos = 0;
    UTF8Encoded encoded = encode(cp_new); // 预编码 cp_new，避免重复计算
    std::size_t new_len = encoded.len;

    while (pos < data.size()) {
        UTF8Decoded decode_result = decode_one(data, pos);
        if (! decode_result.ok) break;
        std::size_t next = decode_result.next_pos;

        if (decode_result.cp == cp_old) {
            std::size_t old_len = next - pos;
            std::ptrdiff_t diff = static_cast<std::ptrdiff_t>(new_len) - static_cast<std::ptrdiff_t>(old_len);

            if (diff == 0) {
                // 直接覆盖
                std::copy(encoded.begin(), encoded.end(), data.begin() + pos);
                pos = next;
            } else {
                std::size_t tail_begin = next;
                std::size_t tail_len = data.size() - tail_begin;

                if (diff > 0) {
                    // 扩容 + 后移 tail
                    data.resize(data.size() + diff);
                    std::move_backward(data.begin() + tail_begin, data.begin() + tail_begin + tail_len, data.end());
                } else {
                    // 前移 tail（先 move，再 resize）
                    std::move(data.begin() + tail_begin,
                              data.end(),
                              data.begin() + tail_begin + diff // diff < 0
                    );
                    data.resize(data.size() + diff);
                }

                // 替换
                std::copy(encoded.begin(), encoded.end(), data.begin() + pos);
                pos = pos + new_len;
            }
        } else {
            pos = next;
        }
    }
}

void replace_first(ByteVec& data, CodePoint cp_old, CodePoint cp_new) {
    std::size_t pos = 0;
    while (pos < data.size()) {
        UTF8Decoded decode_result = decode_one(data, pos);
        if (! decode_result.ok) break;

        if (decode_result.cp == cp_old) {
            replace_at(data, pos, cp_new);
            return;
        }

        pos = decode_result.next_pos;
    }
}

// 是否是 ASCII 纯文本
bool is_all_ascii(const ByteSpan& data) {
    return simdutf::validate_ascii(reinterpret_cast<const char*>(data.data()), data.size());
}

std::string codepoint_to_string(CodePoint cp) {
    UTF8Encoded bytes = utf8::encode(cp);
    return std::string(bytes.begin(), bytes.end()); // 转为 std::string 输出
}
} // namespace utf8
