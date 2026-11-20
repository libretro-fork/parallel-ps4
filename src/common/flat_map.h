// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// Sorted arrays used as a map or a set: one block of entries, found by binary
// search, kept in order by inserting in place. For the handful of entries a
// shader pass deals with, this beats a tree. The first N entries are stored
// in place. They replace Boost.Container's flat_map, small_flat_map and
// flat_set.

#include <stddef.h>

#include "common/small_vector.h"

namespace Common {

/// Orders keys with their operator<
template <typename K>
struct Less {
    constexpr bool operator()(const K& a, const K& b) const {
        return a < b;
    }
};

/// A map's entry: its key and its value
template <typename K, typename V>
struct KeyValue {
    K first;
    V second;
};

/// Where an insert went, and whether it added the entry
template <typename It>
struct InsertResult {
    It first;
    bool second;
};

template <typename K, typename V, size_t N = 8, typename Compare = Less<K>>
class FlatMap {
public:
    using value_type = KeyValue<K, V>;
    using Storage = SmallVector<value_type, N>;
    using iterator = typename Storage::iterator;
    using const_iterator = typename Storage::const_iterator;

    iterator begin() noexcept {
        return entries.begin();
    }
    iterator end() noexcept {
        return entries.end();
    }
    const_iterator begin() const noexcept {
        return entries.begin();
    }
    const_iterator end() const noexcept {
        return entries.end();
    }
    size_t size() const noexcept {
        return entries.size();
    }
    bool empty() const noexcept {
        return entries.empty();
    }
    void clear() noexcept {
        entries.clear();
    }

    /// The first entry whose key is not before @key
    iterator lower_bound(const K& key) {
        return entries.begin() + LowerBound(key);
    }
    const_iterator lower_bound(const K& key) const {
        return entries.begin() + LowerBound(key);
    }
    iterator find(const K& key) {
        const iterator it = lower_bound(key);
        return (it != end() && !less(key, it->first)) ? it : end();
    }
    const_iterator find(const K& key) const {
        const const_iterator it = lower_bound(key);
        return (it != end() && !less(key, it->first)) ? it : end();
    }
    bool contains(const K& key) const {
        return find(key) != end();
    }

    template <typename... Args>
    InsertResult<iterator> try_emplace(const K& key, Args&&... args) {
        iterator it = lower_bound(key);
        if (it != end() && !less(key, it->first)) {
            return {it, false};
        }
        it = entries.emplace(it, value_type{key, V(static_cast<Args&&>(args)...)});
        return {it, true};
    }
    InsertResult<iterator> insert(const value_type& v) {
        return try_emplace(v.first, v.second);
    }
    V& operator[](const K& key) {
        return try_emplace(key).first->second;
    }
    size_t erase(const K& key) {
        const iterator it = find(key);
        if (it == end()) {
            return 0;
        }
        entries.erase(it);
        return 1;
    }

private:
    size_t LowerBound(const K& key) const {
        size_t first = 0;
        size_t count = entries.size();
        while (count > 0) {
            const size_t half = count / 2;
            if (less(entries[first + half].first, key)) {
                first += half + 1;
                count -= half + 1;
            } else {
                count = half;
            }
        }
        return first;
    }

    Storage entries;
    [[no_unique_address]] Compare less{};
};

template <typename K, size_t N = 8, typename Compare = Less<K>>
class FlatSet {
public:
    using Storage = SmallVector<K, N>;
    using iterator = typename Storage::const_iterator;
    using const_iterator = typename Storage::const_iterator;

    const_iterator begin() const noexcept {
        return entries.begin();
    }
    const_iterator end() const noexcept {
        return entries.end();
    }
    size_t size() const noexcept {
        return entries.size();
    }
    bool empty() const noexcept {
        return entries.empty();
    }
    void clear() noexcept {
        entries.clear();
    }

    const_iterator find(const K& key) const {
        const const_iterator it = entries.begin() + LowerBound(key);
        return (it != end() && !less(key, *it)) ? it : end();
    }
    bool contains(const K& key) const {
        return find(key) != end();
    }
    InsertResult<const_iterator> insert(const K& key) {
        const size_t at = LowerBound(key);
        if (at != entries.size() && !less(key, entries[at])) {
            return {entries.begin() + at, false};
        }
        return {entries.insert(entries.begin() + at, key), true};
    }
    size_t erase(const K& key) {
        const const_iterator it = find(key);
        if (it == end()) {
            return 0;
        }
        entries.erase(it);
        return 1;
    }

private:
    size_t LowerBound(const K& key) const {
        size_t first = 0;
        size_t count = entries.size();
        while (count > 0) {
            const size_t half = count / 2;
            if (less(entries[first + half], key)) {
                first += half + 1;
                count -= half + 1;
            } else {
                count = half;
            }
        }
        return first;
    }

    Storage entries;
    [[no_unique_address]] Compare less{};
};

} // namespace Common
