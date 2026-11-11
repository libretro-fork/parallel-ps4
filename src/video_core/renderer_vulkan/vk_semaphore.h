// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <condition_variable>
#include <thread>
#include <queue>
#include <retro_atomic.h>
#include "common/types.h"
#include "video_core/renderer_vulkan/vk_common.h"

namespace Vulkan {

class Instance;
class Scheduler;

class Semaphore {
public:
    explicit Semaphore(const Instance& instance_);
    ~Semaphore();

    [[nodiscard]] u64 CurrentTick() const noexcept {
        return static_cast<u64>(retro_atomic_load_acquire_64(&current_tick));
    }

    [[nodiscard]] u64 KnownGpuTick() const noexcept {
        return static_cast<u64>(retro_atomic_load_acquire_64(&gpu_tick));
    }

    [[nodiscard]] bool IsFree(u64 tick) const noexcept {
        return KnownGpuTick() >= tick;
    }

    [[nodiscard]] u64 NextTick() noexcept {
        return static_cast<u64>(retro_atomic_fetch_add_64(&current_tick, 1));
    }

    [[nodiscard]] vk::Semaphore Handle() const noexcept {
        return semaphore.get();
    }

    /// Refresh the known GPU tick
    void Refresh();

    /// Waits for a tick to be hit on the GPU
    void Wait(u64 tick);

protected:
    const Instance& instance;
    vk::UniqueSemaphore semaphore;    ///< Timeline semaphore.
    retro_atomic_64_t gpu_tick{0};    ///< Current known GPU tick.
    retro_atomic_64_t current_tick{1}; ///< Current logical tick.
};

} // namespace Vulkan
