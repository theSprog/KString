#pragma once
#include <cassert>
#include "../internal/utf8.hpp"
#include "./char.hpp"

namespace KString {
using internal::utf8::ByteSpan;
using internal::utf8::CodePoint;

class KStr {
  private:
    struct CharIterator {
        ByteSpan data;
        std::size_t cur_pos; // 当前字符的起始字节位置
        std::size_t end_pos; // 字符串末尾字节位置
        mutable KChar current;
        mutable bool decoded;

        CharIterator(ByteSpan data, std::size_t start_pos)
            : data(data), cur_pos(start_pos), end_pos(data.size()), current(), decoded(false) {}

        KChar operator*() const {
            if (cur_pos >= end_pos) return KChar(); // 返回空字符
            if (! decoded) {
                auto dec = internal::utf8::decode(data, cur_pos);
                current = dec.ok ? KChar(dec.cp) : KChar();
                decoded = true;
            }
            return current;
        }

        CharIterator& operator++() {
            if (decoded) { // 已经解码则直接复用
                std::size_t size = current.utf8_size();
                cur_pos += (size == 0) ? 1 : size;
            } else { // 尚未解码则解码获取 next_pos
                auto dec = internal::utf8::decode(data, cur_pos);
                cur_pos = dec.ok ? dec.next_pos : cur_pos + 1;
            }

            decoded = false;
            return *this;
        }

        bool operator==(const CharIterator& other) const {
            return cur_pos == other.cur_pos && data.data() == other.data.data();
        }

        bool operator!=(const CharIterator& other) const {
            return ! (*this == other);
        }

        using iterator_category = std::input_iterator_tag;
        using value_type = KChar;
        using difference_type = std::ptrdiff_t;
        using pointer = void;
        using reference = KChar;
    };

    struct CharRange {
        ByteSpan data;

        explicit CharRange(ByteSpan d) : data(d) {}

        CharIterator begin() const {
            return CharIterator(data, 0);
        }

        CharIterator end() const {
            return CharIterator(data, data.size());
        }
    };

    struct CharIndex {
        KChar ch;
        std::size_t byte_offset;
        std::size_t char_index;

        CharIndex() : ch(), byte_offset(0), char_index(0) {}

        CharIndex(KChar c, std::size_t byte_off, std::size_t char_idx)
            : ch(c), byte_offset(byte_off), char_index(char_idx) {}

        friend std::ostream& operator<<(std::ostream& os, const CharIndex& ci) {
            return os << "[" << ci.ch << ", byte=" << ci.byte_offset << ", char=" << ci.char_index << "]";
        }

        friend bool operator==(const CharIndex& a, const CharIndex& b) {
            return a.ch == b.ch && a.byte_offset == b.byte_offset && a.char_index == b.char_index;
        }

        friend bool operator!=(const CharIndex& a, const CharIndex& b) {
            return ! (a == b);
        }

        friend bool operator<(const CharIndex& a, const CharIndex& b) {
            return a.byte_offset < b.byte_offset;
        }
    };


  public:
    static constexpr size_t knpos = static_cast<size_t>(-1);

    KStr() : data_() {}

    // 该函数假设 cstr 是以 null 结尾的有效 UTF-8 字符串
    KStr(const char* cstr) : data_(reinterpret_cast<const uint8_t*>(cstr), std::strlen(cstr)) {}

    // 非 null-terminated 字符串切片支持, 但UTF-8 合法性不保证，仅视为字节串
    KStr(const char* ptr, std::size_t len) : data_(reinterpret_cast<const uint8_t*>(ptr), len) {}

    // 面向底层操作场景，如 mmap buffer
    KStr(const uint8_t* ptr, std::size_t len) : data_(ptr, len) {}

    explicit KStr(ByteSpan bytes) : data_(bytes) {}

    friend bool operator==(const KStr& lhs, const KStr& rhs) {
        if (lhs.data_.size() != rhs.data_.size()) return false;
        return std::memcmp(lhs.data_.data(), rhs.data_.data(), lhs.data_.size()) == 0;
    }

    friend bool operator==(const KStr& lhs, const char* rhs) {
        return lhs == KStr(rhs);
    }

    friend bool operator==(const char* lhs, const KStr& rhs) {
        return KStr(lhs) == rhs;
    }

    friend bool operator!=(const KStr& lhs, const KStr& rhs) {
        return ! (lhs == rhs);
    }

    friend std::ostream& operator<<(std::ostream& os, const KStr& s) {
        return os.write(reinterpret_cast<const char*>(s.as_bytes().data()), s.as_bytes().size());
    }

    friend bool operator<(const KStr& a, const KStr& b) {
        return std::lexicographical_compare(a.as_bytes().begin(),
                                            a.as_bytes().end(),
                                            b.as_bytes().begin(),
                                            b.as_bytes().end());
    }

    friend bool operator>(const KStr& a, const KStr& b) {
        return b < a;
    }

    friend bool operator<=(const KStr& a, const KStr& b) {
        return ! (b < a);
    }

    friend bool operator>=(const KStr& a, const KStr& b) {
        return ! (a < b);
    }

#if __cplusplus >= 201703L
    std::string_view to_string_view() const {
        return std::string_view(reinterpret_cast<const char*>(data_.data()), data_.size());
    }
#endif


    bool empty() const {
        return data_.empty();
    }

    // 字节视图
    ByteSpan as_bytes() const {
        return data_;
    }

    // 返回字符数（非字节数）
    std::size_t char_size() const {
        std::size_t count = 0, pos = 0;
        while (pos < data_.size()) {
            internal::utf8::UTF8Decoded dec = internal::utf8::decode(data_, pos);
            ++count;
            pos = dec.ok ? dec.next_pos : pos + 1; // 无效字节视作一个字符
        }
        return count;
    }

    std::size_t byte_size() {
        return data_.size();
    }

    // 字符迭代器, 返回可迭代字符视图，等价于 as_chars()，每次返回一个 KChar（已解码）
    CharRange iter_chars() const {
        return CharRange(data_);
    }

    // 获取第 idx 个字符（按字符下标），返回为子串字节
    ByteSpan operator[](std::size_t idx) const {
        std::size_t pos = 0, i = 0;
        while (pos < data_.size()) {
            internal::utf8::UTF8Decoded dec = internal::utf8::decode(data_, pos);
            if (! dec.ok) {
                throw std::runtime_error("KStr::operator[] decode failed at byte offset " + std::to_string(pos));
            }
            if (i == idx) {
                return ByteSpan(&data_[pos], dec.next_pos - pos);
            }
            pos = dec.next_pos;
            ++i;
        }

        throw std::out_of_range("KStr::operator[] index exceeds character count");
    }

    KChar char_at(std::size_t idx) const {
        std::size_t pos = 0, i = 0;
        while (pos < data_.size()) {
            auto dec = internal::utf8::decode(data_, pos);
            if (! dec.ok) {
                if (i == idx) {
                    return KChar(KString::KChar::Ill); // 视作独立非法字节
                }
                ++i;
                ++pos;
            } else {
                if (i == idx) {
                    return KChar(dec.cp);
                }
                ++i;
                pos = dec.next_pos;
            }
        }
        throw std::out_of_range("KStr::char_at index out of bounds");
    }

    uint8_t byte_at(std::size_t idx) const {
        if (idx >= data_.size()) {
            throw std::out_of_range("KStr::byte_at index out of bounds");
        }
        return data_[idx];
    }

    /**
     * @brief 统计给定 byte offset 之前有多少个字符
     * 字符下标表示从开头到该字节偏移前，共有多少个字符。
     * 
     * @warning 本函数是 O(N) 的，因为必须线性遍历每个 UTF-8 字符。
     * @param byte_offset 字节偏移（必须小于等于总字节长度）
     * @return std::size_t 从起始位置到该偏移之间的字符数量（即字符下标）
     * @example：
     *   KStr s = "你a好"; // UTF-8 字节数 = 3 + 1 + 3 = 7
     *   s.count_chars_before(0) -> 0
     *   s.count_chars_before(3) -> 1   // '你'
     *   s.count_chars_before(4) -> 2   // '你a'
     *   s.count_chars_before(7) -> 3   // '你a好'
     *
     *   s.count_chars_before(1) -> 1  // 视为单个非法字符
     *   s.count_chars_before(2) -> 2  // 视为两个非法字符
     */
    std::size_t count_chars_before(std::size_t byte_offset) const {
        if (byte_offset > data_.size()) {
            throw std::out_of_range("byte_offset exceeds data size");
        }

        return KStr(ByteSpan(data_.data(), byte_offset)).char_size();
    }

    std::size_t char_index_to_byte_offset(std::size_t idx) const {
        std::size_t pos = 0, i = 0;
        while (pos < data_.size()) {
            if (i == idx) return pos;
            auto dec = internal::utf8::decode(data_, pos);
            pos = dec.ok ? dec.next_pos : pos + 1;
            ++i;
        }
        throw std::out_of_range("KStr::char index exceeds character count");
    }

    // 查找字符位置（按字符下标），找不到返回 npos
    std::size_t find(KStr substr) const {
        auto hay = as_bytes();
        auto pat = substr.as_bytes();

        if (pat.empty()) return 0;
        if (pat.size() > hay.size()) return knpos;
        for (std::size_t i = 0; i <= hay.size() - pat.size(); ++i) {
            if (std::memcmp(hay.data() + i, pat.data(), pat.size()) == 0) {
                // 回推前面有多少个 codepoint 以获取字符下标
                return count_chars_before(i);
            }
        }
        return knpos;
    }

    std::size_t rfind(KStr substr) const {
        auto hay = as_bytes();
        auto pat = substr.as_bytes();

        if (pat.empty()) return char_size(); // 返回字符数作为尾部

        if (pat.size() > hay.size()) return knpos;

        for (std::size_t i = hay.size() - pat.size() + 1; i-- > 0;) {
            if (std::memcmp(hay.data() + i, pat.data(), pat.size()) == 0) {
                // 回推前面有多少个 codepoint 以获取字符下标
                return count_chars_before(i);
            }
        }
        return knpos;
    }

    bool contains(KStr substr) const {
        return find(substr) != knpos;
    }

    bool starts_with(KStr prefix) const {
        auto a = as_bytes();
        auto b = prefix.as_bytes();
        return a.size() >= b.size() && std::memcmp(a.data(), b.data(), b.size()) == 0;
    }

    bool ends_with(KStr suffix) const {
        auto a = as_bytes();
        auto b = suffix.as_bytes();
        return a.size() >= b.size() && std::memcmp(a.data() + a.size() - b.size(), b.data(), b.size()) == 0;
    }

    KStr substr(std::size_t start, std::size_t count) const {
        std::size_t pos = 0, idx = 0;
        std::size_t begin_byte = 0;
        bool found_begin = false;

        while (pos < data_.size()) {
            if (idx == start) {
                begin_byte = pos;
                found_begin = true;
            }

            auto dec = internal::utf8::decode(data_, pos);
            pos = dec.ok ? dec.next_pos : pos + 1;
            ++idx;

            if (idx == start + count) {
                return KStr(ByteSpan(&data_[begin_byte], pos - begin_byte));
            }
        }

        if (! found_begin) {
            throw std::out_of_range("KStr::substr start index out of bounds");
        }

        // 如果走到这，说明没够 count 个字符，取到末尾
        return KStr(ByteSpan(&data_[begin_byte], data_.size() - begin_byte));
    }

    // [start, end) 区间
    KStr subrange(std::size_t start, std::size_t end) const {
        if (start > end) {
            throw std::out_of_range("KStr::subrange invalid range: start > end");
        }

        std::size_t pos = 0, idx = 0;
        std::size_t begin_byte = 0, end_byte = 0;
        bool found_begin = false, found_end = false;

        while (pos < data_.size()) {
            if (idx == start) {
                begin_byte = pos;
                found_begin = true;
            }
            if (idx == end) {
                end_byte = pos;
                found_end = true;
                break;
            }

            auto dec = internal::utf8::decode(data_, pos);
            pos = dec.ok ? dec.next_pos : pos + 1;
            ++idx;
        }

        if (! found_begin) {
            throw std::out_of_range("KStr::subrange start index out of bounds");
        }
        if (! found_end) {
            end_byte = data_.size(); // end 落在末尾之后
        }

        return KStr(ByteSpan(&data_[begin_byte], end_byte - begin_byte));
    }

    std::vector<CharIndex> char_indices() const {
        std::vector<CharIndex> result;
        std::size_t pos = 0;
        std::size_t char_idx = 0;

        while (pos < data_.size()) {
            std::size_t offset = pos;
            auto dec = internal::utf8::decode(data_, pos);
            if (dec.ok) {
                result.emplace_back(KChar(dec.cp), offset, char_idx);
                pos = dec.next_pos;
            } else {
                result.emplace_back(KChar(data_[pos]), offset, char_idx);
                ++pos;
            }
            ++char_idx;
        }

        return result;
    }

    std::pair<KStr, KStr> split_at(std::size_t mid) const {
        std::size_t byte_offset = char_index_to_byte_offset(mid);
        return {KStr(ByteSpan(&data_[0], byte_offset)),
                KStr(ByteSpan(&data_[byte_offset], data_.size() - byte_offset))};
    }

    std::pair<KStr, KStr> split_exclusive_at(std::size_t mid) const {
        auto pair = split_at(mid);
        const ByteSpan right_bytes = pair.second.as_bytes();

        assert(!right_bytes.empty());
        
        std::size_t skip = 0;
        auto dec = internal::utf8::decode(right_bytes, skip);
        std::size_t skip_len = dec.ok ? dec.next_pos : 1;

        return {pair.first, KStr(ByteSpan(right_bytes.data() + skip_len, right_bytes.size() - skip_len))};
    }

    // 核心 split 函数, 最多分割 count 次
    std::vector<KStr> split_count(KStr delim, std::size_t max_splits) const {
        if (delim.empty()) {
            throw std::invalid_argument("KStr::split_count(KStr) with empty delimiter is not allowed");
        }

        std::vector<KStr> result;
        auto delim_bytes = delim.as_bytes();
        std::size_t start = 0;
        std::size_t pos = 0;
        std::size_t splits_done = 0;

        while (pos + delim_bytes.size() <= data_.size()) {
            if (std::memcmp(&data_[pos], delim_bytes.data(), delim_bytes.size()) == 0) {
                result.emplace_back(ByteSpan(&data_[start], pos - start));
                pos += delim_bytes.size();
                start = pos;
                ++splits_done;
                if (splits_done >= max_splits) {
                    break;
                }
            } else {
                ++pos;
            }
        }

        result.emplace_back(ByteSpan(&data_[start], data_.size() - start));
        return result;
    }

    std::vector<KStr> rsplit_count(KStr delim, std::size_t max_splits) const {
        if (delim.empty()) {
            throw std::invalid_argument("KStr::rsplit_count(KStr) with empty delimiter is not allowed");
        }

        std::vector<KStr> result;
        auto hay = as_bytes();
        auto pat = delim.as_bytes();

        std::size_t end = hay.size();
        std::size_t splits_done = 0;

        while (end >= pat.size() && splits_done < max_splits) {
            bool matched = false;

            for (std::size_t i = end - pat.size(); i != static_cast<std::size_t>(-1); --i) {
                if (std::memcmp(hay.data() + i, pat.data(), pat.size()) == 0) {
                    result.emplace_back(ByteSpan(&hay[i + pat.size()], end - i - pat.size()));
                    end = i;
                    ++splits_done;
                    matched = true;
                    break;
                }
            }

            if (! matched) break;
        }

        result.emplace_back(ByteSpan(&hay[0], end));
        return result;
    }

    std::vector<KStr> split(KStr delim) const {
        return split_count(delim, static_cast<std::size_t>(-1));
    }

    std::pair<KStr, KStr> split_once(KStr delim) const {
        auto result = split_count(delim, 1);
        if (result.size() == 1) return {result[0], KStr()};
        return {result[0], result[1]};
    }

    std::vector<KStr> rsplit(KStr delim) const {
        return rsplit_count(delim, static_cast<std::size_t>(-1));
    }

    std::pair<KStr, KStr> rsplit_once(KStr delim) const {
        auto result = rsplit_count(delim, 1);
        if (result.size() == 1) return {KStr(), result[0]};
        return {result[0], result[1]};
    }

    std::vector<KStr> split_whitespace() const {
        std::vector<KStr> result;
        std::size_t pos = 0;
        std::size_t token_start = 0;
        bool in_whitespace = true;

        while (pos < data_.size()) {
            std::size_t current = pos;
            auto dec = internal::utf8::decode(data_, pos);

            if (! dec.ok) {
                ++pos; // 跳过非法字节
                continue;
            }

            KChar ch(dec.cp);
            pos = dec.next_pos;

            if (ch.is_whitespace()) {
                if (! in_whitespace) {
                    result.emplace_back(ByteSpan(&data_[token_start], current - token_start));
                    in_whitespace = true;
                } else {
                    // already in whitespace, skip
                }
            } else {
                // 如果之前在 whitespace 需要更新
                if (in_whitespace) {
                    token_start = current;
                    in_whitespace = false;
                }
            }
        }

        if (! in_whitespace) {
            result.emplace_back(ByteSpan(&data_[token_start], data_.size() - token_start));
        }

        return result;
    }

    std::vector<KStr> lines() const {
        std::vector<KStr> result;
        std::size_t start = 0;
        std::size_t pos = 0;

        while (pos < data_.size()) {
            if (data_[pos] == '\r') {
                // \r\n 处理
                if (pos + 1 < data_.size() && data_[pos + 1] == '\n') {
                    result.emplace_back(ByteSpan(&data_[start], pos - start));
                    pos += 2;
                    start = pos;
                } else { // 只处理 \r
                    result.emplace_back(ByteSpan(&data_[start], pos - start));
                    ++pos;
                    start = pos;
                }
            } else if (data_[pos] == '\n') { // 只处理 \n
                result.emplace_back(ByteSpan(&data_[start], pos - start));
                ++pos;
                start = pos;
            } else {
                ++pos;
            }
        }

        if (start <= data_.size()) {
            result.emplace_back(ByteSpan(&data_[start], data_.size() - start));
        }

        return result;
    }

    template <typename Predicate>
    std::vector<KStr> match(Predicate pred) const {
        std::vector<KStr> result;
        std::size_t pos = 0;
        std::size_t start = 0;
        bool in_match = false;

        while (pos < data_.size()) {
            std::size_t current = pos;
            auto dec = internal::utf8::decode(data_, pos);

            if (! dec.ok) {
                ++pos;
                continue;
            }

            KChar ch(dec.cp);
            pos = dec.next_pos;

            if (pred(ch)) {
                if (! in_match) {
                    start = current;
                    in_match = true;
                }
            } else {
                if (in_match) {
                    result.emplace_back(ByteSpan(&data_[start], current - start));
                    in_match = false;
                }
            }
        }

        if (in_match) {
            result.emplace_back(ByteSpan(&data_[start], data_.size() - start));
        }

        return result;
    }

    template <typename Predicate>
    std::vector<KStr> rmatch(Predicate pred) const {
        std::vector<KStr> result;
        auto chars = char_indices(); // 获取 (KChar, byte_pos)

        std::size_t i = chars.size();
        std::size_t end_byte = data_.size();
        bool in_match = false;

        while (i-- > 0) {
            auto& entry = chars[i];
            const KChar& ch = entry.ch;
            std::size_t byte_pos = entry.byte_offset;

            if (pred(ch)) {
                if (! in_match) {
                    end_byte = byte_pos + ch.utf8_size(); // ch.len() 是 UTF-8 长度，可缓存或用 decode
                    in_match = true;
                }
            } else {
                if (in_match) {
                    result.emplace_back(
                        ByteSpan(&data_[byte_pos + ch.utf8_size()], end_byte - (byte_pos + ch.utf8_size())));
                    in_match = false;
                }
            }
        }

        if (in_match) {
            result.emplace_back(ByteSpan(&data_[0], end_byte));
        }

        return result;
    }

    template <typename Predicate>
    std::vector<std::pair<std::size_t, KStr>> match_indices(Predicate pred) const {
        std::vector<std::pair<std::size_t, KStr>> result;
        std::size_t pos = 0, char_idx = 0;
        std::size_t start_byte = 0, start_idx = 0;
        bool in_match = false;

        while (pos < data_.size()) {
            std::size_t cur = pos;
            auto dec = internal::utf8::decode(data_, pos);
            if (! dec.ok) {
                ++pos;
                ++char_idx;
                continue;
            }

            KChar ch(dec.cp);
            if (pred(ch)) {
                if (! in_match) {
                    start_byte = cur;
                    start_idx = char_idx;
                    in_match = true;
                }
            } else {
                if (in_match) {
                    result.emplace_back(start_idx, KStr(ByteSpan(&data_[start_byte], cur - start_byte)));
                    in_match = false;
                }
            }

            ++char_idx;
        }

        if (in_match) {
            result.emplace_back(start_idx, KStr(ByteSpan(&data_[start_byte], data_.size() - start_byte)));
        }

        return result;
    }

    template <typename Predicate>
    std::vector<std::pair<std::size_t, KStr>> rmatch_indices(Predicate pred) const {
        std::vector<std::pair<std::size_t, KStr>> result;
        auto chars = char_indices(); // [(KChar, byte_offset, char_index)]

        std::size_t i = chars.size();
        std::size_t end_byte = data_.size();
        bool in_match = false;

        while (i-- > 0) {
            const KChar& ch = chars[i].ch;
            std::size_t byte_pos = chars[i].byte_offset;
            std::size_t char_idx = i;

            if (pred(ch)) {
                if (! in_match) {
                    end_byte = byte_pos + ch.utf8_size();
                    in_match = true;
                }
            } else {
                if (in_match) {
                    result.emplace_back(
                        char_idx + 1,
                        KStr(ByteSpan(&data_[byte_pos + ch.utf8_size()], end_byte - (byte_pos + ch.utf8_size()))));
                    in_match = false;
                }
            }
        }

        if (in_match) {
            result.emplace_back(0, KStr(ByteSpan(&data_[0], end_byte)));
        }

        return result;
    }

    KStr trim_start() const {
        std::size_t pos = 0;

        while (pos < data_.size()) {
            auto dec = internal::utf8::decode(data_, pos);
            if (! dec.ok) {
                ++pos;
                continue;
            }

            if (! KChar(dec.cp).is_whitespace()) {
                break;
            }

            pos = dec.next_pos;
        }

        return KStr(ByteSpan(&data_[pos], data_.size() - pos));
    }

    KStr trim_end() const {
        std::size_t pos = 0;
        std::size_t last_non_space = data_.size(); // 初始认为全是空白

        while (pos < data_.size()) {
            auto dec = internal::utf8::decode(data_, pos);
            if (! dec.ok) {
                ++pos;
                continue;
            }

            if (! KChar(dec.cp).is_whitespace()) {
                last_non_space = pos;
            }

            pos = dec.next_pos;
        }

        return KStr(ByteSpan(&data_[0], last_non_space));
    }

    KStr trim() const {
        return trim_start().trim_end();
    }

    template <typename Predicate>
    KStr trim_start_matches(Predicate pred) const {
        std::size_t pos = 0;
        while (pos < data_.size()) {
            auto dec = internal::utf8::decode(data_, pos);
            if (! dec.ok) {
                ++pos;
                continue;
            }
            if (! pred(KChar(dec.cp))) break;
            pos = dec.next_pos;
        }
        return KStr(ByteSpan(&data_[pos], data_.size() - pos));
    }

    template <typename Predicate>
    KStr trim_end_matches(Predicate pred) const {
        std::size_t pos = 0;
        std::size_t last_non_match = data_.size();

        while (pos < data_.size()) {
            auto cur = pos;
            auto dec = internal::utf8::decode(data_, pos);
            if (! dec.ok) {
                ++pos;
                continue;
            }
            if (! pred(KChar(dec.cp))) {
                last_non_match = pos;
            }
            pos = dec.next_pos;
        }

        return KStr(ByteSpan(&data_[0], last_non_match));
    }

    template <typename Predicate>
    KStr trim_matches(Predicate pred) const {
        return trim_start_matches(pred).trim_end_matches(pred);
    }

    KStr strip_prefix(KStr prefix) const {
        auto a = as_bytes();
        auto b = prefix.as_bytes();
        if (a.size() >= b.size() && std::memcmp(a.data(), b.data(), b.size()) == 0) {
            return KStr(ByteSpan(&a[b.size()], a.size() - b.size()));
        }
        return *this;
    }

    KStr strip_suffix(KStr suffix) const {
        auto a = as_bytes();
        auto b = suffix.as_bytes();
        if (a.size() >= b.size() && std::memcmp(a.data() + a.size() - b.size(), b.data(), b.size()) == 0) {
            return KStr(ByteSpan(&a[0], a.size() - b.size()));
        }
        return *this;
    }

  private:
    ByteSpan data_;
};

constexpr std::size_t KStr::knpos;
} // namespace KString
