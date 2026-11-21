// SPDX-FileCopyrightText: Copyright 2025-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/assert.h"
#include "common/types.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "common/concepts.h"

namespace Serialization {

/// What Writer and Reader take whole, with a count first: a container of elements
template <typename T>
concept Container = requires(T t) {
    typename T::iterator;
    requires Common::IsSame<decltype(t.begin()), typename T::iterator>;
    requires Common::IsSame<decltype(t.end()), typename T::iterator>;
    static_cast<size_t>(t.size());
};

/// Bytes to serialize into, owned (on the heap, grown as written), or to deserialize from,
/// owned or lent (a mapped cache file read where it lies).
struct Archive {
    /// @n zeroed bytes, the whole archive
    void Alloc(size_t n) {
        used = 0;
        Grow(n);
    }

    /// @n zeroed bytes more
    void Grow(size_t n) {
        Reserve(used + n);
        memset(buffer + used, 0, n);
        used += n;
        Own();
    }

    void Merge(const Archive& ar) {
        Reserve(used + ar.size);
        memcpy(buffer + used, ar.base, ar.size);
        used += ar.size;
        Own();
        offset = static_cast<u32>(used);
    }

    [[nodiscard]] size_t SizeBytes() const {
        return size;
    }

    u8* CurrPtr() {
        ASSERT_MSG(base == buffer, "Writing into lent bytes");
        return buffer + offset;
    }

    const u8* ReadPtr() const {
        return base + offset;
    }

    void Advance(size_t bytes) {
        ASSERT_MSG(offset + bytes <= size,
                   "Invalid or corrupted deserialization container/shader cache");
        offset += static_cast<u32>(bytes);
    }

    /// The bytes written, handed over: made with malloc, for the caller to free; their count
    /// in *@out_size. The archive is empty after.
    u8* TakeOff(size_t* out_size) {
        u8* const taken = buffer;
        *out_size = used;
        buffer = nullptr;
        used = 0;
        capacity = 0;
        offset = 0;
        base = nullptr;
        size = 0;
        return taken;
    }

    [[nodiscard]] bool IsEoS() const {
        return offset >= size;
    }

    Archive() = default;
    /// Reads @lent in place; it must outlive the archive.
    explicit Archive(Common::Span<const u8> lent) : base{lent.data()}, size{lent.size()} {}
    ~Archive() {
        free(buffer);
    }

    Archive(const Archive&) = delete;
    Archive& operator=(const Archive&) = delete;

private:
    void Own() {
        base = buffer;
        size = used;
    }

    /// Room for @n bytes, doubled as it grows
    void Reserve(size_t n) {
        if (n <= capacity) {
            return;
        }
        size_t grown = capacity ? capacity * 2 : 256;
        while (grown < n) {
            grown *= 2;
        }
        u8* const moved = static_cast<u8*>(realloc(buffer, grown));
        ASSERT_MSG(moved, "Out of memory for a serialization archive");
        buffer = moved;
        capacity = grown;
    }

    u32 offset{};
    u8* buffer{};
    size_t used{};
    size_t capacity{};
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
        memcpy(ar.CurrPtr(), reinterpret_cast<const void*>(ptr), size);
        ar.Advance(size);
    }

    template <typename T>
        requires(!Container<T>)
    void Write(const T& value) {
        Write(&value, sizeof(value));
    }

    template <typename C>
        requires Container<C>
    void Write(const C& v) {
        Write(v.size());
        for (const typename C::value_type& elem : v) {
            Write(elem);
        }
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
        memcpy(reinterpret_cast<void*>(ptr), ar.ReadPtr(), size);
        ar.Advance(size);
    }

    template <typename T>
        requires(!Container<T>)
    void Read(T& value) {
        Read(&value, sizeof(value));
    }

    template <typename C>
        requires Container<C>
    void Read(C& v) {
        size_t num_elements{};
        Read(num_elements);
        for (size_t i = 0; i < num_elements; ++i) {
            v.emplace_back();
            Read(v.back());
        }
    }

    Reader() = delete;
    explicit Reader(Archive& ar_) : ar{ar_} {}

    Archive& ar;
};

} // namespace Serialization
