// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/small_vector.h"
#include "common/types.h"
#include "video_core/buffer_cache/buffer.h"

namespace Vulkan {

class Instance;
class Scheduler;

struct StagingBufferRef {
    VideoCore::Buffer* buffer{};
    u64 offset{};
    u64 size{};
    u8* mapped{};
    VideoCore::MemoryType type{};

    void Flush() const noexcept {
        buffer->Flush(offset, size);
    }

    void Invalidate() const noexcept {
        buffer->Invalidate(offset, size);
    }
};

class StagingBufferPool {
public:
    explicit StagingBufferPool(const Instance& instance, Scheduler& scheduler);
    ~StagingBufferPool();

    StagingBufferPool(const StagingBufferPool&) = delete;
    StagingBufferPool& operator=(const StagingBufferPool&) = delete;

    /// Requests size bytes of memory of the given type.
    [[nodiscard]] StagingBufferRef Request(u64 size, VideoCore::MemoryType type, u64 alignment = 0,
                                           bool deferred = false, bool unsynchronized = false);

    /// Releases a deferred allocation. It becomes reusable once the current tick completes.
    void FreeDeferred(const StagingBufferRef& ref);

    /// Releases idle memory.
    void TickFrame();

private:
    struct Block {
        /// Owned: made with new, deleted when the block is let go
        VideoCore::StreamBuffer* buffer;
        u64 last_used_frame;
    };

    /// Room for the blocks of a ring before any is made on the heap
    static constexpr size_t RING_BLOCKS_ROOM = 32;

    struct Ring {
        Common::SmallVector<Block, RING_BLOCKS_ROOM> blocks;
        size_t current = 0;
    };

    struct LargeBuffer {
        /// Owned: made with new, deleted when the buffer is let go
        VideoCore::Buffer* buffer;
        u64 tick;
        u64 last_used_frame;
        bool held;
    };

    /// Room for the large buffers of a type before any is made on the heap
    static constexpr size_t LARGE_ROOM = 16;
    using LargeCache = Common::SmallVector<LargeBuffer, LARGE_ROOM>;

    static constexpr size_t NUM_TYPES = 3;

    StagingBufferRef RequestFromRing(Ring& ring, u64 size, u64 alignment,
                                     VideoCore::MemoryType type, bool unsynchronized);
    StagingBufferRef RequestLarge(u64 size, VideoCore::MemoryType type, bool deferred,
                                  bool unsynchronized);

    void TrimRing(Ring& ring);
    void TrimLarge(LargeCache& cache);

    const Instance& instance;
    Scheduler& scheduler;

    Ring rings[NUM_TYPES];
    LargeCache large_caches[NUM_TYPES];
    u64 frame = 0;
};

} // namespace Vulkan
