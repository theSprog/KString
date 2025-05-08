#pragma once
#include <stdexcept>
#include <string>
#include "base.hpp"

namespace kstring {
class SSOBytes {
  public:
    enum : std::size_t {
        HeapViewSize = sizeof(std::vector<Byte>) + sizeof(uint8_t),
        SSO_CAPACITY = HeapViewSize - 1
    };

  private:
    enum : uint8_t {
        kHeapFlag = 0x80
    };

    union {
        struct {
            Byte data[SSO_CAPACITY];
            uint8_t len;
        } sso;

        struct {
            std::vector<Byte> vec;
            uint8_t tag;
        } heap;
    };

    void promote_to_heap() {
        if (! is_sso()) return;
        /*
        不能原地构造, 必须通过构造 tmp 再 move ！！！
        标准库里 vector 的实现，往往会先在这块内存里写入它自己的 pointer / size / capacity
        再去把 [first, last) 那段内存拷贝到 heap 上
        由于拷贝源就是刚刚被自己当作“对象存放区”写过元数据的 sso_data
        所以前面那几个字节已经不再是 'a','b','c'…，因此复制到堆上的数据就发生了破坏，看上去就像乱码。
        */
        std::vector<Byte> tmp(sso.data, sso.data + sso.len);
        new (&heap.vec) std::vector<Byte>(std::move(tmp));
        heap.tag = kHeapFlag;
    }


  public:
    bool is_sso() const;

    ~SSOBytes();

    // 默认构造
    SSOBytes();

    explicit SSOBytes(Byte ch);

    explicit SSOBytes(const char* pattern);

    explicit SSOBytes(const std::string& pattern);

    // 构造 n 个相同字节
    explicit SSOBytes(Byte ch, std::size_t repeat);

    // 构造 n 次 pattern 重复（pattern 为 C 字符串）
    explicit SSOBytes(const char* pattern, std::size_t repeat);

    SSOBytes(const SSOBytes& other);

    SSOBytes& operator=(const SSOBytes& other);

    SSOBytes(SSOBytes&& other) noexcept;

    SSOBytes& operator=(SSOBytes&& other) noexcept;

    bool operator==(const SSOBytes& other) const;

    bool operator!=(const SSOBytes& other) const;

    Byte& operator[](std::size_t idx);

    const Byte& operator[](std::size_t idx) const;

    Byte& at(std::size_t idx);

    std::size_t size() const;

    std::size_t capacity() const;

    bool empty() const;

    Byte* data();

    const Byte* data() const;

    Byte& front();

    const Byte& front() const;

    Byte& back();

    const Byte& back() const;

    void clear();

    void push_back(Byte byte);

    void pop_back();
    void append(const Byte* src, std::size_t len);

    void append(const char* cstr);

    void append(std::initializer_list<Byte> list);

    void append(const std::string& str);

    void insert(std::size_t pos, Byte byte);

    void resize(std::size_t n, Byte val = 0);

    void reserve(std::size_t n);

    void erase(std::size_t pos);

    void shrink_to_fit();

    void swap(SSOBytes& other) noexcept;

    friend void swap(SSOBytes& lhs, SSOBytes& rhs) noexcept;

    using iterator = Byte*;
    using const_iterator = const Byte*;

    iterator begin();

    iterator end();

    const_iterator begin() const;

    const_iterator end() const;

    const_iterator cbegin() const;

    const_iterator cend() const;

    template <typename It>
    void insert(std::size_t pos, It first, It last) {
        std::size_t count = static_cast<std::size_t>(std::distance(first, last));
        if (! (pos <= size())) {
            throw std::out_of_range("SSOBytes::insert<It>()");
        }

        if (is_sso() && sso.len + count <= SSO_CAPACITY) {
            for (std::size_t i = sso.len; i > pos; --i) sso.data[i + count - 1] = sso.data[i - 1];
            for (std::size_t i = 0; i < count; ++i) sso.data[pos + i] = static_cast<Byte>(*first++);
            sso.len += static_cast<Byte>(count);
        } else {
            promote_to_heap();
            heap.vec.insert(heap.vec.begin() + pos, first, last);
        }
    }

    template <typename It>
    void assign(It begin, It end) {
        std::size_t n = static_cast<std::size_t>(std::distance(begin, end));
        if (is_sso() && n <= SSO_CAPACITY) {
            for (std::size_t i = 0; i < n; ++i) sso.data[i] = static_cast<Byte>(*(begin++));
            sso.len = static_cast<Byte>(n);
        } else {            // 不是 SSO, 或者 n 过大, 总之都要进入堆模式
            if (is_sso()) { //
                promote_to_heap();
            }
            heap.vec.assign(begin, end);
        }
    }

    void assign(std::initializer_list<Byte> list) {
        assign(list.begin(), list.end());
    }
};
} // namespace kstring
