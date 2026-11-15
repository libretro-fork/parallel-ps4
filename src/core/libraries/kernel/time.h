// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <chrono>
#include <memory>
#include <type_traits>
#include <rthreads/retro_eventcount.h>
#include <sys/types.h>
#include "common/assert.h"
#include "common/game_clock.h"
#include "common/types.h"

namespace Common {
class NativeClock;
}

namespace Core::Loader {
class SymbolsResolver;
}

namespace Libraries::Kernel {

struct OrbisKernelTimeval {
    s64 tv_sec;
    s64 tv_usec;
};

struct OrbisKernelTimezone {
    s32 tz_minuteswest;
    s32 tz_dsttime;
};

struct OrbisKernelTimespec {
    s64 tv_sec;
    s64 tv_nsec;

    std::chrono::system_clock::time_point TimePoint() const noexcept {
        using namespace std::chrono;
        const auto duration =
            duration_cast<system_clock::duration>(seconds{tv_sec} + nanoseconds{tv_nsec});
        return system_clock::time_point{duration};
    }
};

struct OrbisTimesec {
    time_t t;
    u32 west_sec;
    u32 dst_sec;
};

constexpr int ORBIS_CLOCK_REALTIME = 0;
constexpr int ORBIS_CLOCK_VIRTUAL = 1;
constexpr int ORBIS_CLOCK_PROF = 2;
constexpr int ORBIS_CLOCK_MONOTONIC = 4;
constexpr int ORBIS_CLOCK_UPTIME = 5;
constexpr int ORBIS_CLOCK_UPTIME_PRECISE = 7;
constexpr int ORBIS_CLOCK_UPTIME_FAST = 8;
constexpr int ORBIS_CLOCK_REALTIME_PRECISE = 9;
constexpr int ORBIS_CLOCK_REALTIME_FAST = 10;
constexpr int ORBIS_CLOCK_MONOTONIC_PRECISE = 11;
constexpr int ORBIS_CLOCK_MONOTONIC_FAST = 12;
constexpr int ORBIS_CLOCK_SECOND = 13;
constexpr int ORBIS_CLOCK_THREAD_CPUTIME_ID = 14;
constexpr int ORBIS_CLOCK_PROCTIME = 15;
constexpr int ORBIS_CLOCK_EXT_NETWORK = 16;
constexpr int ORBIS_CLOCK_EXT_DEBUG_NETWORK = 17;
constexpr int ORBIS_CLOCK_EXT_AD_NETWORK = 18;
constexpr int ORBIS_CLOCK_EXT_RAW_NETWORK = 19;

/// The game's time, which the frontend's frames move (common/game_clock.h)
game_clock_t* GameClock();

/// The game's time now, in nanoseconds
inline u64 GameNow() {
    return game_clock_now(GameClock());
}

/// A deadline @ns from now on the game's time: never 0, which means no deadline
inline u64 GameDeadlineIn(u64 ns) {
    const u64 deadline = GameNow() + ns;
    return deadline != 0 ? deadline : 1;
}

/// A deadline on the game's time from a time of @clock_id (a time of day, or the monotonic
/// time): 0 if it has passed
u64 GameDeadline(const struct OrbisKernelTimespec* abstime, u32 clock_id);

/// The time of day, on the game's time: the date it started at, plus the game's time
u64 GameRealtimeNs();

/// Sleeps until the game's time reaches @deadline (0: forever), whatever wakes the thread
/// meanwhile
void GameSleepUntil(u64 deadline);

/// Whether the calling thread is one of the game's that the game's time counts as running
/// (game_clock_idle and game_clock_busy): set from its start to its end. Asked where the
/// thread is about to wait or end: one that yielded counts running again from here.
bool IsGameThread();

/// The calling thread yields (sched_yield, usleep(0), a spin on a lock): a game thread waits
/// on nothing tracked, so it counts idle until it next waits or ends (game_clock_yield)
void GameYield();
void SetGameThread(bool counted);

/// Takes from @sema (a Libraries::Kernel::Semaphore) until it has something (true) or the
/// game's time reaches @deadline (false; 0: never), as a game thread not counted running
/// meanwhile. A timer on the game's time releases it, so its maximum must leave room for one
/// release more than its other wakers give.
template <typename Sema>
bool GameSemaWait(Sema& sema, u64 deadline) {
    game_clock_t* clock = GameClock();
    if (deadline != 0 && deadline <= game_clock_now(clock)) {
        return sema.try_acquire_pending();
    }
    struct Timer {
        Sema* sema;
        bool counted;
    } timer_ctx{&sema, IsGameThread()};
    const int timer = deadline != 0 ? game_clock_arm(
                                          clock, deadline,
                                          [](void* ctx) {
                                              auto* t = static_cast<Timer*>(ctx);
                                              if (t->counted) {
                                                  game_clock_busy(GameClock());
                                              }
                                              t->sema->release();
                                          },
                                          &timer_ctx)
                                    : -1;
    ASSERT_MSG(deadline == 0 || timer >= 0, "All of the game's timers are armed");
    // Parked, idle: a release of it in flight holds the time until it has run
    int slot = -1;
    if (timer_ctx.counted) {
        slot =
            game_clock_park(clock, &sema.releases, retro_atomic_load_seq_cst_int(&sema.releases));
        game_clock_idle(clock);
    }
    sema.acquire();
    // A timer that fired handed this thread its count back
    const bool fired = timer >= 0 && game_clock_disarm(clock, timer) != 0;
    if (!fired) {
        if (timer_ctx.counted) {
            game_clock_busy(clock);
        }
        game_clock_unpark(clock, slot);
        return true;
    }
    game_clock_unpark(clock, slot);
    // The timer's release, or another as well: then both are taken
    return sema.try_acquire_pending();
}

/// Waits on @ec until @ready() or the game's time reaches @deadline (0: none), as a game
/// thread not counted running meanwhile (@counted: one that is counted): no host timeout, no
/// wall clock (game_clock_wait). @return whether @ready() was what ended it.
template <typename Ready>
bool GameWaitAs(retro_eventcount_t* ec, u64 deadline, Ready&& ready, bool counted) {
    return game_clock_wait(
               GameClock(), ec, deadline,
               [](void* ctx) -> int {
                   return (*static_cast<std::remove_reference_t<Ready>*>(ctx))() ? 1 : 0;
               },
               const_cast<void*>(static_cast<const void*>(std::addressof(ready))), counted) != 0;
}

/// The same, for the calling thread as it is counted
template <typename Ready>
bool GameWait(retro_eventcount_t* ec, u64 deadline, Ready&& ready) {
    return GameWaitAs(ec, deadline, ready, IsGameThread());
}

namespace Dev {
u64& GetInitialPtc();

Common::NativeClock* GetClock();
} // namespace Dev

u64 PS4_SYSV_ABI sceKernelGetTscFrequency();
u64 PS4_SYSV_ABI sceKernelGetProcessTime();
u64 PS4_SYSV_ABI sceKernelGetProcessTimeCounter();
u64 PS4_SYSV_ABI sceKernelGetProcessTimeCounterFrequency();
u64 PS4_SYSV_ABI sceKernelReadTsc();
s32 PS4_SYSV_ABI sceKernelClockGettime(u32 clock_id, OrbisKernelTimespec* tp);
s32 PS4_SYSV_ABI sceKernelGettimezone(OrbisKernelTimezone* tz);
s32 PS4_SYSV_ABI sceKernelConvertLocaltimeToUtc(time_t param_1, int64_t param_2, time_t* seconds,
                                                OrbisKernelTimezone* timezone, s32* dst_seconds);

s32 PS4_SYSV_ABI sceKernelConvertUtcToLocaltime(time_t time, time_t* local_time, OrbisTimesec* st,
                                                u64* dst_sec);
s32 PS4_SYSV_ABI sceKernelUsleep(u32 microseconds);
s32 PS4_SYSV_ABI posix_clock_gettime(u32 clock_id, OrbisKernelTimespec* ts);
s32 PS4_SYSV_ABI posix_clock_settime(s32 clock_id, OrbisKernelTimespec* tp);
s32 PS4_SYSV_ABI posix_settimeofday(OrbisKernelTimeval* _tv, OrbisKernelTimezone* _tz);
s32 PS4_SYSV_ABI sceKernelSettimeofday(OrbisKernelTimeval* _tv, OrbisKernelTimezone* _tz);

void RegisterTime(Core::Loader::SymbolsResolver* sym);

} // namespace Libraries::Kernel
