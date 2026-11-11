// SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>        // std::scoped_lock, std::unique_lock, used with it
#include <shared_mutex> // std::shared_lock

#include <retro_atomic.h>
#include <rthreads/retro_eventcount.h>

namespace Common {

// Like std::shared_mutex, but reader has priority over writer.
//
// One atomic word holds the state, taken and given back with compare-exchanges; a thread that
// has to wait parks on a retro_eventcount, which a release notifies only when someone is
// parked. No host mutex: what blocks is the waiting the lock is for, nothing else.
class SharedFirstMutex {
public:
    SharedFirstMutex() {
        retro_eventcount_init(&released);
    }
    ~SharedFirstMutex() {
        retro_eventcount_free(&released);
    }
    SharedFirstMutex(const SharedFirstMutex&) = delete;
    SharedFirstMutex& operator=(const SharedFirstMutex&) = delete;

    void lock() {
        WaitFor([this] { return try_lock(); });
    }

    bool try_lock() {
        u32 expected = 0;
        return (retro_atomic_cas_int(&state, expected, Writer) ||
                ((expected = static_cast<u32>(retro_atomic_load_acquire_int(&state))), false));
    }

    template <typename Clock, typename Duration>
    bool try_lock_until(const std::chrono::time_point<Clock, Duration>& abs_time) {
        return WaitUntil([this] { return try_lock(); }, abs_time);
    }

    void unlock() {
        retro_atomic_store_release_int(&state, 0);
        retro_eventcount_notify(&released);
    }

    void lock_shared() {
        WaitFor([this] { return try_lock_shared(); });
    }

    bool try_lock_shared() {
        u32 current = static_cast<u32>(retro_atomic_load_relaxed_int(&state));
        while (!(current & Writer)) {
            if ((retro_atomic_cas_int(&state, current, current + 1) ||
                 ((current = static_cast<u32>(retro_atomic_load_acquire_int(&state))), false))) {
                return true;
            }
        }
        return false;
    }

    template <typename Clock, typename Duration>
    bool try_lock_shared_until(const std::chrono::time_point<Clock, Duration>& abs_time) {
        return WaitUntil([this] { return try_lock_shared(); }, abs_time);
    }

    void unlock_shared() {
        if (static_cast<u32>(retro_atomic_fetch_sub_int(&state, 1)) == 1) {
            retro_eventcount_notify(&released);
        }
    }

private:
    using u32 = std::uint32_t;
    static constexpr u32 Writer = 1u << 31; // the rest counts the readers

    template <typename F>
    void WaitFor(F&& take) {
        while (!take()) {
            const int key = retro_eventcount_prepare_wait(&released);
            if (take()) {
                retro_eventcount_cancel_wait(&released);
                return;
            }
            retro_eventcount_commit_wait(&released, key);
        }
    }

    template <typename F, typename Clock, typename Duration>
    bool WaitUntil(F&& take, const std::chrono::time_point<Clock, Duration>& abs_time) {
        while (!take()) {
            const int key = retro_eventcount_prepare_wait(&released);
            if (take()) {
                retro_eventcount_cancel_wait(&released);
                return true;
            }
            const auto left =
                std::chrono::ceil<std::chrono::microseconds>(abs_time - Clock::now()).count();
            if (left <= 0) {
                retro_eventcount_cancel_wait(&released);
                return take();
            }
            retro_eventcount_commit_wait_timeout(&released, key, left);
        }
        return true;
    }

    retro_atomic_int_t state{0};
    retro_eventcount_t released{};
};

} // namespace Common
