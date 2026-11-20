// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdlib.h>
#include <string.h>

#include "common/enum_names.h"

#include "common/alignment.h"
#include "core/memory.h"
#include "video_core/amdgpu/liverpool.h"
#include "video_core/buffer_cache/buffer.h"
#include "video_core/buffer_cache/buffer_cache.h"
#include "video_core/buffer_cache/guest_import.h"
#include "video_core/buffer_cache/memory_tracker.h"
#include "video_core/buffer_cache/region_definitions.h"
#include "video_core/renderer_vulkan/vk_graphics_pipeline.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_runtime.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/texture_cache/texture_cache.h"

namespace VideoCore {

static constexpr size_t GDS_BUFFER_SIZE = 64_KB;
static constexpr size_t NULL_INDEX_BUFFER_SIZE = 16;
static constexpr size_t STREAM_BUFFER_SIZE = 128_MB;

static u64 MinU64(u64 a, u64 b) {
    return a < b ? a : b;
}
static u64 MaxU64(u64 a, u64 b) {
    return a > b ? a : b;
}
static u64 ClampU64(u64 v, u64 lo, u64 hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}
/// The number of bits @v takes (0 for 0), as std::bit_width
static u32 BitWidth(u64 v) {
    return v ? 64u - static_cast<u32>(__builtin_clzll(v)) : 0u;
}

/// The value a cleared CMask expands to; 0 for a sample count without one
constexpr u32 CmaskColorExpandedValue(u32 num_samples) {
    switch (num_samples) {
    case 1:
        return 0xFFFFFFFF;
    case 2:
        return 0xDDDDDDDD;
    case 4:
        return 0xEEEEEEEE;
    case 8:
        return 0xFFFFFFFF;
    default:
        return 0;
    }
}
static_assert(CmaskColorExpandedValue(1) == 0xFFFFFFFF);
static_assert(CmaskColorExpandedValue(2) == 0xDDDDDDDD);
static_assert(CmaskColorExpandedValue(4) == 0xEEEEEEEE);
static_assert(CmaskColorExpandedValue(8) == 0xFFFFFFFF);

static constexpr VkBufferUsageFlags ARENA_USAGE =
    VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
    VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
    VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

/// The first memory type of @memory_type_bits with all of @wanted, into *@out; false if none
static bool FindMemoryType(const VkPhysicalDeviceMemoryProperties& properties,
                           VkMemoryPropertyFlags wanted, u32 memory_type_bits, u32* out) {
    for (u32 i = 0; i < properties.memoryTypeCount; ++i) {
        if (((memory_type_bits >> i) & 1) == 0) {
            continue;
        }
        const VkMemoryPropertyFlags flags = properties.memoryTypes[i].propertyFlags;
        if ((flags & wanted) == wanted) {
            *out = i;
            return true;
        }
    }
    return false;
}

BufferCache::BufferCache(const Vulkan::Instance& instance_, Vulkan::Scheduler& scheduler_,
                         Vulkan::Runtime& runtime_, AmdGpu::Liverpool* liverpool_,
                         TextureCache& texture_cache_, PageManager& tracker)
    : instance{instance_}, scheduler{scheduler_}, runtime{runtime_},
      staging_pool{runtime_.GetStagingPool()}, liverpool{liverpool_},
      memory{Core::Memory::Instance()}, texture_cache{texture_cache_},
      snapshots{emu_settings_get_readbacks_mode() == EMU_READBACKS_DISABLED
                    ? write_snapshots_create()
                    : nullptr},
      memory_tracker{new MemoryTracker(tracker, snapshots)},
      stream_buffer{instance, scheduler, MemoryType::Stream, STREAM_BUFFER_SIZE},
      gds_buffer{instance, 0, GDS_BUFFER_SIZE, MemoryType::Stream, "GDS Buffer"},
      memory_semaphore{instance} {
    VkBufferCreateFlags sparse_flags =
        VK_BUFFER_CREATE_SPARSE_BINDING_BIT | VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT;
    // An arena is a sparse buffer over 4 GiB of the guest's address space, and a game uses
    // several. A device whose sparse resources share less address space than a few of those
    // (Adreno 618 has 4 GiB in all) could not make the first one, so there the arenas are
    // smaller: a sixty-fourth of what the device has, no less than 64 MiB. And a device
    // without sparse residency is not asked for it: the ranges a draw uses are bound before
    // it (EnsureResident), only direct memory access reads outside them.
    if (!instance.IsSparseResidencyBufferSupported()) {
        sparse_flags = VK_BUFFER_CREATE_SPARSE_BINDING_BIT;
    }
    // A device without sparse buffers at all (Intel HD 615 under Mesa) has every arena backed
    // whole, so they are kept as small as with little sparse address space
    dense_arenas = !instance.IsSparseBindingSupported();
    const u64 sparse_space = instance.SparseAddressSpaceSize();
    if (dense_arenas) {
        sparse_flags = 0;
        ARENA_PAGE_BITS = MIN_ARENA_PAGE_BITS;
        ARENA_PAGE_SIZE = u64{1} << ARENA_PAGE_BITS;
        NUM_ARENA_PAGES = u64{1} << (ADDRESS_SPACE_BITS - ARENA_PAGE_BITS);
        LOG_WARNING(Render,
                    "The device has no sparse buffers: arenas of %" PRIu64 " MiB backed whole",
                    ARENA_PAGE_SIZE >> 20);
    } else if (sparse_space != 0 && sparse_space < (u64{64} << DEFAULT_ARENA_PAGE_BITS)) {
        ARENA_PAGE_BITS =
            ClampU64(BitWidth(sparse_space) - 1 - 6, MIN_ARENA_PAGE_BITS, DEFAULT_ARENA_PAGE_BITS);
        ARENA_PAGE_SIZE = u64{1} << ARENA_PAGE_BITS;
        NUM_ARENA_PAGES = u64{1} << (ADDRESS_SPACE_BITS - ARENA_PAGE_BITS);
        LOG_WARNING(Render,
                    "The device has %" PRIu64
                    " MiB of sparse address space: arena pages of %" PRIu64 " MiB, "
                    "sparse residency %s",
                    sparse_space >> 20, ARENA_PAGE_SIZE >> 20,
                    instance.IsSparseResidencyBufferSupported() ? "used" : "not available");
    }
    address_space = static_cast<const Buffer**>(calloc(NUM_ARENA_PAGES, sizeof(*address_space)));
    ASSERT_MSG(address_space, "Out of memory for the arena table");
    VkBufferCreateInfo probe_ci{};
    probe_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    probe_ci.flags = sparse_flags;
    probe_ci.size = ARENA_PAGE_SIZE;
    probe_ci.usage = ARENA_USAGE;
    probe_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkDeviceBufferMemoryRequirements req_info{};
    req_info.sType = VK_STRUCTURE_TYPE_DEVICE_BUFFER_MEMORY_REQUIREMENTS;
    req_info.pCreateInfo = &probe_ci;
    VkMemoryRequirements2 reqs2{};
    reqs2.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
    vkGetDeviceBufferMemoryRequirements(static_cast<VkDevice>(instance.GetDevice()), &req_info,
                                        &reqs2);
    const VkMemoryRequirements& reqs = reqs2.memoryRequirements;
    block_size = Common::AlignUp(MaxU64(reqs.alignment, MIN_BLOCK_SIZE), reqs.alignment);
    ASSERT_MSG(__builtin_popcount(block_size) == 1, "Sparse block size %u is not a power of 2",
               block_size);
    block_shift = BitWidth(block_size) - 1;
    blocks_per_arena_page = ARENA_PAGE_SIZE / block_size;
    blocks_per_arena_page_shift = ARENA_PAGE_BITS - block_shift;
    const bool found_type = FindMemoryType(
        reinterpret_cast<const VkPhysicalDeviceMemoryProperties&>(instance.GetMemoryProperties()),
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, reqs.memoryTypeBits, &arena_memory_type_index);
    ASSERT_MSG(found_type, "No device-local memory for the buffer arenas");

    const u64 bda_pagetable_size =
        (blocks_per_arena_page * NUM_ARENA_PAGES) * sizeof(VkDeviceAddress);
    fault_manager = new FaultManager(instance, scheduler, *this, block_shift,
                                     blocks_per_arena_page * NUM_ARENA_PAGES);
    bda_pagetable_buffer = new Buffer(instance, 0, bda_pagetable_size, MemoryType::DeviceLocal,
                                      "BDA Page Table Buffer");
    runtime.FillBuffer(bda_pagetable_buffer, 0u, bda_pagetable_size, 0u);

    // Room for the binds of a submit, and the first chunk of memory residency is made of, up
    // front: neither is allocated as the game first draws
    pending_binds = static_cast<ArenaBinds*>(calloc(PENDING_ARENAS_ROOM, sizeof(ArenaBinds)));
    ASSERT_MSG(pending_binds, "Out of memory for the arenas' binds");
    pending_binds_room = PENDING_ARENAS_ROOM;
    for (u32 i = 0; i < pending_binds_room; ++i) {
        pending_binds[i].binds = static_cast<VkSparseMemoryBind*>(
            malloc(PENDING_BINDS_ROOM * sizeof(VkSparseMemoryBind)));
        ASSERT_MSG(pending_binds[i].binds, "Out of memory for the arenas' binds");
        pending_binds[i].binds_room = PENDING_BINDS_ROOM;
    }
    buffer_binds = static_cast<VkSparseBufferMemoryBindInfo*>(
        malloc(PENDING_ARENAS_ROOM * sizeof(VkSparseBufferMemoryBindInfo)));
    ASSERT_MSG(buffer_binds, "Out of memory for the arenas' binds");
    buffer_binds_room = PENDING_ARENAS_ROOM;
    if (!dense_arenas) {
        u64 first = 0;
        (void)BackingMemory(0, &first);
    }

    if (!instance.IsMaintenance6Supported()) {
        null_index_buffer = new Buffer(instance, 0, NULL_INDEX_BUFFER_SIZE, MemoryType::DeviceLocal,
                                       "Null Index Buffer");
        runtime.FillBuffer(null_index_buffer, 0u, NULL_INDEX_BUFFER_SIZE, 0u);
    }

    // The game's memory lent to the GPU, for readbacks to land in (guest_import.h): as a
    // readback first goes into each chunk of it, or as the game maps it, where there are
    // readbacks to land (so that nothing is kept in RAM for none)
    retro_eventcount_init(&writebacks_ec);

    const u32 import_mode = emu_settings_get_readback_import();
    const bool readbacks = emu_settings_get_readbacks_mode() != EMU_READBACKS_DISABLED;
    if (instance.IsExternalMemoryHostSupported() && import_mode != EMU_READBACK_IMPORT_DISABLED) {
        guest_import_info_t info;
        instance.FillHostImport(&info, memory->BackingBase(), memory->BackingBytes(),
                                VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        info.import_on_use = import_mode == EMU_READBACK_IMPORT_ON_USE;
        guest = guest_import_create(&info);
        if (guest) {
            num_guest_chunks = static_cast<u32>(
                (info.size + (u64{1} << GUEST_IMPORT_CHUNK_SHIFT) - 1) >> GUEST_IMPORT_CHUNK_SHIFT);
            guest_chunks = static_cast<Buffer**>(calloc(num_guest_chunks, sizeof(Buffer*)));
            ASSERT_MSG(guest_chunks, "Out of memory for the game memory's chunks");
            guest_at_map = import_mode == EMU_READBACK_IMPORT_AT_MAP && readbacks;
        }
    }
}

BufferCache::~BufferCache() {
    retro_eventcount_free(&writebacks_ec);
    delete memory_tracker;
    memory_tracker = nullptr;
    write_snapshots_destroy(snapshots);
    // The Buffers only borrow the chunks' handles
    for (u32 i = 0; i < num_guest_chunks; ++i) {
        delete guest_chunks[i];
    }
    free(guest_chunks);
    guest_import_destroy(guest);
    free(buffer_binds);
    for (u32 i = 0; i < pending_binds_room; ++i) {
        free(pending_binds[i].binds);
    }
    free(pending_binds);
    for (u32 i = 0; i < num_arenas; ++i) {
        delete arenas[i];
    }
    free(arenas);
    free(address_space);
    delete bda_pagetable_buffer;
    delete fault_manager;
    delete null_index_buffer;
}

BufferCache::ArenaBinds* BufferCache::BindsForArena(const Buffer* arena) {
    for (u32 i = 0; i < num_pending_binds; ++i) {
        if (pending_binds[i].arena == arena) {
            return &pending_binds[i];
        }
    }
    if (num_pending_binds == pending_binds_room) {
        const u32 room = pending_binds_room * 2;
        ArenaBinds* const grown =
            static_cast<ArenaBinds*>(realloc(pending_binds, room * sizeof(ArenaBinds)));
        ASSERT_MSG(grown, "Out of memory for the arenas' binds");
        memset(grown + pending_binds_room, 0, (room - pending_binds_room) * sizeof(ArenaBinds));
        pending_binds = grown;
        pending_binds_room = room;
    }
    ArenaBinds* const slot = &pending_binds[num_pending_binds++];
    slot->arena = arena;
    slot->num_binds = 0;
    return slot;
}

void BufferCache::AddBind(ArenaBinds* binds, const VkSparseMemoryBind& bind) {
    if (binds->num_binds == binds->binds_room) {
        const u32 room = binds->binds_room ? binds->binds_room * 2 : PENDING_BINDS_ROOM;
        VkSparseMemoryBind* const grown = static_cast<VkSparseMemoryBind*>(
            realloc(binds->binds, room * sizeof(VkSparseMemoryBind)));
        ASSERT_MSG(grown, "Out of memory for the arenas' binds");
        binds->binds = grown;
        binds->binds_room = room;
    }
    binds->binds[binds->num_binds++] = bind;
}

Buffer* BufferCache::NewArena(VAddr cpu_addr, u64 size) {
    if (num_arenas == arenas_room) {
        const u32 room = arenas_room ? arenas_room * 2 : 16;
        Buffer** const grown = static_cast<Buffer**>(realloc(arenas, room * sizeof(Buffer*)));
        ASSERT_MSG(grown, "Out of memory for the arenas");
        arenas = grown;
        arenas_room = room;
    }
    Buffer* const arena = new Buffer(instance, cpu_addr, size, MemoryType::Sparse);
    arenas[num_arenas++] = arena;
    return arena;
}

/// The guest_import_run_fn over the memory manager's backing
static u64 GuestRun(void* ctx, u64 addr, u64 size, u64* backing_offset) {
    return static_cast<const Core::MemoryManager*>(ctx)->BackingRun(addr, size, backing_offset);
}

void BufferCache::MapGuestMemory(VAddr device_addr, u64 size) {
    if (!guest_at_map) {
        return;
    }
    while (size) {
        u64 offset = 0;
        const u64 run = memory->BackingRun(device_addr, size, &offset);
        if (run == 0) {
            return;
        }
        if (offset != ~u64{0}) {
            guest_import_prepare(guest, offset, run);
        }
        device_addr += run;
        size -= run;
    }
}

const Buffer* BufferCache::GuestChunk(u32 chunk, VkBuffer handle) {
    Buffer*& slot = guest_chunks[chunk];
    if (!slot) {
        constexpr u64 chunk_size = u64{1} << GUEST_IMPORT_CHUNK_SHIFT;
        const u64 start = u64{chunk} << GUEST_IMPORT_CHUNK_SHIFT;
        slot = new Buffer(instance, handle, MinU64(chunk_size, memory->BackingBytes() - start),
                          "Game memory");
    }
    return slot;
}

const Buffer* BufferCache::GuestTarget(VAddr device_addr, u64 size, u64 alignment,
                                       u64* out_offset) {
    u32 chunk = 0;
    VkDeviceSize offset = 0;
    const VkBuffer handle =
        guest_import_target(guest, GuestRun, memory, device_addr, size, alignment, &chunk, &offset);
    if (handle == VK_NULL_HANDLE) {
        *out_offset = 0;
        return nullptr;
    }
    *out_offset = offset;
    return GuestChunk(chunk, handle);
}

void BufferCache::OnGuestPiece(void* ctx, VkBuffer buffer, u32 chunk, VkDeviceSize buffer_offset,
                               u64 addr, u64 delta, u64 size) {
    BufferCache* const self = static_cast<BufferCache*>(ctx);
    const u64 src_offset = self->guest_src_offset + delta;
    if (buffer == VK_NULL_HANDLE) {
        self->guest_rest.push_back({src_offset, addr, size});
        return;
    }
    const Buffer* const target = self->GuestChunk(chunk, buffer);
    if (target != self->guest_batch) {
        self->FlushGuestCopies();
        self->guest_batch = target;
    }
    self->guest_copies.push_back({src_offset, buffer_offset, size});
}

void BufferCache::FlushGuestCopies() {
    if (guest_batch && !guest_copies.empty()) {
        runtime.CopyBuffer(guest_src, guest_batch, guest_copies.data(),
                           static_cast<u32>(guest_copies.size()));
    }
    guest_copies.clear();
}

template <typename Copies>
void BufferCache::CopyToGuest(const Buffer* src, Copies& copies) {
    if (!guest) {
        return;
    }
    guest_src = src;
    guest_batch = nullptr;
    guest_rest.clear();
    for (const VkBufferCopy& copy : copies) {
        guest_src_offset = copy.srcOffset;
        guest_import_split(guest, GuestRun, memory, copy.dstOffset, copy.size, OnGuestPiece, this);
    }
    FlushGuestCopies();
    copies.clear();
    for (const VkBufferCopy& rest : guest_rest) {
        copies.push_back(rest);
    }
}

int BufferCache::BeginWriteback(VAddr addr, u64 size) {
    for (int i = 0; i < WRITEBACK_SLOTS; i++) {
        Writeback& w = writebacks[i];
        if (retro_atomic_load_acquire_int(&w.live)) {
            continue;
        }
        w.addr = addr;
        w.end = addr + size;
        w.tick = scheduler.CurrentTick();
        retro_atomic_fetch_add_int(&writebacks_live, 1);
        retro_atomic_store_release_int(&w.live, 1);
        return i;
    }
    // As many landing as there is room for: these go first
    WaitWritebacksHere(0, ~u64{0});
    return BeginWriteback(addr, size);
}

void BufferCache::EndWriteback(int slot) {
    if (slot < 0) {
        return;
    }
    retro_atomic_store_release_int(&writebacks[slot].live, 0);
    retro_atomic_fetch_add_int(&writebacks_live, -1);
    retro_eventcount_notify(&writebacks_ec);
}

void BufferCache::WaitWritebacksHere(VAddr addr, u64 size) {
    const VAddr end = size > ~u64{0} - addr ? ~u64{0} : addr + size;
    for (int i = 0; i < WRITEBACK_SLOTS; i++) {
        Writeback& w = writebacks[i];
        if (!retro_atomic_load_acquire_int(&w.live) || w.end <= addr || w.addr >= end) {
            continue;
        }
        // The GPU done with it, then what lands it after: the deferred operations run here,
        // the priority ones on their own thread, waited for until it is ended
        scheduler.Wait(w.tick);
        scheduler.PopPendingOperations();
        if (retro_atomic_load_acquire_int(&w.live)) {
            // Deferred at a later tick (a submit in between): all of it, then
            scheduler.Finish();
            scheduler.PopPendingOperations();
        }
        for (;;) {
            const int key = retro_eventcount_prepare_wait(&writebacks_ec);
            if (!retro_atomic_load_acquire_int(&w.live)) {
                retro_eventcount_cancel_wait(&writebacks_ec);
                break;
            }
            retro_eventcount_commit_wait(&writebacks_ec, key);
        }
    }
}

void BufferCache::WaitWritebacks(VAddr addr, u64 size, bool on_gpu_thread) {
    if (retro_atomic_load_acquire_int(&writebacks_live) == 0) {
        return;
    }
    if (on_gpu_thread) {
        WaitWritebacksHere(addr, size);
    } else {
        liverpool->SendCommand<true>([this, addr, size] { WaitWritebacksHere(addr, size); });
    }
}

void BufferCache::TickFrame() {
    if (fault_process_pending) {
        fault_process_pending = false;
        fault_manager->ProcessFaultBuffer();
    }
}

void BufferCache::InvalidateMemory(VAddr device_addr, u64 size, bool assume_locks) {
    memory_tracker->InvalidateRegion(device_addr, size, [this, device_addr, size, assume_locks] {
        ReadMemory(device_addr, size, true, false, assume_locks);
    });
}

void BufferCache::UnmarkRegionAsGpuModified(VAddr device_addr, u64 size, bool is_write) {
    memory_tracker->UnmarkRegionAsGpuModified(device_addr, size, is_write);
    gpu_modified_ranges.Subtract(device_addr, size);
}

void BufferCache::FlushRegion(VAddr device_addr, u64 size, bool is_write, bool async) {
    if (!async) {
        // The CPU goes on in it after this: an earlier asynchronous read back into it lands
        // first, not on what the game writes next
        WaitWritebacksHere(device_addr, size);
    }
    const u32 first_block = device_addr >> block_shift;
    const u32 last_block = (device_addr + size - 1) >> block_shift;
    const Buffer* arena = GetArena(first_block, last_block);

    // GPU-modified ranges come as many small scattered islands,
    // so the download is widened to a window around the request
    constexpr u64 WindowSize = 512_KB;
    const VAddr arena_end = arena->cpu_addr + arena->size_bytes;
    const VAddr window_start = MaxU64(Common::AlignDown(device_addr, WindowSize), arena->cpu_addr);
    const VAddr window_end =
        MinU64(MaxU64(window_start + WindowSize, device_addr + size), arena_end);
    DownloadMemory(arena, window_start, window_end - window_start, async);
    if (is_write) {
        memory_tracker->MarkRegionAsCpuModified(device_addr, size);
    }
}

void BufferCache::ReadMemory(VAddr device_addr, u64 size, bool is_write, bool async,
                             bool assume_locks) {
    if (assume_locks) {
        FlushRegion(device_addr, size, is_write, async);
    } else {
        liverpool->SendCommand<true>([this, device_addr, size, is_write, async] {
            FlushRegion(device_addr, size, is_write, async);
        });
    }
}

void BufferCache::DownloadMemory(const Buffer* arena, VAddr device_addr, u64 size, bool async) {
    // Each copy first with where in the game's memory it goes as its destination
    Common::SmallVector<VkBufferCopy, 1> copies;
    const VAddr arena_base = arena->cpu_addr;
    memory_tracker->ForEachDownloadRange<false>(device_addr, size, [&](u64 address, u64 size) {
        gpu_modified_ranges.ForEachInRange(address, size, [&](VAddr start, VAddr end) {
            copies.push_back(VkBufferCopy{start - arena_base, start, end - start});
        });
        gpu_modified_ranges.Subtract(address, size);
    });
    if (copies.empty()) {
        return;
    }
    const int writeback = async ? BeginWriteback(device_addr, size) : -1;
    // Straight into the game's memory where it is lent to the GPU, the rest through staging
    CopyToGuest(arena, copies);
    u64 total_size_bytes = 0;
    for (VkBufferCopy& copy : copies) {
        copy.dstOffset = total_size_bytes;
        // Align up to avoid cache conflicts
        constexpr u64 align = 64ULL;
        constexpr u64 mask = ~(align - 1ULL);
        total_size_bytes += (copy.size + align - 1) & mask;
    }
    Vulkan::StagingBufferRef download{};
    if (total_size_bytes != 0) {
        download =
            staging_pool.Request(total_size_bytes, VideoCore::MemoryType::HostCached, 16, async);
        for (VkBufferCopy& copy : copies) {
            copy.dstOffset += download.offset;
        }
        runtime.CopyBuffer(arena, download.buffer, copies.data(), static_cast<u32>(copies.size()));
    }

    if (!async) {
        scheduler.Finish();
        LandDownload(arena_base, device_addr, size, download, false, writeback, copies.data(),
                     static_cast<u32>(copies.size()));
        return;
    }
    scheduler.DeferOperation([this, arena_base, device_addr, size, download, writeback,
                              copies = static_cast<decltype(copies)&&>(copies)] {
        LandDownload(arena_base, device_addr, size, download, true, writeback, copies.data(),
                     static_cast<u32>(copies.size()));
    });
}

void BufferCache::LandDownload(VAddr arena_base, VAddr device_addr, u64 size,
                               const Vulkan::StagingBufferRef& download, bool async, int writeback,
                               const VkBufferCopy* copies, u32 num_copies) {
    if (download.buffer) {
        download.Invalidate();
        for (u32 i = 0; i < num_copies; ++i) {
            const VkBufferCopy& copy = copies[i];
            u8* const dst_addr = reinterpret_cast<u8*>(arena_base + copy.srcOffset);
            memory->TryWriteBacking(dst_addr, download.mapped + (copy.dstOffset - download.offset),
                                    copy.size);
        }
    }
    memory_tracker->UnmarkRegionAsGpuModified(device_addr, size, false);

    if (async && download.buffer) {
        staging_pool.FreeDeferred(download);
    }
    EndWriteback(writeback);
}

const Buffer* BufferCache::ObtainBuffer(VAddr device_addr, u32 size, u64* out_offset,
                                        ObtainBufferFlags flags) {
    const bool is_written = True(flags & ObtainBufferFlags::IsWritten);
    const bool is_texel_buffer = True(flags & ObtainBufferFlags::IsTexelBuffer);
    const bool skip_stream_buffer = True(flags & ObtainBufferFlags::IgnoreStreamBuffer);
    // For read-only buffers use device local stream buffer to reduce renderpass breaks.
    if (!is_written && !skip_stream_buffer && size <= STREAM_THRESHOLD &&
        !IsRegionGpuModified(device_addr, size)) {
        u64 offset = 0;
        u8* const data = stream_buffer.Map(size, &offset, instance.UniformMinAlignment());
        memory->CopySparseMemory(device_addr, data, size);
        stream_buffer.Commit();
        *out_offset = offset;
        return &stream_buffer;
    }
    const u64 first_block = device_addr >> block_shift;
    const u64 last_block = (device_addr + size - 1) >> block_shift;
    const Buffer* arena = GetArena(first_block, last_block);
    EnsureResident(arena, first_block, last_block);
    if (is_written) {
        texture_cache.MarkAsMaybeGpuDirty(device_addr, size);
    }
    SynchronizeMemory(arena, device_addr, size, is_written, is_texel_buffer);
    if (is_written) {
        gpu_modified_ranges.Add(device_addr, size);
    }
    *out_offset = arena->Offset(device_addr);
    return arena;
}

const Buffer* BufferCache::ObtainBufferForImage(VAddr device_addr, u32 size, u64* out_offset) {
    if (IsRegionGpuModified(device_addr, size)) {
        return ObtainBuffer(device_addr, size, out_offset);
    }
    const Vulkan::StagingBufferRef staging = staging_pool.Request(
        size, VideoCore::MemoryType::HostUncached, instance.StorageMinAlignment());
    memory->CopySparseMemory(device_addr, staging.mapped, staging.size);
    staging.Flush();
    *out_offset = staging.offset;
    return staging.buffer;
}

bool BufferCache::IsRegionCpuModified(VAddr addr, size_t size) {
    return memory_tracker->IsRegionCpuModified(addr, size);
}

bool BufferCache::IsRegionGpuModified(VAddr addr, size_t size) {
    return memory_tracker->IsRegionGpuModified(addr, size);
}

void BufferCache::MarkRegionAsGpuModified(VAddr addr, size_t size) {
    texture_cache.MarkAsMaybeGpuDirty(addr, size);
    memory_tracker->MarkRegionAsGpuModified(addr, size);
    gpu_modified_ranges.Add(addr, size);
}

void BufferCache::ReadEdgeImagePages(const Image& image) {
    // May happen that after downloading the image and invalidating region,
    // that there were GPU modified ranges that are lost due to CPU reuploading.
    // This doesn't change tracker state and it is spected to call DownloadImageMemory after this.
    const VAddr image_addr = image.info.guest_address;
    const u64 image_size = image.info.guest_size;
    const VAddr image_end = image_addr + image_size;
    const VAddr page_start = PageManager::GetPageAddr(image_addr);
    const VAddr page_end = PageManager::GetNextPageAddr(image_end - 1);
    // Each copy first with where in the game's memory it goes as its destination
    Common::SmallVector<VkBufferCopy, 2> copies;
    u64 offset = 0;
    const Buffer* buffer = ObtainBuffer(page_start, page_end - page_start, &offset);
    const VAddr buffer_base = buffer->CpuAddr();
    struct AddDownload {
        Common::SmallVector<VkBufferCopy, 2>& copies;
        VAddr base;
        void operator()(VAddr start, VAddr end) const {
            copies.push_back(VkBufferCopy{start - base, start, end - start});
        }
    } const add_download{copies, buffer_base};
    gpu_modified_ranges.ForEachInRange(page_start, image_addr - page_start, add_download);
    gpu_modified_ranges.ForEachInRange(image_end, page_end - image_end, add_download);
    gpu_modified_ranges.Subtract(page_start, page_end - page_start);
    if (copies.empty()) {
        return;
    }
    scheduler.EndRendering();
    const int writeback = BeginWriteback(page_start, page_end - page_start);
    // Straight into the game's memory where it is lent to the GPU, the rest through staging
    CopyToGuest(buffer, copies);
    u64 total_size_bytes = 0;
    for (VkBufferCopy& copy : copies) {
        copy.dstOffset = total_size_bytes;
        // Align up to avoid cache conflicts
        constexpr u64 align = 64ULL;
        constexpr u64 mask = ~(align - 1ULL);
        total_size_bytes += (copy.size + align - 1) & mask;
    }
    if (total_size_bytes == 0) {
        // All of it straight into the game's memory: landed when the GPU is done
        scheduler.DeferOperation([this, writeback] { EndWriteback(writeback); });
        return;
    }
    const Vulkan::StagingBufferRef download =
        staging_pool.Request(total_size_bytes, VideoCore::MemoryType::HostCached, 16, true);
    for (VkBufferCopy& copy : copies) {
        // Modify copies to have the staging offset in mind
        copy.dstOffset += download.offset;
    }
    runtime.CopyBuffer(buffer, download.buffer, copies.data(), static_cast<u32>(copies.size()));
    scheduler.DeferOperation([this, base = buffer_base, download, writeback,
                              copies = static_cast<decltype(copies)&&>(copies)]() {
        download.Invalidate();
        Core::MemoryManager* const memory = Core::Memory::Instance();
        for (const VkBufferCopy& copy : copies) {
            const VAddr copy_device_addr = base + copy.srcOffset;
            const u64 dst_offset = copy.dstOffset - download.offset;
            memory->TryWriteBacking(reinterpret_cast<u8*>(copy_device_addr),
                                    download.mapped + dst_offset, copy.size);
        }
        staging_pool.FreeDeferred(download);
        EndWriteback(writeback);
    });
}

void BufferCache::SynchronizeDmaBuffers() {
    fault_process_pending = true;
    for (const Backing& range : resident_ranges) {
        const u64 page = range.start >> (ARENA_PAGE_BITS - block_shift);
        const VAddr device_addr = range.start << block_shift;
        const u64 size = (range.end - range.start) << block_shift;
        SynchronizeMemory(address_space[page], device_addr, size, false, false);
    }
}

const Buffer* BufferCache::GetArena(u64 first_block, u64 last_block) {
    const u64 first_page = first_block >> blocks_per_arena_page_shift;
    const u64 last_page = last_block >> blocks_per_arena_page_shift;
    // With small arena pages one buffer can cover several of them: one arena over all of
    // them, as long as none has an arena yet.
    if (last_page - first_page > 1) {
        bool all_free = true;
        for (u64 page = first_page; page <= last_page; ++page) {
            all_free &= address_space[page] == nullptr;
        }
        ASSERT_MSG(all_free, "Buffer request spans %" PRIu64 " VA arena pages, some of them in use",
                   last_page - first_page + 1);
        const u64 base_block = Common::AlignDownPow2<u64>(first_block, blocks_per_arena_page);
        const u64 num_pages = last_page - first_page + 1;
        const Buffer* new_arena = NewArena(base_block << block_shift, num_pages << ARENA_PAGE_BITS);
        for (u64 page = first_page; page <= last_page; ++page) {
            address_space[page] = new_arena;
        }
        return new_arena;
    }

    const Buffer* first_arena = address_space[first_page];
    const Buffer* last_arena = address_space[last_page];
    if (first_arena == last_arena) {
        if (!first_arena) {
            const u64 base_block = Common::AlignDownPow2<u64>(first_block, blocks_per_arena_page);
            const u64 num_pages = last_page - first_page + 1;
            const Buffer* new_arena =
                NewArena(base_block << block_shift, num_pages << ARENA_PAGE_BITS);
            address_space[first_page] = new_arena;
            address_space[last_page] = new_arena;
        }
        return address_space[first_page];
    }

    LOG_WARNING(Render, "Migrating arena");

    const u64 first_addr = first_arena ? first_arena->cpu_addr : (first_page << ARENA_PAGE_BITS);
    const u64 first_size = first_arena ? first_arena->size_bytes : ARENA_PAGE_SIZE;
    const u64 last_size = last_arena ? last_arena->size_bytes : ARENA_PAGE_SIZE;

    const u64 base_block = first_addr >> block_shift;
    const u64 total_size = first_size + last_size;
    const u64 end_block = (first_addr + total_size) >> block_shift;
    Buffer* new_arena = NewArena(first_addr, total_size);
    if (dense_arenas) {
        MigrateDenseArena(first_arena, new_arena);
        MigrateDenseArena(last_arena, new_arena);
    }
    ArenaBinds* bind = dense_arenas ? nullptr : BindsForArena(new_arena);
    resident_ranges.ForEachInRange(base_block, end_block, [&](const Backing& backing) {
        if (!bind) {
            return;
        }
        const u64 start = MaxU64(base_block, backing.start);
        const u64 end = MinU64(end_block, backing.end);
        VkSparseMemoryBind sparse{};
        sparse.resourceOffset = (start - base_block) << block_shift;
        sparse.size = (end - start) << block_shift;
        sparse.memory = backing.memory;
        sparse.memoryOffset = (backing.offset + start - backing.start) << block_shift;
        AddBind(bind, sparse);
    });

    u64 base_page = first_addr >> ARENA_PAGE_BITS;
    for (u32 page = 0; page < (first_size >> ARENA_PAGE_BITS); ++page) {
        address_space[base_page + page] = new_arena;
    }
    base_page = last_page;
    for (u32 page = 0; page < (last_size >> ARENA_PAGE_BITS); ++page) {
        address_space[base_page + page] = new_arena;
    }
    return new_arena;
}

/// Device memory of @n blocks of @block_shift, with device addresses; null when out of it
static VkDeviceMemory AllocateBlocks(VkDevice device, u32 memory_type_index, u32 block_shift,
                                     u64 n) {
    VkMemoryAllocateFlagsInfo alloc_flags{};
    alloc_flags.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    alloc_flags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.pNext = &alloc_flags;
    alloc_info.allocationSize = n << block_shift;
    alloc_info.memoryTypeIndex = memory_type_index;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    if (vkAllocateMemory(device, &alloc_info, nullptr, &memory) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }
    return memory;
}

VkDeviceMemory BufferCache::BackingMemory(u64 blocks, u64* out_first) {
    *out_first = 0;
    if (backing_chunk != VK_NULL_HANDLE && blocks <= backing_chunk_blocks - backing_chunk_used) {
        *out_first = backing_chunk_used;
        backing_chunk_used += blocks;
        return backing_chunk;
    }
    // A chunk is a 64th of the device's memory, from 16 MiB to 128 MiB, in whole blocks
    const u64 budget = instance.GetTotalMemoryBudget();
    const u64 chunk_bytes = Common::AlignUp(ClampU64(budget / 64, 16_MB, 128_MB), u64{block_size});
    const u64 chunk_blocks = chunk_bytes >> block_shift;
    const VkDevice device = static_cast<VkDevice>(instance.GetDevice());
    if (blocks > chunk_blocks / 2) {
        // More than half a chunk: memory of its own, and the chunk keeps what it has left
        const VkDeviceMemory memory =
            AllocateBlocks(device, arena_memory_type_index, block_shift, blocks);
        ASSERT_MSG(memory != VK_NULL_HANDLE,
                   "Out of device memory for %" PRIu64 " blocks of buffer residency", blocks);
        return memory;
    }
    VkDeviceMemory chunk =
        AllocateBlocks(device, arena_memory_type_index, block_shift, chunk_blocks);
    u64 got = chunk_blocks;
    if (chunk == VK_NULL_HANDLE && blocks > 0) {
        // No room for a whole chunk: only what is asked for
        chunk = AllocateBlocks(device, arena_memory_type_index, block_shift, blocks);
        got = blocks;
    }
    ASSERT_MSG(chunk != VK_NULL_HANDLE || blocks == 0,
               "Out of device memory for %" PRIu64 " blocks of buffer residency", blocks);
    if (chunk == VK_NULL_HANDLE) {
        return VK_NULL_HANDLE;
    }
    backing_chunk = chunk;
    backing_chunk_blocks = got;
    backing_chunk_used = blocks;
    return chunk;
}

void BufferCache::EnsureResident(const Buffer* arena, u64 first_block, u64 last_block) {
    u32 resident_blocks{};
    IntervalList bind_ranges;
    resident_ranges.ForEachGap(first_block, last_block + 1, [&](u64 start, u64 end) {
        resident_blocks += end - start;
        bind_ranges.Add({start, end});
    });

    if (bind_ranges.Empty()) {
        return;
    }
    if (dense_arenas) {
        // The arena's own memory backs it already: the blocks are only marked resident and
        // given their addresses
        Common::SmallVector<Interval, 4> ranges;
        for (const Interval& range : bind_ranges) {
            resident_ranges.Add(Backing{{range.start, range.end}, VK_NULL_HANDLE, range.start});
            ranges.push_back({range.start, range.end});
        }
        WriteArenaPageTable(arena, ranges.data(), static_cast<u32>(ranges.size()));
        return;
    }

    u64 first_backing_block = 0;
    const VkDeviceMemory device_memory = BackingMemory(resident_blocks, &first_backing_block);

    Common::SmallVector<VkBufferCopy, 8> copies;
    const Vulkan::StagingBufferRef staging =
        staging_pool.Request(resident_blocks * sizeof(VkDeviceAddress), MemoryType::HostUncached);

    u64 memory_offset = first_backing_block << block_shift;
    ArenaBinds* binds = BindsForArena(arena);
    VkDeviceAddress* bda_addrs = reinterpret_cast<VkDeviceAddress*>(staging.mapped);
    u64 offset = staging.offset;
    for (const Interval& range : bind_ranges) {
        Backing backing;
        backing.start = range.start;
        backing.end = range.end;
        backing.memory = device_memory;
        backing.offset = memory_offset >> block_shift;
        resident_ranges.Add(backing);

        LOG_INFO(Render, "Making range start=%" PRIu64 ", end=%" PRIu64 " resident", backing.start,
                 backing.end);

        VkSparseMemoryBind bind{};
        bind.resourceOffset = (range.start << block_shift) - arena->cpu_addr;
        bind.size = (range.end - range.start) << block_shift;
        bind.memory = device_memory;
        bind.memoryOffset = memory_offset;
        AddBind(binds, bind);
        memory_offset += bind.size;

        for (u32 block = 0; block < bind.size; block += block_size) {
            *(bda_addrs++) = arena->BufferDeviceAddress() + bind.resourceOffset + block;
        }
        const u64 copy_size = (backing.end - backing.start) * sizeof(VkDeviceAddress);
        copies.push_back(VkBufferCopy{offset, backing.start * sizeof(VkDeviceAddress), copy_size});
        offset += copy_size;
    }

    staging.Flush();
    runtime.CopyBuffer(staging.buffer, bda_pagetable_buffer, copies.data(),
                       static_cast<u32>(copies.size()));
}

bool BufferCache::SynchronizeMemory(const Buffer* arena, VAddr device_addr, u32 size,
                                    bool is_written, bool is_texel_buffer) {
    Common::SmallVector<VkBufferCopy, 4> copies;
    size_t total_size_bytes{};
    /// Uploads [addr, addr + size) of the game's memory, through the staging buffer
    struct Upload {
        Common::SmallVector<VkBufferCopy, 4>& copies;
        size_t& total_size_bytes;
        void operator()(u64 addr, u64 size) const {
            copies.push_back(VkBufferCopy{total_size_bytes, addr, size});
            total_size_bytes += size;
        }
    } const upload{copies, total_size_bytes};
    struct Range {
        VAddr addr;
        u64 size;
        u64 cutoff;
    };
    Common::SmallVector<Range, 4> ranges;
    memory_tracker->ForEachUploadRange(device_addr, size, is_written, [&](u64 addr, u64 size) {
        if (!snapshots) {
            upload(addr, size);
            return;
        }
        // Read where the pages are being tracked again: a snapshot older than this is of a
        // write before that, one newer is for the pages' next upload
        ranges.push_back(Range{addr, size, write_snapshots_cutoff(snapshots)});
    });
    // Readbacks off: the bytes the GPU wrote are only in the arena, and guest memory has stale
    // ones beneath them. A page the guest wrote to elsewhere is uploaded around them; of the
    // GPU's bytes, only those the guest wrote since the page was kept (write_snapshots.h: the
    // ones it changed, and short runs it left as they were between them) are, so the rest
    // stay as the GPU left them. The pages are tracked by now: a write from
    // here on faults and is for the next upload.
    for (const Range& range : ranges) {
        VAddr done = range.addr;
        gpu_modified_ranges.ForEachInRange(
            range.addr, range.size, [&](VAddr gpu_start, VAddr gpu_end) {
                if (gpu_start > done) {
                    upload(done, gpu_start - done);
                }
                done = gpu_end;
                for (VAddr page = Common::AlignDown(gpu_start, u64{WRITE_SNAPSHOT_PAGE});
                     page < gpu_end; page += WRITE_SNAPSHOT_PAGE) {
                    const VAddr start = MaxU64(gpu_start, page);
                    const VAddr end = MinU64(gpu_end, page + WRITE_SNAPSHOT_PAGE);
                    u32 slot;
                    const u8* before = write_snapshots_claim(snapshots, page, range.cutoff, &slot);
                    if (!before) {
                        // Nothing to tell them apart by: the guest's, as it always was
                        upload(start, end - start);
                        continue;
                    }
                    // The page as it is now, read where it lies in the game's memory; copied
                    // out only where it does not lie in one run of the backing
                    u8 copied[WRITE_SNAPSHOT_PAGE];
                    const u8* now = copied;
                    u64 backing_offset = 0;
                    if (memory->BackingRun(page, WRITE_SNAPSHOT_PAGE, &backing_offset) ==
                            WRITE_SNAPSHOT_PAGE &&
                        backing_offset != ~u64{0}) {
                        now = memory->BackingBase() + backing_offset;
                    } else {
                        memory->CopySparseMemory(page, copied, sizeof(copied));
                    }
                    struct Runs {
                        const Upload& upload;
                        VAddr page;
                    } runs{upload, page};
                    // Cut by the page, the GPU's range goes on into the one before or after
                    write_snapshots_written(
                        now, before, static_cast<u32>(start - page), static_cast<u32>(end - page),
                        start == page && gpu_start < page,
                        end == page + WRITE_SNAPSHOT_PAGE && gpu_end > end,
                        [](void* ctx, u32 begin, u32 size) {
                            Runs* runs = static_cast<Runs*>(ctx);
                            runs->upload(runs->page + begin, size);
                        },
                        &runs);
                    write_snapshots_release(snapshots, slot);
                }
            });
        if (done < range.addr + range.size) {
            upload(done, range.addr + range.size - done);
        }
    }
    if (!copies.empty()) {
        const Vulkan::StagingBufferRef staging =
            staging_pool.Request(total_size_bytes, MemoryType::HostUncached);
        for (VkBufferCopy& copy : copies) {
            memory->CopySparseMemory(copy.dstOffset, staging.mapped + copy.srcOffset, copy.size);
            copy.srcOffset += staging.offset;
            copy.dstOffset -= arena->cpu_addr;
        }
        staging.Flush();
        runtime.CopyBuffer(staging.buffer, arena, copies.data(), static_cast<u32>(copies.size()));
    }
    if (is_texel_buffer && !is_written) {
        return SynchronizeMemoryFromImage(arena, device_addr, size);
    }
    return false;
}

bool BufferCache::SynchronizeMemoryFromImage(const Buffer* arena, VAddr device_addr, u32 size) {
    // The texture cache's own optional, until it converts
    const std::optional<TextureCache::MetaDataInfo> meta = texture_cache.GetMetaInfo(device_addr);
    if (meta) {
        if (meta->type == TextureCache::MetaType::HTile) {
            static constexpr u32 ZmaskUncompressed = 0xf;
            runtime.FillBuffer(arena, arena->Offset(device_addr), size, ZmaskUncompressed);
            return true;
        } else if (meta->type == TextureCache::MetaType::CMask) {
            // An unsupported sample count expands to 0, as it always has
            const u32 expanded_value = CmaskColorExpandedValue(meta->num_samples);
            runtime.FillBuffer(arena, arena->Offset(device_addr), size, expanded_value);
            return true;
        } else {
            LOG_WARNING(Render_Vulkan, "Unhandled metadata type %s",
                        enum_name(&enum_names_VideoCore_TextureCache_MetaType,
                                  static_cast<s64>(meta->type)));
        }
    }
    const ImageId image_id = texture_cache.FindImageFromRange(device_addr, size);
    if (!image_id) {
        return false;
    }
    Image& image = texture_cache.GetImage(image_id);
    if (image.IsScaled()) {
        // Render Resolution: smaller than the guest's memory of it, so not copied there
        return false;
    }
    ASSERT_MSG(device_addr == image.info.guest_address,
               "Texel buffer aliases image subresources %" PRIxPTR " : %" PRIxPTR, device_addr,
               image.info.guest_address);
    const u64 arena_offset = arena->Offset(device_addr);
    // The tiler writes the whole image, which may be more than the range that was asked for:
    // an image that runs past the arena is left, and all of it is backed before it is written
    if (arena_offset + image.info.guest_size > arena->size_bytes) {
        return false;
    }
    EnsureResident(arena, device_addr >> block_shift,
                   (device_addr + image.info.guest_size - 1) >> block_shift);
    Common::SmallVector<vk::BufferImageCopy, 8> buffer_copies;
    for (u32 mip = 0; mip < image.info.resources.levels; mip++) {
        const MipInfo& mip_info = image.info.mips_layout[mip];
        const u32 width = static_cast<u32>(MaxU64(image.info.size.width >> mip, 1u));
        const u32 height = static_cast<u32>(MaxU64(image.info.size.height >> mip, 1u));
        const u32 depth = static_cast<u32>(MaxU64(image.info.size.depth >> mip, 1u));
        if (arena_offset + mip_info.offset + mip_info.size > arena->size_bytes) {
            break;
        }
        buffer_copies.push_back(vk::BufferImageCopy{
            .bufferOffset = mip_info.offset,
            .bufferRowLength = mip_info.pitch,
            .bufferImageHeight = mip_info.height,
            .imageSubresource{
                .aspectMask = image.aspect_mask & ~vk::ImageAspectFlagBits::eStencil,
                .mipLevel = mip,
                .baseArrayLayer = 0,
                .layerCount = image.info.resources.layers,
            },
            .imageOffset = {0, 0, 0},
            .imageExtent = {width, height, depth},
        });
    }
    if (buffer_copies.empty()) {
        return false;
    }
    TileManager& tile_manager = texture_cache.GetTileManager();
    tile_manager.TileImage(image, buffer_copies.data(), static_cast<u32>(buffer_copies.size()),
                           arena, arena_offset);
    return true;
}

void BufferCache::MigrateDenseArena(const Buffer* old_arena, const Buffer* new_arena) {
    if (!old_arena) {
        return;
    }
    // What the old arena holds is copied into the new one, and the page table sends the GPU's
    // accesses by address there
    const u64 base_block = old_arena->cpu_addr >> block_shift;
    const u64 end_block = (old_arena->cpu_addr + old_arena->size_bytes) >> block_shift;
    Common::SmallVector<VkBufferCopy, 8> copies;
    Common::SmallVector<Interval, 8> ranges;
    resident_ranges.ForEachInRange(base_block, end_block, [&](const Backing& backing) {
        const u64 start = MaxU64(base_block, backing.start);
        const u64 end = MinU64(end_block, backing.end);
        const VAddr addr = start << block_shift;
        copies.push_back(VkBufferCopy{old_arena->Offset(addr), new_arena->Offset(addr),
                                      (end - start) << block_shift});
        ranges.push_back({start, end});
    });
    if (copies.empty()) {
        return;
    }
    runtime.CopyBuffer(old_arena, new_arena, copies.data(), static_cast<u32>(copies.size()));
    WriteArenaPageTable(new_arena, ranges.data(), static_cast<u32>(ranges.size()));
}

void BufferCache::WriteArenaPageTable(const Buffer* arena, const Interval* ranges, u32 num_ranges) {
    u64 num_blocks{};
    for (u32 i = 0; i < num_ranges; ++i) {
        num_blocks += ranges[i].end - ranges[i].start;
    }
    const Vulkan::StagingBufferRef staging =
        staging_pool.Request(num_blocks * sizeof(VkDeviceAddress), MemoryType::HostUncached);
    Common::SmallVector<VkBufferCopy, 8> copies;
    VkDeviceAddress* bda_addrs = reinterpret_cast<VkDeviceAddress*>(staging.mapped);
    u64 offset = staging.offset;
    for (u32 i = 0; i < num_ranges; ++i) {
        const Interval& range = ranges[i];
        const u64 arena_offset = (range.start << block_shift) - arena->cpu_addr;
        for (u64 block = 0; block < range.end - range.start; ++block) {
            *(bda_addrs++) = arena->BufferDeviceAddress() + arena_offset + (block << block_shift);
        }
        const u64 copy_size = (range.end - range.start) * sizeof(VkDeviceAddress);
        copies.push_back(VkBufferCopy{offset, range.start * sizeof(VkDeviceAddress), copy_size});
        offset += copy_size;
    }
    staging.Flush();
    runtime.CopyBuffer(staging.buffer, bda_pagetable_buffer, copies.data(),
                       static_cast<u32>(copies.size()));
}

void BufferCache::SubmitPendingArenaBinds(Vulkan::SubmitInfo& info) {
    if (num_pending_binds == 0) {
        return;
    }

    if (num_pending_binds > buffer_binds_room) {
        VkSparseBufferMemoryBindInfo* const grown = static_cast<VkSparseBufferMemoryBindInfo*>(
            realloc(buffer_binds, num_pending_binds * sizeof(VkSparseBufferMemoryBindInfo)));
        ASSERT_MSG(grown, "Out of memory for the arenas' binds");
        buffer_binds = grown;
        buffer_binds_room = num_pending_binds;
    }
    for (u32 i = 0; i < num_pending_binds; ++i) {
        const ArenaBinds& binds = pending_binds[i];
        buffer_binds[i].buffer = static_cast<VkBuffer>(binds.arena->Handle());
        buffer_binds[i].bindCount = binds.num_binds;
        buffer_binds[i].pBinds = binds.binds;
    }

    const u64 signal_tick = memory_semaphore.NextTick();
    const VkSemaphore signal_sema = static_cast<VkSemaphore>(memory_semaphore.Handle());

    VkTimelineSemaphoreSubmitInfo timeline_si{};
    timeline_si.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
    timeline_si.signalSemaphoreValueCount = 1u;
    timeline_si.pSignalSemaphoreValues = &signal_tick;

    VkBindSparseInfo sparse_info{};
    sparse_info.sType = VK_STRUCTURE_TYPE_BIND_SPARSE_INFO;
    sparse_info.pNext = &timeline_si;
    sparse_info.bufferBindCount = num_pending_binds;
    sparse_info.pBufferBinds = buffer_binds;
    sparse_info.signalSemaphoreCount = 1u;
    sparse_info.pSignalSemaphores = &signal_sema;

    info.AddWait(vk::Semaphore{signal_sema}, signal_tick);
    const VkResult submit_result = vkQueueBindSparse(
        static_cast<VkQueue>(instance.GetGraphicsQueue()), 1, &sparse_info, VK_NULL_HANDLE);
    if (submit_result == VK_ERROR_DEVICE_LOST) {
        instance.ReportDeviceLost();
    }
    ASSERT_MSG(submit_result != VK_ERROR_DEVICE_LOST, "Device lost during submit");

    // The slots and their room stay for the next submit's binds
    num_pending_binds = 0;
}

} // namespace VideoCore
