// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <retro_atomic.h>
#include <rthreads/retro_eventcount.h>

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
        return null_index_buffer;
    }

    /// Retrieves the device local DBA page table buffer.
    [[nodiscard]] Buffer* GetBdaPageTableBuffer() noexcept {
        return bda_pagetable_buffer;
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

    /// An asynchronous write into the game's memory, recorded on the GPU thread: what is read
    /// back lands when the GPU is done (straight into the game's memory, or copied there by a
    /// deferred operation). Until EndWriteback, a CPU access to the range waits for it
    /// (WaitWritebacks), so that what the game writes there is not landed on afterwards.
    /// Returns the slot to end, -1 for none (none free: then it was waited for).
    int BeginWriteback(VAddr addr, u64 size);
    void EndWriteback(int slot);
    /// Before the CPU goes on in @addr, @size: waits for the write-backs into it still landing.
    /// On the GPU thread when @on_gpu_thread, else it is sent there.
    void WaitWritebacks(VAddr addr, u64 size, bool on_gpu_thread);

    /// Finds a buffer for the specified region; *@out_offset gets where in it the region is.
    [[nodiscard]] const Buffer* ObtainBuffer(VAddr device_addr, u32 size, u64* out_offset,
                                             ObtainBufferFlags flags = ObtainBufferFlags::None);

    /// Attempts to obtain a buffer without modifying the cache contents.
    [[nodiscard]] const Buffer* ObtainBufferForImage(VAddr device_addr, u32 size, u64* out_offset);

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
    /// run of the backing and in one chunk (guest_import.h); null otherwise, and then the
    /// caller reads back through a staging buffer as before. @alignment is what the offset
    /// (*@out_offset) must be a multiple of.
    [[nodiscard]] const Buffer* GuestTarget(VAddr device_addr, u64 size, u64 alignment,
                                            u64* out_offset);

    /// The game has mapped [device_addr, device_addr + size): with Readbacks Into Game Memory
    /// set to When Game Maps Memory, the memory behind it is lent to the GPU now. From the
    /// thread that maps it.
    void MapGuestMemory(VAddr device_addr, u64 size);

    /// Synchronizes all buffers needed for DMA.
    void SynchronizeDmaBuffers();

    /// Commits pending sparse buffer memory binds. Must be called before every scheduler submit.
    void SubmitPendingArenaBinds(Vulkan::SubmitInfo& info);

private:
    struct ArenaBinds {
        const Buffer* arena;
        VkSparseMemoryBind* binds;
        u32 num_binds;
        u32 binds_room;
    };

    /// The binds of @arena for the next submit. The slots are kept from submit to submit, each
    /// with the room its binds have taken, so that binding allocates only past the most ever
    /// pending at once.
    ArenaBinds* BindsForArena(const Buffer* arena);

    /// Adds @bind to @binds, making room past the most it has had
    static void AddBind(ArenaBinds* binds, const VkSparseMemoryBind& bind);

    /// Device memory for @blocks blocks of residency; *@out_first gets the block it starts at
    VkDeviceMemory BackingMemory(u64 blocks, u64* out_first);

    /// A new arena over @size bytes from @cpu_addr, kept until the cache goes
    Buffer* NewArena(VAddr cpu_addr, u64 size);

    /// Downloads [@device_addr, @device_addr + @size) of the arena over it into the game's
    /// memory (ReadMemory's request, on the GPU thread)
    void FlushRegion(VAddr device_addr, u64 size, bool is_write, bool async);

    const Buffer* GetArena(u64 first_block, u64 last_block);

    void EnsureResident(const Buffer* arena, u64 first_block, u64 last_block);

    void DownloadMemory(const Buffer* arena, VAddr device_addr, u64 size, bool async);

    /// Writes what DownloadMemory read back through @download into the game's memory, once the
    /// GPU is done with it
    void LandDownload(VAddr arena_base, VAddr device_addr, u64 size,
                      const Vulkan::StagingBufferRef& download, bool async, int writeback,
                      const VkBufferCopy* copies, u32 num_copies);

    bool SynchronizeMemory(const Buffer* arena, VAddr device_addr, u32 size, bool is_written,
                           bool is_texel_buffer);

    bool SynchronizeMemoryFromImage(const Buffer* arena, VAddr device_addr, u32 size);

    /// The Buffer over imported chunk @chunk, @handle, made the first time it is wanted
    const Buffer* GuestChunk(u32 chunk, VkBuffer handle);

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
    MemoryTracker* memory_tracker{};
    /// Write-backs still landing (BeginWriteback): set on the GPU thread, freed from wherever
    /// they land (a deferred operation, or the priority operations' thread)
    struct Writeback {
        retro_atomic_int_t live;
        VAddr addr;
        VAddr end;
        u64 tick;
    };
    static constexpr int WRITEBACK_SLOTS = 64;
    Writeback writebacks[WRITEBACK_SLOTS]{};
    retro_atomic_int_t writebacks_live{};
    retro_eventcount_t writebacks_ec{};
    void WaitWritebacksHere(VAddr addr, u64 size);

    StreamBuffer stream_buffer;
    Buffer gds_buffer;
    Buffer* null_index_buffer{};
    RangeSet gpu_modified_ranges;

    FaultManager* fault_manager{};
    Buffer* bda_pagetable_buffer{};
    bool fault_process_pending{};

    // The size of an arena page, in bits. A constant upstream; the libretro core picks it
    // for the device (see the constructor).
    u64 ARENA_PAGE_BITS{DEFAULT_ARENA_PAGE_BITS};
    u64 ARENA_PAGE_SIZE{u64{1} << DEFAULT_ARENA_PAGE_BITS};
    u64 NUM_ARENA_PAGES{u64{1} << (ADDRESS_SPACE_BITS - DEFAULT_ARENA_PAGE_BITS)};
    /// The arena over each arena page, null where there is none yet (NUM_ARENA_PAGES of them)
    const Buffer** address_space{};
    /// Every arena made, freed with the cache
    Buffer** arenas{};
    u32 num_arenas{};
    u32 arenas_room{};
    ArenaBinds* pending_binds{};
    u32 num_pending_binds{};
    u32 pending_binds_room{};
    VkSparseBufferMemoryBindInfo* buffer_binds{};
    u32 buffer_binds_room{};
    static constexpr u32 PENDING_ARENAS_ROOM = 16;
    static constexpr u32 PENDING_BINDS_ROOM = 64;
    // Residency is made out of chunks of device memory, a few blocks at a time, rather than
    // with an allocation of its own each time: the chunk, its size and the blocks taken
    VkDeviceMemory backing_chunk{VK_NULL_HANDLE};
    u64 backing_chunk_blocks{};
    u64 backing_chunk_used{};
    Vulkan::Semaphore memory_semaphore;

    struct Backing : public Interval {
        VkDeviceMemory memory;
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
    void WriteArenaPageTable(const Buffer* arena, const Interval* ranges, u32 num_ranges);
    u32 arena_memory_type_index{};
    u32 block_size{};
    u32 block_shift{};
    u32 blocks_per_arena_page{};
    u32 blocks_per_arena_page_shift{};

    // The game's memory lent to the GPU, null without VK_EXT_external_memory_host; a Buffer
    // over each chunk of it, made the first time one is wanted (the table itself at start),
    // and the copies being gathered for one (CopyToGuest)
    ::guest_import* guest{};
    Buffer** guest_chunks{};
    u32 num_guest_chunks{};
    Common::SmallVector<VkBufferCopy, 8> guest_copies;
    Common::SmallVector<VkBufferCopy, 64> guest_rest;
    const Buffer* guest_src{};
    const Buffer* guest_batch{};
    u64 guest_src_offset{};
    bool guest_at_map{};
};

} // namespace VideoCore
