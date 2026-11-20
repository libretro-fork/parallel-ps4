// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <new>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "common/alignment.h"
#include "common/assert.h"
#include "common/types.h"

namespace Common {

struct SlotId {
    static constexpr u32 INVALID_INDEX = UINT32_MAX;

    SlotId() noexcept = default;
    constexpr SlotId(u32 index) noexcept : index(index) {}

    constexpr bool operator==(const SlotId& other) const noexcept {
        return index == other.index;
    }
    constexpr bool operator!=(const SlotId& other) const noexcept {
        return index != other.index;
    }
    constexpr bool operator<(const SlotId& other) const noexcept {
        return index < other.index;
    }

    constexpr explicit operator bool() const noexcept {
        return index != INVALID_INDEX;
    }

    u32 index = INVALID_INDEX;
};

void* ReserveMemoryPages(size_t size) noexcept;
void CommitMemoryPages(void* address, size_t size) noexcept;
void ReleaseMemoryPages(void* base, [[maybe_unused]] size_t size) noexcept;

template <class T>
class SlotVector {
    static constexpr size_t CHUNK_SIZE = 2_MB;
    static_assert(sizeof(T) <= CHUNK_SIZE);

public:
    explicit SlotVector() = default;
    explicit SlotVector(u32 max_entries) {
        Create(max_entries);
    }

    ~SlotVector() noexcept {
        Destroy();
    }

    SlotVector(const SlotVector&) = delete;
    SlotVector& operator=(const SlotVector&) = delete;
    SlotVector(SlotVector&&) = delete;
    SlotVector& operator=(SlotVector&&) = delete;

    [[nodiscard]] T& operator[](SlotId id) noexcept {
        ValidateIndex(id);
        return values[id.index];
    }

    [[nodiscard]] const T& operator[](SlotId id) const noexcept {
        ValidateIndex(id);
        return values[id.index];
    }

    void ResizeDestructive(u32 max_entries) {
        Destroy();
        Create(max_entries);
    }

    bool IsAllocated(SlotId id) const noexcept {
        return id.index < capacity && ReadStorageBit(id.index);
    }

    template <typename... Args>
    SlotId Insert(Args&&... args) noexcept {
        const u32 index = FreeValueIndex();
        new (&values[index]) T(static_cast<Args&&>(args)...);
        SetStorageBit(index);
        return SlotId{index};
    }

    void Erase(SlotId id) noexcept {
        ValidateIndex(id);
        values[id.index].~T();
        free_list[num_free++] = id.index;
        ResetStorageBit(id.index);
    }

    SlotId GetSlotId(const T& value) {
        const u32 index = static_cast<u32>(&value - values);
        return SlotId{index};
    }

    size_t Size() const noexcept {
        return capacity - num_free;
    }

private:
    void SetStorageBit(u32 index) noexcept {
        stored_bitset[index / 64] |= u64(1) << (index % 64);
    }

    void ResetStorageBit(u32 index) noexcept {
        stored_bitset[index / 64] &= ~(u64(1) << (index % 64));
    }

    bool ReadStorageBit(u32 index) const noexcept {
        return ((stored_bitset[index / 64] >> (index % 64)) & 1) != 0;
    }

    void ValidateIndex([[maybe_unused]] SlotId id) const noexcept {
        DEBUG_ASSERT(id);
        DEBUG_ASSERT(id.index < capacity);
        DEBUG_ASSERT(id.index / 64 < num_bitset_words);
        DEBUG_ASSERT(((stored_bitset[id.index / 64] >> (id.index % 64)) & 1) != 0);
    }

    [[nodiscard]] u32 FreeValueIndex() noexcept {
        if (num_free == 0) {
            AddChunk();
        }
        return free_list[--num_free];
    }

    void AddChunk() noexcept {
        ASSERT_MSG(!small_vector && num_chunks < max_chunks, "Run out of space");
        u8* const chunk_address = reinterpret_cast<u8*>(values) + num_chunks * CHUNK_SIZE;
        CommitMemoryPages(chunk_address, CHUNK_SIZE);
        ++num_chunks;
        const u32 new_capacity = num_chunks * CHUNK_SIZE / sizeof(T);
        const u32 new_entries = new_capacity - capacity;
        Grow(new_capacity);
        for (u32 i = new_entries; i-- > 0;) {
            free_list[num_free++] = capacity + i;
        }
        capacity = new_capacity;
    }

    void Create(u32 max_entries) {
        const u64 max_bytes = max_entries * sizeof(T);
        const u64 reserved_bytes = Common::AlignUpPow2(max_bytes, CHUNK_SIZE);
        max_chunks = reserved_bytes / CHUNK_SIZE;
        small_vector = max_bytes <= CHUNK_SIZE;
        if (small_vector) {
            capacity = max_entries;
            values = static_cast<T*>(calloc(capacity, sizeof(T)));
            Grow(capacity);
            // Taken from the back: the first index first
            for (u32 i = 0; i < capacity; ++i) {
                free_list[i] = capacity - 1 - i;
            }
            num_free = capacity;
        } else {
            values = static_cast<T*>(ReserveMemoryPages(reserved_bytes));
        }
    }

    void Destroy() {
        size_t index = 0;
        for (u32 word = 0; word < num_bitset_words; ++word) {
            u64 bits = stored_bitset[word];
            for (size_t bit = 0; bits; ++bit, bits >>= 1) {
                if ((bits & 1) != 0) {
                    values[index + bit].~T();
                }
            }
            index += 64;
        }
        if (values) {
            if (small_vector) {
                free(values);
            } else {
                ReleaseMemoryPages(values, max_chunks * CHUNK_SIZE);
            }
            values = nullptr;
        }

        free(stored_bitset);
        free(free_list);
        stored_bitset = nullptr;
        free_list = nullptr;
        num_bitset_words = 0;
        num_free = 0;
        max_chunks = 0;
        num_chunks = 0;
        capacity = 0;
        small_vector = false;
    }

    /// Room for @new_capacity entries in the bitset (new words clear) and the free list
    void Grow(u32 new_capacity) noexcept {
        const u32 words = (new_capacity + 63) / 64;
        u64* const bitset = static_cast<u64*>(realloc(stored_bitset, words * sizeof(u64)));
        u32* const list = static_cast<u32*>(realloc(free_list, new_capacity * sizeof(u32)));
        ASSERT_MSG(bitset && list, "Out of memory growing a slot vector");
        memset(bitset + num_bitset_words, 0, (words - num_bitset_words) * sizeof(u64));
        stored_bitset = bitset;
        free_list = list;
        num_bitset_words = words;
    }

    T* values{};
    u64* stored_bitset{};
    u32 num_bitset_words{};
    /// The free entries, the next taken from the end
    u32* free_list{};
    u32 num_free{};
    u32 max_chunks{};
    u32 num_chunks{};
    u32 capacity{};
    bool small_vector{};
};

} // namespace Common
