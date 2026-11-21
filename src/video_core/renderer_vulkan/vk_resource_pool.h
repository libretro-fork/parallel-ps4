// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/types.h"
#include "common/u64_map.h"
#include "video_core/renderer_vulkan/vk_common.h"

namespace Vulkan {

class Instance;
class Semaphore;

/**
 * Handles a pool of resources protected by fences. Manages resource overflow allocating more
 * resources.
 */
class ResourcePool {
public:
    explicit ResourcePool(Semaphore* work_semaphore, size_t grow_step);
    virtual ~ResourcePool();

    ResourcePool(const ResourcePool&) = delete;
    ResourcePool& operator=(const ResourcePool&) = delete;

protected:
    size_t CommitResource();

    /// Called when a chunk of resources have to be allocated.
    virtual void Allocate(size_t begin, size_t end) = 0;

private:
    /// Manages pool overflow allocating new resources.
    size_t ManageOverflow();

    /// The first free resource in [begin, end), taken for the current tick; @end when none
    size_t Search(size_t begin, size_t end, u64 gpu_tick);

protected:
    Semaphore* work_semaphore{};
    size_t grow_step = 0;     ///< Number of new resources created after an overflow
    size_t hint_iterator = 0; ///< Hint to where the next free resources is likely to be found
    u64* ticks = nullptr;     ///< Ticks for each resource
    size_t num_ticks = 0;
};

class CommandPool final : public ResourcePool {
public:
    explicit CommandPool(const Instance& instance, Semaphore* work_semaphore);
    ~CommandPool() override;

    void Allocate(size_t begin, size_t end) override;

    VkCommandBuffer Commit();

private:
    const Instance& instance;
    VkDevice device;
    VkCommandPool cmd_pool{VK_NULL_HANDLE};
    VkCommandBuffer* cmd_buffers = nullptr;
};

class DescriptorHeap final {
    static constexpr u32 DescriptorSetBatch = 32;

public:
    DescriptorHeap(const DescriptorHeap&) = delete;
    DescriptorHeap& operator=(const DescriptorHeap&) = delete;

    /// @pool_sizes must outlive the heap
    explicit DescriptorHeap(const Instance& instance, Semaphore* work_semaphore,
                            const VkDescriptorPoolSize* pool_sizes, u32 num_pool_sizes,
                            u32 descriptor_heap_count = 1024);
    ~DescriptorHeap();

    VkDescriptorSet Commit(VkDescriptorSetLayout set_layout);

private:
    void CreateDescriptorPool();

private:
    VkDevice device;
    Semaphore* work_semaphore;
    u32 descriptor_heap_count;
    const VkDescriptorPoolSize* pool_sizes;
    u32 num_pool_sizes;
    VkDescriptorPool curr_pool{VK_NULL_HANDLE};
    /// Pools out of sets, oldest first, each with the tick after which it can be reset: a ring
    /// that doubles when full
    struct PendingPool {
        VkDescriptorPool pool;
        u64 tick;
    };
    PendingPool* pending_pools = nullptr;
    u32 pending_capacity = 0;
    u32 pending_head = 0;
    u32 pending_count = 0;
    // Sets allocated ahead for one layout, taken from the back
    struct DescSetBatch {
        VkDescriptorSet sets[DescriptorSetBatch];
        u32 count;
    };
    u64_map_t descriptor_sets{}; // set layout handle -> DescSetBatch
};

} // namespace Vulkan
