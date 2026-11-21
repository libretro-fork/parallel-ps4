// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/alignment.h"
#include "common/assert.h"
#include "common/types.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_staging_buffer_pool.h"

namespace Vulkan {

using VideoCore::MemoryType;

namespace {

constexpr u64 RING_IDLE_FRAMES = 300;
constexpr u64 LARGE_IDLE_FRAMES = 120;
constexpr u64 LARGE_MIN_GRANULARITY = 64_KB;
constexpr u64 BLOCK_SIZE = 16_MB;
// Blocks of the rings uploads and the tiler go through, made with the pool and kept: the first
// upload of a game, and the first after a quiet spell, allocates nothing. Blocks past them are
// made as a burst needs them and let go once idle.
constexpr size_t RING_KEPT_BLOCKS = 2;

u64 RoundAllocationSize(u64 size) {
    const u64 step =
        Common::Max((size ? u64(1) << (Common::BitWidth(size) - 1) : 0) / 4, LARGE_MIN_GRANULARITY);
    return Common::AlignUp(size, step);
}

} // Anonymous namespace

StagingBufferPool::StagingBufferPool(const Instance& instance_, Scheduler& scheduler_)
    : instance{instance_}, scheduler{scheduler_} {
    for (size_t type = 0; type < NUM_TYPES; ++type) {
        Ring& ring = rings[type];
        // Read backs mostly go straight into the game's memory or their own buffers
        // (RequestLarge); their ring is made as it is wanted
        if (static_cast<MemoryType>(type) == MemoryType::HostCached) {
            continue;
        }
        for (size_t i = 0; i < RING_KEPT_BLOCKS; ++i) {
            ring.blocks.push_back(Block{
                .buffer = new VideoCore::StreamBuffer(instance, scheduler,
                                                      static_cast<MemoryType>(type), BLOCK_SIZE),
                .last_used_frame = 0,
            });
        }
    }
}

StagingBufferPool::~StagingBufferPool() {
    for (Ring& ring : rings) {
        for (Block& block : ring.blocks) {
            delete block.buffer;
        }
    }
    for (LargeCache& cache : large_caches) {
        for (LargeBuffer& entry : cache) {
            delete entry.buffer;
        }
    }
}

StagingBufferRef StagingBufferPool::Request(u64 size, MemoryType type, u64 alignment, bool deferred,
                                            bool unsynchronized) {
    Ring& ring = rings[u32(type)];
    if (deferred || size > BLOCK_SIZE) {
        return RequestLarge(size, type, deferred, unsynchronized);
    }
    return RequestFromRing(ring, size, alignment, type, unsynchronized);
}

StagingBufferRef StagingBufferPool::RequestFromRing(Ring& ring, u64 size, u64 alignment,
                                                    MemoryType type, bool unsynchronized) {
    Block* best{};
    u64 best_offset{};
    for (size_t i = 0; i < ring.blocks.size(); ++i) {
        const size_t index = (ring.current + i) % ring.blocks.size();
        Block* block = &ring.blocks[index];
        u64 offset = 0;
        if (block->buffer->Reserve(size, &offset, alignment, false)) {
            ring.current = index;
            best = block;
            best_offset = offset;
            break;
        }
    }

    if (!best) {
        if (!ring.blocks.empty() && unsynchronized) {
            best = &ring.blocks[(ring.current + 1) % ring.blocks.size()];
            best_offset = 0;
        } else {
            best = &ring.blocks.emplace_back(Block{
                .buffer = new VideoCore::StreamBuffer(instance, scheduler, type, BLOCK_SIZE),
                .last_used_frame = frame,
            });
            ring.current = ring.blocks.size() - 1;
            const bool reserved = best->buffer->Reserve(size, &best_offset, alignment, false);
            ASSERT_MSG(reserved, "A new staging block cannot hold 0x%" PRIx64 " bytes", size);
        }
    }

    VideoCore::StreamBuffer& buffer = *best->buffer;
    best->last_used_frame = frame;

    StagingBufferRef ref{
        .buffer = &buffer,
        .offset = best_offset,
        .size = size,
        .type = type,
    };
    if (buffer.mapped_data) {
        ref.mapped = buffer.mapped_data + best_offset;
    }
    return ref;
}

StagingBufferRef StagingBufferPool::RequestLarge(u64 size, MemoryType type, bool deferred,
                                                 bool unsynchronized) {
    LargeCache& cache = large_caches[u32(type)];
    const u64 max_size = Common::Max(size + size / 4, RoundAllocationSize(size));

    LargeBuffer* best{};
    for (LargeBuffer& entry : cache) {
        const u64 entry_size = entry.buffer->SizeBytes();
        if (entry.held || entry_size < size || entry_size > max_size ||
            (!scheduler.IsFree(entry.tick) && !unsynchronized)) {
            continue;
        }
        if (!best || entry_size < best->buffer->SizeBytes()) {
            best = &entry;
        }
    }

    if (!best) {
        best = &cache.emplace_back(LargeBuffer{
            .buffer = new VideoCore::Buffer(instance, 0, RoundAllocationSize(size), type,
                                            "StagingBufferPool:Dedicated"),
            .tick = 0,
            .last_used_frame = frame,
            .held = false,
        });
    }

    best->tick = scheduler.CurrentTick();
    best->last_used_frame = frame;
    best->held = deferred;

    VideoCore::Buffer& buffer = *best->buffer;
    StagingBufferRef ref{
        .buffer = &buffer,
        .offset = 0,
        .size = size,
        .type = type,
    };
    if (buffer.mapped_data) {
        ref.mapped = buffer.mapped_data;
    }
    return ref;
}

void StagingBufferPool::FreeDeferred(const StagingBufferRef& ref) {
    LargeCache& cache = large_caches[u32(ref.type)];
    LargeBuffer* it = nullptr;
    for (LargeBuffer& entry : cache) {
        if (entry.buffer == ref.buffer) {
            it = &entry;
            break;
        }
    }
    ASSERT_MSG(it && it->held, "FreeDeferred on an allocation that is not held");
    it->held = false;
    it->tick = scheduler.CurrentTick();
    it->last_used_frame = frame;
}

void StagingBufferPool::TickFrame() {
    ++frame;
    for (Ring& ring : rings) {
        TrimRing(ring);
    }
    for (LargeCache& cache : large_caches) {
        TrimLarge(cache);
    }
}

void StagingBufferPool::TrimRing(Ring& ring) {
    Common::SmallVector<Block, RING_BLOCKS_ROOM>& blocks = ring.blocks;
    for (size_t i = blocks.size(); i-- > RING_KEPT_BLOCKS;) {
        const Block& block = blocks[i];
        if (frame - block.last_used_frame < RING_IDLE_FRAMES ||
            !scheduler.IsFree(block.buffer->LastTick())) {
            continue;
        }
        delete block.buffer;
        blocks.erase(blocks.begin() + i);
        if (i < ring.current) {
            --ring.current;
        } else if (i == ring.current) {
            ring.current = 0;
        }
    }
    if (ring.current >= blocks.size()) {
        ring.current = 0;
    }
}

void StagingBufferPool::TrimLarge(LargeCache& cache) {
    for (size_t i = cache.size(); i-- > 0;) {
        const LargeBuffer& entry = cache[i];
        if (!entry.held && frame - entry.last_used_frame >= LARGE_IDLE_FRAMES &&
            scheduler.IsFree(entry.tick)) {
            delete entry.buffer;
            cache.erase(cache.begin() + i);
        }
    }
}

} // namespace Vulkan
