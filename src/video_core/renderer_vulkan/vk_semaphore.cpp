// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdint.h>
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_semaphore.h"

#include <retro_atomic.h>
#include "common/assert.h"

namespace Vulkan {

constexpr u64 WAIT_TIMEOUT = UINT64_MAX;

Semaphore::Semaphore(const Instance& instance_)
    : instance{instance_}, device{static_cast<VkDevice>(instance_.GetDevice())},
      semaphore{VK_NULL_HANDLE} {
    VkSemaphoreTypeCreateInfo type_ci{};
    type_ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    type_ci.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    type_ci.initialValue = 0;
    VkSemaphoreCreateInfo semaphore_ci{};
    semaphore_ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphore_ci.pNext = &type_ci;
    const VkResult result = vkCreateSemaphore(device, &semaphore_ci, nullptr, &semaphore);
    ASSERT_MSG(result == VK_SUCCESS, "Failed to create master semaphore: %s", ResultName(result));
}

Semaphore::~Semaphore() {
    vkDestroySemaphore(device, semaphore, nullptr);
}

void Semaphore::Refresh() {
    u64 this_tick{};
    u64 counter{};
    do {
        this_tick = static_cast<u64>(retro_atomic_load_acquire_64(&gpu_tick));
        const VkResult counter_result = vkGetSemaphoreCounterValue(device, semaphore, &counter);
        if (counter_result == VK_ERROR_DEVICE_LOST) {
            instance.ReportDeviceLost();
        }
        ASSERT_MSG(counter_result == VK_SUCCESS, "Failed to get master semaphore value: %s",
                   ResultName(counter_result));
        if (counter < this_tick) {
            return;
        }
    } while (!(retro_atomic_cas_64(&gpu_tick, this_tick, counter) ||
               ((this_tick = static_cast<u64>(retro_atomic_load_acquire_64(&gpu_tick))), false)));
}

void Semaphore::Wait(u64 tick) {
    // No need to wait if the GPU is ahead of the tick
    if (IsFree(tick)) {
        return;
    }
    // Update the GPU tick and try again
    Refresh();
    if (IsFree(tick)) {
        return;
    }

    // If none of the above is hit, fallback to a regular wait
    VkSemaphoreWaitInfo wait_info{};
    wait_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
    wait_info.semaphoreCount = 1;
    wait_info.pSemaphores = &semaphore;
    wait_info.pValues = &tick;

    while (vkWaitSemaphores(device, &wait_info, WAIT_TIMEOUT) != VK_SUCCESS) {
    }
    Refresh();
}

} // namespace Vulkan
