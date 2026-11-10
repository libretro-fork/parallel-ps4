// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// Sorted arrays used as a map or a set: one block of entries, found by binary
// search, kept in order by inserting in place. For the handful of entries a
// shader pass deals with, this beats a tree. The first N entries are stored
// in place. They replace Boost.Container's flat_map, small_flat_map and
// flat_set.

#include <algorithm>
#include <cstddef>
#include <functional>
#include <tuple>
#include <utility>

#include "common/small_vector.h"

namespace Common {

template <typename K, typename V, std::size_t N = 8, typename Compare = std::less<K>>
class FlatMap {
public:
    using value_type = std::pair<K, V>;
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
    std::size_t size() const noexcept {
        return entries.size();
    }
    bool empty() const noexcept {
        return entries.empty();
    }
    void clear() noexcept {
        entries.clear();
    }

    iterator lower_bound(const K& key) {
        return std::lower_bound(
            entries.begin(), entries.end(), key,
            [this](const value_type& e, const K& k) { return less(e.first, k); });
    }
    const_iterator lower_bound(const K& key) const {
        return std::lower_bound(
            entries.begin(), entries.end(), key,
            [this](const value_type& e, const K& k) { return less(e.first, k); });
    }
    iterator find(const K& key) {
        const auto it = lower_bound(key);
        return (it != end() && !less(key, it->first)) ? it : end();
    }
    const_iterator find(const K& key) const {
        const auto it = lower_bound(key);
        return (it != end() && !less(key, it->first)) ? it : end();
    }
    bool contains(const K& key) const {
        return find(key) != end();
    }

    template <typename... Args>
    std::pair<iterator, bool> try_emplace(const K& key, Args&&... args) {
        auto it = lower_bound(key);
        if (it != end() && !less(key, it->first)) {
            return {it, false};
        }
        it = entries.emplace(it, std::piecewise_construct, std::forward_as_tuple(key),
                             std::forward_as_tuple(std::forward<Args>(args)...));
        return {it, true};
    }
    std::pair<iterator, bool> insert(const value_type& v) {
        return try_emplace(v.first, v.second);
    }
    V& operator[](const K& key) {
        return try_emplace(key).first->second;
    }
    std::size_t erase(const K& key) {
        const auto it = find(key);
        if (it == end()) {
            return 0;
        }
        entries.erase(it);
        return 1;
    }

private:
    Storage entries;
    [[no_unique_address]] Compare less{};
};

template <typename K, std::size_t N = 8, typename Compare = std::less<K>>
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
    std::size_t size() const noexcept {
        return entries.size();
    }
    bool empty() const noexcept {
        return entries.empty();
    }
    void clear() noexcept {
        entries.clear();
    }

    const_iterator find(const K& key) const {
        const auto it = std::lower_bound(entries.begin(), entries.end(), key, less);
        return (it != end() && !less(key, *it)) ? it : end();
    }
    bool contains(const K& key) const {
        return find(key) != end();
    }
    std::pair<const_iterator, bool> insert(const K& key) {
        auto it = std::lower_bound(entries.begin(), entries.end(), key, less);
        if (it != entries.end() && !less(key, *it)) {
            return {it, false};
        }
        it = entries.insert(it, key);
        return {it, true};
    }
    std::size_t erase(const K& key) {
        const auto it = find(key);
        if (it == end()) {
            return 0;
        }
        entries.erase(it);
        return 1;
    }

private:
    Storage entries;
    [[no_unique_address]] Compare less{};
};

} // namespace Common
