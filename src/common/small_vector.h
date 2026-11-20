// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// Arrays with their first N elements stored in place, in the shape of a C
// struct: a buffer, a count and, for SmallVector, a pointer to where the
// elements live.
//
// StaticVector<T, N> never holds more than N. SmallVector<T, N> keeps up to N
// in place and moves to the heap, doubling, past that. Both keep elements
// contiguous, so iterators are plain pointers. They replace Boost.Container's
// static_vector and small_vector.

#include <stddef.h>
#include <string.h>

// Language support only: brace lists, and placement and aligned new
#include <initializer_list>
#include <new>

#include "common/assert.h"

namespace Common {

namespace Detail {

template <typename T>
constexpr T&& Move(T& value) noexcept {
    return static_cast<T&&>(value);
}

template <typename T>
void DestroyRange(T* first, T* last) noexcept {
    for (; first != last; ++first) {
        first->~T();
    }
}

/// Moves [@first, @last) into raw storage at @to
template <typename T>
void UninitializedMove(T* first, T* last, T* to) {
    for (; first != last; ++first, ++to) {
        ::new (static_cast<void*>(to)) T(Move(*first));
    }
}

template <typename T>
void ReverseRange(T* first, T* last) {
    while (first < last) {
        --last;
        T tmp(Move(*first));
        *first = Move(*last);
        *last = Move(tmp);
        ++first;
    }
}

/// [@first, @last) turned so that @mid comes first
template <typename T>
void Rotate(T* first, T* mid, T* last) {
    ReverseRange(first, mid);
    ReverseRange(mid, last);
    ReverseRange(first, last);
}

/// A pointer walked backwards
template <typename T>
struct ReverseIterator {
    T* p;

    T& operator*() const noexcept {
        return *(p - 1);
    }
    T* operator->() const noexcept {
        return p - 1;
    }
    ReverseIterator& operator++() noexcept {
        --p;
        return *this;
    }
    ReverseIterator operator++(int) noexcept {
        ReverseIterator old = *this;
        --p;
        return old;
    }
    friend bool operator==(ReverseIterator a, ReverseIterator b) noexcept {
        return a.p == b.p;
    }
    friend bool operator!=(ReverseIterator a, ReverseIterator b) noexcept {
        return a.p != b.p;
    }
};

/// What can be walked from one to another: a pointer, an iterator
template <typename It>
concept InputIterator = requires(It it) {
    *it;
    ++it;
};

// The element-handling half both share; Storage gives data(), capacity()
// and Grow(), and holds count.
template <typename Derived, typename T>
class InlineVectorOps {
public:
    using value_type = T;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;
    using iterator = T*;
    using const_iterator = const T*;
    using reverse_iterator = ReverseIterator<T>;
    using const_reverse_iterator = ReverseIterator<const T>;

    T* data() noexcept {
        return Self().Data();
    }
    const T* data() const noexcept {
        return Self().Data();
    }
    size_t size() const noexcept {
        return Self().count;
    }
    size_t capacity() const noexcept {
        return Self().Capacity();
    }
    bool empty() const noexcept {
        return Self().count == 0;
    }

    iterator begin() noexcept {
        return data();
    }
    iterator end() noexcept {
        return data() + size();
    }
    const_iterator begin() const noexcept {
        return data();
    }
    const_iterator end() const noexcept {
        return data() + size();
    }
    const_iterator cbegin() const noexcept {
        return begin();
    }
    const_iterator cend() const noexcept {
        return end();
    }
    reverse_iterator rbegin() noexcept {
        return reverse_iterator{end()};
    }
    reverse_iterator rend() noexcept {
        return reverse_iterator{begin()};
    }
    const_reverse_iterator rbegin() const noexcept {
        return const_reverse_iterator{end()};
    }
    const_reverse_iterator rend() const noexcept {
        return const_reverse_iterator{begin()};
    }

    T& operator[](size_t i) noexcept {
        return data()[i];
    }
    const T& operator[](size_t i) const noexcept {
        return data()[i];
    }
    T& at(size_t i) {
        ASSERT(i < size());
        return data()[i];
    }
    const T& at(size_t i) const {
        ASSERT(i < size());
        return data()[i];
    }
    T& front() noexcept {
        return data()[0];
    }
    const T& front() const noexcept {
        return data()[0];
    }
    T& back() noexcept {
        return data()[size() - 1];
    }
    const T& back() const noexcept {
        return data()[size() - 1];
    }

    void reserve(size_t n) {
        if (n > capacity()) {
            Self().Grow(n);
        }
    }

    template <typename... Args>
    T& emplace_back(Args&&... args) {
        if (size() == capacity()) {
            // The arguments may refer to an element that is about to move
            T tmp(static_cast<Args&&>(args)...);
            Self().Grow(size() + 1);
            T* p = ::new (static_cast<void*>(data() + size())) T(Move(tmp));
            ++Self().count;
            return *p;
        }
        T* p = ::new (static_cast<void*>(data() + size())) T(static_cast<Args&&>(args)...);
        ++Self().count;
        return *p;
    }
    void push_back(const T& v) {
        emplace_back(v);
    }
    void push_back(T&& v) {
        emplace_back(Move(v));
    }
    void pop_back() noexcept {
        --Self().count;
        (data() + size())->~T();
    }

    void clear() noexcept {
        DestroyRange(begin(), end());
        Self().count = 0;
    }

    void resize(size_t n) {
        Resize(n, [](T* p) { ::new (static_cast<void*>(p)) T(); });
    }
    void resize(size_t n, const T& v) {
        Resize(n, [&v](T* p) { ::new (static_cast<void*>(p)) T(v); });
    }

    template <typename It>
    void assign(It first, It last) {
        clear();
        for (; first != last; ++first) {
            emplace_back(*first);
        }
    }
    void assign(size_t n, const T& v) {
        clear();
        resize(n, v);
    }
    void assign(std::initializer_list<T> il) {
        assign(il.begin(), il.end());
    }

    template <typename... Args>
    iterator emplace(const_iterator pos, Args&&... args) {
        const size_t at = static_cast<size_t>(pos - begin());
        emplace_back(static_cast<Args&&>(args)...);
        Rotate(begin() + at, end() - 1, end());
        return begin() + at;
    }
    iterator insert(const_iterator pos, const T& v) {
        return emplace(pos, v);
    }
    iterator insert(const_iterator pos, T&& v) {
        return emplace(pos, Move(v));
    }
    template <InputIterator It>
    iterator insert(const_iterator pos, It first, It last) {
        const size_t at = static_cast<size_t>(pos - begin());
        const size_t old = size();
        for (; first != last; ++first) {
            emplace_back(*first);
        }
        Rotate(begin() + at, begin() + old, end());
        return begin() + at;
    }
    iterator insert(const_iterator pos, std::initializer_list<T> il) {
        return insert(pos, il.begin(), il.end());
    }

    iterator erase(const_iterator pos) {
        return erase(pos, pos + 1);
    }
    iterator erase(const_iterator first, const_iterator last) {
        T* f = begin() + (first - begin());
        T* l = begin() + (last - begin());
        if (f == l) {
            return f;
        }
        T* new_end = f;
        for (T* from = l; from != end(); ++from, ++new_end) {
            *new_end = Move(*from);
        }
        DestroyRange(new_end, end());
        Self().count = static_cast<decltype(Self().count)>(new_end - begin());
        return f;
    }

    friend bool operator==(const Derived& a, const Derived& b) {
        if (a.size() != b.size()) {
            return false;
        }
        for (size_t i = 0; i < a.size(); ++i) {
            if (!(a[i] == b[i])) {
                return false;
            }
        }
        return true;
    }

protected:
    Derived& Self() noexcept {
        return static_cast<Derived&>(*this);
    }
    const Derived& Self() const noexcept {
        return static_cast<const Derived&>(*this);
    }

    template <typename Make>
    void Resize(size_t n, Make make) {
        if (n < size()) {
            DestroyRange(begin() + n, end());
            Self().count = static_cast<decltype(Self().count)>(n);
            return;
        }
        reserve(n);
        while (size() < n) {
            make(data() + size());
            ++Self().count;
        }
    }

    // Moves @n elements from @from to raw storage at @to and ends them at @from
    static void Relocate(T* from, size_t n, T* to) {
        if constexpr (__is_trivially_copyable(T)) {
            if (n) {
                memcpy(static_cast<void*>(to), static_cast<const void*>(from), n * sizeof(T));
            }
        } else {
            UninitializedMove(from, from + n, to);
            DestroyRange(from, from + n);
        }
    }
};

} // namespace Detail

/// At most N elements, all in place.
template <typename T, size_t N>
class StaticVector : public Detail::InlineVectorOps<StaticVector<T, N>, T> {
    using Ops = Detail::InlineVectorOps<StaticVector<T, N>, T>;
    friend Ops;

public:
    StaticVector() noexcept = default;
    explicit StaticVector(size_t n) {
        this->resize(n);
    }
    StaticVector(size_t n, const T& v) {
        this->resize(n, v);
    }
    StaticVector(std::initializer_list<T> il) {
        this->assign(il.begin(), il.end());
    }
    template <Detail::InputIterator It>
    StaticVector(It first, It last) {
        this->assign(first, last);
    }
    StaticVector(const StaticVector& o) {
        this->assign(o.begin(), o.end());
    }
    StaticVector(StaticVector&& o) noexcept(__is_nothrow_constructible(T, T&&)) {
        Detail::UninitializedMove(o.begin(), o.end(), Data());
        count = o.count;
        o.clear();
    }
    StaticVector& operator=(const StaticVector& o) {
        if (this != &o) {
            this->assign(o.begin(), o.end());
        }
        return *this;
    }
    StaticVector& operator=(StaticVector&& o) noexcept(__is_nothrow_constructible(T, T&&)) {
        if (this != &o) {
            this->clear();
            Detail::UninitializedMove(o.begin(), o.end(), Data());
            count = o.count;
            o.clear();
        }
        return *this;
    }
    StaticVector& operator=(std::initializer_list<T> il) {
        this->assign(il.begin(), il.end());
        return *this;
    }
    ~StaticVector() {
        this->clear();
    }

    static constexpr size_t max_size() noexcept {
        return N;
    }

private:
    T* Data() noexcept {
        return reinterpret_cast<T*>(storage);
    }
    const T* Data() const noexcept {
        return reinterpret_cast<const T*>(storage);
    }
    static constexpr size_t Capacity() noexcept {
        return N;
    }
    void Grow(size_t) {
        ASSERT_MSG(false, "StaticVector of %zu is full", N);
    }

    alignas(T) unsigned char storage[N * sizeof(T)];
    size_t count = 0;
};

/// Up to N elements in place, then on the heap. SizeT is the type the count
/// and capacity are kept in, for structs that want to stay small.
template <typename T, size_t N, typename SizeT = size_t>
class SmallVector : public Detail::InlineVectorOps<SmallVector<T, N, SizeT>, T> {
    using Ops = Detail::InlineVectorOps<SmallVector<T, N, SizeT>, T>;
    friend Ops;

public:
    SmallVector() noexcept = default;
    explicit SmallVector(size_t n) {
        this->resize(n);
    }
    SmallVector(size_t n, const T& v) {
        this->resize(n, v);
    }
    SmallVector(std::initializer_list<T> il) {
        this->assign(il.begin(), il.end());
    }
    template <Detail::InputIterator It>
    SmallVector(It first, It last) {
        this->assign(first, last);
    }
    SmallVector(const SmallVector& o) {
        this->assign(o.begin(), o.end());
    }
    SmallVector(SmallVector&& o) noexcept(__is_nothrow_constructible(T, T&&)) {
        TakeFrom(o);
    }
    SmallVector& operator=(const SmallVector& o) {
        if (this != &o) {
            this->assign(o.begin(), o.end());
        }
        return *this;
    }
    SmallVector& operator=(SmallVector&& o) noexcept(__is_nothrow_constructible(T, T&&)) {
        if (this != &o) {
            this->clear();
            Release();
            TakeFrom(o);
        }
        return *this;
    }
    SmallVector& operator=(std::initializer_list<T> il) {
        this->assign(il.begin(), il.end());
        return *this;
    }
    ~SmallVector() {
        this->clear();
        Release();
    }

    void shrink_to_fit() noexcept {}

private:
    T* Inline() noexcept {
        return reinterpret_cast<T*>(storage);
    }
    T* Data() noexcept {
        return heap ? heap : Inline();
    }
    const T* Data() const noexcept {
        return heap ? heap : reinterpret_cast<const T*>(storage);
    }
    size_t Capacity() const noexcept {
        return heap ? heap_capacity : N;
    }

    void Grow(size_t need) {
        size_t cap = Capacity() * 2 > need ? Capacity() * 2 : need;
        T* p = static_cast<T*>(::operator new(cap * sizeof(T), std::align_val_t{alignof(T)}));
        Ops::Relocate(Data(), count, p);
        Release();
        heap = p;
        heap_capacity = static_cast<SizeT>(cap);
    }

    void Release() noexcept {
        if (heap) {
            ::operator delete(heap, std::align_val_t{alignof(T)});
            heap = nullptr;
            heap_capacity = 0;
        }
    }

    // Takes @o's heap block, or moves its inline elements; @o ends empty
    void TakeFrom(SmallVector& o) {
        if (o.heap) {
            heap = o.heap;
            heap_capacity = o.heap_capacity;
            count = o.count;
            o.heap = nullptr;
            o.heap_capacity = 0;
            o.count = 0;
            return;
        }
        Ops::Relocate(o.Inline(), o.count, Inline());
        count = o.count;
        o.count = 0;
    }

    T* heap = nullptr;
    SizeT heap_capacity = 0;
    SizeT count = 0;
    alignas(T) unsigned char storage[N * sizeof(T)];
};

} // namespace Common
