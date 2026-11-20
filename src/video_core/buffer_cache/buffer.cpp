// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdlib.h>

#include "common/alignment.h"
#include "common/assert.h"
#include "video_core/buffer_cache/buffer.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_memory.h"
#include "video_core/renderer_vulkan/vk_platform.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"

namespace VideoCore {

const char* BufferTypeName(MemoryType type) {
    switch (type) {
    case MemoryType::HostUncached:
        return "HostUncached";
    case MemoryType::HostCached:
        return "HostCached";
    case MemoryType::Stream:
        return "Stream";
    case MemoryType::DeviceLocal:
        return "DeviceLocal";
    case MemoryType::Sparse:
        return "Sparse";
    default:
        return "Invalid";
    }
}

[[nodiscard]] Vulkan::MemoryUsage MemoryUsageOf(MemoryType type) {
    switch (type) {
    case MemoryType::HostUncached:
        return Vulkan::MemoryUsage::Upload;
    case MemoryType::HostCached:
        return Vulkan::MemoryUsage::Download;
    case MemoryType::Stream:
        return Vulkan::MemoryUsage::Stream;
    case MemoryType::DeviceLocal:
    default:
        return Vulkan::MemoryUsage::DeviceLocal;
    }
}

UniqueBuffer::UniqueBuffer(VkDevice device_, Vulkan::MemoryAllocator* allocator_)
    : device{device_}, allocator{allocator_} {}

UniqueBuffer::~UniqueBuffer() {
    Destroy();
}

void UniqueBuffer::Destroy() {
    if (borrowed) {
        borrowed = false;
    } else if (allocation) {
        allocator->DestroyBuffer(buffer, allocation);
    } else if (buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, buffer, nullptr);
    }
    buffer = VK_NULL_HANDLE;
    allocation = nullptr;
    bda_addr = 0;
}

void UniqueBuffer::Create(VkBufferCreateInfo& buffer_ci, MemoryType mem_type) {
    const bool with_bda = (buffer_ci.usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) != 0;
    if (mem_type != MemoryType::Sparse) {
        // A buffer with a device address gets memory of its own, as it did under VMA
        const Vulkan::MemoryUsage usage = MemoryUsageOf(mem_type);
        VkBuffer created = VK_NULL_HANDLE;
        VkResult result =
            allocator->CreateBuffer(buffer_ci, usage, with_bda, true, created, allocation);
        if (result == VK_ERROR_OUT_OF_DEVICE_MEMORY) {
            LOG_WARNING(Render_Vulkan, "Allocating buffer of size %" PRIu64 " bytes over budget.",
                        buffer_ci.size);
            result =
                allocator->CreateBuffer(buffer_ci, usage, with_bda, false, created, allocation);
        }
        ASSERT_MSG(result == VK_SUCCESS, "Failed allocating buffer with error %s",
                   Vulkan::ResultName(result));
        buffer = created;
    } else {
        // The libretro core's Buffer sets the flags itself where the device has no sparse
        // residency
        if (!(buffer_ci.flags & VK_BUFFER_CREATE_SPARSE_BINDING_BIT)) {
            buffer_ci.flags |=
                VK_BUFFER_CREATE_SPARSE_BINDING_BIT | VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT;
        }
        const VkResult result = vkCreateBuffer(device, &buffer_ci, nullptr, &buffer);
        ASSERT_MSG(result == VK_SUCCESS, "Failed to create sparse buffer: %s",
                   Vulkan::ResultName(result));
    }

    if (with_bda) {
        VkBufferDeviceAddressInfo bda_info{};
        bda_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        bda_info.buffer = buffer;
        bda_addr = vkGetBufferDeviceAddress(device, &bda_info);
        ASSERT_MSG(bda_addr != 0, "Failed to get buffer device address");
    }
}

Buffer::Buffer(const Vulkan::Instance& instance, VAddr cpu_addr_, u64 size_bytes_,
               MemoryType mem_type_, const char* debug_name)
    : cpu_addr{cpu_addr_}, size_bytes{size_bytes_}, mem_type{mem_type_},
      buffer{static_cast<VkDevice>(instance.GetDevice()), instance.GetAllocator()} {

    VkBufferCreateInfo buffer_ci{};
    buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_ci.size = size_bytes;
    buffer_ci.usage = AllFlags;
    buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    // A device without sparse buffers (Intel Gen9 under Mesa) gets arenas fully backed by
    // memory of their own, made when the buffer cache first needs them
    if (mem_type == MemoryType::Sparse && !instance.IsSparseBindingSupported()) {
        mem_type = MemoryType::DeviceLocal;
    }
    if (mem_type == MemoryType::Sparse && !instance.IsSparseResidencyBufferSupported()) {
        buffer_ci.flags |= VK_BUFFER_CREATE_SPARSE_BINDING_BIT;
    }
    buffer.Create(buffer_ci, mem_type);

    const vk::Device device = instance.GetDevice();
    if (debug_name && debug_name[0]) {
        Vulkan::SetObjectName(device, vk::Buffer{Handle()}, "%s", debug_name);
    } else {
        Vulkan::SetObjectName(device, vk::Buffer{Handle()}, "Buffer 0x%" PRIxPTR ":0x%zx", cpu_addr,
                              size_bytes);
    }

    if (mem_type != MemoryType::Sparse) {
        // Lent by the heap: the block's one mapping, at this buffer's offset
        if (buffer.allocation->mapped) {
            mapped_data = static_cast<u8*>(buffer.allocation->mapped);
        }
        is_coherent = buffer.allocation->flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    }
}

Buffer::Buffer(const Vulkan::Instance& instance, VkBuffer handle, u64 size_bytes_,
               const char* debug_name)
    : size_bytes{size_bytes_},
      buffer{static_cast<VkDevice>(instance.GetDevice()), instance.GetAllocator()} {
    buffer.buffer = handle;
    buffer.borrowed = true;
    Vulkan::SetObjectName(instance.GetDevice(), vk::Buffer{Handle()}, "%s", debug_name);
}

void Buffer::Flush(u64 offset, u64 size) {
    if (!mapped_data || is_coherent) {
        return;
    }
    buffer.allocator->Flush(buffer.allocation, offset, size);
}

void Buffer::Invalidate(u64 offset, u64 size) {
    if (!mapped_data || is_coherent) {
        return;
    }
    buffer.allocator->Invalidate(buffer.allocation, offset, size);
}

constexpr u64 WATCHES_INITIAL_RESERVE = 0x100;
constexpr u64 WATCHES_RESERVE_CHUNK = 0x100;

StreamBuffer::StreamBuffer(const Vulkan::Instance& instance, Vulkan::Scheduler& scheduler_,
                           MemoryType mem_type, u64 size_bytes)
    : Buffer{instance, 0, size_bytes, mem_type}, scheduler{scheduler_},
      non_coherent_atom_size{instance.NonCoherentAtomSize()} {
    ReserveWatches(current_watches, num_current_watches, WATCHES_INITIAL_RESERVE);
    ReserveWatches(previous_watches, num_previous_watches, WATCHES_INITIAL_RESERVE);
    Vulkan::SetObjectName(instance.GetDevice(), vk::Buffer{Handle()}, "StreamBuffer(%s):0x%" PRIx64,
                          BufferTypeName(mem_type), size_bytes);
}

StreamBuffer::~StreamBuffer() {
    free(current_watches);
    free(previous_watches);
}

namespace {

u64 Gcd(u64 a, u64 b) {
    while (b != 0) {
        const u64 rest = a % b;
        a = b;
        b = rest;
    }
    return a;
}

} // Anonymous namespace

bool StreamBuffer::PrepareMap(u64 size, u64 alignment, bool allow_wait) {
    if (mapped_data && !is_coherent) {
        size = Common::AlignUp(size, non_coherent_atom_size);
        // The least common multiple of the two
        alignment = alignment > 0 ? alignment / Gcd(alignment, non_coherent_atom_size) *
                                        non_coherent_atom_size
                                  : non_coherent_atom_size;
    }

    if (size > this->size_bytes) {
        return false;
    }

    if (alignment > 0) {
        offset = Common::AlignUp(offset, alignment);
    }

    if (offset + size > this->size_bytes) {
        // The buffer would overflow, save the amount of used watches and reset the state.
        has_invalidation_mark = true;
        invalidation_mark = current_watch_cursor;
        current_watch_cursor = 0;
        offset = 0;

        // Swap watches and reset waiting cursors.
        Watch* const watches = previous_watches;
        const size_t num_watches = num_previous_watches;
        previous_watches = current_watches;
        num_previous_watches = num_current_watches;
        current_watches = watches;
        num_current_watches = num_watches;
        wait_cursor = 0;
        wait_bound = 0;
    }

    if (!WaitPendingOperations(offset + size, allow_wait)) {
        return false;
    }

    mapped_size = size;
    return true;
}

u8* StreamBuffer::Map(u64 size, u64* out_offset, u64 alignment, bool allow_wait) {
    if (!PrepareMap(size, alignment, allow_wait)) {
        *out_offset = 0;
        return nullptr;
    }
    *out_offset = offset;
    return mapped_data ? mapped_data + offset : nullptr;
}

void StreamBuffer::Commit() {
    if (mem_type == MemoryType::HostCached) {
        Invalidate(offset, mapped_size);
    } else {
        Flush(offset, mapped_size);
    }
    AdvanceAndWatch();
}

bool StreamBuffer::Reserve(u64 size, u64* out_offset, u64 alignment, bool allow_wait) {
    if (!PrepareMap(size, alignment, allow_wait)) {
        return false;
    }
    *out_offset = offset;
    AdvanceAndWatch();
    return true;
}

void StreamBuffer::AdvanceAndWatch() {
    offset += mapped_size;
    const u64 tick = scheduler.CurrentTick();
    last_tick = tick;

    // Extend the last watch if it belongs to the same tick.
    if (current_watch_cursor != 0 && current_watches[current_watch_cursor - 1].tick == tick) {
        current_watches[current_watch_cursor - 1].upper_bound = offset;
        return;
    }

    if (current_watch_cursor + 1 >= num_current_watches) {
        // Ensure that there are enough watches.
        ReserveWatches(current_watches, num_current_watches, WATCHES_RESERVE_CHUNK);
    }

    Watch& watch = current_watches[current_watch_cursor++];
    watch.upper_bound = offset;
    watch.tick = tick;
}

void StreamBuffer::ReserveWatches(Watch*& watches, size_t& count, size_t grow_size) {
    Watch* const grown = static_cast<Watch*>(realloc(watches, (count + grow_size) * sizeof(Watch)));
    ASSERT_MSG(grown, "Out of memory growing a stream buffer's watches");
    memset(grown + count, 0, grow_size * sizeof(Watch));
    watches = grown;
    count += grow_size;
}

bool StreamBuffer::WaitPendingOperations(u64 requested_upper_bound, bool allow_wait) {
    if (!has_invalidation_mark) {
        return true;
    }
    while (requested_upper_bound > wait_bound && wait_cursor < invalidation_mark) {
        const Watch& watch = previous_watches[wait_cursor];
        if (!scheduler.IsFree(watch.tick) && !allow_wait) {
            return false;
        }
        scheduler.Wait(watch.tick);
        wait_bound = watch.upper_bound;
        ++wait_cursor;
    }
    return true;
}

} // namespace VideoCore
