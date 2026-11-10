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

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

#include "common/assert.h"

namespace Common {

namespace Detail {

// The element-handling half both share; Storage gives data(), capacity()
// and Grow(), and holds count.
template <typename Derived, typename T>
class InlineVectorOps {
public:
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;
    using iterator = T*;
    using const_iterator = const T*;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    T* data() noexcept {
        return Self().Data();
    }
    const T* data() const noexcept {
        return Self().Data();
    }
    std::size_t size() const noexcept {
        return Self().count;
    }
    std::size_t capacity() const noexcept {
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

    T& operator[](std::size_t i) noexcept {
        return data()[i];
    }
    const T& operator[](std::size_t i) const noexcept {
        return data()[i];
    }
    T& at(std::size_t i) {
        ASSERT(i < size());
        return data()[i];
    }
    const T& at(std::size_t i) const {
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

    void reserve(std::size_t n) {
        if (n > capacity()) {
            Self().Grow(n);
        }
    }

    template <typename... Args>
    T& emplace_back(Args&&... args) {
        if (size() == capacity()) {
            // The arguments may refer to an element that is about to move
            T tmp(std::forward<Args>(args)...);
            Self().Grow(size() + 1);
            T* p = ::new (static_cast<void*>(data() + size())) T(std::move(tmp));
            ++Self().count;
            return *p;
        }
        T* p = ::new (static_cast<void*>(data() + size())) T(std::forward<Args>(args)...);
        ++Self().count;
        return *p;
    }
    void push_back(const T& v) {
        emplace_back(v);
    }
    void push_back(T&& v) {
        emplace_back(std::move(v));
    }
    void pop_back() noexcept {
        --Self().count;
        std::destroy_at(data() + size());
    }

    void clear() noexcept {
        std::destroy(begin(), end());
        Self().count = 0;
    }

    void resize(std::size_t n) {
        Resize(n, [](T* p) { ::new (static_cast<void*>(p)) T(); });
    }
    void resize(std::size_t n, const T& v) {
        Resize(n, [&v](T* p) { ::new (static_cast<void*>(p)) T(v); });
    }

    template <typename It>
    void assign(It first, It last) {
        clear();
        for (; first != last; ++first) {
            emplace_back(*first);
        }
    }
    void assign(std::size_t n, const T& v) {
        clear();
        resize(n, v);
    }
    void assign(std::initializer_list<T> il) {
        assign(il.begin(), il.end());
    }

    template <typename... Args>
    iterator emplace(const_iterator pos, Args&&... args) {
        const std::size_t at = static_cast<std::size_t>(pos - begin());
        emplace_back(std::forward<Args>(args)...);
        std::rotate(begin() + at, end() - 1, end());
        return begin() + at;
    }
    iterator insert(const_iterator pos, const T& v) {
        return emplace(pos, v);
    }
    iterator insert(const_iterator pos, T&& v) {
        return emplace(pos, std::move(v));
    }
    template <typename It, typename = typename std::iterator_traits<It>::iterator_category>
    iterator insert(const_iterator pos, It first, It last) {
        const std::size_t at = static_cast<std::size_t>(pos - begin());
        const std::size_t old = size();
        for (; first != last; ++first) {
            emplace_back(*first);
        }
        std::rotate(begin() + at, begin() + old, end());
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
        T* new_end = std::move(l, end(), f);
        std::destroy(new_end, end());
        Self().count = static_cast<decltype(Self().count)>(new_end - begin());
        return f;
    }

    friend bool operator==(const Derived& a, const Derived& b) {
        return std::equal(a.begin(), a.end(), b.begin(), b.end());
    }

protected:
    Derived& Self() noexcept {
        return static_cast<Derived&>(*this);
    }
    const Derived& Self() const noexcept {
        return static_cast<const Derived&>(*this);
    }

    template <typename Make>
    void Resize(std::size_t n, Make make) {
        if (n < size()) {
            std::destroy(begin() + n, end());
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
    static void Relocate(T* from, std::size_t n, T* to) {
        if constexpr (std::is_trivially_copyable_v<T>) {
            if (n) {
                std::memcpy(static_cast<void*>(to), static_cast<const void*>(from), n * sizeof(T));
            }
        } else {
            std::uninitialized_move(from, from + n, to);
            std::destroy(from, from + n);
        }
    }
};

} // namespace Detail

/// At most N elements, all in place.
template <typename T, std::size_t N>
class StaticVector : public Detail::InlineVectorOps<StaticVector<T, N>, T> {
    using Ops = Detail::InlineVectorOps<StaticVector<T, N>, T>;
    friend Ops;

public:
    StaticVector() noexcept = default;
    explicit StaticVector(std::size_t n) {
        this->resize(n);
    }
    StaticVector(std::size_t n, const T& v) {
        this->resize(n, v);
    }
    StaticVector(std::initializer_list<T> il) {
        this->assign(il.begin(), il.end());
    }
    template <typename It, typename = typename std::iterator_traits<It>::iterator_category>
    StaticVector(It first, It last) {
        this->assign(first, last);
    }
    StaticVector(const StaticVector& o) {
        this->assign(o.begin(), o.end());
    }
    StaticVector(StaticVector&& o) noexcept(std::is_nothrow_move_constructible_v<T>) {
        std::uninitialized_move(o.begin(), o.end(), Data());
        count = o.count;
        o.clear();
    }
    StaticVector& operator=(const StaticVector& o) {
        if (this != &o) {
            this->assign(o.begin(), o.end());
        }
        return *this;
    }
    StaticVector& operator=(StaticVector&& o) noexcept(std::is_nothrow_move_constructible_v<T>) {
        if (this != &o) {
            this->clear();
            std::uninitialized_move(o.begin(), o.end(), Data());
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

    static constexpr std::size_t max_size() noexcept {
        return N;
    }

private:
    T* Data() noexcept {
        return std::launder(reinterpret_cast<T*>(storage));
    }
    const T* Data() const noexcept {
        return std::launder(reinterpret_cast<const T*>(storage));
    }
    static constexpr std::size_t Capacity() noexcept {
        return N;
    }
    void Grow(std::size_t) {
        ASSERT_MSG(false, "StaticVector of {} is full", N);
    }

    alignas(T) unsigned char storage[N * sizeof(T)];
    std::size_t count = 0;
};

/// Up to N elements in place, then on the heap. SizeT is the type the count
/// and capacity are kept in, for structs that want to stay small.
template <typename T, std::size_t N, typename SizeT = std::size_t>
class SmallVector : public Detail::InlineVectorOps<SmallVector<T, N, SizeT>, T> {
    using Ops = Detail::InlineVectorOps<SmallVector<T, N, SizeT>, T>;
    friend Ops;

public:
    SmallVector() noexcept = default;
    explicit SmallVector(std::size_t n) {
        this->resize(n);
    }
    SmallVector(std::size_t n, const T& v) {
        this->resize(n, v);
    }
    SmallVector(std::initializer_list<T> il) {
        this->assign(il.begin(), il.end());
    }
    template <typename It, typename = typename std::iterator_traits<It>::iterator_category>
    SmallVector(It first, It last) {
        this->assign(first, last);
    }
    SmallVector(const SmallVector& o) {
        this->assign(o.begin(), o.end());
    }
    SmallVector(SmallVector&& o) noexcept(std::is_nothrow_move_constructible_v<T>) {
        TakeFrom(o);
    }
    SmallVector& operator=(const SmallVector& o) {
        if (this != &o) {
            this->assign(o.begin(), o.end());
        }
        return *this;
    }
    SmallVector& operator=(SmallVector&& o) noexcept(std::is_nothrow_move_constructible_v<T>) {
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
        return std::launder(reinterpret_cast<T*>(storage));
    }
    T* Data() noexcept {
        return heap ? heap : Inline();
    }
    const T* Data() const noexcept {
        return heap ? heap : std::launder(reinterpret_cast<const T*>(storage));
    }
    std::size_t Capacity() const noexcept {
        return heap ? heap_capacity : N;
    }

    void Grow(std::size_t need) {
        std::size_t cap = std::max<std::size_t>(Capacity() * 2, need);
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
