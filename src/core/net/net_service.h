// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <atomic>
#include <thread>

#include <rthreads/retro_eventcount.h>

#include "common/types.h"

namespace Core::Net {

// The one thread that drives libretro-common's net_http: its DNS cache and connection pool
// belong to a single thread. Jobs are posted through a lock-free ring and run in order.
class NetService {
public:
    using Job = void (*)(void* user);

    static NetService& Instance();

    // Runs job(user) on the service thread. False when the ring is full.
    bool Post(Job job, void* user);
    // Runs job(user) on the service thread and waits for it, for callers that asked to wait
    // (a blocking libSceHttp call). Never call it from a job.
    bool Run(Job job, void* user);

private:
    NetService();
    ~NetService();
    NetService(const NetService&) = delete;
    NetService& operator=(const NetService&) = delete;

    void Loop();

    static constexpr u32 Slots = 64;
    struct Cell {
        std::atomic<u32> seq{0};
        Job job = nullptr;
        void* user = nullptr;
    };
    std::array<Cell, Slots> m_cells;
    std::atomic<u32> m_tail{0}; // posters
    u32 m_head = 0;             // the service thread
    std::atomic<bool> m_stop{false};
    retro_eventcount_t m_work{}; // jobs posted, stop
    retro_eventcount_t m_done{}; // a waited-for job finished
    bool m_ok = false;
    std::thread m_thread;
};

} // namespace Core::Net
