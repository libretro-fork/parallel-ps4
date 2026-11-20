// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstddef>
#include <optional>

#include "common/assert.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_resource_pool.h"
#include "video_core/renderer_vulkan/vk_semaphore.h"

namespace Vulkan {

ResourcePool::ResourcePool(Semaphore* work_semaphore_, std::size_t grow_step_)
    : work_semaphore{work_semaphore_}, grow_step{grow_step_} {}

std::size_t ResourcePool::CommitResource() {
    u64 gpu_tick = work_semaphore->KnownGpuTick();
    const auto search = [this, gpu_tick](std::size_t begin,
                                         std::size_t end) -> std::optional<std::size_t> {
        for (std::size_t iterator = begin; iterator < end; ++iterator) {
            if (gpu_tick >= ticks[iterator]) {
                ticks[iterator] = work_semaphore->CurrentTick();
                return iterator;
            }
        }
        return std::nullopt;
    };

    // Try to find a free resource from the hinted position to the end.
    auto found = search(hint_iterator, ticks.size());
    if (!found) {
        // Refresh semaphore to query updated results
        work_semaphore->Refresh();
        gpu_tick = work_semaphore->KnownGpuTick();
        found = search(hint_iterator, ticks.size());
    }
    if (!found) {
        // Search from beginning to the hinted position.
        found = search(0, hint_iterator);
        if (!found) {
            // Both searches failed, the pool is full; handle it.
            const std::size_t free_resource = ManageOverflow();

            ticks[free_resource] = work_semaphore->CurrentTick();
            found = free_resource;
        }
    }

    // Free iterator is hinted to the resource after the one that's been commited.
    hint_iterator = (*found + 1) % ticks.size();
    return *found;
}

std::size_t ResourcePool::ManageOverflow() {
    const std::size_t old_capacity = ticks.size();
    ticks.resize(old_capacity + grow_step);
    Allocate(old_capacity, old_capacity + grow_step);
    return old_capacity;
}

constexpr std::size_t COMMAND_BUFFER_POOL_SIZE = 4;

CommandPool::CommandPool(const Instance& instance, Semaphore* work_semaphore)
    : ResourcePool{work_semaphore, COMMAND_BUFFER_POOL_SIZE}, instance{instance},
      device{static_cast<VkDevice>(instance.GetDevice())} {
    VkCommandPoolCreateInfo pool_create_info{};
    pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_create_info.flags =
        VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_create_info.queueFamilyIndex = instance.GetGraphicsQueueFamilyIndex();
    const VkResult result = vkCreateCommandPool(device, &pool_create_info, nullptr, &cmd_pool);
    ASSERT_MSG(result == VK_SUCCESS, "Failed to create command pool: %s",
               vk::to_string(static_cast<vk::Result>(result)).c_str());
    SetObjectName(instance.GetDevice(), vk::CommandPool{cmd_pool}, "CommandPool");
}

CommandPool::~CommandPool() {
    // Its command buffers go with it
    vkDestroyCommandPool(device, cmd_pool, nullptr);
}

void CommandPool::Allocate(std::size_t begin, std::size_t end) {
    cmd_buffers.resize(end);

    VkCommandBufferAllocateInfo buffer_alloc_info{};
    buffer_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    buffer_alloc_info.commandPool = cmd_pool;
    buffer_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    buffer_alloc_info.commandBufferCount = COMMAND_BUFFER_POOL_SIZE;

    const VkResult result =
        vkAllocateCommandBuffers(device, &buffer_alloc_info, cmd_buffers.data() + begin);
    ASSERT(result == VK_SUCCESS);

    for (std::size_t i = begin; i < end; ++i) {
        SetObjectName(instance.GetDevice(), vk::CommandBuffer{cmd_buffers[i]},
                      "CommandPool: Command Buffer %zu", i);
    }
}

vk::CommandBuffer CommandPool::Commit() {
    const std::size_t index = CommitResource();
    return vk::CommandBuffer{cmd_buffers[index]};
}

DescriptorHeap::DescriptorHeap(const Instance& instance, Semaphore* work_semaphore_,
                               std::span<const vk::DescriptorPoolSize> pool_sizes_,
                               u32 descriptor_heap_count_)
    : device{static_cast<VkDevice>(instance.GetDevice())}, work_semaphore{work_semaphore_},
      descriptor_heap_count{descriptor_heap_count_}, pool_sizes{pool_sizes_} {
    // One entry per set layout in use; made once, it doubles only past that
    u64_map_init(&descriptor_sets, sizeof(DescSetBatch), 256);
    CreateDescriptorPool();
}

DescriptorHeap::~DescriptorHeap() {
    u64_map_free(&descriptor_sets);
    vkDestroyDescriptorPool(device, curr_pool, nullptr);
    for (const auto [pool, tick] : pending_pools) {
        work_semaphore->Wait(tick);
        vkDestroyDescriptorPool(device, pool, nullptr);
    }
}

vk::DescriptorSet DescriptorHeap::Commit(vk::DescriptorSetLayout set_layout) {
    const VkDescriptorSetLayout layout = static_cast<VkDescriptorSetLayout>(set_layout);
    const u64 set_key = std::bit_cast<u64>(layout);
    auto* batch = static_cast<DescSetBatch*>(u64_map_insert(&descriptor_sets, set_key, nullptr));
    ASSERT(batch);

    // Check if allocated sets exist and pick one.
    if (batch->count != 0) {
        return vk::DescriptorSet{batch->sets[--batch->count]};
    }

    std::array<VkDescriptorSetLayout, DescriptorSetBatch> layouts;
    layouts.fill(layout);

    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = curr_pool;
    alloc_info.descriptorSetCount = DescriptorSetBatch;
    alloc_info.pSetLayouts = layouts.data();

    // Attempt to allocate the descriptor set batch straight into the table.
    VkResult result = vkAllocateDescriptorSets(device, &alloc_info, batch->sets);
    if (result == VK_SUCCESS) {
        batch->count = DescriptorSetBatch - 1;
        return vk::DescriptorSet{batch->sets[DescriptorSetBatch - 1]};
    }

    // The pool has run out. Record current tick and place it in pending list.
    ASSERT_MSG(result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL,
               "Unexpected error during descriptor set allocation: %s",
               vk::to_string(static_cast<vk::Result>(result)).c_str());
    pending_pools.emplace_back(curr_pool, work_semaphore->CurrentTick());
    if (const auto [pool, tick] = pending_pools.front(); work_semaphore->IsFree(tick)) {
        curr_pool = pool;
        pending_pools.pop_front();

        const VkResult reset_result = vkResetDescriptorPool(device, curr_pool, 0);
        ASSERT_MSG(reset_result == VK_SUCCESS, "Unexpected error resetting descriptor pool: %s",
                   vk::to_string(static_cast<vk::Result>(reset_result)).c_str());
    } else {
        CreateDescriptorPool();
    }

    // We've changed pool so also reset descriptor batch cache.
    u64_map_clear(&descriptor_sets);
    batch = static_cast<DescSetBatch*>(u64_map_insert(&descriptor_sets, set_key, nullptr));

    // Attempt to allocate again with fresh pool.
    alloc_info.descriptorPool = curr_pool;
    result = vkAllocateDescriptorSets(device, &alloc_info, batch->sets);
    ASSERT_MSG(result == VK_SUCCESS, "Unexpected error during descriptor set allocation %s",
               vk::to_string(static_cast<vk::Result>(result)).c_str());
    batch->count = DescriptorSetBatch - 1;
    return vk::DescriptorSet{batch->sets[DescriptorSetBatch - 1]};
}

void DescriptorHeap::CreateDescriptorPool() {
    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    pool_info.maxSets = descriptor_heap_count;
    pool_info.poolSizeCount = static_cast<u32>(pool_sizes.size());
    // vk::DescriptorPoolSize is VkDescriptorPoolSize's layout (vulkan.hpp asserts it)
    pool_info.pPoolSizes = reinterpret_cast<const VkDescriptorPoolSize*>(pool_sizes.data());
    const VkResult result = vkCreateDescriptorPool(device, &pool_info, nullptr, &curr_pool);
    ASSERT_MSG(result == VK_SUCCESS, "Failed to create descriptor pool: %s",
               vk::to_string(static_cast<vk::Result>(result)).c_str());
}

} // namespace Vulkan
