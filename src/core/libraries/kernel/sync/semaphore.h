// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <retro_atomic.h>
#include <rthreads/retro_eventcount.h>
#include "common/assert.h"
#include "common/types.h"

#ifdef SHADPS4_ENABLE_FEX_GUEST_CPU
namespace Core::Fex {
void FlushPendingGuestOrbisSignal() noexcept;
} // namespace Core::Fex
#endif

namespace Libraries::Kernel {

void GameNoteWake(retro_atomic_int_t* word); // core/libraries/kernel/time.h

/// A counting semaphore on libretro-common's lock-free primitives: the count is an atomic,
/// taken with a compare-exchange and given with an add, and only a thread that finds it empty
/// parks, on a retro_eventcount (a futex, WaitOnAddress). Nothing is made in the system for it
/// and nothing is locked: neither a release nor a take that finds the count has something
/// enters the system. A guest signal sent to a thread parked in it is a special user APC on
/// Windows, run whatever the thread waits in, and a host signal elsewhere, which ends the
/// futex's wait early: the wait is looked at again after either.
template <s64 max>
class Semaphore {
public:
    explicit Semaphore(s32 initialCount) {
        retro_atomic_int_init(&count, initialCount);
        retro_eventcount_init(&ec);
    }

    ~Semaphore() {
        retro_eventcount_free(&ec);
    }

    Semaphore(const Semaphore&) = delete;
    Semaphore& operator=(const Semaphore&) = delete;

    /// Bumped by every release, before it: a game thread parked on it (GameSemaWait) is woken
    /// once it has moved on (game_clock_park)
    retro_atomic_int_t releases{};

    void release() {
        retro_atomic_fetch_add_seq_cst_int(&releases, 1);
        s32 have = static_cast<s32>(retro_atomic_load_acquire_int(&count));
        for (;;) {
            ASSERT_MSG(static_cast<s64>(have) < max, "Semaphore released past its maximum {}", max);
            if (retro_atomic_cas_int(&count, have, have + 1)) {
                break;
            }
            have = static_cast<s32>(retro_atomic_load_acquire_int(&count));
        }
        retro_eventcount_notify(&ec);
        GameNoteWake(&releases);
    }

    void acquire() {
        for (;;) {
            if (try_acquire_pending()) {
                return;
            }
            const int key = retro_eventcount_prepare_wait(&ec);
            if (try_acquire_pending()) {
                retro_eventcount_cancel_wait(&ec);
                return;
            }
#ifdef SHADPS4_ENABLE_FEX_GUEST_CPU
            // A guest signal handler queued for this thread runs while it waits
            Core::Fex::FlushPendingGuestOrbisSignal();
#endif
            retro_eventcount_commit_wait(&ec, key);
#ifdef SHADPS4_ENABLE_FEX_GUEST_CPU
            Core::Fex::FlushPendingGuestOrbisSignal();
#endif
        }
    }

    bool try_acquire() {
        return try_acquire_pending();
    }

    /// Takes one if there is one, never waiting
    bool try_acquire_pending() {
        s32 have = static_cast<s32>(retro_atomic_load_acquire_int(&count));
        while (have > 0) {
            if (retro_atomic_cas_int(&count, have, have - 1)) {
                return true;
            }
            have = static_cast<s32>(retro_atomic_load_acquire_int(&count));
        }
        return false;
    }

private:
    retro_atomic_int_t count;
    retro_eventcount_t ec;
};

using BinarySemaphore = Semaphore<1>;
// A thread can receive one normal synchronization wake, one cancellation wake and its sleep's
// timer on the game's time (Pthread::SleepUntil) concurrently.
using WakeSemaphore = Semaphore<3>;
using CountingSemaphore = Semaphore<0x7FFFFFFF /*ORBIS_KERNEL_SEM_VALUE_MAX*/>;

} // namespace Libraries::Kernel
