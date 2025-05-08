#pragma once
#include "base.hpp"
#include "kchar.hpp"
#include "utf8.hpp"

namespace kstring {
struct CharIterator {
    ByteSpan data_;
    std::size_t cur_pos; // 当前字符的起始字节位置
    std::size_t end_pos; // 字符串末尾字节位置
    mutable KChar current;
    mutable bool decoded;

    CharIterator(ByteSpan data, std::size_t start_pos)
        : data_(data), cur_pos(start_pos), end_pos(data.size()), current(), decoded(false) {}

    KChar operator*() const {
        if (cur_pos >= end_pos) return KChar(); // 返回空字符
        if (! decoded) {
            auto dec = utf8::decode(data_, cur_pos);
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
            auto dec = utf8::decode(data_, cur_pos);
            cur_pos = dec.next_pos;
        }

        decoded = false;
        return *this;
    }

    bool operator==(const CharIterator& other) const {
        return cur_pos == other.cur_pos && data_.data() == other.data_.data();
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

struct ReverseCharIterator {
    ByteSpan data_;
    std::size_t cur_pos; // 当前字符的末尾字节位置（注意：是末尾！）
    mutable KChar current;
    mutable bool decoded;

    ReverseCharIterator(ByteSpan data, std::size_t end_pos) : data_(data), cur_pos(end_pos), current(), decoded(false) {}

    KChar operator*() const {
        if (cur_pos == 0) return KChar(); // 到达字符串开头，返回空字符
        if (! decoded) {
            // 需要从当前位置向前寻找一个有效的UTF-8字符起始位置
            auto dec = utf8::decode_prev(data_, cur_pos);
            current = dec.ok ? KChar(dec.cp) : KChar();
            decoded = true;
        }
        return current;
    }

    ReverseCharIterator& operator++() {
        if (decoded) { // 已经解码则直接复用
            std::size_t size = current.utf8_size();
            cur_pos -= (size == 0) ? 1 : size;
        } else { // 尚未解码则解码获取 next_pos
            auto dec = utf8::decode_prev(data_, cur_pos);
            cur_pos = dec.next_pos;
        }

        decoded = false;
        return *this;
    }

    bool operator==(const ReverseCharIterator& other) const {
        return cur_pos == other.cur_pos && data_.data() == other.data_.data();
    }

    bool operator!=(const ReverseCharIterator& other) const {
        return ! (*this == other);
    }

    using iterator_category = std::input_iterator_tag;
    using value_type = KChar;
    using difference_type = std::ptrdiff_t;
    using pointer = void;
    using reference = KChar;
};

struct ReverseCharRange {
    ByteSpan data;

    explicit ReverseCharRange(ByteSpan d) : data(d) {}

    ReverseCharIterator begin() const {
        return ReverseCharIterator(data, data.size());
    }

    ReverseCharIterator end() const {
        return ReverseCharIterator(data, 0);
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
}; // namespace kstring
