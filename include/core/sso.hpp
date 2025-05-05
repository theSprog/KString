#pragma once
#include <cstdint>
#include <vector>
#include <cstring>
#include <cassert>
#include "../internal/utf8.hpp"

namespace KString {
using internal::utf8::Byte;

class SSOBytes {
    static constexpr std::size_t HeapPtrSize = sizeof(std::vector<Byte>);
    static constexpr std::size_t SSO_CAPACITY = HeapPtrSize - 1; // 末尾留 1 字节存长度

    union {
        struct {
            Byte sso_data[SSO_CAPACITY];
            uint8_t sso_len;
        };

        std::vector<Byte> heap_vec;
    };

    bool is_sso_;

    void promote_to_heap() {
        if (! is_sso_) return;
        /*
        不能原地构造, 必须通过构造 tmp 再 move ！！！
        标准库里 vector 的实现，往往会先在这块内存里写入它自己的 pointer / size / capacity
        再去把 [first, last) 那段内存拷贝到 heap 上
        由于拷贝源就是刚刚被自己当作“对象存放区”写过元数据的 sso_data
        所以前面那几个字节已经不再是 'a','b','c'…，因此复制到堆上的数据就发生了破坏，看上去就像乱码。
        */
        std::vector<Byte> tmp(sso_data, sso_data + sso_len);
        new (&heap_vec) std::vector<Byte>(std::move(tmp));
        is_sso_ = false;
    }

  public:
    SSOBytes() : heap_vec(), is_sso_(true) {
        sso_len = 0;
    }

    SSOBytes(const SSOBytes& other) : is_sso_(other.is_sso_) {
        if (is_sso_) {
            std::memcpy(sso_data, other.sso_data, other.sso_len);
            sso_len = other.sso_len;
        } else {
            new (&heap_vec) std::vector<Byte>(other.heap_vec);
        }
    }

    SSOBytes& operator=(const SSOBytes& other) {
        if (this == &other) return *this;
        this->~SSOBytes();
        new (this) SSOBytes(other);
        return *this;
    }

    SSOBytes(SSOBytes&& other) noexcept : is_sso_(other.is_sso_) {
        if (is_sso_) {
            std::memcpy(sso_data, other.sso_data, other.sso_len);
            sso_len = other.sso_len;
        } else {
            new (&heap_vec) std::vector<Byte>(std::move(other.heap_vec));
        }
    }

    SSOBytes& operator=(SSOBytes&& other) noexcept {
        if (this == &other) return *this;
        this->~SSOBytes();
        new (this) SSOBytes(std::move(other));
        return *this;
    }

    ~SSOBytes() {
        if (! is_sso_) {
            heap_vec.~vector();
        }
    }

    Byte& operator[](std::size_t idx) {
        return is_sso_ ? reinterpret_cast<Byte&>(sso_data[idx]) : heap_vec[idx];
    }

    const Byte& operator[](std::size_t idx) const {
        return is_sso_ ? reinterpret_cast<const Byte&>(sso_data[idx]) : heap_vec[idx];
    }

    bool is_sso() const {
        return is_sso_;
    }

    std::size_t size() const {
        return is_sso_ ? sso_len : heap_vec.size();
    }

    std::size_t capacity() const {
        return is_sso_ ? SSO_CAPACITY : heap_vec.capacity();
    }

    bool empty() const {
        return size() == 0;
    }

    Byte* data() {
        return is_sso_ ? reinterpret_cast<Byte*>(sso_data) : heap_vec.data();
    }

    const Byte* data() const {
        return is_sso_ ? reinterpret_cast<const Byte*>(sso_data) : heap_vec.data();
    }

    Byte& front() {
        return is_sso_ ? reinterpret_cast<Byte&>(sso_data[0]) : heap_vec.front();
    }

    const Byte& front() const {
        return is_sso_ ? reinterpret_cast<const Byte&>(sso_data[0]) : heap_vec.front();
    }

    Byte& back() {
        return is_sso_ ? reinterpret_cast<Byte&>(sso_data[sso_len - 1]) : heap_vec.back();
    }

    const Byte& back() const {
        return is_sso_ ? reinterpret_cast<const Byte&>(sso_data[sso_len - 1]) : heap_vec.back();
    }

    void clear() {
        if (is_sso_) {
            sso_len = 0;
        } else {
            heap_vec.clear();
        }
    }

    void push_back(Byte byte) {
        if (is_sso_) {
            if (sso_len < SSO_CAPACITY) {
                sso_data[sso_len++] = byte;
            } else {
                promote_to_heap();
                heap_vec.push_back(byte);
            }
        } else {
            heap_vec.push_back(byte);
        }
    }

    void append(const Byte* src, std::size_t len) {
        if (is_sso_ && sso_len + len <= SSO_CAPACITY) {
            std::memcpy(sso_data + sso_len, src, len);
            sso_len += static_cast<uint8_t>(len);
        } else {
            promote_to_heap();
            heap_vec.insert(heap_vec.end(), src, src + len);
        }
    }

    void insert(std::size_t pos, Byte byte) {
        assert(pos <= size());
        if (is_sso_ && sso_len < SSO_CAPACITY) {
            for (std::size_t i = sso_len; i > pos; --i) sso_data[i] = sso_data[i - 1];
            sso_data[pos] = byte;
            ++sso_len;
        } else {
            promote_to_heap();
            heap_vec.insert(heap_vec.begin() + pos, byte);
        }
    }

    template <typename It>
    void insert(std::size_t pos, It first, It last) {
        std::size_t count = static_cast<std::size_t>(std::distance(first, last));
        assert(pos <= size());

        if (is_sso_ && sso_len + count <= SSO_CAPACITY) {
            for (std::size_t i = sso_len; i > pos; --i) sso_data[i + count - 1] = sso_data[i - 1];
            for (std::size_t i = 0; i < count; ++i) sso_data[pos + i] = static_cast<Byte>(*first++);
            sso_len += static_cast<Byte>(count);
        } else {
            promote_to_heap();
            heap_vec.insert(heap_vec.begin() + pos, first, last);
        }
    }

    void resize(std::size_t n, Byte val = 0) {
        if (! is_sso_) {
            heap_vec.resize(n, val);
            return;
        }

        // is sso
        if (n <= SSO_CAPACITY) { // 如果 resize 后仍然在栈上
            if (n > sso_len) {   // 扩展
                for (std::size_t i = sso_len; i < n; ++i) sso_data[i] = val;
            }
            sso_len = static_cast<Byte>(n);
        } else { // 扩展到堆上, 进入 heap 模式
            promote_to_heap();
            heap_vec.resize(n, val); // 使用 vector 的 resize
        }
    }

    void reserve(std::size_t n) {
        if (! is_sso_) {
            heap_vec.reserve(n);
        } else if (n > SSO_CAPACITY) {
            promote_to_heap();
            heap_vec.reserve(n);
        }
    }

    void erase(std::size_t pos) {
        if (is_sso_) {
            assert(pos < sso_len);
            for (std::size_t i = pos; i + 1 < sso_len; ++i) sso_data[i] = sso_data[i + 1];
            --sso_len;
        } else {
            heap_vec.erase(heap_vec.begin() + pos);
        }
    }

    template <typename It>
    void assign(It begin, It end) {
        std::size_t n = static_cast<std::size_t>(std::distance(begin, end));
        if (is_sso_ && n <= SSO_CAPACITY) {
            for (std::size_t i = 0; i < n; ++i) sso_data[i] = static_cast<Byte>(*(begin++));
            sso_len = static_cast<Byte>(n);
        } else {           // 不是 SSO, 或者 n 过大, 总之都要进入堆模式
            if (is_sso_) { //
                promote_to_heap();
            }
            heap_vec.assign(begin, end);
        }
    }

    bool operator==(const SSOBytes& other) const {
        if (size() != other.size()) return false;
        const Byte* lhs = data();
        const Byte* rhs = other.data();
        return std::equal(lhs, lhs + size(), rhs);
    }

    bool operator!=(const SSOBytes& other) const {
        return ! (*this == other);
    }

    void shrink_to_fit() {
        if (! is_sso_) {
            heap_vec.shrink_to_fit();
        }
    }

    void swap(SSOBytes& other) {
        if (this == &other) return;

        if (is_sso_ && other.is_sso_) { // 两者都是 SSO
            Byte tmp[SSO_CAPACITY];
            std::uint8_t tmp_len = sso_len;

            std::memcpy(tmp, sso_data, sso_len);
            std::memcpy(sso_data, other.sso_data, other.sso_len);
            std::memcpy(other.sso_data, tmp, tmp_len);

            std::swap(sso_len, other.sso_len);
        } else if (! is_sso_ && ! other.is_sso_) { // 两者都不是 SSO
            heap_vec.swap(other.heap_vec);
        } else {
            SSOBytes tmp = std::move(*this);
            *this = std::move(other);
            other = std::move(tmp);
        }
    }

    using iterator = Byte*;
    using const_iterator = const Byte*;

    iterator begin() {
        return data();
    }

    iterator end() {
        return data() + size();
    }

    const_iterator begin() const {
        return data();
    }

    const_iterator end() const {
        return data() + size();
    }

    const_iterator cbegin() const {
        return begin();
    }

    const_iterator cend() const {
        return end();
    }
};
} // namespace KString
