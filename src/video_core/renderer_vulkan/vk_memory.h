// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/combiner.h"
#include "common/types.h"
#include "video_core/renderer_vulkan/vk_common.h"
#include "video_core/renderer_vulkan/vk_heap.h"

namespace Vulkan {

/// A piece of device memory handed out by the MemoryAllocator: its memory, offset, size,
/// mapped pointer and property flags are read off it directly while it lives.
using MemoryAllocation = vk_heap_alloc_t*;

/// What a buffer's memory is for
enum class MemoryUsage : u8 {
    DeviceLocal, ///< Device local, never mapped
    Upload,      ///< Mapped, written by the host in order; host memory preferred
    Download,    ///< Mapped, read back by the host; cached host memory preferred
    Stream,      ///< Mapped, written by the host in order; device local preferred
};

/// The renderer's device memory (vk_heap.c), serialized in a Common::Combiner: images and
/// buffers are created and bound outside it, and only the taking and giving back of memory
/// runs in it.
class MemoryAllocator {
public:
    MemoryAllocator(vk::PhysicalDevice physical_device, vk::Device device,
                    const vk::PhysicalDeviceMemoryProperties& memory,
                    const vk::PhysicalDeviceLimits& limits, bool device_address,
                    bool memory_budget);
    ~MemoryAllocator();

    MemoryAllocator(const MemoryAllocator&) = delete;
    MemoryAllocator& operator=(const MemoryAllocator&) = delete;

    /// Creates an image with device local memory bound. Over the heap's budget it fails with
    /// eErrorOutOfDeviceMemory unless !@within_budget.
    vk::Result CreateImage(const vk::ImageCreateInfo& image_ci, bool within_budget,
                           vk::Image& image, MemoryAllocation& allocation);

    /// Creates a buffer with memory for @usage bound; @dedicated gives it memory of its own.
    vk::Result CreateBuffer(const vk::BufferCreateInfo& buffer_ci, MemoryUsage usage,
                            bool dedicated, bool within_budget, vk::Buffer& buffer,
                            MemoryAllocation& allocation);

    void DestroyImage(vk::Image image, MemoryAllocation allocation);
    void DestroyBuffer(vk::Buffer buffer, MemoryAllocation allocation);

    /// For noncoherent memory; nothing to do for coherent memory. From any thread.
    void Flush(MemoryAllocation allocation, u64 offset, u64 size) const;
    void Invalidate(MemoryAllocation allocation, u64 offset, u64 size) const;

private:
    vk::Result Allocate(const VkMemoryRequirements& requirements, bool driver_dedicated,
                        const vk_heap_request_t& request, MemoryAllocation& allocation);
    void Free(MemoryAllocation allocation);

    vk::Device device;
    vk_heap_t* heap{};
    Common::Combiner combiner;
};

} // namespace Vulkan
