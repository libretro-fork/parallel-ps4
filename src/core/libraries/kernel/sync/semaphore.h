// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>

#include <retro_atomic.h>
#include "common/assert.h"
#include "common/types.h"

#ifdef _WIN64
#include <windows.h>
#elif defined(__APPLE__)
#include <dispatch/dispatch.h>
#else
#include <semaphore>
#endif

#ifdef SHADPS4_ENABLE_FEX_GUEST_CPU
namespace Core::Fex {
void FlushPendingGuestOrbisSignal() noexcept;
} // namespace Core::Fex
#endif

namespace Libraries::Kernel {

void GameNoteWake(retro_atomic_int_t* word); // core/libraries/kernel/time.h

#ifdef _WIN64
/// Win32 semaphore objects of semaphores that have gone, emptied and kept for the next ones of
/// the same maximum: a thread's record makes two each time it is used again, which made two
/// kernel objects and closed two each time a game made a thread. A fixed set of slots, taken
/// and given with an exchange each; an object with no slot free is closed as before.
template <s64 max>
struct SemaphoreObjects {
    static constexpr int Slots = 256;
    static inline retro_atomic_ptr_t slots[Slots]{};

    static HANDLE Take() {
        for (int i = 0; i < Slots; i++) {
            void* const object = retro_atomic_load_acquire_ptr(&slots[i]);
            if (object != nullptr && retro_atomic_cas_ptr(&slots[i], object, nullptr)) {
                return static_cast<HANDLE>(object);
            }
        }
        return nullptr;
    }

    static bool Give(HANDLE object) {
        for (int i = 0; i < Slots; i++) {
            if (retro_atomic_load_acquire_ptr(&slots[i]) == nullptr &&
                retro_atomic_cas_ptr(&slots[i], nullptr, object)) {
                return true;
            }
        }
        return false;
    }
};
#endif

template <s64 max>
class Semaphore {
public:
    Semaphore(s32 initialCount)
#if !defined(_WIN64) && !defined(__APPLE__)
        : sem{initialCount}
#endif
    {
#ifdef _WIN64
        sem = SemaphoreObjects<max>::Take();
        if (sem != nullptr) {
            // Empty, as it was given back
            if (initialCount > 0) {
                ReleaseSemaphore(sem, initialCount, nullptr);
            }
            return;
        }
        sem = CreateSemaphore(nullptr, initialCount, max, nullptr);
        ASSERT_MSG(sem != nullptr, "Failed to create Win32 semaphore");
#elif defined(__APPLE__)
        sem = dispatch_semaphore_create(initialCount);
        ASSERT_MSG(sem != nullptr, "Failed to create dispatch semaphore");
#endif
    }

    ~Semaphore() {
#ifdef _WIN64
        // Emptied, without running anything queued to this thread (not alertable), and kept
        while (WaitForSingleObjectEx(sem, 0, FALSE) == WAIT_OBJECT_0) {
        }
        if (!SemaphoreObjects<max>::Give(sem)) {
            CloseHandle(sem);
        }
#elif defined(__APPLE__)
        dispatch_release(sem);
#endif
    }

    /// Bumped by every release, before it: a game thread parked on it (GameSemaWait) is woken
    /// once it has moved on (game_clock_park)
    retro_atomic_int_t releases{};

    void release() {
        retro_atomic_fetch_add_seq_cst_int(&releases, 1);
#ifdef _WIN64
        ASSERT_MSG(ReleaseSemaphore(sem, 1, nullptr) != 0, "Failed to release Win32 semaphore: {}",
                   GetLastError());
#elif defined(__APPLE__)
        dispatch_semaphore_signal(sem);
#else
        sem.release();
#endif
        GameNoteWake(&releases);
    }

    void acquire() {
#ifdef _WIN64
        for (;;) {
            u64 res = WaitForSingleObjectEx(sem, INFINITE, true);
            if (res == WAIT_OBJECT_0) {
                return;
            }
            ASSERT_MSG(res == WAIT_IO_COMPLETION,
                       "Unexpected Win32 semaphore wait result {:#x}: {}", res, GetLastError());
        }
#elif defined(__APPLE__)
        for (;;) {
            const auto res = dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
            if (res == 0) {
                return;
            }
        }
#else
#ifdef SHADPS4_ENABLE_FEX_GUEST_CPU
        // A guest signal handler queued for this thread runs while it waits
        for (;;) {
            if (sem.try_acquire_for(std::chrono::milliseconds{25})) {
                Core::Fex::FlushPendingGuestOrbisSignal();
                return;
            }
            Core::Fex::FlushPendingGuestOrbisSignal();
        }
#else
        sem.acquire();
#endif
#endif
    }

    bool try_acquire() {
#ifdef _WIN64
        return WaitForSingleObjectEx(sem, 0, true) == WAIT_OBJECT_0;
#elif defined(__APPLE__)
        return dispatch_semaphore_wait(sem, DISPATCH_TIME_NOW) == 0;
#else
        return sem.try_acquire();
#endif
    }

    // Consume a pending permit without entering an alertable wait. Windows needs a dedicated
    // non-alertable call; the native Darwin and standard C++ semaphore calls already behave so.
    bool try_acquire_pending() {
#ifdef _WIN64
        return WaitForSingleObjectEx(sem, 0, false) == WAIT_OBJECT_0;
#elif defined(__APPLE__)
        return dispatch_semaphore_wait(sem, DISPATCH_TIME_NOW) == 0;
#else
        return sem.try_acquire();
#endif
    }

    template <class Rep, class Period>
    bool try_acquire_for(const std::chrono::duration<Rep, Period>& rel_time) {
#ifdef _WIN64
        using Clock = std::chrono::steady_clock;
        if (rel_time <= std::chrono::duration<Rep, Period>::zero()) {
            return try_acquire_pending();
        }

        const auto now = Clock::now();
        const auto clock_duration = std::chrono::duration_cast<Clock::duration>(rel_time);
        const auto deadline = clock_duration < Clock::time_point::max() - now
                                  ? now + clock_duration
                                  : Clock::time_point::max();
        for (;;) {
            const auto current = Clock::now();
            if (current >= deadline) {
                return try_acquire_pending();
            }

            const auto remaining_ms =
                std::chrono::ceil<std::chrono::milliseconds>(deadline - current);
            constexpr auto MaxFiniteWait = static_cast<s64>(INFINITE) - 1;
            const DWORD timeout_ms =
                static_cast<DWORD>(std::min<s64>(remaining_ms.count(), MaxFiniteWait));
            const DWORD res = WaitForSingleObjectEx(sem, timeout_ms, true);
            if (res == WAIT_OBJECT_0) {
                return true;
            }
            if (res != WAIT_IO_COMPLETION && res != WAIT_TIMEOUT) {
                return false;
            }
        }
#elif defined(__APPLE__)
        const auto rel_time_ns = std::chrono::ceil<std::chrono::nanoseconds>(rel_time);
        const auto timeout = dispatch_time(DISPATCH_TIME_NOW, rel_time_ns.count());
        return dispatch_semaphore_wait(sem, timeout) == 0;
#else
#ifdef SHADPS4_ENABLE_FEX_GUEST_CPU
        const auto deadline = std::chrono::steady_clock::now() + rel_time;
        for (;;) {
            const auto now = std::chrono::steady_clock::now();
            if (now >= deadline) {
                const bool acquired = sem.try_acquire();
                Core::Fex::FlushPendingGuestOrbisSignal();
                return acquired;
            }
            const auto slice = std::min<std::chrono::nanoseconds>(
                deadline - now, std::chrono::milliseconds{25});
            if (sem.try_acquire_for(slice)) {
                Core::Fex::FlushPendingGuestOrbisSignal();
                return true;
            }
            Core::Fex::FlushPendingGuestOrbisSignal();
        }
#else
        return sem.try_acquire_for(rel_time);
#endif
#endif
    }

    template <class Clock, class Duration>
    bool try_acquire_until(const std::chrono::time_point<Clock, Duration>& abs_time) {
        const auto current = Clock::now();
        if (current >= abs_time) {
            return try_acquire_pending();
        }
        return try_acquire_for(abs_time - current);
    }

private:
#ifdef _WIN64
    HANDLE sem;
#elif defined(__APPLE__)
    dispatch_semaphore_t sem;
#else
    std::counting_semaphore<max> sem;
#endif
};

using BinarySemaphore = Semaphore<1>;
// A thread can receive one normal synchronization wake, one cancellation wake and its sleep's
// timer on the game's time (Pthread::SleepUntil) concurrently.
using WakeSemaphore = Semaphore<3>;
using CountingSemaphore = Semaphore<0x7FFFFFFF /*ORBIS_KERNEL_SEM_VALUE_MAX*/>;

} // namespace Libraries::Kernel
