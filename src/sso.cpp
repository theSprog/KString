#include <cstring>
#include <array>
#include "sso.hpp"

namespace kstring {
bool SSOBytes::is_sso() const {
    const uint8_t tag_value = heap.tag; // 强制访问为 heap 联合成员
    return (tag_value & kHeapFlag) == 0;
}

SSOBytes::~SSOBytes() {
    if (! is_sso()) {
        heap.vec.~vector();
    }
}

// 默认构造
SSOBytes::SSOBytes() {
    sso.len = 0;
    heap.tag = 0;
}

SSOBytes::SSOBytes(Byte ch) : SSOBytes(ch, 1) {}

SSOBytes::SSOBytes(const char* pattern) : SSOBytes(pattern, 1) {}

SSOBytes::SSOBytes(const std::string& pattern) : SSOBytes(pattern.c_str(), 1) {}

// 构造 n 个相同字节
SSOBytes::SSOBytes(Byte ch, std::size_t repeat) {
    if (repeat <= SSO_CAPACITY) {
        sso.len = static_cast<uint8_t>(repeat);
        std::fill(sso.data, sso.data + repeat, ch);
    } else {
        new (&heap.vec) std::vector<Byte>(repeat, ch);
        heap.tag = kHeapFlag;
    }
}

// 构造 n 次 pattern 重复（pattern 为 C 字符串）
SSOBytes::SSOBytes(const char* pattern, std::size_t repeat) {
    if (! pattern || *pattern == '\0' || repeat == 0) {
        sso.len = 0;
        return;
    }

    std::size_t pat_len = std::strlen(pattern);
    std::size_t total = pat_len * repeat;

    if (total <= SSO_CAPACITY) {
        for (std::size_t i = 0; i < repeat; ++i) std::memcpy(sso.data + i * pat_len, pattern, pat_len);
        sso.len = static_cast<uint8_t>(total);
    } else {
        new (&heap.vec) std::vector<Byte>();
        heap.vec.reserve(total);
        for (std::size_t i = 0; i < repeat; ++i) heap.vec.insert(heap.vec.end(), pattern, pattern + pat_len);
        heap.tag = kHeapFlag;
    }
}

SSOBytes::SSOBytes(const SSOBytes& other) {
    if (other.is_sso()) {
        std::memcpy(sso.data, other.sso.data, other.sso.len);
        sso.len = other.sso.len;
    } else {
        new (&heap.vec) std::vector<Byte>(other.heap.vec);
        heap.tag = kHeapFlag;
    }
}

SSOBytes& SSOBytes::operator=(const SSOBytes& other) {
    if (this == &other) return *this;
    this->~SSOBytes();
    new (this) SSOBytes(other);
    return *this;
}

SSOBytes::SSOBytes(SSOBytes&& other) noexcept {
    if (other.is_sso()) {
        std::memcpy(sso.data, other.sso.data, other.sso.len);
        sso.len = other.sso.len;
    } else {
        new (&heap.vec) std::vector<Byte>(std::move(other.heap.vec));
        heap.tag = kHeapFlag;
    }
}

SSOBytes& SSOBytes::operator=(SSOBytes&& other) noexcept {
    if (this == &other) return *this;
    this->~SSOBytes();
    new (this) SSOBytes(std::move(other));
    return *this;
}

bool SSOBytes::operator==(const SSOBytes& other) const {
    if (size() != other.size()) return false;
    const Byte* lhs = data();
    const Byte* rhs = other.data();
    return std::equal(lhs, lhs + size(), rhs);
}

bool SSOBytes::operator!=(const SSOBytes& other) const {
    return ! (*this == other);
}

Byte& SSOBytes::operator[](std::size_t idx) {
    return is_sso() ? reinterpret_cast<Byte&>(sso.data[idx]) : heap.vec[idx];
}

const Byte& SSOBytes::operator[](std::size_t idx) const {
    return is_sso() ? reinterpret_cast<const Byte&>(sso.data[idx]) : heap.vec[idx];
}

Byte& SSOBytes::at(std::size_t idx) {
    if (idx >= size()) throw std::out_of_range("SSOBytes::at()");
    return (*this)[idx];
}

std::size_t SSOBytes::size() const {
    return is_sso() ? sso.len : heap.vec.size();
}

std::size_t SSOBytes::capacity() const {
    return is_sso() ? SSO_CAPACITY : heap.vec.capacity();
}

bool SSOBytes::empty() const {
    return size() == 0;
}

Byte* SSOBytes::data() {
    return is_sso() ? reinterpret_cast<Byte*>(sso.data) : heap.vec.data();
}

const Byte* SSOBytes::data() const {
    return is_sso() ? reinterpret_cast<const Byte*>(sso.data) : heap.vec.data();
}

Byte& SSOBytes::front() {
    return is_sso() ? reinterpret_cast<Byte&>(sso.data[0]) : heap.vec.front();
}

const Byte& SSOBytes::front() const {
    return is_sso() ? reinterpret_cast<const Byte&>(sso.data[0]) : heap.vec.front();
}

Byte& SSOBytes::back() {
    return is_sso() ? reinterpret_cast<Byte&>(sso.data[sso.len - 1]) : heap.vec.back();
}

const Byte& SSOBytes::back() const {
    return is_sso() ? reinterpret_cast<const Byte&>(sso.data[sso.len - 1]) : heap.vec.back();
}

void SSOBytes::clear() {
    if (is_sso()) {
        sso.len = 0;
    } else {
        heap.vec.clear();
    }
}

void SSOBytes::push_back(Byte byte) {
    if (is_sso()) {
        if (sso.len < SSO_CAPACITY) {
            sso.data[sso.len++] = byte;
        } else {
            promote_to_heap();
            heap.vec.push_back(byte);
        }
    } else {
        heap.vec.push_back(byte);
    }
}

void SSOBytes::pop_back() {
    if (empty()) {
        throw std::runtime_error("SSOBytes::pop_back(): pop on empty SSO");
    }
    if (is_sso()) {
        --sso.len;
    } else {
        heap.vec.pop_back();
    }
}

void SSOBytes::append(const Byte* src, std::size_t len) {
    if (len == 0) return;

    if (is_sso() && sso.len + len <= SSO_CAPACITY) {
        std::memcpy(sso.data + sso.len, src, len);
        sso.len += static_cast<uint8_t>(len);
    } else {
        promote_to_heap();
        heap.vec.insert(heap.vec.end(), src, src + len);
    }
}

void SSOBytes::append(const char* cstr) {
    append(reinterpret_cast<const Byte*>(cstr), std::strlen(cstr));
}

void SSOBytes::append(std::initializer_list<Byte> list) {
    append(list.begin(), list.size());
}

void SSOBytes::append(const std::string& str) {
    append(reinterpret_cast<const Byte*>(str.data()), str.size());
}

void SSOBytes::insert(std::size_t pos, Byte byte) {
    if (! (pos <= size())) {
        throw std::out_of_range("SSOBytes::insert()");
    }
    if (is_sso() && sso.len < SSO_CAPACITY) {
        for (std::size_t i = sso.len; i > pos; --i) sso.data[i] = sso.data[i - 1];
        sso.data[pos] = byte;
        ++sso.len;
    } else {
        promote_to_heap();
        heap.vec.insert(std::next(heap.vec.begin(), static_cast<std::ptrdiff_t>(pos)), byte);
    }
}

void SSOBytes::resize(std::size_t n, Byte val) {
    if (! is_sso()) {
        heap.vec.resize(n, val);
        return;
    }

    // is sso
    if (n <= SSO_CAPACITY) { // 如果 resize 后仍然在栈上
        if (n > sso.len) {   // 扩展
            std::fill(sso.data + sso.len, sso.data + n, val);
        }
        sso.len = static_cast<Byte>(n);
    } else { // 扩展到堆上, 进入 heap 模式
        promote_to_heap();
        heap.vec.resize(n, val); // 使用 vector 的 resize
    }
}

void SSOBytes::reserve(std::size_t n) {
    if (! is_sso()) {
        heap.vec.reserve(n);
    } else if (n > SSO_CAPACITY) {
        promote_to_heap();
        heap.vec.reserve(n);
    }
}

void SSOBytes::erase(std::size_t pos) {
    if (is_sso()) {
        if (! (pos < sso.len)) {
            throw std::out_of_range("SSOBytes::erase()");
        }
        for (std::size_t i = pos; i + 1 < sso.len; ++i) sso.data[i] = sso.data[i + 1];
        --sso.len;
    } else {
        heap.vec.erase(std::next(heap.vec.begin(), static_cast<std::ptrdiff_t>(pos)));
    }
}

void SSOBytes::shrink_to_fit() {
    if (! is_sso()) {
        heap.vec.shrink_to_fit();
    }
}

void SSOBytes::swap(SSOBytes& other) noexcept {
    if (this == &other) return;

    if (is_sso() && other.is_sso()) { // 两者都是 SSO
        std::array<Byte, SSO_CAPACITY> tmp;
        std::uint8_t tmp_len = sso.len;

        std::memcpy(tmp.data(), sso.data, sso.len);
        std::memcpy(sso.data, other.sso.data, other.sso.len);
        std::memcpy(other.sso.data, tmp.data(), tmp_len);

        std::swap(sso.len, other.sso.len);
    } else if (! is_sso() && ! other.is_sso()) { // 两者都不是 SSO
        heap.vec.swap(other.heap.vec);
    } else {
        SSOBytes tmp = std::move(*this);
        *this = std::move(other);
        other = std::move(tmp);
    }
}

void swap(SSOBytes& lhs, SSOBytes& rhs) noexcept {
    lhs.swap(rhs);
}

SSOBytes::iterator SSOBytes::begin() {
    return data();
}

SSOBytes::iterator SSOBytes::end() {
    return data() + size();
}

SSOBytes::const_iterator SSOBytes::begin() const {
    return data();
}

SSOBytes::const_iterator SSOBytes::end() const {
    return data() + size();
}

SSOBytes::const_iterator SSOBytes::cbegin() const {
    return begin();
}

SSOBytes::const_iterator SSOBytes::cend() const {
    return end();
}
}; // namespace kstring
