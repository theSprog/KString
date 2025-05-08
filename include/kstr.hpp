#pragma once
#include "iter.hpp"

namespace KString {
class KStr {
  public:
    KStr();

    // 该函数假设 cstr 是以 null 结尾的有效 UTF-8 字符串
    KStr(const char* cstr);

    // 非 null-terminated 字符串切片支持, 但UTF-8 合法性不保证，仅视为字节串
    KStr(const char* ptr, std::size_t len);

    // 面向底层操作场景，如 mmap buffer
    KStr(const uint8_t* ptr, std::size_t len);

    // initializer_list 不拥有所有权
    // KStr(std::initializer_list<utf8::Byte>) {    }

    explicit KStr(ByteSpan bytes);

    friend bool operator==(const KStr& lhs, const KStr& rhs);

    friend bool operator==(const KStr& lhs, const char* rhs);

    friend bool operator==(const char* lhs, const KStr& rhs);

    friend bool operator==(const KStr& lhs, const std::string& rhs);

    friend bool operator==(const std::string& lhs, const KStr& rhs);

    friend bool operator!=(const KStr& lhs, const KStr& rhs);

    friend std::ostream& operator<<(std::ostream& os, const KStr& s);

    friend bool operator<(const KStr& a, const KStr& b);

    friend bool operator>(const KStr& a, const KStr& b);
    friend bool operator<=(const KStr& a, const KStr& b);

    friend bool operator>=(const KStr& a, const KStr& b);

#if __cplusplus >= 201703L
    std::string_view as_string_view() const;
#endif


    bool empty() const;

    // 字节视图
    ByteSpan as_bytes() const;

    // 返回字符数（非字节数）
    std::size_t char_size() const;

    std::size_t byte_size() const;

    // 字符迭代器, 返回可迭代字符视图，等价于 as_chars()，每次返回一个 KChar（已解码）
    CharRange iter_chars() const;
    ReverseCharRange iter_chars_rev() const;

    // 获取第 idx 个字符（按字符下标），返回为子串字节
    ByteSpan operator[](std::size_t idx) const;

    KChar char_at(std::size_t idx) const;

    uint8_t byte_at(std::size_t idx) const;

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
    std::size_t count_chars_before(std::size_t byte_offset) const;

    std::size_t char_index_to_byte_offset(std::size_t idx) const;

    std::size_t find_bytes(ByteSpan hay, ByteSpan pat) const;

    // 朴素逆序匹配, BM 算法难以处理逆序
    std::size_t rfind_bytes(ByteSpan hay, ByteSpan pat) const;

    // 查找字符位置（按字符下标），找不到返回 npos
    std::size_t find(KStr substr) const;

    std::size_t find_in_byte(KStr substr) const;

    std::size_t rfind(KStr substr) const;

    std::size_t rfind_in_byte(KStr substr) const;

    bool contains(KStr substr) const;

    bool starts_with(KStr prefix) const;

    bool ends_with(KStr suffix) const;

    KStr substr(std::size_t start, std::size_t count) const;

    // [start, end) 区间
    KStr subrange(std::size_t start, std::size_t end) const;

    std::vector<CharIndex> char_indices() const;

    std::pair<KStr, KStr> split_at(std::size_t mid) const;
    std::pair<KStr, KStr> split_exclusive_at(std::size_t mid) const;
    // 核心 split 函数, 最多分割 count 次
    std::vector<KStr> split_count(KStr delim, std::size_t max_splits) const;
    std::vector<KStr> rsplit_count(KStr delim, std::size_t max_splits) const;
    std::vector<KStr> split(KStr delim) const;
    std::pair<KStr, KStr> split_once(KStr delim) const;
    std::vector<KStr> rsplit(KStr delim) const;
    std::pair<KStr, KStr> rsplit_once(KStr delim) const;
    std::vector<KStr> split_whitespace() const;

    std::vector<KStr> lines() const;

    KStr strip_prefix(KStr prefix) const;
    KStr strip_suffix(KStr suffix) const;

    KStr trim_start() const;
    KStr trim_end() const;
    KStr trim() const;

    template <typename Predicate, typename Emit>
    void match_loop(const CharRange& chars, Predicate pred, Emit emit) const {
        bool in_match = false;
        std::size_t start_byte = 0;
        std::size_t start_idx = 0;
        std::size_t cur_idx = 0;
        std::size_t byte_pos = 0;

        for (const auto& entry : chars) {
            if (pred(entry)) {
                if (! in_match) {
                    start_byte = byte_pos;
                    start_idx = cur_idx;
                    in_match = true;
                }
            } else {
                if (in_match) {
                    emit(start_idx, start_byte, byte_pos);
                    in_match = false;
                }
            }

            ++cur_idx;
            byte_pos += entry.utf8_size();
        }

        if (in_match) {
            emit(start_idx, start_byte, byte_pos);
        }
    }

    template <typename Predicate>
    std::vector<KStr> match(Predicate pred) const {
        std::vector<KStr> out;
        match_loop(iter_chars(), pred, [&](std::size_t, std::size_t start, std::size_t end) {
            out.emplace_back(&data_[start], end - start);
        });
        return out;
    }

    template <typename Predicate>
    std::vector<std::pair<std::size_t, KStr>> match_indices(Predicate pred) const {
        std::vector<std::pair<std::size_t, KStr>> out;
        match_loop(iter_chars(), pred, [&](std::size_t idx, std::size_t start, std::size_t end) {
            out.emplace_back(idx, KStr(ByteSpan(&data_[start], end - start)));
        });
        return out;
    }

    template <typename Predicate>
    KStr trim_start_matches(Predicate pred) const {
        std::size_t byte_start = 0;
        for (const auto& ch : iter_chars()) {
            if (! pred(ch)) break;
            byte_start += ch.utf8_size();
        }
        return KStr(ByteSpan(&data_[byte_start], data_.size() - byte_start));
    }

    template <typename Predicate>
    KStr trim_end_matches(Predicate pred) const {
        std::size_t byte_end = data_.size();
        for (const auto& ch : iter_chars_rev()) {
            if (! pred(ch)) break;
            byte_end -= ch.utf8_size();
        }
        return KStr(ByteSpan(&data_[0], byte_end));
    }

    template <typename Predicate>
    KStr trim_matches(Predicate pred) const {
        return trim_start_matches(pred).trim_end_matches(pred);
    }

  private:
    ByteSpan data_;
};
} // namespace KString
