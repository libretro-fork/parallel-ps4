// SPDX-FileCopyrightText: Copyright 2025-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/assert.h"
#include "common/types.h"

#include <cstddef>
#include <cstring>
#include <span>
#include <string>
#include <vector>

namespace Serialization {

template <typename T>
concept Container = requires(T t) {
    typename T::iterator;
    { t.begin() } -> std::same_as<typename T::iterator>;
    { t.end() } -> std::same_as<typename T::iterator>;
    { t.size() } -> std::convertible_to<std::size_t>;
};

/// Bytes to serialize into, owned, or to deserialize from, owned or lent (a mapped cache file
/// read where it lies).
struct Archive {
    void Alloc(size_t size) {
        container.resize(size);
        Own();
    }

    void Grow(size_t size) {
        container.resize(container.size() + size);
        Own();
    }

    void Merge(const Archive& ar) {
        container.insert(container.end(), ar.base, ar.base + ar.size);
        Own();
        offset = container.size();
    }

    [[nodiscard]] size_t SizeBytes() const {
        return size;
    }

    u8* CurrPtr() {
        ASSERT_MSG(base == container.data(), "Writing into lent bytes");
        return container.data() + offset;
    }

    const u8* ReadPtr() const {
        return base + offset;
    }

    void Advance(size_t bytes) {
        ASSERT_MSG(offset + bytes <= size,
                   "Invalid or corrupted deserialization container/shader cache");
        offset += bytes;
    }

    std::vector<u8>&& TakeOff() {
        offset = 0;
        base = nullptr;
        size = 0;
        return std::move(container);
    }

    [[nodiscard]] bool IsEoS() const {
        return offset >= size;
    }

    Archive() = default;
    explicit Archive(std::vector<u8>&& v) : container{std::move(v)} {
        Own();
    }
    /// Reads @lent in place; it must outlive the archive.
    explicit Archive(std::span<const u8> lent) : base{lent.data()}, size{lent.size()} {}

    Archive(const Archive&) = delete;
    Archive& operator=(const Archive&) = delete;

private:
    void Own() {
        base = container.data();
        size = container.size();
    }

    u32 offset{};
    std::vector<u8> container{};
    const u8* base{};
    size_t size{};

    friend struct Writer;
    friend struct Reader;
};

struct Writer {
    template <typename T>
    void Write(const T* ptr, size_t size) {
        if (ar.offset + size >= ar.size) {
            ar.Grow(size);
        }
        std::memcpy(ar.CurrPtr(), reinterpret_cast<const void*>(ptr), size);
        ar.Advance(size);
    }

    template <typename T>
        requires(!Container<T>)
    void Write(const T& value) {
        const auto size = sizeof(value);
        Write(&value, size);
    }

    void Write(const auto& v) {
        Write(v.size());
        for (const auto& elem : v) {
            Write(elem);
        }
    }

    void Write(const std::string& s) {
        Write(s.size());
        Write(s.c_str(), s.size());
    }

    Writer() = delete;
    explicit Writer(Archive& ar_) : ar{ar_} {}

    Archive& ar;
};

struct Reader {
    template <typename T>
    void Read(T* ptr, size_t size) {
        ASSERT_MSG(ar.offset + size <= ar.size,
                   "Invalid or corrupted deserialization container/shader cache");
        std::memcpy(reinterpret_cast<void*>(ptr), ar.ReadPtr(), size);
        ar.Advance(size);
    }

    template <typename T>
        requires(!Container<T>)
    void Read(T& value) {
        const auto size = sizeof(value);
        Read(&value, size);
    }

    void Read(auto& v) {
        size_t num_elements{};
        Read(num_elements);
        for (int i = 0; i < num_elements; ++i) {
            v.emplace_back();
            Read(v.back());
        }
    }

    void Read(std::string& s) {
        size_t length{};
        Read(length);
        s.resize(length);
        Read(s.data(), length);
    }

    Reader() = delete;
    explicit Reader(Archive& ar_) : ar{ar_} {}

    Archive& ar;
};

} // namespace Serialization
