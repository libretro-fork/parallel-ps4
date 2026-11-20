// SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "video_core/buffer_cache/buffer.h"
#include "video_core/buffer_cache/range_set.h"

namespace VideoCore {

class BufferCache;

class FaultManager {
    static constexpr u32 MaxPendingFaults = 8;

public:
    explicit FaultManager(const Vulkan::Instance& instance, Vulkan::Scheduler& scheduler,
                          BufferCache& buffer_cache, u32 sparse_pagebits, u64 sparse_num_pages);
    ~FaultManager();

    FaultManager(const FaultManager&) = delete;
    FaultManager& operator=(const FaultManager&) = delete;

    [[nodiscard]] Buffer* GetFaultBuffer() noexcept {
        return &fault_buffer;
    }

    void ProcessFaultBuffer();

private:
    VkDevice device;
    Vulkan::Scheduler& scheduler;
    BufferCache& buffer_cache;
    RangeSet fault_ranges;
    u64 sparse_pagesize;
    u64 sparse_num_pages;
    u64 fault_buffer_size;
    Buffer fault_buffer;
    Buffer download_buffer;
    u64 fault_areas[MaxPendingFaults]{};
    u32 current_area{};
    VkDescriptorSetLayout fault_process_desc_layout{VK_NULL_HANDLE};
    VkPipeline fault_process_pipeline{VK_NULL_HANDLE};
    VkPipelineLayout fault_process_pipeline_layout{VK_NULL_HANDLE};
};

} // namespace VideoCore
