#include "../include/kastr.hpp"
#include <stdexcept>

namespace KAString {
using kstring::Byte;

std::size_t KAStr::find(KAStr substr) const {
    if (substr.byte_size() == 0) return 0;
    if (substr.byte_size() > byte_size()) return kstring::knpos;

#if defined(__GLIBC__) || defined(__linux__)
    const void* where = ::memmem(begin(), byte_size(), substr.begin(), substr.byte_size());
    if (where) return static_cast<const Byte*>(where) - data_.begin();
#else
    auto* it = std::search(begin(), end(), substr.begin(), substr.end(), [](Byte a, Byte b) { return a == b; });
    if (it != data_.end()) return it - data_.begin();
#endif
    return kstring::knpos;
}

std::size_t KAStr::rfind(KAStr substr) const {
    if (substr.byte_size() > byte_size()) return kstring::knpos;
    for (std::size_t i = byte_size() - substr.byte_size() + 1; i-- > 0;) {
        // 朴素遍历
        if (std::memcmp(data_.begin() + i, substr.begin(), substr.byte_size()) == 0) return i;
    }
    return kstring::knpos;
}

bool KAStr::contains(KAStr substr) const {
    return find(substr) != kstring::knpos;
}

bool KAStr::starts_with(KAStr prefix) const {
    if (prefix.byte_size() > byte_size()) return false;
    return std::memcmp(data_.begin(), prefix.begin(), prefix.byte_size()) == 0;
}

bool KAStr::ends_with(KAStr suffix) const {
    if (suffix.byte_size() > byte_size()) return false;
    return std::memcmp(data_.end() - suffix.byte_size(), suffix.begin(), suffix.byte_size()) == 0;
}

KAStr KAStr::substr(std::size_t start, std::size_t count) const {
    if (start > byte_size()) return KAStr();
    count = std::min(count, byte_size() - start);
    return KAStr(data_.begin() + start, count);
}

KAStr KAStr::substr(std::size_t start) const {
    if (start >= byte_size()) return KAStr();
    return substr(start, byte_size() - start);
}

KAStr KAStr::subrange(std::size_t start, std::size_t end) const {
    if (start >= end) return KAStr();
    return substr(start, end - start);
}

KAStr KAStr::subrange(std::size_t start) const {
    return subrange(start, byte_size());
}

bool operator==(const KAStr& lhs, const KAStr& rhs) {
    if (lhs.byte_size() != rhs.byte_size()) return false;
    if (lhs.byte_size() == 0) return true; // 都是空串
    return lhs.byte_size() == rhs.byte_size() && std::memcmp(lhs.begin(), rhs.begin(), lhs.byte_size()) == 0;
}

bool operator!=(const KAStr& lhs, const KAStr& rhs) {
    return ! (lhs == rhs);
}

bool operator==(const KAStr& lhs, const char* rhs) {
    return lhs == KAStr(rhs);
}

std::ostream& operator<<(std::ostream& os, const KAStr& s) {
    return os.write(reinterpret_cast<const char*>(s.begin()), s.byte_size());
}

std::pair<KAStr, KAStr> KAStr::split_at(std::size_t mid) const {
    if (mid > byte_size()) {
        throw std::runtime_error("KAStr::split_at: mid offset " + std::to_string(mid) + " > byte_offset() " +
                                 std::to_string(byte_size()));
    }
    return {KAStr(data_.begin(), mid), KAStr(data_.begin() + mid, byte_size() - mid)};
}

std::pair<KAStr, KAStr> KAStr::split_exclusive_at(std::size_t mid) const {
    if (mid >= byte_size()) {
        throw std::runtime_error("KAStr::split_exclusive_at: mid offset " + std::to_string(mid) + " > byte_offset() " +
                                 std::to_string(byte_size()));
    }
    return {KAStr(data_.begin(), mid), KAStr(data_.begin() + mid + 1, byte_size() - mid - 1)};
}

std::vector<KAStr> KAStr::split_count(KAStr delim, std::size_t max_splits) const {
    std::vector<KAStr> result;
    std::size_t pos = 0, splits = 0;

    if (delim.empty()) {
        std::size_t len = this->byte_size();
        splits = std::min(len, max_splits);

        for (std::size_t i = 0; i < splits; ++i) result.emplace_back(this->begin() + i, 1);

        if (splits < len) result.emplace_back(this->begin() + splits, len - splits);
        return result;
    }

    while (splits < max_splits) {
        std::size_t found = this->subrange(pos, byte_size()).find(delim);
        if (found == kstring::knpos) break;

        result.emplace_back(data_.begin() + pos, found);
        pos += found + delim.byte_size();
        ++splits;
    }

    if (pos <= byte_size()) result.emplace_back(data_.begin() + pos, byte_size() - pos);
    return result;
}

std::vector<KAStr> KAStr::rsplit_count(KAStr delim, std::size_t max_splits) const {
    std::vector<KAStr> result;
    std::size_t end = byte_size();
    std::size_t splits = 0;

    if (delim.empty()) {
        std::size_t len = this->byte_size();
        splits = std::min(len, max_splits);
        std::size_t remain = len - splits;

        // 每个字符作为独立分段
        for (std::size_t i = len; i-- > remain;) {
            result.emplace_back(this->begin() + i, 1);
        }
        // 保留前缀部分为剩余段
        if (remain > 0) result.emplace_back(this->begin(), remain);

        return result;
    }

    while (splits < max_splits) {
        std::size_t found = this->subrange(0, end).rfind(delim);
        if (found == kstring::knpos) break;

        std::size_t after = found + delim.byte_size();
        result.emplace_back(data_.begin() + after, end - after);
        end = found;
        ++splits;
    }

    result.emplace_back(data_.data(), end);
    return result;
}

std::vector<KAStr> KAStr::split(KAStr delim) const {
    return split_count(delim, static_cast<std::size_t>(-1));
}

std::vector<KAStr> KAStr::rsplit(KAStr delim) const {
    return rsplit_count(delim, static_cast<std::size_t>(-1));
}

std::pair<KAStr, KAStr> KAStr::split_once(KAStr delim) const {
    auto vec = split_count(delim, 1);
    if (vec.size() == 1) return {vec[0], KAStr()};
    return {vec[0], vec[1]};
}

std::pair<KAStr, KAStr> KAStr::rsplit_once(KAStr delim) const {
    auto vec = rsplit_count(delim, 1);
    if (vec.size() == 1) return {vec[0], KAStr()};
    return {vec[0], vec[1]};
}

std::vector<KAStr> KAStr::split_whitespace() const {
    std::vector<KAStr> result;
    std::size_t start = 0;
    while (start < byte_size()) {
        while (start < byte_size() && isspace(data_[start])) ++start;
        std::size_t end = start;
        while (end < byte_size() && ! isspace(data_[end])) ++end;
        if (start < end) result.emplace_back(data_.begin() + start, end - start);
        start = end;
    }
    return result;
}

std::vector<KAStr> KAStr::lines() const {
    std::vector<KAStr> result;
    std::size_t start = 0;

    for (std::size_t i = 0; i < byte_size(); ++i) {
        if (data_[i] == '\n') {
            result.emplace_back(data_.begin() + start, i - start);
            start = i + 1;
        } else if (data_[i] == '\r') {
            std::size_t len = i - start;
            if (i + 1 < byte_size() && data_[i + 1] == '\n') {
                result.emplace_back(data_.begin() + start, len);
                start = i + 2;
                ++i; // skip \n
            } else {
                result.emplace_back(data_.begin() + start, len);
                start = i + 1;
            }
        }
    }

    if (start < byte_size()) {
        result.emplace_back(data_.begin() + start, byte_size() - start);
    }
    return result;
}

KAStr KAStr::strip_prefix(KAStr prefix) const {
    if (starts_with(prefix)) return KAStr(data_.begin() + prefix.byte_size(), byte_size() - prefix.byte_size());
    return *this;
}

KAStr KAStr::strip_suffix(KAStr suffix) const {
    if (ends_with(suffix)) return KAStr(data_.begin(), byte_size() - suffix.byte_size());
    return *this;
}

KAStr KAStr::trim_start() const {
    std::size_t start = 0;
    while (start < byte_size() && isspace(data_[start])) ++start;
    return KAStr(data_.begin() + start, byte_size() - start);
}

KAStr KAStr::trim_end() const {
    std::size_t end = byte_size();
    while (end > 0 && isspace(data_[end - 1])) --end;
    return KAStr(data_.begin(), end);
}

KAStr KAStr::trim() const {
    return trim_start().trim_end();
}


}; // namespace KAString

