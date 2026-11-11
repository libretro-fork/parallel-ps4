// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/assert.h"
#include "video_core/renderer_vulkan/vk_memory.h"

namespace Vulkan {

namespace {

vk_heap_request_t RequestFor(MemoryUsage usage) {
    vk_heap_request_t request{};
    switch (usage) {
    case MemoryUsage::DeviceLocal:
        request.preferred = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        // Leaves the host visible device memory (the BAR) to what is mapped
        request.not_preferred = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        break;
    case MemoryUsage::Stream:
        request.flags = VK_HEAP_MAPPED;
        request.preferred =
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        request.not_preferred = VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
        break;
    case MemoryUsage::Upload:
        request.flags = VK_HEAP_MAPPED;
        request.preferred = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        request.not_preferred =
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
        break;
    case MemoryUsage::Download:
        request.flags = VK_HEAP_MAPPED;
        request.preferred =
            VK_MEMORY_PROPERTY_HOST_CACHED_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        request.not_preferred = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        break;
    }
    return request;
}

} // Anonymous namespace

MemoryAllocator::MemoryAllocator(vk::PhysicalDevice physical_device, vk::Device device_,
                                 const vk::PhysicalDeviceMemoryProperties& memory,
                                 const vk::PhysicalDeviceLimits& limits, bool device_address,
                                 bool memory_budget)
    : device{device_} {
    const auto& dispatch = VULKAN_HPP_DEFAULT_DISPATCHER;
    vk_heap_info_t info{};
    info.physical_device = static_cast<VkPhysicalDevice>(physical_device);
    info.device = static_cast<VkDevice>(device);
    info.memory = static_cast<VkPhysicalDeviceMemoryProperties>(memory);
    info.buffer_image_granularity = limits.bufferImageGranularity;
    info.non_coherent_atom_size = limits.nonCoherentAtomSize;
    info.device_address = device_address;
    info.fns.allocate_memory = dispatch.vkAllocateMemory;
    info.fns.free_memory = dispatch.vkFreeMemory;
    info.fns.map_memory = dispatch.vkMapMemory;
    info.fns.unmap_memory = dispatch.vkUnmapMemory;
    info.fns.flush_ranges = dispatch.vkFlushMappedMemoryRanges;
    info.fns.invalidate_ranges = dispatch.vkInvalidateMappedMemoryRanges;
    info.fns.get_memory_properties2 =
        memory_budget ? dispatch.vkGetPhysicalDeviceMemoryProperties2 : nullptr;
    heap = vk_heap_create(&info);
    ASSERT_MSG(heap, "Failed to create the Vulkan memory heap");
}

MemoryAllocator::~MemoryAllocator() {
    vk_heap_destroy(heap);
}

vk::Result MemoryAllocator::Allocate(const VkMemoryRequirements& requirements,
                                     bool driver_dedicated, const vk_heap_request_t& request,
                                     MemoryAllocation& allocation) {
    return combiner.Run([&] {
        return vk::Result{
            vk_heap_alloc(heap, &requirements, driver_dedicated, &request, &allocation)};
    });
}

void MemoryAllocator::Free(MemoryAllocation allocation) {
    if (allocation) {
        combiner.Run([&] { vk_heap_free(heap, allocation); });
    }
}

vk::Result MemoryAllocator::CreateImage(const vk::ImageCreateInfo& image_ci, bool within_budget,
                                        vk::Image& image, MemoryAllocation& allocation) {
    allocation = nullptr;
    auto [result, new_image] = device.createImage(image_ci);
    if (result != vk::Result::eSuccess) {
        return result;
    }
    const auto chain =
        device
            .getImageMemoryRequirements2<vk::MemoryRequirements2, vk::MemoryDedicatedRequirements>(
                vk::ImageMemoryRequirementsInfo2{.image = new_image});
    const auto& dedicated = chain.get<vk::MemoryDedicatedRequirements>();
    vk_heap_request_t request = RequestFor(MemoryUsage::DeviceLocal);
    request.image = static_cast<VkImage>(new_image);
    if (image_ci.tiling == vk::ImageTiling::eLinear) {
        request.flags |= VK_HEAP_LINEAR;
    }
    if (within_budget) {
        request.flags |= VK_HEAP_WITHIN_BUDGET;
    }
    const VkMemoryRequirements requirements =
        chain.get<vk::MemoryRequirements2>().memoryRequirements;
    result = Allocate(requirements,
                      dedicated.requiresDedicatedAllocation || dedicated.prefersDedicatedAllocation,
                      request, allocation);
    if (result == vk::Result::eSuccess) {
        result = device.bindImageMemory(new_image, allocation->memory, allocation->offset);
    }
    if (result != vk::Result::eSuccess) {
        device.destroyImage(new_image);
        Free(allocation);
        allocation = nullptr;
        return result;
    }
    image = new_image;
    return result;
}

vk::Result MemoryAllocator::CreateBuffer(const vk::BufferCreateInfo& buffer_ci, MemoryUsage usage,
                                         bool dedicated_, bool within_budget, vk::Buffer& buffer,
                                         MemoryAllocation& allocation) {
    allocation = nullptr;
    auto [result, new_buffer] = device.createBuffer(buffer_ci);
    if (result != vk::Result::eSuccess) {
        return result;
    }
    const auto chain =
        device
            .getBufferMemoryRequirements2<vk::MemoryRequirements2, vk::MemoryDedicatedRequirements>(
                vk::BufferMemoryRequirementsInfo2{.buffer = new_buffer});
    const auto& dedicated = chain.get<vk::MemoryDedicatedRequirements>();
    vk_heap_request_t request = RequestFor(usage);
    request.buffer = static_cast<VkBuffer>(new_buffer);
    request.flags |= VK_HEAP_LINEAR;
    if (dedicated_) {
        request.flags |= VK_HEAP_DEDICATED;
    }
    if (within_budget) {
        request.flags |= VK_HEAP_WITHIN_BUDGET;
    }
    const VkMemoryRequirements requirements =
        chain.get<vk::MemoryRequirements2>().memoryRequirements;
    result = Allocate(requirements,
                      dedicated.requiresDedicatedAllocation || dedicated.prefersDedicatedAllocation,
                      request, allocation);
    if (result == vk::Result::eSuccess) {
        result = device.bindBufferMemory(new_buffer, allocation->memory, allocation->offset);
    }
    if (result != vk::Result::eSuccess) {
        device.destroyBuffer(new_buffer);
        Free(allocation);
        allocation = nullptr;
        return result;
    }
    buffer = new_buffer;
    return result;
}

void MemoryAllocator::DestroyImage(vk::Image image, MemoryAllocation allocation) {
    if (image) {
        device.destroyImage(image);
    }
    Free(allocation);
}

void MemoryAllocator::DestroyBuffer(vk::Buffer buffer, MemoryAllocation allocation) {
    if (buffer) {
        device.destroyBuffer(buffer);
    }
    Free(allocation);
}

void MemoryAllocator::Flush(MemoryAllocation allocation, u64 offset, u64 size) const {
    vk_heap_flush(heap, allocation, offset, size);
}

void MemoryAllocator::Invalidate(MemoryAllocation allocation, u64 offset, u64 size) const {
    vk_heap_invalidate(heap, allocation, offset, size);
}

} // namespace Vulkan
