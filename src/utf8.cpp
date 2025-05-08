#include <cstddef>
#include <cstring>
#include <string>
#include <vector>
#include <cstdint>
#include "utf8.hpp"
#include "base.hpp"
#include "third_party/simdutf.h"

namespace utf8 {

namespace {
struct Utf8DecodeResult {
    std::size_t ok_chars;       // 成功处理的 UTF-32 字符数
    std::size_t consumed_bytes; // 成功消费的 UTF-8 字节数
    simdutf::error_code error;  // 解码是否失败

    Utf8DecodeResult(std::size_t c, std::size_t b, simdutf::error_code e) : ok_chars(c), consumed_bytes(b), error(e) {}

    bool is_ok() {
        return error == simdutf::error_code::SUCCESS;
    }
};

Utf8DecodeResult decode_valid_prefix(const ByteSpan& input, std::vector<CodePoint>& output) {
    std::size_t ok_chars = 0;
    std::size_t consumed_bytes = 0;
    simdutf::error_code error = simdutf::error_code::SUCCESS;
    if (input.empty()) return Utf8DecodeResult(0, 0, simdutf::error_code::SUCCESS);

    std::size_t old_size = output.size();
    output.resize(old_size + input.size()); // 最多扩展 input.size() 个 code point

    auto* out_ptr = reinterpret_cast<char32_t*>(output.data() + old_size);
    auto* in_ptr = reinterpret_cast<const char*>(input.data());

    simdutf::result res = simdutf::convert_utf8_to_utf32_with_errors(in_ptr, input.size(), out_ptr);

    // 计算成功写入了多少个 code points
    if (res.is_ok()) {
        // 成功时 result.count 代表已经写入的字符数
        ok_chars = res.count;
        consumed_bytes = input.size();
        error =  simdutf::error_code::SUCCESS;
    } else {
        // 失败时需要手动计算出消费的字符数
        const char* ptr = reinterpret_cast<const char*>(input.data());
        if (res.count > 0) {
            // 处理错误位置之前的有效部分
            ok_chars = simdutf::count_utf8(ptr, res.count);
            consumed_bytes = res.count;
        }
        error = res.error;
    }

    output.resize(old_size + ok_chars);
    return Utf8DecodeResult(ok_chars, consumed_bytes, error);

}

// 尝试将 input[in_pos...] 的合法前缀转换为 UTF-8 并写入 output
inline simdutf::result encode_valid_prefix(kstring::span<const CodePoint> input, ByteVec& output) {
    if (input.empty()) return {simdutf::error_code::SUCCESS, 0};

    std::size_t old_size = output.size();
    output.resize(old_size + input.size() * sizeof(CodePoint)); // 最多每个 code point 4 字节

    auto* in_ptr = reinterpret_cast<const char32_t*>(input.data());
    auto* out_ptr = reinterpret_cast<char*>(output.data() + old_size);

    simdutf::result res = simdutf::convert_utf32_to_utf8_with_errors(in_ptr, input.size(), out_ptr);

    output.resize(old_size + res.count); // 保留写入的部分
    return res;
}

} // namespace

// 获取当前字节起始的 UTF-8 编码的总长度（若非法则返回 0）
std::size_t lead_utf8_length(Byte lead) {
    if (lead <= 0x7F) return 1;                 // 0xxxxxxx
    if (lead >= 0xC0 && lead <= 0xDF) return 2; // 110xxxxx (0xC0, 0xC1 are overlong)
    if (lead >= 0xE0 && lead <= 0xEF) return 3; // 1110xxxx
    if (lead >= 0xF0 && lead <= 0xF4) return 4; // 11110xxx (max legal is 0xF4 due to U+10FFFF)
    return 0;                                   // invalid
}

// 获取第一个非法字节位置（返回 -1 表示全部合法）
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

bool is_surrogate_codepoint(CodePoint cp) {
    return cp >= 0xD800 && cp <= 0xDFFF;
}

bool is_overflow_codepoint(CodePoint cp) {
    return cp > 0x10FFFF;
}

bool is_noncharacter(CodePoint cp) {
    return (cp & 0xFFFE) == 0xFFFE && ! is_overflow_codepoint(cp);
}

bool is_valid_codepoint(CodePoint cp) {
    // 超出 Unicode 范围 或者 surrogate 区则非法
    return ! (is_overflow_codepoint(cp)) && ! (is_surrogate_codepoint(cp));
}

// 获取 code point 所需 UTF-8 长度（1~4 字节）, 非法则返回 0 长度
std::size_t utf8_size(CodePoint cp) {
    if (! is_valid_codepoint(cp)) return 0;
    if (cp <= 0x7F) return 1;
    if (cp <= 0x7FF) return 2;
    if (cp <= 0xFFFF) return 3;
    return 4;
}

// 编码一个 code point 为 UTF-8 序列
UTF8Encoded encode(CodePoint cp) {
    UTF8Encoded out;
    out.len = utf8_size(cp);

    if (out.len == 1) {
        out.bytes[0] = static_cast<uint8_t>(cp);
    } else if (out.len == 2) {
        out.bytes[0] = static_cast<uint8_t>(0xC0 | (cp >> 6));
        out.bytes[1] = static_cast<uint8_t>(0x80 | (cp & 0x3F));
    } else if (out.len == 3) {
        out.bytes[0] = static_cast<uint8_t>(0xE0 | (cp >> 12));
        out.bytes[1] = static_cast<uint8_t>(0x80 | ((cp >> 6) & 0x3F));
        out.bytes[2] = static_cast<uint8_t>(0x80 | (cp & 0x3F));
    } else if (out.len == 4) {
        out.bytes[0] = static_cast<uint8_t>(0xF0 | (cp >> 18));
        out.bytes[1] = static_cast<uint8_t>(0x80 | ((cp >> 12) & 0x3F));
        out.bytes[2] = static_cast<uint8_t>(0x80 | ((cp >> 6) & 0x3F));
        out.bytes[3] = static_cast<uint8_t>(0x80 | (cp & 0x3F));
    } else { // len == 0, 非法字符用 ILL_CODEPOINT 替换
        return encode(kstring::ILL_CODEPOINT);
    }
    return out;
}

// 尝试解码 data[pos...] 开头的字符（失败时返回 false）, next_pos 存放解码后应处的 pos
UTF8Decoded decode_one(const ByteSpan& data, std::size_t pos) {
    if (pos >= data.size()) return UTF8Decoded::ill(data.size());

    Byte lead = data[pos];
    std::size_t len = lead_utf8_length(lead);
    if (len == 0) return UTF8Decoded::ill(pos + 1);
    if (pos + len > data.size()) return UTF8Decoded::ill(data.size());

    // 检查 continuation 字节是否以 0x10 打头
    for (std::size_t i = 1; i < len; ++i) {
        if ((data[pos + i] & 0b11000000) != 0b10000000) {
            return UTF8Decoded::ill(pos + i);
        }
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

    // Unicode 要求：字符必须用最短的 UTF-8 表示, 例如 U+0000 被编码为 2 字节 0xC0 0x80 是非法的
    if (utf8_size(cp) != len) return UTF8Decoded::ill(pos + len);
    // surrogate 区间属于 UTF-16 用作代理对的码点区, 不允许作为码点
    if (is_surrogate_codepoint(cp)) return UTF8Decoded::ill(pos + len);
    // 最大合法码点是 U+10FFFF, 超过也是非法的
    if (is_overflow_codepoint(cp)) return UTF8Decoded::ill(pos + len);

    return UTF8Decoded(cp, pos + len);
}

std::vector<CodePoint> decode_all(const ByteSpan& data) {
    std::vector<CodePoint> result;
    std::size_t pos = 0;
    std::size_t size = data.size();
    if (size == 0) return result;

    result.reserve(size); // 最多每个字节对应一个 code point

    while (pos < size) {
        ByteSpan input = data.subspan(pos);
        auto res = decode_valid_prefix(input, result);
        if (res.is_ok()) {
            break;
        }

        // 部分成功, 前缀部分已写入 result，跳过成功解码的那段
        pos += res.consumed_bytes;
        // 此时 pos 就已经是错误编码开始处
        UTF8Decoded bad = decode_one(data, pos);
        result.push_back(kstring::ILL_CODEPOINT);
        pos = bad.next_pos;
    }

    return result;
}

ByteVec encode_all(const std::vector<CodePoint>& code_vec) {
    ByteVec result;
    if (code_vec.empty()) return result;

    result.reserve(code_vec.size() * sizeof(CodePoint)); // 预估空间
    kstring::span<const CodePoint> input(code_vec);
    while (! code_vec.empty()) {
        simdutf::result res = utf8::encode_valid_prefix(input, result);

        if (res.is_ok()) break;

        // fallback: 写入 U+FFFD
        UTF8Encoded fallback = encode(kstring::ILL_CODEPOINT);
        result.insert(result.end(), fallback.begin(), fallback.end());

        // 跳过成功的 + 错误的那个
        input = input.subspan(res.count + 1);
    }

    return result;
}

// decode range [start, end)
std::vector<CodePoint> decode_range(const ByteSpan& data, size_t start, size_t end) {
    std::vector<CodePoint> results;

    if (start >= end) return results;
    if (start > data.size()) start = data.size();
    if (end > data.size()) end = data.size();

    return decode_all(data.subspan(start, end - start));
}

UTF8Decoded decode_one_prev(const ByteSpan& data, std::size_t pos) {
    if (pos == 0) return UTF8Decoded(0, false, 0);

    // 最多看 4 字节
    std::size_t start = (pos >= 4) ? pos - 4 : 0;

    std::size_t p = pos - 1;
    do {
        auto dec = utf8::decode_one(data, p);
        if (dec.ok) {
            if (dec.next_pos == pos) // 正好 decode 到 [p, pos)
                return UTF8Decoded(dec.codepoint, p);
            else // 先迭代非法字符, FIXME
                return UTF8Decoded(kstring::ILL_CODEPOINT, false, pos - 1);
        }
        // decode 失败, 继续往前探测
        if (p == start) break;
        --p;
    } while (true);

    // fallback: treat last byte (pos - 1) as illegal, 一个非法字节 = 一个字符
    return UTF8Decoded(0, false, pos - 1);
}

// 查找字符数（不是字节数）
std::size_t char_count(const ByteSpan& data) {
    std::size_t pos = 0;
    std::size_t count = 0;
    size_t size = data.size();

    std::vector<CodePoint> buf(size); // 最多每字节一个字符

    while (pos < size) {
        ByteSpan input = data.subspan(pos);
        auto result = decode_valid_prefix(input, buf);
        count += result.ok_chars;

        if (result.is_ok()) {
            break;
        }

        // is_err() 部分成功
        // 我们需要确定这个偏移量内有多少个完整字符(此时在偏移量内必然是合法字符)
        // 更新位置到错误处, 开始继续解码
        pos += result.consumed_bytes;
        UTF8Decoded bad = decode_one(data, pos);
        pos = bad.next_pos;
        count++;
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
        if (decode_result.codepoint == target_cp) return index;
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

        if (decode_result.codepoint == cp_old) {
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

        if (decode_result.codepoint == cp_old) {
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
