#include <cstring>
#include <array>
#include "sso.hpp"

namespace kstring {
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

// void resize(std::size_t n, Byte val = 0);
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
}; // namespace kstring
