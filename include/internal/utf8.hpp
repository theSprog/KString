#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstdint>
#include <iostream>

namespace internal {
namespace utf8 {
using Byte = uint8_t;
using ByteVec = std::vector<Byte>;
using CodePoint = uint32_t;

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
    CodePoint cp;
    bool ok;
    size_t next_pos;

    UTF8Decoded() : cp(0), ok(false), next_pos(0) {}

    // ok must be false
    UTF8Decoded(bool ok) : cp(0), ok(ok), next_pos(0) {
        if (ok) {
            throw std::runtime_error("Cannot init UTF8Decoded with bool true");
        }
    }

    UTF8Decoded(CodePoint cp, size_t next_pos) : cp(cp), ok(true), next_pos(next_pos) {}

    friend std::ostream& operator<<(std::ostream& os, const UTF8Decoded& d) {
    if (d.ok) {
        os << "UTF8Decoded{cp=U+" << std::hex << std::uppercase << d.cp
           << ", next_pos=" << std::dec << d.next_pos << ", ok=true}";
    } else {
        os << "UTF8Decoded{<invalid>}";
    }
    return os;
}

};

namespace {
// 获取当前字节起始的 UTF-8 编码的总长度（若非法则返回 0）
std::size_t lead_utf8_length(Byte lead) {
    if ((lead & 0b10000000) == 0b00000000) return 1;
    if ((lead & 0b11100000) == 0b11000000) return 2;
    if ((lead & 0b11110000) == 0b11100000) return 3;
    if ((lead & 0b11111000) == 0b11110000) return 4;
    return 0;
}

// 判断某个位置的 len 个字节是否是合法的 UTF-8 编码
bool is_valid_range(const ByteVec& data, std::size_t pos, std::size_t len) {
    if (pos + len > data.size()) return false;

    uint32_t cp = 0;
    if (len == 1) {
        cp = data[pos];
    } else {
        cp = data[pos] & ((1 << (7 - len)) - 1); // mask prefix bits
        for (std::size_t i = 1; i < len; ++i) {
            if ((data[pos + i] & 0b11000000) != 0b10000000) return false;
            cp = (cp << 6) | (data[pos + i] & 0b00111111);
        }
    }

    // Overlong check
    static const uint32_t min_cp[] = {0, 0x00, 0x80, 0x800, 0x10000};
    if (cp < min_cp[len]) return false;

    // Surrogate range check
    if (cp >= 0xD800 && cp <= 0xDFFF) return false;

    // Max Unicode value check
    if (cp > 0x10FFFF) return false;

    return true;
}
} // namespace

// 获取第一个非法位置（返回 data.size() 表示合法）
inline std::size_t first_invalid(const ByteVec& data) {
    std::size_t i = 0;
    while (i < data.size()) {
        std::size_t len = lead_utf8_length(data[i]);
        if (len == 0) return i;

        if (! is_valid_range(data, i, len)) return i;

        i += len;
    }
    return data.size();
}

// 是否是合法 UTF-8 编码
inline bool is_valid(const ByteVec& data) {
    return first_invalid(data) == data.size();
}

// 获取 code point 所需 UTF-8 长度（1~4 字节）
inline std::size_t codepoint_utf8_size(CodePoint cp) {
    if (cp <= 0x7F) return 1;
    if (cp <= 0x7FF) return 2;
    if (cp <= 0xFFFF) return 3;
    if (cp <= 0x10FFFF) return 4;
    return 0; // invalid codepoint
}

// 编码一个 code point 为 UTF-8 序列
inline UTF8Encoded encode(CodePoint cp) {
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
inline UTF8Decoded decode(const ByteVec& data, std::size_t pos) {
    if (pos >= data.size()) return false;

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
        return false;
    }

    if (pos + len > data.size()) return false;

    // 检查 continuation 字节
    for (std::size_t i = 1; i < len; ++i) {
        if ((data[pos + i] & 0b11000000) != 0b10000000) return false;
    }

    CodePoint cp = 0;
    if (len == 1) {
        cp = lead;
    } else if (len == 2) {
        cp = ((lead & 0b00011111) << 6) | (data[pos + 1] & 0b00111111);
    } else if (len == 3) {
        cp = ((lead & 0b00001111) << 12) | ((data[pos + 1] & 0b00111111) << 6) | (data[pos + 2] & 0b00111111);
    } else if (len == 4) {
        cp = ((lead & 0b00000111) << 18) | ((data[pos + 1] & 0b00111111) << 12) | ((data[pos + 2] & 0b00111111) << 6) |
             (data[pos + 3] & 0b00111111);
    }

    // 再次验证（例如禁止 overlong 或 surrogate）
    if (codepoint_utf8_size(cp) != len) return false;
    if (cp >= 0xD800 && cp <= 0xDFFF) return false;
    if (cp > 0x10FFFF) return false;

    return UTF8Decoded(cp, pos + len);
}

inline std::vector<UTF8Decoded> decode_all(const ByteVec& data) {
    std::vector<UTF8Decoded> results;
    std::size_t pos = 0;

    while (pos < data.size()) {
        UTF8Decoded decode_result = decode(data, pos);
        results.push_back(decode_result);

        if (! decode_result.ok) {
            // 出错时只前进 1 字节，避免死循环
            ++pos;
        } else {
            pos = decode_result.next_pos;
        }
    }

    return results;
}

// decode range [start, end)
inline std::vector<UTF8Decoded> decode_range(const ByteVec& data, size_t start, size_t end) {
    std::vector<UTF8Decoded> results;

    if (start >= end) return results;
    if (start > data.size()) start = data.size();
    if (end > data.size()) end = data.size();

    std::size_t pos = start;
    while (pos < end) {
        UTF8Decoded decode_result = decode(data, pos);

        if (decode_result.ok && decode_result.next_pos <= end) {
            results.push_back(decode_result);
            pos = decode_result.next_pos;
        } else {
            // 解码失败 / next_pos 越界都视为非法
            results.emplace_back(false); // UTF8Decoded(false)
            if (! decode_result.ok)      // 解码失败只前进一步
                ++pos;
            else // next_pos 越界则 pos 直接越界
                pos = decode_result.next_pos;
        }
    }

    return results;
}

// 查找字符数（不是字节数）
inline std::size_t char_count(const ByteVec& data) {
    std::size_t count = 0;
    for (std::size_t pos = 0; pos < data.size();) {
        UTF8Decoded decode_result = decode(data, pos);
        if (! decode_result.ok) break;
        ++count;
        pos = decode_result.next_pos;
    }
    return count;
}

// 查找首次出现的 code point（按字符计数）
inline std::size_t find(const ByteVec& data, CodePoint target_codepoint) {
    std::size_t index = 0;
    for (std::size_t pos = 0; pos < data.size();) {
        UTF8Decoded decode_result = decode(data, pos);
        if (! decode_result.ok) break;
        if (decode_result.cp == target_codepoint) return index;
        pos = decode_result.next_pos;
        ++index;
    }
    return static_cast<std::size_t>(-1);
}

inline void replace_at(ByteVec& data, size_t index, CodePoint cp_new) {
    UTF8Decoded decode_result = decode(data, index);
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

inline void replace_all(ByteVec& data, CodePoint cp_old, CodePoint cp_new) {
    std::size_t pos = 0;
    UTF8Encoded encoded = encode(cp_new); // 预编码 cp_new，避免重复计算
    std::size_t new_len = encoded.len;

    while (pos < data.size()) {
        UTF8Decoded decode_result = decode(data, pos);
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

inline void replace_first(ByteVec& data, CodePoint cp_old, CodePoint cp_new) {
    std::size_t pos = 0;
    while (pos < data.size()) {
        UTF8Decoded decode_result = decode(data, pos);
        if (! decode_result.ok) break;

        if (decode_result.cp == cp_old) {
            replace_at(data, pos, cp_new);
            return;
        }

        pos = decode_result.next_pos;
    }
}

// 是否是 ASCII 纯文本
inline bool is_ascii(const ByteVec& data) {
    for (Byte b : data) {
        if (b >= 0x80) return false;
    }
    return true;
}

inline std::string debug_codepoint(CodePoint cp) {
    UTF8Encoded bytes = utf8::encode(cp);
    return std::string(bytes.begin(), bytes.end()); // 转为 std::string 输出
}
} // namespace utf8
} // namespace internal
