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
    : device{static_cast<VkDevice>(device_)} {
    vk_heap_info_t info{};
    info.physical_device = static_cast<VkPhysicalDevice>(physical_device);
    info.device = device;
    info.memory = static_cast<VkPhysicalDeviceMemoryProperties>(memory);
    info.buffer_image_granularity = limits.bufferImageGranularity;
    info.non_coherent_atom_size = limits.nonCoherentAtomSize;
    info.device_address = device_address;
    // The symbol wrapper's entry points
    info.fns.allocate_memory = vkAllocateMemory;
    info.fns.free_memory = vkFreeMemory;
    info.fns.map_memory = vkMapMemory;
    info.fns.unmap_memory = vkUnmapMemory;
    info.fns.flush_ranges = vkFlushMappedMemoryRanges;
    info.fns.invalidate_ranges = vkInvalidateMappedMemoryRanges;
    info.fns.get_memory_properties2 =
        memory_budget ? vkGetPhysicalDeviceMemoryProperties2 : nullptr;
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
    VkImage new_image = VK_NULL_HANDLE;
    VkResult result = vkCreateImage(device, &static_cast<const VkImageCreateInfo&>(image_ci),
                                    nullptr, &new_image);
    if (result != VK_SUCCESS) {
        return static_cast<vk::Result>(result);
    }
    VkImageMemoryRequirementsInfo2 requirements_info{};
    requirements_info.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2;
    requirements_info.image = new_image;
    VkMemoryDedicatedRequirements dedicated{};
    dedicated.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS;
    VkMemoryRequirements2 requirements2{};
    requirements2.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
    requirements2.pNext = &dedicated;
    vkGetImageMemoryRequirements2(device, &requirements_info, &requirements2);
    vk_heap_request_t request = RequestFor(MemoryUsage::DeviceLocal);
    request.image = new_image;
    if (image_ci.tiling == vk::ImageTiling::eLinear) {
        request.flags |= VK_HEAP_LINEAR;
    }
    if (within_budget) {
        request.flags |= VK_HEAP_WITHIN_BUDGET;
    }
    result = static_cast<VkResult>(
        Allocate(requirements2.memoryRequirements,
                 dedicated.requiresDedicatedAllocation || dedicated.prefersDedicatedAllocation,
                 request, allocation));
    if (result == VK_SUCCESS) {
        result = vkBindImageMemory(device, new_image, allocation->memory, allocation->offset);
    }
    if (result != VK_SUCCESS) {
        vkDestroyImage(device, new_image, nullptr);
        Free(allocation);
        allocation = nullptr;
        return static_cast<vk::Result>(result);
    }
    image = vk::Image{new_image};
    return vk::Result::eSuccess;
}

VkResult MemoryAllocator::CreateBuffer(const VkBufferCreateInfo& buffer_ci, MemoryUsage usage,
                                       bool dedicated_, bool within_budget, VkBuffer& buffer,
                                       MemoryAllocation& allocation) {
    allocation = nullptr;
    VkBuffer new_buffer = VK_NULL_HANDLE;
    VkResult result = vkCreateBuffer(device, &buffer_ci, nullptr, &new_buffer);
    if (result != VK_SUCCESS) {
        return result;
    }
    VkBufferMemoryRequirementsInfo2 requirements_info{};
    requirements_info.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2;
    requirements_info.buffer = new_buffer;
    VkMemoryDedicatedRequirements dedicated{};
    dedicated.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS;
    VkMemoryRequirements2 requirements2{};
    requirements2.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
    requirements2.pNext = &dedicated;
    vkGetBufferMemoryRequirements2(device, &requirements_info, &requirements2);
    vk_heap_request_t request = RequestFor(usage);
    request.buffer = new_buffer;
    request.flags |= VK_HEAP_LINEAR;
    if (dedicated_) {
        request.flags |= VK_HEAP_DEDICATED;
    }
    if (within_budget) {
        request.flags |= VK_HEAP_WITHIN_BUDGET;
    }
    result = static_cast<VkResult>(
        Allocate(requirements2.memoryRequirements,
                 dedicated.requiresDedicatedAllocation || dedicated.prefersDedicatedAllocation,
                 request, allocation));
    if (result == VK_SUCCESS) {
        result = vkBindBufferMemory(device, new_buffer, allocation->memory, allocation->offset);
    }
    if (result != VK_SUCCESS) {
        vkDestroyBuffer(device, new_buffer, nullptr);
        Free(allocation);
        allocation = nullptr;
        return result;
    }
    buffer = new_buffer;
    return VK_SUCCESS;
}

void MemoryAllocator::DestroyImage(vk::Image image, MemoryAllocation allocation) {
    if (image) {
        vkDestroyImage(device, static_cast<VkImage>(image), nullptr);
    }
    Free(allocation);
}

void MemoryAllocator::DestroyBuffer(VkBuffer buffer, MemoryAllocation allocation) {
    if (buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, buffer, nullptr);
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
