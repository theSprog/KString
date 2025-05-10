#pragma once

#include "base.hpp"
#include <cstddef>
#include <cstring>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <ostream>

#include "./kastr.hpp"
#include "./sso.hpp"

namespace kstring {
class KAString {
  public:
    KAString() : data_() {}

    KAString(const char* cstr) : data_(cstr) {}

    KAString(const std::string& str) : data_(str) {}

    KAString(const char* ptr, std::size_t len) : data_(ptr, len) {}

    KAString(const Byte* ptr, std::size_t len) : data_(ptr, len) {}

    KAString(std::initializer_list<Byte> vec) : data_(vec) {}

    KAString(KAStr kastr) : data_(kastr.data(), kastr.byte_size()) {}

    // 拷贝构造/赋值, 移动构造/赋值, 析构
    KAString(const KAString&) = default;
    KAString& operator=(const KAString&) = default;
    KAString(KAString&&) noexcept = default;
    KAString& operator=(KAString&&) noexcept = default;
    ~KAString() = default;

    operator std::string() const {
        return std::string(reinterpret_cast<const char*>(data_.data()), data_.size());
    }

    // 隐式转换 KAStr, 也可以通过 as_kastr() 显式转换
    operator KAStr() const {
        return KAStr(data_.data(), data_.size());
    }

    friend std::ostream& operator<<(std::ostream& os, const KAString& s) {
        return os << static_cast<std::string>(s); // 或 s.to_string()
    }

    char operator[](std::size_t idx) const {
        return static_cast<char>(byte_at(idx));
    }

    char& operator[](std::size_t idx) {
        if (idx >= data_.size()) {
            throw std::out_of_range("KAString::operator[] index out of bounds");
        }
        return reinterpret_cast<char&>(data_[idx]);
    }

    friend bool operator==(const KAString& lhs, const KAString& rhs) {
        if (lhs.byte_size() != rhs.byte_size()) return false;
        if (lhs.byte_size() == 0) return true; // 都是空串不用比较
        return std::memcmp(lhs.data(), rhs.data(), lhs.byte_size()) == 0;
    }

    friend bool operator!=(const KAString& lhs, const KAString& rhs) {
        return ! (lhs == rhs);
    }

    // KAString == const char*
    friend bool operator==(const KAString& lhs, const char* rhs) {
        if (rhs == nullptr) return lhs.empty();
        std::size_t len = std::strlen(rhs);
        if (lhs.empty()) return len == 0;
        return lhs.byte_size() == len && std::memcmp(lhs.data(), rhs, len) == 0;
    }

    friend bool operator==(const char* lhs, const KAString& rhs) {
        return rhs == lhs;
    }

    friend bool operator==(const KAString& lhs, const std::string& rhs) {
        return lhs.byte_size() == rhs.size() && std::memcmp(lhs.data(), rhs.data(), rhs.size()) == 0;
    }

    friend bool operator==(const std::string& lhs, const KAString& rhs) {
        return rhs == lhs;
    }

    friend bool operator!=(const KAString& lhs, const char* rhs) {
        return ! (lhs == rhs);
    }

    friend bool operator!=(const char* lhs, const KAString& rhs) {
        return ! (lhs == rhs);
    }

    friend bool operator!=(const KAString& lhs, const std::string& rhs) {
        return ! (lhs == rhs);
    }

    friend bool operator!=(const std::string& lhs, const KAString& rhs) {
        return ! (lhs == rhs);
    }

    // KAString + KAString
    friend KAString operator+(const KAString& lhs, const KAString& rhs) {
        KAString result;
        result.data_.reserve(lhs.byte_size() + rhs.byte_size());
        result.append(lhs);
        result.append(rhs);
        return result;
    }

    // KAString + const char*
    friend KAString operator+(const KAString& lhs, const char* rhs) {
        KAString result = lhs;
        result.append(rhs);
        return result;
    }

    // const char* + KAString
    friend KAString operator+(const char* lhs, const KAString& rhs) {
        KAString result(lhs);
        result.append(rhs);
        return result;
    }

    // KAString + std::string
    friend KAString operator+(const KAString& lhs, const std::string& rhs) {
        KAString result = lhs;
        result.append(rhs.data(), rhs.size());
        return result;
    }

    // std::string + KAString
    friend KAString operator+(const std::string& lhs, const KAString& rhs) {
        KAString result(lhs);
        result.append(rhs);
        return result;
    }

    // KAString + char
    friend KAString operator+(const KAString& lhs, char ch) {
        KAString result = lhs;
        result.append(ch);
        return result;
    }

    // char + KAString
    friend KAString operator+(char ch, const KAString& rhs) {
        KAString result;
        result.append(ch);
        result.append(rhs);
        return result;
    }

    KAString& operator+=(const KAString& rhs) {
        this->append(rhs);
        return *this;
    }

    KAString& operator+=(const KAStr& rhs) {
        this->append(rhs);
        return *this;
    }

    KAString& operator+=(const char* rhs) {
        this->append(rhs);
        return *this;
    }

    KAString& operator+=(const std::string& rhs) {
        this->append(rhs.data(), rhs.size());
        return *this;
    }

    KAString& operator+=(char ch) {
        this->append(ch);
        return *this;
    }

    void append(char ch) {
        data_.push_back(static_cast<Byte>(ch));
    }

    void append(const char* cstr) {
        if (cstr == nullptr) return;
        const std::size_t len = std::strlen(cstr);
        append(cstr, len);
    }

    void append(const char* ptr, std::size_t len) {
        if (ptr == nullptr || len == 0) return;
        data_.append(reinterpret_cast<const Byte*>(ptr), len);
    }

    void append(const KAString& other) {
        data_.append(other.begin(), other.byte_size());
    }

    void append(const KAStr& strview) {
        data_.append(strview.begin(), strview.byte_size());
    }

    int compare(const KAString& other) const {
        if (this->byte_size() < other.byte_size()) return -1;
        if (this->byte_size() > other.byte_size()) return 1;
        if (this->byte_size() == 0) return 0; // 都是空串

        const std::size_t n = std::min(this->byte_size(), other.byte_size());
        return std::memcmp(this->data(), other.data(), n);
    }

    bool operator<(const KAString& other) const {
        return this->compare(other) < 0;
    }

    bool empty() const {
        return data_.empty();
    }

    std::size_t byte_size() const {
        return data_.size();
    }

    std::size_t char_size() const {
        return data_.size(); // ASCII only
    }

    const Byte* data() const {
        return data_.data();
    }

    Byte* data() {
        return data_.data();
    }

    const Byte* begin() const {
        return data_.data();
    }

    Byte* begin() {
        return data_.data();
    }

    const Byte* end() const {
        return data_.data() + data_.size();
    }

    Byte* end() {
        return data_.data() + data_.size();
    }

    using const_reverse_iterator = std::reverse_iterator<const Byte*>;

    const_reverse_iterator rbegin() const {
        return const_reverse_iterator(end());
    }

    const_reverse_iterator rend() const {
        return const_reverse_iterator(begin());
    }

    using reverse_iterator = std::reverse_iterator<Byte*>;

    reverse_iterator rbegin() {
        return reverse_iterator(end());
    }

    reverse_iterator rend() {
        return reverse_iterator(begin());
    }

    uint8_t byte_at(std::size_t idx) const {
        if (idx >= data_.size()) {
            throw std::out_of_range("KAString::byte_at index out of bounds");
        }
        return data_[idx];
    }

    void reserve(size_t cap) {
        data_.reserve(cap);
    }

    void resize(size_t new_size) {
        data_.resize(new_size, 0);
    }

    void resize(size_t new_size, const Byte& b) {
        data_.resize(new_size, b);
    }

    KAStr as_kastr() const {
        return KAStr(data_.data(), data_.size());
    }

    std::size_t find(KAStr substr) const {
        return as_kastr().find(substr);
    }

    std::size_t rfind(KAStr substr) const {
        return as_kastr().rfind(substr);
    }

    bool contains(KAStr substr) const {
        return as_kastr().contains(substr);
    }

    bool starts_with(KAStr prefix) const {
        return as_kastr().starts_with(prefix);
    }

    bool ends_with(KAStr suffix) const {
        return as_kastr().ends_with(suffix);
    }

    KAStr substr(std::size_t start, std::size_t count) const {
        return as_kastr().substr(start, count);
    }

    KAStr substr(std::size_t start) const {
        return as_kastr().substr(start);
    }

    KAStr subrange(std::size_t start, std::size_t end) const {
        return as_kastr().subrange(start, end);
    }

    KAStr subrange(std::size_t start) const {
        return as_kastr().subrange(start);
    }

    std::pair<KAStr, KAStr> split_at(std::size_t mid) const {
        return as_kastr().split_at(mid);
    }

    std::pair<KAStr, KAStr> split_exclusive_at(std::size_t mid) const {
        return as_kastr().split_exclusive_at(mid);
    }

    std::vector<KAStr> split_count(KAStr delim, std::size_t max_splits) const {
        return as_kastr().split_count(delim, max_splits);
    }

    std::vector<KAStr> rsplit_count(KAStr delim, std::size_t max_splits) const {
        return as_kastr().rsplit_count(delim, max_splits);
    }

    std::vector<KAStr> split(KAStr delim) const {
        return as_kastr().split(delim);
    }

    std::vector<KAStr> rsplit(KAStr delim) const {
        return as_kastr().rsplit(delim);
    }

    std::pair<KAStr, KAStr> split_once(KAStr delim) const {
        return as_kastr().split_once(delim);
    }

    std::pair<KAStr, KAStr> rsplit_once(KAStr delim) const {
        return as_kastr().rsplit_once(delim);
    }

    std::vector<KAStr> split_whitespace() const {
        return as_kastr().split_whitespace();
    }

    std::vector<KAStr> lines() const {
        return as_kastr().lines();
    }

    KAStr strip_prefix(KAStr prefix) const {
        return as_kastr().strip_prefix(prefix);
    }

    KAStr strip_suffix(KAStr suffix) const {
        return as_kastr().strip_suffix(suffix);
    }

    KAStr trim_start() const {
        return as_kastr().trim_start();
    }

    KAStr trim_end() const {
        return as_kastr().trim_end();
    }

    KAStr trim() const {
        return as_kastr().trim();
    }

    template <typename Predicate>
    std::vector<KAStr> match(Predicate pred) const {
        return as_kastr().match(pred);
    }

    template <typename Predicate>
    std::vector<std::pair<std::size_t, KAStr>> match_indices(Predicate pred) const {
        return as_kastr().match_indices(pred);
    }

    template <typename Predicate>
    KAStr trim_start_matches(Predicate pred) const {
        return as_kastr().trim_start_matches(pred);
    }

    template <typename Predicate>
    KAStr trim_end_matches(Predicate pred) const {
        return as_kastr().trim_end_matches(pred);
    }

    template <typename Predicate>
    KAStr trim_matches(Predicate pred) const {
        return as_kastr().trim_matches(pred);
    }

  private:
    SSOBytes data_;
};
} // namespace kstring

namespace std {
template <>
struct hash<kstring::KAString> {
    std::size_t operator()(const kstring::KAString& s) const {
        return std::hash<kstring::KAStr>()(s);
    }
};
} // namespace std
