// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <new>
#include <stddef.h>
#include <stdlib.h>

#include "common/assert.h"

namespace Common {

/// Objects made one after another in chunks and all given back at once (ReleaseContents):
/// what the shader recompiler makes its instructions, blocks and statements in
template <typename T>
class ObjectPool {
    static_assert(alignof(T) <= alignof(max_align_t), "malloc does not align this far");

public:
    explicit ObjectPool(size_t chunk_size = 1024) : new_chunk_size{chunk_size} {
        AddChunk(new_chunk_size);
    }

    ~ObjectPool() {
        for (size_t i = 0; i < num_chunks; ++i) {
            DestroyChunk(chunks[i]);
        }
        free(chunks);
    }

    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;

    template <typename... Args>
    [[nodiscard]] T* Create(Args&&... args) {
        return ::new (static_cast<void*>(Memory())) T(static_cast<Args&&>(args)...);
    }

    void ReleaseContents() {
        if (num_chunks == 0) {
            return;
        }
        Chunk& root = chunks[0];
        if (root.used_objects == root.num_objects) {
            // Root chunk has been filled, squash allocations into it
            const size_t total_objects = root.num_objects + new_chunk_size * (num_chunks - 1);
            for (size_t i = 0; i < num_chunks; ++i) {
                DestroyChunk(chunks[i]);
            }
            num_chunks = 0;
            AddChunk(total_objects);
        } else {
            ReleaseObjects(root);
            for (size_t i = 1; i < num_chunks; ++i) {
                DestroyChunk(chunks[i]);
            }
            num_chunks = 1;
        }
        node = 0;
    }

private:
    struct Chunk {
        T* storage;
        size_t used_objects;
        size_t num_objects;
    };

    static void ReleaseObjects(Chunk& chunk) {
        for (size_t i = 0; i < chunk.used_objects; i++) {
            chunk.storage[i].~T();
        }
        chunk.used_objects = 0;
    }

    static void DestroyChunk(Chunk& chunk) {
        ReleaseObjects(chunk);
        free(chunk.storage);
        chunk.storage = nullptr;
        chunk.num_objects = 0;
    }

    void AddChunk(size_t size) {
        if (num_chunks == chunk_capacity) {
            const size_t capacity = chunk_capacity ? chunk_capacity * 2 : 4;
            Chunk* const grown = static_cast<Chunk*>(realloc(chunks, capacity * sizeof(Chunk)));
            ASSERT_MSG(grown, "Out of memory growing an object pool");
            chunks = grown;
            chunk_capacity = capacity;
        }
        T* const storage = static_cast<T*>(malloc(size * sizeof(T)));
        ASSERT_MSG(storage, "Out of memory making an object pool's chunk");
        chunks[num_chunks] = Chunk{storage, 0, size};
        node = num_chunks++;
    }

    [[nodiscard]] T* Memory() {
        if (chunks[node].used_objects == chunks[node].num_objects) {
            AddChunk(new_chunk_size);
        }
        Chunk& chunk = chunks[node];
        return &chunk.storage[chunk.used_objects++];
    }

    Chunk* chunks{};
    size_t num_chunks{};
    size_t chunk_capacity{};
    size_t node{};
    size_t new_chunk_size{};
};

} // namespace Common
