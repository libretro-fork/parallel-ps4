// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <stddef.h>
#include <string.h>

#include "common/assert.h"
#include "common/types.h"
#include "video_core/renderer_vulkan/vk_common.h"

struct vk_heap_span;

namespace Vulkan {
class Instance;
class MemoryAllocator;
class Scheduler;
} // namespace Vulkan

namespace VideoCore {

/// Hints and requirements for the backing memory type of a commit
enum class MemoryType : u8 {
    DeviceLocal,  ///< Requests device local buffer.
    HostUncached, ///< Requires a host visible memory type optimized for CPU to GPU uploads
    HostCached,   ///< Requires a host visible memory type optimized for GPU to CPU readbacks
    Stream,       ///< Requests device local host visible buffer, falling back host memory.
    Sparse,       ///< Requires an unbacked sparse resident buffer.
};

constexpr VkBufferUsageFlags ReadFlags =
    VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
    VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
    VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

constexpr VkBufferUsageFlags AllFlags =
    ReadFlags | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

struct UniqueBuffer {
    explicit UniqueBuffer(VkDevice device, Vulkan::MemoryAllocator* allocator);
    ~UniqueBuffer();

    UniqueBuffer(const UniqueBuffer&) = delete;
    UniqueBuffer& operator=(const UniqueBuffer&) = delete;

    UniqueBuffer(UniqueBuffer&& other) noexcept
        : device{other.device}, buffer{other.buffer}, bda_addr{other.bda_addr},
          allocator{other.allocator}, allocation{other.allocation}, borrowed{other.borrowed} {
        other.Release();
    }

    UniqueBuffer& operator=(UniqueBuffer&& other) noexcept {
        if (this != &other) {
            Destroy();
            device = other.device;
            buffer = other.buffer;
            bda_addr = other.bda_addr;
            allocator = other.allocator;
            allocation = other.allocation;
            borrowed = other.borrowed;
            other.Release();
        }
        return *this;
    }

    /// Forgets the buffer without destroying it: it has been moved elsewhere
    void Release() {
        buffer = VK_NULL_HANDLE;
        bda_addr = 0;
        allocator = nullptr;
        allocation = nullptr;
        borrowed = false;
    }

    void Create(VkBufferCreateInfo& buffer_ci, MemoryType mem_type);

    void Destroy();

    VkDevice device;
    VkBuffer buffer{VK_NULL_HANDLE};
    VkDeviceAddress bda_addr{};
    Vulkan::MemoryAllocator* allocator;
    vk_heap_span* allocation{};
    /// A buffer someone else made and destroys (Buffer's borrowing constructor)
    bool borrowed{};
};

struct Buffer {
    explicit Buffer(const Vulkan::Instance& instance, VAddr cpu_addr_, u64 size_bytes_,
                    MemoryType mem_type, const char* debug_name = nullptr);

    /// A buffer over @handle, made and destroyed by someone else, so that it can be copied to
    /// and from as any other (the game's memory imported, guest_import.h)
    explicit Buffer(const Vulkan::Instance& instance, VkBuffer handle, u64 size_bytes_,
                    const char* debug_name);

    Buffer& operator=(const Buffer&) = delete;
    Buffer(const Buffer&) = delete;

    Buffer& operator=(Buffer&&) = default;
    Buffer(Buffer&&) = default;

    [[nodiscard]] bool IsInBounds(VAddr addr, u64 size) const noexcept {
        return addr >= cpu_addr && addr + size <= cpu_addr + SizeBytes();
    }

    [[nodiscard]] VAddr CpuAddr() const noexcept {
        return cpu_addr;
    }

    [[nodiscard]] u64 Offset(VAddr other_cpu_addr) const noexcept {
        return other_cpu_addr - cpu_addr;
    }

    size_t SizeBytes() const {
        return size_bytes;
    }

    VkBuffer Handle() const noexcept {
        return buffer.buffer;
    }

    VkDeviceAddress BufferDeviceAddress() const noexcept {
        ASSERT_MSG(buffer.bda_addr != 0, "Can't get BDA from a non BDA buffer");
        return buffer.bda_addr;
    }

    /// Makes host writes in provided range visible to the device.
    void Flush(u64 offset, u64 size);

    /// Makes device writes in provided range visible to the host.
    void Invalidate(u64 offset, u64 size);

    VAddr cpu_addr = 0;
    size_t size_bytes = 0;
    /// Where the whole buffer is mapped (size_bytes from there), null when it is not
    u8* mapped_data{};
    bool is_coherent{};
    MemoryType mem_type{MemoryType::DeviceLocal};
    UniqueBuffer buffer;
};

struct StreamBuffer : public Buffer {
    explicit StreamBuffer(const Vulkan::Instance& instance, Vulkan::Scheduler& scheduler,
                          MemoryType mem_type, u64 size_bytes);
    ~StreamBuffer();

    StreamBuffer(const StreamBuffer&) = delete;
    StreamBuffer& operator=(const StreamBuffer&) = delete;
    StreamBuffer(StreamBuffer&&) = delete;
    StreamBuffer& operator=(StreamBuffer&&) = delete;

    /// Reserves a region of memory from the stream buffer, at *@out_offset in it, and returns
    /// where it is mapped (null when it could not be reserved). Must be followed by Commit().
    u8* Map(u64 size, u64* out_offset, u64 alignment = 0, bool allow_wait = true);

    /// Ensures that reserved bytes of memory are available to the GPU.
    void Commit();

    /// Reserves and commits a region in one step, at *@out_offset. False when it cannot.
    bool Reserve(u64 size, u64* out_offset, u64 alignment = 0, bool allow_wait = true);

    /// Tick of the most recent commit into this buffer, 0 if never used.
    [[nodiscard]] u64 LastTick() const noexcept {
        return last_tick;
    }

    /// Maps and commits a memory region with user provided data
    u64 Copy(const void* src, size_t size, size_t alignment = 0) {
        u64 copy_offset = 0;
        u8* const data = Map(size, &copy_offset, alignment);
        ASSERT_MSG(data, "Failed to copy 0x%zx bytes", size);
        memcpy(data, src, size);
        Commit();
        return copy_offset;
    }

private:
    struct Watch {
        u64 tick{};
        u64 upper_bound{};
    };

    /// Aligns, wraps if needed and waits for the GPU. On success mapped_size is set and the
    /// region starts at offset.
    bool PrepareMap(u64 size, u64 alignment, bool allow_wait);

    /// Advances offset past the mapped region and records the current tick for it.
    void AdvanceAndWatch();

    /// Increases the amount of watches available.
    static void ReserveWatches(Watch*& watches, size_t& count, size_t grow_size);

    /// Waits pending watches until requested upper bound.
    bool WaitPendingOperations(u64 requested_upper_bound, bool allow_wait);

private:
    Vulkan::Scheduler& scheduler;
    VkDeviceSize non_coherent_atom_size{};
    u64 offset{};
    u64 mapped_size{};
    u64 last_tick{};
    Watch* current_watches{};
    size_t num_current_watches{};
    size_t current_watch_cursor{};
    /// Since the buffer last wrapped: the watches used before it, still to be waited on
    bool has_invalidation_mark{};
    size_t invalidation_mark{};
    Watch* previous_watches{};
    size_t num_previous_watches{};
    size_t wait_cursor{};
    u64 wait_bound{};
};

} // namespace VideoCore
