// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdlib.h>
#include <string.h>

#include "common/assert.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_resource_pool.h"
#include "video_core/renderer_vulkan/vk_semaphore.h"

namespace Vulkan {

ResourcePool::ResourcePool(Semaphore* work_semaphore_, size_t grow_step_)
    : work_semaphore{work_semaphore_}, grow_step{grow_step_} {}

ResourcePool::~ResourcePool() {
    free(ticks);
}

size_t ResourcePool::Search(size_t begin, size_t end, u64 gpu_tick) {
    for (size_t i = begin; i < end; ++i) {
        if (gpu_tick >= ticks[i]) {
            ticks[i] = work_semaphore->CurrentTick();
            return i;
        }
    }
    return end;
}

size_t ResourcePool::CommitResource() {
    u64 gpu_tick = work_semaphore->KnownGpuTick();
    // Try to find a free resource from the hinted position to the end.
    size_t found = Search(hint_iterator, num_ticks, gpu_tick);
    if (found == num_ticks) {
        // Refresh semaphore to query updated results
        work_semaphore->Refresh();
        gpu_tick = work_semaphore->KnownGpuTick();
        found = Search(hint_iterator, num_ticks, gpu_tick);
    }
    if (found == num_ticks) {
        // Search from beginning to the hinted position.
        found = Search(0, hint_iterator, gpu_tick);
        if (found == hint_iterator) {
            // Both searches failed, the pool is full; handle it.
            found = ManageOverflow();
            ticks[found] = work_semaphore->CurrentTick();
        }
    }

    // Free iterator is hinted to the resource after the one that's been commited.
    hint_iterator = (found + 1) % num_ticks;
    return found;
}

size_t ResourcePool::ManageOverflow() {
    const size_t old_capacity = num_ticks;
    u64* const grown = static_cast<u64*>(realloc(ticks, (old_capacity + grow_step) * sizeof(u64)));
    ASSERT_MSG(grown, "Out of memory growing a resource pool");
    ticks = grown;
    memset(ticks + old_capacity, 0, grow_step * sizeof(u64));
    num_ticks = old_capacity + grow_step;
    Allocate(old_capacity, old_capacity + grow_step);
    return old_capacity;
}

constexpr size_t COMMAND_BUFFER_POOL_SIZE = 4;

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
    free(cmd_buffers);
}

void CommandPool::Allocate(size_t begin, size_t end) {
    VkCommandBuffer* const grown =
        static_cast<VkCommandBuffer*>(realloc(cmd_buffers, end * sizeof(VkCommandBuffer)));
    ASSERT_MSG(grown, "Out of memory growing the command pool");
    cmd_buffers = grown;

    VkCommandBufferAllocateInfo buffer_alloc_info{};
    buffer_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    buffer_alloc_info.commandPool = cmd_pool;
    buffer_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    buffer_alloc_info.commandBufferCount = COMMAND_BUFFER_POOL_SIZE;

    const VkResult result =
        vkAllocateCommandBuffers(device, &buffer_alloc_info, cmd_buffers + begin);
    ASSERT(result == VK_SUCCESS);

    for (size_t i = begin; i < end; ++i) {
        SetObjectName(instance.GetDevice(), vk::CommandBuffer{cmd_buffers[i]},
                      "CommandPool: Command Buffer %zu", i);
    }
}

vk::CommandBuffer CommandPool::Commit() {
    const size_t index = CommitResource();
    return vk::CommandBuffer{cmd_buffers[index]};
}

DescriptorHeap::DescriptorHeap(const Instance& instance, Semaphore* work_semaphore_,
                               const VkDescriptorPoolSize* pool_sizes_, u32 num_pool_sizes_,
                               u32 descriptor_heap_count_)
    : device{static_cast<VkDevice>(instance.GetDevice())}, work_semaphore{work_semaphore_},
      descriptor_heap_count{descriptor_heap_count_}, pool_sizes{pool_sizes_},
      num_pool_sizes{num_pool_sizes_} {
    // Room for a few pools waiting on the GPU, made here: it doubles only past that
    pending_capacity = 8;
    pending_pools = static_cast<PendingPool*>(malloc(pending_capacity * sizeof(PendingPool)));
    ASSERT_MSG(pending_pools, "Out of memory making the descriptor heap");
    // One entry per set layout in use; made once, it doubles only past that
    u64_map_init(&descriptor_sets, sizeof(DescSetBatch), 256);
    CreateDescriptorPool();
}

DescriptorHeap::~DescriptorHeap() {
    u64_map_free(&descriptor_sets);
    vkDestroyDescriptorPool(device, curr_pool, nullptr);
    for (u32 i = 0; i < pending_count; ++i) {
        const PendingPool& pending = pending_pools[(pending_head + i) % pending_capacity];
        work_semaphore->Wait(pending.tick);
        vkDestroyDescriptorPool(device, pending.pool, nullptr);
    }
    free(pending_pools);
}

vk::DescriptorSet DescriptorHeap::Commit(vk::DescriptorSetLayout set_layout) {
    const VkDescriptorSetLayout layout = static_cast<VkDescriptorSetLayout>(set_layout);
    const u64 set_key = reinterpret_cast<u64>(layout);
    DescSetBatch* batch =
        static_cast<DescSetBatch*>(u64_map_insert(&descriptor_sets, set_key, nullptr));
    ASSERT(batch);

    // Check if allocated sets exist and pick one.
    if (batch->count != 0) {
        return vk::DescriptorSet{batch->sets[--batch->count]};
    }

    VkDescriptorSetLayout layouts[DescriptorSetBatch];
    for (u32 i = 0; i < DescriptorSetBatch; ++i) {
        layouts[i] = layout;
    }

    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = curr_pool;
    alloc_info.descriptorSetCount = DescriptorSetBatch;
    alloc_info.pSetLayouts = layouts;

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
    if (pending_count == pending_capacity) {
        // Unrolled into a ring twice the size
        PendingPool* const grown =
            static_cast<PendingPool*>(malloc(pending_capacity * 2 * sizeof(PendingPool)));
        ASSERT_MSG(grown, "Out of memory growing the descriptor heap");
        for (u32 i = 0; i < pending_count; ++i) {
            grown[i] = pending_pools[(pending_head + i) % pending_capacity];
        }
        free(pending_pools);
        pending_pools = grown;
        pending_capacity *= 2;
        pending_head = 0;
    }
    pending_pools[(pending_head + pending_count) % pending_capacity] = {
        curr_pool, work_semaphore->CurrentTick()};
    ++pending_count;
    const PendingPool oldest = pending_pools[pending_head];
    if (work_semaphore->IsFree(oldest.tick)) {
        curr_pool = oldest.pool;
        pending_head = (pending_head + 1) % pending_capacity;
        --pending_count;

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
    pool_info.poolSizeCount = num_pool_sizes;
    pool_info.pPoolSizes = pool_sizes;
    const VkResult result = vkCreateDescriptorPool(device, &pool_info, nullptr, &curr_pool);
    ASSERT_MSG(result == VK_SUCCESS, "Failed to create descriptor pool: %s",
               vk::to_string(static_cast<vk::Result>(result)).c_str());
}

} // namespace Vulkan
