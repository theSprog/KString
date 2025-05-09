#include "base.hpp"
#include <cstring>
#include <stdexcept>
#include <string>
#include <ostream>

namespace KAString {
using kstring::Byte;

// ascii-only string, read-only and hasn't ownership
class KAStr {
  public:
    KAStr() : data_() {}

    KAStr(const char* cstr) : data_(reinterpret_cast<const Byte*>(cstr), std::strlen(cstr)) {}

    KAStr(const std::string s) : data_(reinterpret_cast<const Byte*>(s.c_str()), s.size()) {}

    KAStr(const char* ptr, std::size_t len) : data_(reinterpret_cast<const Byte*>(ptr), len) {}

    KAStr(const Byte* ptr, std::size_t len) : data_(ptr, len) {}

    bool empty() const {
        return data_.empty();
    }

    std::size_t byte_size() const {
        return data_.size();
    }

    std::size_t char_size() const {
        return data_.size();
    } // ASCII only: 1 byte = 1 char

    const Byte* data() const {
        return data_.data();
    }

    std::string to_string() const {
        return std::string(reinterpret_cast<const char*>(data_.data()), data_.size());
    }

    const Byte* begin() const {
        return data_.begin();
    }

    const Byte* end() const {
        return data_.end();
    }

    uint8_t byte_at(std::size_t idx) const {
        if (idx >= data_.size()) {
            throw std::out_of_range("KAString::byte_at index out of bounds");
        }
        return data_[idx];
    }

    char operator[](std::size_t idx) const {
        return static_cast<char>(byte_at(idx));
    }

    std::size_t find(KAStr substr) const;

    std::size_t rfind(KAStr substr) const;

    bool contains(KAStr substr) const;

    bool starts_with(KAStr prefix) const;

    bool ends_with(KAStr suffix) const;

    KAStr substr(std::size_t start, std::size_t count) const;
    KAStr substr(std::size_t start) const;

    KAStr subrange(std::size_t start, std::size_t end) const;
    KAStr subrange(std::size_t start) const;

    friend bool operator==(const KAStr& lhs, const KAStr& rhs);

    friend bool operator!=(const KAStr& lhs, const KAStr& rhs);

    friend bool operator==(const KAStr& lhs, const char* rhs);

    friend std::ostream& operator<<(std::ostream& os, const KAStr& s);

    std::pair<KAStr, KAStr> split_at(std::size_t mid) const;

    std::pair<KAStr, KAStr> split_exclusive_at(std::size_t mid) const;

    std::vector<KAStr> split_count(KAStr delim, std::size_t max_splits = static_cast<std::size_t>(-1)) const;

    std::vector<KAStr> rsplit_count(KAStr delim, std::size_t max_splits = static_cast<std::size_t>(-1)) const;

    std::vector<KAStr> split(KAStr delim) const;

    std::vector<KAStr> rsplit(KAStr delim) const;

    std::pair<KAStr, KAStr> split_once(KAStr delim) const;

    std::pair<KAStr, KAStr> rsplit_once(KAStr delim) const;

    std::vector<KAStr> split_whitespace() const;

    std::vector<KAStr> lines() const;

    KAStr strip_prefix(KAStr prefix) const;

    KAStr strip_suffix(KAStr suffix) const;

    KAStr trim_start() const;

    KAStr trim_end() const;

    KAStr trim() const;

    template <typename Predicate>
    std::vector<KAStr> match(Predicate pred) const {
        std::vector<KAStr> out;
        std::size_t start = 0;
        while (start < byte_size()) {
            while (start < byte_size() && ! pred(data_[start])) ++start;
            std::size_t end = start;
            while (end < byte_size() && pred(data_[end])) ++end;
            if (start < end) out.emplace_back(data_.begin() + start, end - start);
            start = end;
        }
        return out;
    }

    template <typename Predicate>
    std::vector<std::pair<std::size_t, KAStr>> match_indices(Predicate pred) const {
        std::vector<std::pair<std::size_t, KAStr>> out;
        std::size_t start = 0;
        while (start < byte_size()) {
            while (start < byte_size() && ! pred(data_[start])) ++start;
            std::size_t end = start;
            while (end < byte_size() && pred(data_[end])) ++end;
            if (start < end) out.emplace_back(start, KAStr(data_.begin() + start, end - start));
            start = end;
        }
        return out;
    }

    template <typename Predicate>
    KAStr trim_start_matches(Predicate pred) const {
        std::size_t start = 0;
        while (start < byte_size() && pred(data_[start])) ++start;
        return KAStr(data_.begin() + start, byte_size() - start);
    }

    template <typename Predicate>
    KAStr trim_end_matches(Predicate pred) const {
        std::size_t end = byte_size();
        while (end > 0 && pred(data_[end - 1])) --end;
        return KAStr(data_.begin(), end);
    }

    template <typename Predicate>
    KAStr trim_matches(Predicate pred) const {
        return trim_start_matches(pred).trim_end_matches(pred);
    }


  private:
    // 不拥有所有权
    kstring::ByteSpan data_;
};

} // namespace KAString
