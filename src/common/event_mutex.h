// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>

#include <rthreads/retro_eventcount.h>

namespace Common {

/// Mutual exclusion for what is held across calls (taken in one function, given back in
/// another), where a Common::Combiner cannot stand in. No host mutex: one atomic flag, taken
/// with an exchange; a thread that finds it taken parks on a retro_eventcount, which unlock
/// notifies only when someone is parked. Meets Lockable, for std::scoped_lock and the like.
class EventMutex {
public:
    EventMutex() {
        retro_eventcount_init(&released);
    }
    ~EventMutex() {
        retro_eventcount_free(&released);
    }
    EventMutex(const EventMutex&) = delete;
    EventMutex& operator=(const EventMutex&) = delete;

    bool try_lock() {
        return !held.load(std::memory_order_relaxed) &&
               !held.exchange(true, std::memory_order_acquire);
    }

    void lock() {
        while (!try_lock()) {
            const int key = retro_eventcount_prepare_wait(&released);
            if (try_lock()) {
                retro_eventcount_cancel_wait(&released);
                return;
            }
            retro_eventcount_commit_wait(&released, key);
        }
    }

    void unlock() {
        held.store(false, std::memory_order_release);
        retro_eventcount_notify(&released);
    }

private:
    std::atomic<bool> held{false};
    retro_eventcount_t released{};
};

} // namespace Common
