// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <array>
#include <bit>
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
constexpr std::optional<u32> CmaskColorExpandedValue(u32 num_samples) {
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

static constexpr auto ARENA_USAGE =
    vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst |
    vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eStorageBuffer |
    vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eVertexBuffer |
    vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress;

std::optional<u32> FindMemoryType(const vk::PhysicalDeviceMemoryProperties& properties,
                                  vk::MemoryPropertyFlags wanted, u32 memory_type_bits) {
    for (u32 i = 0; i < properties.memoryTypeCount; ++i) {
        if (((memory_type_bits >> i) & 1) == 0) {
            continue;
        }
        const auto flags = properties.memoryTypes[i].propertyFlags;
        if ((flags & wanted) == wanted) {
            return i;
        }
    }
    return std::nullopt;
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
      memory_tracker{std::make_unique<MemoryTracker>(tracker, snapshots)},
      stream_buffer{instance, scheduler, MemoryType::Stream, STREAM_BUFFER_SIZE},
      gds_buffer{instance, 0, GDS_BUFFER_SIZE, MemoryType::Stream, "GDS Buffer"},
      memory_semaphore{instance} {
    vk::BufferCreateFlags sparse_flags =
        vk::BufferCreateFlagBits::eSparseBinding | vk::BufferCreateFlagBits::eSparseResidency;
    // An arena is a sparse buffer over 4 GiB of the guest's address space, and a game uses
    // several. A device whose sparse resources share less address space than a few of those
    // (Adreno 618 has 4 GiB in all) could not make the first one, so there the arenas are
    // smaller: a sixty-fourth of what the device has, no less than 64 MiB. And a device
    // without sparse residency is not asked for it: the ranges a draw uses are bound before
    // it (EnsureResident), only direct memory access reads outside them.
    if (!instance.IsSparseResidencyBufferSupported()) {
        sparse_flags = vk::BufferCreateFlagBits::eSparseBinding;
    }
    // A device without sparse buffers at all (Intel HD 615 under Mesa) has every arena backed
    // whole, so they are kept as small as with little sparse address space
    dense_arenas = !instance.IsSparseBindingSupported();
    const u64 sparse_space = instance.SparseAddressSpaceSize();
    if (dense_arenas) {
        sparse_flags = {};
        ARENA_PAGE_BITS = MIN_ARENA_PAGE_BITS;
        ARENA_PAGE_SIZE = u64{1} << ARENA_PAGE_BITS;
        NUM_ARENA_PAGES = u64{1} << (ADDRESS_SPACE_BITS - ARENA_PAGE_BITS);
        LOG_WARNING(Render, "The device has no sparse buffers: arenas of {} MiB backed whole",
                    ARENA_PAGE_SIZE >> 20);
    } else if (sparse_space != 0 && sparse_space < (u64{64} << DEFAULT_ARENA_PAGE_BITS)) {
        ARENA_PAGE_BITS = std::clamp<u64>(std::bit_width(sparse_space) - 1 - 6, MIN_ARENA_PAGE_BITS,
                                          DEFAULT_ARENA_PAGE_BITS);
        ARENA_PAGE_SIZE = u64{1} << ARENA_PAGE_BITS;
        NUM_ARENA_PAGES = u64{1} << (ADDRESS_SPACE_BITS - ARENA_PAGE_BITS);
        LOG_WARNING(Render,
                    "The device has {} MiB of sparse address space: arena pages of {} MiB, "
                    "sparse residency {}",
                    sparse_space >> 20, ARENA_PAGE_SIZE >> 20,
                    instance.IsSparseResidencyBufferSupported() ? "used" : "not available");
    }
    address_space.resize(NUM_ARENA_PAGES);
    const vk::BufferCreateInfo probe_ci = {
        .flags = sparse_flags,
        .size = ARENA_PAGE_SIZE,
        .usage = ARENA_USAGE,
        .sharingMode = vk::SharingMode::eExclusive,
    };
    const vk::DeviceBufferMemoryRequirements req_info = {
        .pCreateInfo = &probe_ci,
    };
    const auto device = instance.GetDevice();
    const auto reqs = device.getBufferMemoryRequirements(req_info).memoryRequirements;
    block_size = Common::AlignUp(std::max<u64>(reqs.alignment, MIN_BLOCK_SIZE), reqs.alignment);
    ASSERT_MSG(std::popcount(block_size) == 1, "Sparse block size {} is not a power of 2",
               block_size);
    block_shift = std::bit_width(block_size) - 1;
    blocks_per_arena_page = ARENA_PAGE_SIZE / block_size;
    blocks_per_arena_page_shift = ARENA_PAGE_BITS - block_shift;
    arena_memory_type_index =
        FindMemoryType(instance.GetMemoryProperties(), vk::MemoryPropertyFlagBits::eDeviceLocal,
                       reqs.memoryTypeBits)
            .value();

    const u64 bda_pagetable_size =
        (blocks_per_arena_page * NUM_ARENA_PAGES) * sizeof(vk::DeviceAddress);
    fault_manager = std::make_unique<FaultManager>(instance, scheduler, *this, block_shift,
                                                   blocks_per_arena_page * NUM_ARENA_PAGES);
    bda_pagetable_buffer = std::make_unique<Buffer>(
        instance, 0, bda_pagetable_size, MemoryType::DeviceLocal, "BDA Page Table Buffer");
    runtime.FillBuffer(bda_pagetable_buffer.get(), 0u, bda_pagetable_size, 0u);

    // Room for the binds of a submit, and the first chunk of memory residency is made of, up
    // front: neither is allocated as the game first draws
    pending_binds.resize(PENDING_ARENAS_ROOM);
    for (auto& slot : pending_binds) {
        slot.binds.reserve(PENDING_BINDS_ROOM);
    }
    buffer_binds.reserve(PENDING_ARENAS_ROOM);
    if (!dense_arenas) {
        (void)BackingMemory(0);
    }

    if (!instance.IsMaintenance6Supported()) {
        null_index_buffer = std::make_unique<Buffer>(instance, 0, NULL_INDEX_BUFFER_SIZE,
                                                     MemoryType::DeviceLocal, "Null Index Buffer");
        runtime.FillBuffer(null_index_buffer.get(), 0u, NULL_INDEX_BUFFER_SIZE, 0u);
    }

    // The game's memory lent to the GPU, for readbacks to land in (guest_import.h): as a
    // readback first goes into each chunk of it, or as the game maps it, where there are
    // readbacks to land (so that nothing is kept in RAM for none)
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
            guest_chunks.resize((info.size + (u64{1} << GUEST_IMPORT_CHUNK_SHIFT) - 1) >>
                                GUEST_IMPORT_CHUNK_SHIFT);
            guest_rest.reserve(64);
            guest_at_map = import_mode == EMU_READBACK_IMPORT_AT_MAP && readbacks;
        }
    }
}

BufferCache::~BufferCache() {
    memory_tracker.reset();
    write_snapshots_destroy(snapshots);
    // The Buffers only borrow the chunks' handles
    guest_chunks.clear();
    guest_import_destroy(guest);
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

const Buffer* BufferCache::GuestChunk(u32 chunk, vk::Buffer handle) {
    auto& slot = guest_chunks[chunk];
    if (!slot) {
        constexpr u64 chunk_size = u64{1} << GUEST_IMPORT_CHUNK_SHIFT;
        const u64 start = u64{chunk} << GUEST_IMPORT_CHUNK_SHIFT;
        slot.emplace(instance, handle, std::min(chunk_size, memory->BackingBytes() - start),
                     "Game memory");
    }
    return &*slot;
}

std::pair<const Buffer*, u64> BufferCache::GuestTarget(VAddr device_addr, u64 size, u64 alignment) {
    u32 chunk = 0;
    VkDeviceSize offset = 0;
    const VkBuffer handle =
        guest_import_target(guest, GuestRun, memory, device_addr, size, alignment, &chunk, &offset);
    if (handle == VK_NULL_HANDLE) {
        return {nullptr, 0};
    }
    return {GuestChunk(chunk, handle), offset};
}

void BufferCache::OnGuestPiece(void* ctx, VkBuffer buffer, u32 chunk, VkDeviceSize buffer_offset,
                               u64 addr, u64 delta, u64 size) {
    auto* const self = static_cast<BufferCache*>(ctx);
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
        runtime.CopyBuffer(guest_src, guest_batch, guest_copies);
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
    for (const auto& copy : copies) {
        guest_src_offset = copy.srcOffset;
        guest_import_split(guest, GuestRun, memory, copy.dstOffset, copy.size, OnGuestPiece, this);
    }
    FlushGuestCopies();
    copies.clear();
    copies.insert(copies.end(), guest_rest.begin(), guest_rest.end());
}

void BufferCache::TickFrame() {
    if (std::exchange(fault_process_pending, false)) {
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

void BufferCache::ReadMemory(VAddr device_addr, u64 size, bool is_write, bool async,
                             bool assume_locks) {
    const auto flush_request = [this, device_addr, size, is_write, async] {
        const u32 first_block = device_addr >> block_shift;
        const u32 last_block = (device_addr + size - 1) >> block_shift;
        const auto* arena = GetArena(first_block, last_block);

        // GPU-modified ranges come as many small scattered islands,
        // so the download is widened to a window around the request
        constexpr u64 WindowSize = 512_KB;
        const VAddr arena_end = arena->cpu_addr + arena->size_bytes;
        const VAddr window_start =
            std::max<VAddr>(Common::AlignDown(device_addr, WindowSize), arena->cpu_addr);
        const VAddr window_end = std::min<VAddr>(
            std::max<VAddr>(window_start + WindowSize, device_addr + size), arena_end);
        DownloadMemory(arena, window_start, window_end - window_start, async);
        if (is_write) {
            memory_tracker->MarkRegionAsCpuModified(device_addr, size);
        }
    };
    if (assume_locks) {
        flush_request();
    } else {
        liverpool->SendCommand<true>(std::move(flush_request));
    }
}

void BufferCache::DownloadMemory(const Buffer* arena, VAddr device_addr, u64 size, bool async) {
    // Each copy first with where in the game's memory it goes as its destination
    Common::SmallVector<vk::BufferCopy, 1> copies;
    const VAddr arena_base = arena->cpu_addr;
    memory_tracker->ForEachDownloadRange<false>(device_addr, size, [&](u64 address, u64 size) {
        gpu_modified_ranges.ForEachInRange(address, size, [&](VAddr start, VAddr end) {
            copies.push_back(vk::BufferCopy{
                .srcOffset = start - arena_base,
                .dstOffset = start,
                .size = end - start,
            });
        });
        gpu_modified_ranges.Subtract(address, size);
    });
    if (copies.empty()) {
        return;
    }
    // Straight into the game's memory where it is lent to the GPU, the rest through staging
    CopyToGuest(arena, copies);
    u64 total_size_bytes = 0;
    for (auto& copy : copies) {
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
        for (auto& copy : copies) {
            copy.dstOffset += download.offset;
        }
        runtime.CopyBuffer(arena, download.buffer, copies);
    }

    const auto write_data = [this, arena_base, device_addr, size, download, async,
                             copies = std::move(copies)] {
        if (download.buffer) {
            download.Invalidate();
            for (const auto& copy : copies) {
                auto* dst_addr = std::bit_cast<u8*>(arena_base + copy.srcOffset);
                memory->TryWriteBacking(
                    dst_addr, download.mapped + (copy.dstOffset - download.offset), copy.size);
            }
        }
        memory_tracker->UnmarkRegionAsGpuModified(device_addr, size, false);

        if (async && download.buffer) {
            staging_pool.FreeDeferred(download);
        }
    };

    if (async) {
        scheduler.DeferOperation(std::move(write_data));
    } else {
        scheduler.Finish();
        write_data();
    }
}

std::pair<const Buffer*, u64> BufferCache::ObtainBuffer(VAddr device_addr, u32 size,
                                                        ObtainBufferFlags flags) {
    const bool is_written = True(flags & ObtainBufferFlags::IsWritten);
    const bool is_texel_buffer = True(flags & ObtainBufferFlags::IsTexelBuffer);
    const bool skip_stream_buffer = True(flags & ObtainBufferFlags::IgnoreStreamBuffer);
    // For read-only buffers use device local stream buffer to reduce renderpass breaks.
    if (!is_written && !skip_stream_buffer && size <= STREAM_THRESHOLD &&
        !IsRegionGpuModified(device_addr, size)) {
        const auto [data, offset] = stream_buffer.Map(size, instance.UniformMinAlignment());
        memory->CopySparseMemory(device_addr, data, size);
        stream_buffer.Commit();
        return {&stream_buffer, offset};
    }
    const u64 first_block = device_addr >> block_shift;
    const u64 last_block = (device_addr + size - 1) >> block_shift;
    const auto* arena = GetArena(first_block, last_block);
    EnsureResident(arena, first_block, last_block);
    if (is_written) {
        texture_cache.MarkAsMaybeGpuDirty(device_addr, size);
    }
    SynchronizeMemory(arena, device_addr, size, is_written, is_texel_buffer);
    if (is_written) {
        gpu_modified_ranges.Add(device_addr, size);
    }
    return {arena, arena->Offset(device_addr)};
}

std::pair<const Buffer*, u64> BufferCache::ObtainBufferForImage(VAddr device_addr, u32 size) {
    if (IsRegionGpuModified(device_addr, size)) {
        return ObtainBuffer(device_addr, size);
    }
    const auto staging = staging_pool.Request(size, VideoCore::MemoryType::HostUncached,
                                              instance.StorageMinAlignment());
    memory->CopySparseMemory(device_addr, staging.mapped, staging.size);
    staging.Flush();
    return {staging.buffer, staging.offset};
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
    Common::SmallVector<vk::BufferCopy, 2> copies;
    const auto [buffer, offset] = ObtainBuffer(page_start, page_end - page_start);
    const auto add_download = [&](VAddr start, VAddr end) {
        copies.push_back(vk::BufferCopy{
            .srcOffset = start - buffer->CpuAddr(),
            .dstOffset = start,
            .size = end - start,
        });
    };
    gpu_modified_ranges.ForEachInRange(page_start, image_addr - page_start, add_download);
    gpu_modified_ranges.ForEachInRange(image_end, page_end - image_end, add_download);
    gpu_modified_ranges.Subtract(page_start, page_end - page_start);
    if (copies.empty()) {
        return;
    }
    scheduler.EndRendering();
    // Straight into the game's memory where it is lent to the GPU, the rest through staging
    CopyToGuest(buffer, copies);
    u64 total_size_bytes = 0;
    for (auto& copy : copies) {
        copy.dstOffset = total_size_bytes;
        // Align up to avoid cache conflicts
        constexpr u64 align = 64ULL;
        constexpr u64 mask = ~(align - 1ULL);
        total_size_bytes += (copy.size + align - 1) & mask;
    }
    if (total_size_bytes == 0) {
        return;
    }
    const auto download =
        staging_pool.Request(total_size_bytes, VideoCore::MemoryType::HostCached, 16, true);
    for (auto& copy : copies) {
        // Modify copies to have the staging offset in mind
        copy.dstOffset += download.offset;
    }
    runtime.CopyBuffer(buffer, download.buffer, copies);
    scheduler.DeferOperation(
        [this, base = buffer->CpuAddr(), download, copies = std::move(copies)]() {
            download.Invalidate();
            auto* memory = Core::Memory::Instance();
            for (const auto& copy : copies) {
                const VAddr copy_device_addr = base + copy.srcOffset;
                const u64 dst_offset = copy.dstOffset - download.offset;
                memory->TryWriteBacking(std::bit_cast<u8*>(copy_device_addr),
                                        download.mapped + dst_offset, copy.size);
            }
            staging_pool.FreeDeferred(download);
        });
}

void BufferCache::SynchronizeDmaBuffers() {
    fault_process_pending = true;
    for (const auto& range : resident_ranges) {
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
        const bool all_free = std::all_of(address_space.begin() + first_page,
                                          address_space.begin() + last_page + 1,
                                          [](const Buffer* arena) { return arena == nullptr; });
        ASSERT_MSG(all_free, "Buffer request spans {} VA arena pages, some of them in use",
                   last_page - first_page + 1);
        const u64 base_block = Common::AlignDownPow2<u64>(first_block, blocks_per_arena_page);
        const u64 num_pages = last_page - first_page + 1;
        const auto* new_arena =
            &arenas.emplace_back(instance, base_block << block_shift, num_pages << ARENA_PAGE_BITS,
                                 MemoryType::Sparse);
        std::fill(address_space.begin() + first_page, address_space.begin() + last_page + 1,
                  new_arena);
        return new_arena;
    }

    const auto* first_arena = address_space[first_page];
    const auto* last_arena = address_space[last_page];
    if (first_arena == last_arena) {
        if (!first_arena) {
            const u64 base_block = Common::AlignDownPow2<u64>(first_block, blocks_per_arena_page);
            const u64 num_pages = last_page - first_page + 1;
            const auto* new_arena =
                &arenas.emplace_back(instance, base_block << block_shift,
                                     num_pages << ARENA_PAGE_BITS, MemoryType::Sparse);
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
    auto* new_arena = &arenas.emplace_back(instance, first_addr, total_size, MemoryType::Sparse);
    if (dense_arenas) {
        MigrateDenseArena(first_arena, new_arena);
        MigrateDenseArena(last_arena, new_arena);
    }
    auto* bind = dense_arenas ? nullptr : BindsForArena(new_arena);
    resident_ranges.ForEachInRange(base_block, end_block, [&](const Backing& backing) {
        if (!bind) {
            return;
        }
        const u64 start = std::max(base_block, backing.start);
        const u64 end = std::min(end_block, backing.end);
        bind->binds.push_back(vk::SparseMemoryBind{
            .resourceOffset = (start - base_block) << block_shift,
            .size = (end - start) << block_shift,
            .memory = backing.memory,
            .memoryOffset = (backing.offset + start - backing.start) << block_shift,
        });
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

std::pair<vk::DeviceMemory, u64> BufferCache::BackingMemory(u64 blocks) {
    if (backing_chunk && blocks <= backing_chunk_blocks - backing_chunk_used) {
        const u64 first = backing_chunk_used;
        backing_chunk_used += blocks;
        return {backing_chunk, first};
    }
    // A chunk is a 64th of the device's memory, from 16 MiB to 128 MiB, in whole blocks
    const u64 budget = instance.GetTotalMemoryBudget();
    const u64 chunk_bytes =
        Common::AlignUp(std::clamp<u64>(budget / 64, 16_MB, 128_MB), u64{block_size});
    const u64 chunk_blocks = chunk_bytes >> block_shift;
    const auto allocate = [&](u64 n) -> vk::DeviceMemory {
        const vk::MemoryAllocateFlagsInfo alloc_flags = {
            .flags = vk::MemoryAllocateFlagBits::eDeviceAddress,
        };
        const vk::MemoryAllocateInfo alloc_info = {
            .pNext = &alloc_flags,
            .allocationSize = n << block_shift,
            .memoryTypeIndex = arena_memory_type_index,
        };
        const auto [result, memory] = instance.GetDevice().allocateMemory(alloc_info);
        return result == vk::Result::eSuccess ? memory : vk::DeviceMemory{};
    };
    if (blocks > chunk_blocks / 2) {
        // More than half a chunk: memory of its own, and the chunk keeps what it has left
        const vk::DeviceMemory memory = allocate(blocks);
        ASSERT_MSG(memory, "Out of device memory for {} blocks of buffer residency", blocks);
        return {memory, 0};
    }
    vk::DeviceMemory chunk = allocate(chunk_blocks);
    u64 got = chunk_blocks;
    if (!chunk && blocks > 0) {
        // No room for a whole chunk: only what is asked for
        chunk = allocate(blocks);
        got = blocks;
    }
    ASSERT_MSG(chunk || blocks == 0, "Out of device memory for {} blocks of buffer residency",
               blocks);
    if (!chunk) {
        return {};
    }
    backing_chunk = chunk;
    backing_chunk_blocks = got;
    backing_chunk_used = blocks;
    return {chunk, 0};
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
        for (const auto& range : bind_ranges) {
            resident_ranges.Add(Backing{{range.start, range.end}, vk::DeviceMemory{}, range.start});
            ranges.push_back({range.start, range.end});
        }
        WriteArenaPageTable(arena, ranges);
        return;
    }

    const auto [device_memory, first_backing_block] = BackingMemory(resident_blocks);

    Common::SmallVector<vk::BufferCopy, 8> copies;
    const auto staging =
        staging_pool.Request(resident_blocks * sizeof(vk::DeviceAddress), MemoryType::HostUncached);

    u64 memory_offset = first_backing_block << block_shift;
    ArenaBinds* binds = BindsForArena(arena);
    auto* bda_addrs = reinterpret_cast<vk::DeviceAddress*>(staging.mapped);
    u64 offset = staging.offset;
    for (const auto& range : bind_ranges) {
        Backing backing;
        backing.start = range.start;
        backing.end = range.end;
        backing.memory = device_memory;
        backing.offset = memory_offset >> block_shift;
        resident_ranges.Add(backing);

        LOG_INFO(Render, "Making range start={}, end={} resident", backing.start, backing.end);

        const auto& bind = binds->binds.emplace_back(vk::SparseMemoryBind{
            .resourceOffset = (range.start << block_shift) - arena->cpu_addr,
            .size = (range.end - range.start) << block_shift,
            .memory = device_memory,
            .memoryOffset = memory_offset,
        });
        memory_offset += bind.size;

        for (u32 block = 0; block < bind.size; block += block_size) {
            *(bda_addrs++) = arena->BufferDeviceAddress() + bind.resourceOffset + block;
        }
        const u64 copy_size = (backing.end - backing.start) * sizeof(vk::DeviceAddress);
        copies.emplace_back(offset, backing.start * sizeof(vk::DeviceAddress), copy_size);
        offset += copy_size;
    }

    staging.Flush();
    runtime.CopyBuffer(staging.buffer, bda_pagetable_buffer.get(), copies);
}

bool BufferCache::SynchronizeMemory(const Buffer* arena, VAddr device_addr, u32 size,
                                    bool is_written, bool is_texel_buffer) {
    Common::SmallVector<vk::BufferCopy, 4> copies;
    size_t total_size_bytes{};
    const auto upload = [&](u64 addr, u64 size) {
        copies.emplace_back(total_size_bytes, addr, size);
        total_size_bytes += size;
    };
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
        ranges.emplace_back(addr, size, write_snapshots_cutoff(snapshots));
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
                    const VAddr start = std::max(gpu_start, page);
                    const VAddr end = std::min<VAddr>(gpu_end, page + WRITE_SNAPSHOT_PAGE);
                    u32 slot;
                    const u8* before = write_snapshots_claim(snapshots, page, range.cutoff, &slot);
                    if (!before) {
                        // Nothing to tell them apart by: the guest's, as it always was
                        upload(start, end - start);
                        continue;
                    }
                    // The page as it is now, read where it lies in the game's memory; copied
                    // out only where it does not lie in one run of the backing
                    std::array<u8, WRITE_SNAPSHOT_PAGE> copied;
                    const u8* now = copied.data();
                    u64 backing_offset = 0;
                    if (memory->BackingRun(page, WRITE_SNAPSHOT_PAGE, &backing_offset) ==
                            WRITE_SNAPSHOT_PAGE &&
                        backing_offset != ~u64{0}) {
                        now = memory->BackingBase() + backing_offset;
                    } else {
                        memory->CopySparseMemory(page, copied.data(), copied.size());
                    }
                    struct Runs {
                        decltype(upload)& upload;
                        VAddr page;
                    } runs{upload, page};
                    // Cut by the page, the GPU's range goes on into the one before or after
                    write_snapshots_written(
                        now, before, static_cast<u32>(start - page),
                        static_cast<u32>(end - page), start == page && gpu_start < page,
                        end == page + WRITE_SNAPSHOT_PAGE && gpu_end > end,
                        [](void* ctx, u32 begin, u32 size) {
                            auto* runs = static_cast<Runs*>(ctx);
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
        const auto staging = staging_pool.Request(total_size_bytes, MemoryType::HostUncached);
        for (auto& copy : copies) {
            memory->CopySparseMemory(copy.dstOffset, staging.mapped + copy.srcOffset, copy.size);
            copy.srcOffset += staging.offset;
            copy.dstOffset -= arena->cpu_addr;
        }
        staging.Flush();
        runtime.CopyBuffer(staging.buffer, arena, copies);
    }
    if (is_texel_buffer && !is_written) {
        return SynchronizeMemoryFromImage(arena, device_addr, size);
    }
    return false;
}

bool BufferCache::SynchronizeMemoryFromImage(const Buffer* arena, VAddr device_addr, u32 size) {
    if (const auto meta = texture_cache.GetMetaInfo(device_addr)) {
        if (meta->type == TextureCache::MetaType::HTile) {
            static constexpr u32 ZmaskUncompressed = 0xf;
            runtime.FillBuffer(arena, arena->Offset(device_addr), size, ZmaskUncompressed);
            return true;
        } else if (meta->type == TextureCache::MetaType::CMask) {
            const auto expanded_value = CmaskColorExpandedValue(meta->num_samples);
            if (!expanded_value) {
                LOG_WARNING(Render_Vulkan,
                            "Unsupported CMask sample count: address={:#x}, size={:#x}, "
                            "samples={}, owner={}:{}",
                            device_addr, size, meta->num_samples, meta->owner_id.index,
                            meta->owner_uid);
                return false;
            }
            runtime.FillBuffer(arena, arena->Offset(device_addr), size, *expanded_value);
            return true;
        } else {
            LOG_WARNING(Render_Vulkan, "Unhandled metadata type {}",
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
               "Texel buffer aliases image subresources {:x} : {:x}", device_addr,
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
        const auto& mip_info = image.info.mips_layout[mip];
        const u32 width = std::max(image.info.size.width >> mip, 1u);
        const u32 height = std::max(image.info.size.height >> mip, 1u);
        const u32 depth = std::max(image.info.size.depth >> mip, 1u);
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
    auto& tile_manager = texture_cache.GetTileManager();
    tile_manager.TileImage(image, buffer_copies, arena, arena_offset);
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
    Common::SmallVector<vk::BufferCopy, 8> copies;
    Common::SmallVector<Interval, 8> ranges;
    resident_ranges.ForEachInRange(base_block, end_block, [&](const Backing& backing) {
        const u64 start = std::max(base_block, backing.start);
        const u64 end = std::min(end_block, backing.end);
        const VAddr addr = start << block_shift;
        copies.push_back(vk::BufferCopy{
            .srcOffset = old_arena->Offset(addr),
            .dstOffset = new_arena->Offset(addr),
            .size = (end - start) << block_shift,
        });
        ranges.push_back({start, end});
    });
    if (copies.empty()) {
        return;
    }
    runtime.CopyBuffer(old_arena, new_arena, copies);
    WriteArenaPageTable(new_arena, ranges);
}

void BufferCache::WriteArenaPageTable(const Buffer* arena, std::span<const Interval> ranges) {
    u64 num_blocks{};
    for (const auto& range : ranges) {
        num_blocks += range.end - range.start;
    }
    const auto staging =
        staging_pool.Request(num_blocks * sizeof(vk::DeviceAddress), MemoryType::HostUncached);
    Common::SmallVector<vk::BufferCopy, 8> copies;
    auto* bda_addrs = reinterpret_cast<vk::DeviceAddress*>(staging.mapped);
    u64 offset = staging.offset;
    for (const auto& range : ranges) {
        const u64 arena_offset = (range.start << block_shift) - arena->cpu_addr;
        for (u64 block = 0; block < range.end - range.start; ++block) {
            *(bda_addrs++) = arena->BufferDeviceAddress() + arena_offset + (block << block_shift);
        }
        const u64 copy_size = (range.end - range.start) * sizeof(vk::DeviceAddress);
        copies.emplace_back(offset, range.start * sizeof(vk::DeviceAddress), copy_size);
        offset += copy_size;
    }
    staging.Flush();
    runtime.CopyBuffer(staging.buffer, bda_pagetable_buffer.get(), copies);
}

void BufferCache::SubmitPendingArenaBinds(Vulkan::SubmitInfo& info) {
    if (num_pending_binds == 0) {
        return;
    }

    buffer_binds.clear();
    for (std::size_t i = 0; i < num_pending_binds; ++i) {
        const auto& binds = pending_binds[i];
        buffer_binds.emplace_back(vk::SparseBufferMemoryBindInfo{
            .buffer = binds.arena->Handle(),
            .bindCount = static_cast<u32>(binds.binds.size()),
            .pBinds = binds.binds.data(),
        });
    }

    const u64 signal_tick = memory_semaphore.NextTick();
    const auto signal_sema = memory_semaphore.Handle();

    const vk::TimelineSemaphoreSubmitInfo timeline_si = {
        .signalSemaphoreValueCount = 1u,
        .pSignalSemaphoreValues = &signal_tick,
    };

    const vk::BindSparseInfo sparse_info = {
        .pNext = &timeline_si,
        .bufferBindCount = static_cast<u32>(buffer_binds.size()),
        .pBufferBinds = buffer_binds.data(),
        .signalSemaphoreCount = 1u,
        .pSignalSemaphores = &signal_sema,
    };

    info.AddWait(signal_sema, signal_tick);
    auto submit_result = instance.GetGraphicsQueue().bindSparse(sparse_info);
    if (submit_result == vk::Result::eErrorDeviceLost) {
        instance.ReportDeviceLost();
    }
    ASSERT_MSG(submit_result != vk::Result::eErrorDeviceLost, "Device lost during submit");

    // The slots and their room stay for the next submit's binds
    num_pending_binds = 0;
}

} // namespace VideoCore
