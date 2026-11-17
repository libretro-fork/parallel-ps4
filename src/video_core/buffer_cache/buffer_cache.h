// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <deque>
#include <optional>
#include <span>
#include <vector>
#include "common/small_vector.h"

#include "common/enum.h"
#include "common/interval_set.h"
#include "common/types.h"
#include "video_core/buffer_cache/buffer.h"
#include "video_core/buffer_cache/fault_manager.h"
#include "video_core/buffer_cache/range_set.h"
#include "video_core/renderer_vulkan/vk_semaphore.h"
#include "video_core/texture_cache/image.h"

struct write_snapshots;
struct guest_import;

namespace AmdGpu {
struct Liverpool;
}

namespace Core {
class MemoryManager;
}

namespace Vulkan {
class GraphicsPipeline;
struct SubmitInfo;
class Runtime;
class StagingBufferPool;
struct StagingBufferRef;
} // namespace Vulkan

namespace VideoCore {

class TextureCache;
class MemoryTracker;
class PageManager;

enum class ObtainBufferFlags {
    None = 0,
    IsWritten = 1 << 0,
    IsTexelBuffer = 1 << 1,
    IgnoreStreamBuffer = 1 << 2,
    InvalidateTextureCache = 1 << 3,
};
DECLARE_ENUM_FLAG_OPERATORS(ObtainBufferFlags)

class BufferCache {
    static constexpr u64 ADDRESS_SPACE_BITS = 40;
    static constexpr u64 DEFAULT_ARENA_PAGE_BITS = 32;
    // The smallest an arena page gets on a device with little sparse address space
    static constexpr u64 MIN_ARENA_PAGE_BITS = 26;
    static constexpr u64 MIN_BLOCK_SIZE = 16_KB;
    static constexpr u64 STREAM_THRESHOLD = 16_KB;

public:
    explicit BufferCache(const Vulkan::Instance& instance, Vulkan::Scheduler& scheduler,
                         Vulkan::Runtime& runtime, AmdGpu::Liverpool* liverpool,
                         TextureCache& texture_cache, PageManager& tracker);
    ~BufferCache();

    /// Returns a pointer to GDS device local buffer.
    [[nodiscard]] const Buffer* GetGdsBuffer() const noexcept {
        return &gds_buffer;
    }

    [[nodiscard]] const Buffer* GetNullIndexBuffer() const noexcept {
        return null_index_buffer.get();
    }

    /// Retrieves the device local DBA page table buffer.
    [[nodiscard]] Buffer* GetBdaPageTableBuffer() noexcept {
        return bda_pagetable_buffer.get();
    }

    /// Retrieves the fault buffer.
    [[nodiscard]] Buffer* GetFaultBuffer() noexcept {
        return fault_manager->GetFaultBuffer();
    }

    /// Retrieves the stream buffer.
    [[nodiscard]] StreamBuffer& GetStreamBuffer() noexcept {
        return stream_buffer;
    }

    /// Returns minimum granularity of a sparse memory bind.
    u32 GetSparsePageShift() const noexcept {
        return block_shift;
    }

    void TickFrame();

    /// Invalidates any buffer in the logical page range.
    void InvalidateMemory(VAddr device_addr, u64 size, bool assume_locks = false);

    /// Unmark a region as modified from the GPU.
    void UnmarkRegionAsGpuModified(VAddr device_addr, u64 size, bool is_write = false);

    /// Flushes any GPU modified buffer in the logical page range back to CPU memory.
    void ReadMemory(VAddr device_addr, u64 size, bool is_write = false, bool async = false,
                    bool assume_locks = false);

    /// Flushes GPU modified ranges of the uncovered part of the edge pages of an image.
    void ReadEdgeImagePages(const Image& image);

    /// Finds a buffer for the specified region.
    [[nodiscard]] std::pair<const Buffer*, u64> ObtainBuffer(
        VAddr device_addr, u32 size, ObtainBufferFlags flags = ObtainBufferFlags::None);

    /// Attempts to obtain a buffer without modifying the cache contents.
    [[nodiscard]] std::pair<const Buffer*, u64> ObtainBufferForImage(VAddr device_addr, u32 size);

    /// Return true when a region is modified from the CPU
    [[nodiscard]] bool IsRegionCpuModified(VAddr addr, size_t size);

    /// Return true when a region is modified from the GPU
    [[nodiscard]] bool IsRegionGpuModified(VAddr addr, size_t size);

    /// True if the GPU wrote any of these very bytes (not: any byte of their pages)
    [[nodiscard]] bool IsRangeGpuWritten(VAddr addr, size_t size) const {
        bool written = false;
        gpu_modified_ranges.ForEachInRange(addr, size, [&](VAddr, VAddr) { written = true; });
        return written;
    }

    /// Mark region as modified from the GPU
    void MarkRegionAsGpuModified(VAddr addr, size_t size);

    /// Where the GPU writes [@device_addr, @device_addr + @size) of the game's memory itself:
    /// the imported buffer over it and the offset in that buffer, provided the range lies in one
    /// run of the backing and in one chunk (guest_import.h); {nullptr, 0} otherwise, and then
    /// the caller reads back through a staging buffer as before. @alignment is what the offset
    /// must be a multiple of.
    [[nodiscard]] std::pair<const Buffer*, u64> GuestTarget(VAddr device_addr, u64 size,
                                                            u64 alignment);

    /// Synchronizes all buffers needed for DMA.
    void SynchronizeDmaBuffers();

    /// Commits pending sparse buffer memory binds. Must be called before every scheduler submit.
    void SubmitPendingArenaBinds(Vulkan::SubmitInfo& info);

private:
    struct ArenaBinds {
        const Buffer* arena;
        std::vector<vk::SparseMemoryBind> binds;
    };

    /// The binds of @arena for the next submit. The slots are kept from submit to submit, each
    /// with the room its binds have taken, so that binding allocates only past the most ever
    /// pending at once.
    ArenaBinds* BindsForArena(const Buffer* arena) {
        for (std::size_t i = 0; i < num_pending_binds; ++i) {
            if (pending_binds[i].arena == arena) {
                return &pending_binds[i];
            }
        }
        if (num_pending_binds == pending_binds.size()) {
            pending_binds.emplace_back().binds.reserve(PENDING_BINDS_ROOM);
        }
        ArenaBinds* const slot = &pending_binds[num_pending_binds++];
        slot->arena = arena;
        slot->binds.clear();
        return slot;
    }

    /// Device memory for @blocks blocks of residency and the block it starts at
    std::pair<vk::DeviceMemory, u64> BackingMemory(u64 blocks);

    const Buffer* GetArena(u64 first_block, u64 last_block);

    void EnsureResident(const Buffer* arena, u64 first_block, u64 last_block);

    void DownloadMemory(const Buffer* arena, VAddr device_addr, u64 size, bool async);

    bool SynchronizeMemory(const Buffer* arena, VAddr device_addr, u32 size, bool is_written,
                           bool is_texel_buffer);

    bool SynchronizeMemoryFromImage(const Buffer* arena, VAddr device_addr, u32 size);

    /// The Buffer over imported chunk @chunk, @handle, made the first time it is wanted
    const Buffer* GuestChunk(u32 chunk, vk::Buffer handle);

    /// Of @copies (each with where in the game's memory it goes as its dstOffset) records from
    /// @src what the GPU can write into the game's memory itself, and leaves in @copies what
    /// must go through a staging buffer
    template <typename Copies>
    void CopyToGuest(const Buffer* src, Copies& copies);
    void FlushGuestCopies();
    static void OnGuestPiece(void* ctx, VkBuffer buffer, u32 chunk, VkDeviceSize buffer_offset,
                             u64 addr, u64 delta, u64 size);

    const Vulkan::Instance& instance;
    Vulkan::Scheduler& scheduler;
    Vulkan::Runtime& runtime;
    Vulkan::StagingBufferPool& staging_pool;
    AmdGpu::Liverpool* liverpool;
    Core::MemoryManager* memory;
    TextureCache& texture_cache;
    /// Pages as they were before the guest wrote where the GPU has written too (readbacks off)
    ::write_snapshots* snapshots{};
    std::unique_ptr<MemoryTracker> memory_tracker;

    StreamBuffer stream_buffer;
    Buffer gds_buffer;
    std::unique_ptr<Buffer> null_index_buffer;
    RangeSet gpu_modified_ranges;

    std::unique_ptr<FaultManager> fault_manager;
    std::unique_ptr<Buffer> bda_pagetable_buffer;
    bool fault_process_pending{};

    // The size of an arena page, in bits. A constant upstream; the libretro core picks it
    // for the device (see the constructor).
    u64 ARENA_PAGE_BITS{DEFAULT_ARENA_PAGE_BITS};
    u64 ARENA_PAGE_SIZE{u64{1} << DEFAULT_ARENA_PAGE_BITS};
    u64 NUM_ARENA_PAGES{u64{1} << (ADDRESS_SPACE_BITS - DEFAULT_ARENA_PAGE_BITS)};
    std::vector<const Buffer*> address_space;
    std::deque<Buffer> arenas;
    std::vector<ArenaBinds> pending_binds;
    std::size_t num_pending_binds{};
    std::vector<vk::SparseBufferMemoryBindInfo> buffer_binds;
    static constexpr std::size_t PENDING_ARENAS_ROOM = 16;
    static constexpr std::size_t PENDING_BINDS_ROOM = 64;
    // Residency is made out of chunks of device memory, a few blocks at a time, rather than
    // with an allocation of its own each time: the chunk, its size and the blocks taken
    vk::DeviceMemory backing_chunk{};
    u64 backing_chunk_blocks{};
    u64 backing_chunk_used{};
    Vulkan::Semaphore memory_semaphore;

    struct Backing : public Interval {
        vk::DeviceMemory memory;
        u64 offset;
        constexpr bool CanMergeWith(const Backing& other) const noexcept {
            return memory == other.memory && offset + (end - start) == other.offset;
        }
        constexpr Backing SubRange(u64 a, u64 b) const noexcept {
            return {{a, b}, memory, offset + (a - start)};
        }
    };
    IntervalList<Backing> resident_ranges;

    // The device has no sparse buffers: every arena is fully backed by memory of its own,
    // nothing is bound, and an arena that replaces others takes their contents by a copy
    bool dense_arenas{};
    void MigrateDenseArena(const Buffer* old_arena, const Buffer* new_arena);
    void WriteArenaPageTable(const Buffer* arena, std::span<const Interval> ranges);
    u32 arena_memory_type_index{};
    u32 block_size{};
    u32 block_shift{};
    u32 blocks_per_arena_page{};
    u32 blocks_per_arena_page_shift{};

    // The game's memory lent to the GPU, null without VK_EXT_external_memory_host; a Buffer
    // over each chunk of it, made the first time one is wanted (the table itself at start),
    // and the copies being gathered for one (CopyToGuest)
    ::guest_import* guest{};
    std::vector<std::optional<Buffer>> guest_chunks;
    Common::SmallVector<vk::BufferCopy, 8> guest_copies;
    std::vector<vk::BufferCopy> guest_rest;
    const Buffer* guest_src{};
    const Buffer* guest_batch{};
    u64 guest_src_offset{};
};

} // namespace VideoCore
