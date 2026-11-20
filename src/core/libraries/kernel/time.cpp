// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <ctime>

#include <rthreads/rthreads.h>
#include "common/assert.h"
#include "common/game_clock.h"
#include "common/native_clock.h"
#include "core/libraries/kernel/kernel.h"
#include "core/libraries/kernel/orbis_error.h"
#include "core/libraries/kernel/posix_error.h"
#include "core/libraries/kernel/threads/pthread.h"
#include "core/libraries/kernel/time.h"
#include "core/libraries/libs.h"

#ifdef _WIN64
#include <windows.h>
#include "common/ntapi.h"
#else
#include <ctime>
#include <sys/resource.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace Libraries::Kernel {

static u64 initial_ptc;
static std::unique_ptr<Common::NativeClock> clock;

// The game's time: from the frontend's frames (common/game_clock.h), never a wall clock. The
// counters (the process time counter, the TSC) count it at the TSC's frequency; the date is
// the one the game started at, plus the game's time.
static game_clock_t game_clock;
static u64 boot_epoch_ns;
/// What a thread that is not a guest thread sleeps on: only its timer notifies it
static retro_eventcount_t host_sleep_ec;
// Ready before anything runs (the frame thread starts frames before the kernel's functions
// are registered): made as the program loads, once
static const bool game_clock_made = [] {
    (void)game_clock_made;
    return true;
}();

game_clock_t* GameClock() {
    return &game_clock;
}

/// The game's time in ticks of @frequency
static u64 GameTicks(u64 frequency) {
    const u64 ns = game_clock_read(&game_clock);
    return (ns / 1'000'000'000) * frequency + (ns % 1'000'000'000) * frequency / 1'000'000'000;
}

static void GameTimespec(u64 base_ns, OrbisKernelTimespec* ts) {
    const u64 ns = base_ns + game_clock_read(&game_clock);
    ts->tv_sec = static_cast<s64>(ns / 1'000'000'000);
    ts->tv_nsec = static_cast<s64>(ns % 1'000'000'000);
}

namespace {
thread_local bool game_thread = false;
thread_local game_clock_thread_t game_thread_self{};
} // namespace

bool IsGameThread() {
    return GameThreadSelf() != nullptr;
}

game_clock_thread_t* GameThreadSelf() {
    if (!game_thread) {
        return nullptr;
    }
    game_clock_resume(&game_clock, &game_thread_self.yielded);
    return &game_thread_self;
}

namespace {
thread_local bool sound_output_thread = false;
thread_local retro_atomic_int_t* noted_wakes[4]{};
thread_local int noted_count = 0;
} // namespace

void GameNoteWake(retro_atomic_int_t* word) {
    if (!sound_output_thread || noted_count >= 4) {
        return;
    }
    for (int i = 0; i < noted_count; i++) {
        if (noted_wakes[i] == word) {
            return;
        }
    }
    if (game_clock_waiting_on(&game_clock, word)) {
        noted_wakes[noted_count++] = word;
    }
}

int GameTakeWakes(retro_atomic_int_t** words, int max) {
    sound_output_thread = true;
    const int n = std::min(noted_count, max);
    for (int i = 0; i < n; i++) {
        words[i] = noted_wakes[i];
    }
    noted_count = 0;
    return n;
}

void GameThreadIdle() {
    game_clock_thread_idle(&game_clock, &game_thread_self);
}

void GameYield() {
    if (game_thread) {
        game_clock_yield(&game_clock, &game_thread_self.yielded);
    }
    sthread_yield();
}

void SetGameThread(bool counted) {
    game_thread = counted;
}

u64 GameDeadline(const OrbisKernelTimespec* abstime, u32 clock_id) {
    const u64 ns =
        static_cast<u64>(abstime->tv_sec) * 1'000'000'000 + static_cast<u64>(abstime->tv_nsec);
    switch (clock_id) {
    case ORBIS_CLOCK_REALTIME:
    case ORBIS_CLOCK_REALTIME_PRECISE:
    case ORBIS_CLOCK_SECOND:
    case ORBIS_CLOCK_REALTIME_FAST:
    case ORBIS_CLOCK_EXT_NETWORK:
    case ORBIS_CLOCK_EXT_DEBUG_NETWORK:
    case ORBIS_CLOCK_EXT_AD_NETWORK:
    case ORBIS_CLOCK_EXT_RAW_NETWORK:
        return ns > boot_epoch_ns ? ns - boot_epoch_ns : 0;
    default:
        return ns;
    }
}

u64 PS4_SYSV_ABI sceKernelGetTscFrequency() {
    return clock->GetTscFrequency();
}

u64 PS4_SYSV_ABI sceKernelGetProcessTime() {
    // The time the game is told moves on at each read: one seeds a random draw from this
    // and draws again, a call deeper, until the draw differs (game_clock_read)
    return game_clock_read(&game_clock) / 1000;
}

u64 PS4_SYSV_ABI sceKernelGetProcessTimeCounter() {
    return GameTicks(clock->GetTscFrequency());
}

u64 PS4_SYSV_ABI sceKernelGetProcessTimeCounterFrequency() {
    return clock->GetTscFrequency();
}

u64 PS4_SYSV_ABI sceKernelReadTsc() {
    return initial_ptc + GameTicks(clock->GetTscFrequency());
}

// The game's own sleep: until the game's time reaches the deadline, no host sleep. A guest
// thread sleeps on its wake semaphore (Pthread::SleepUntil), so a cancellation, which releases
// it, cuts an interruptible sleep (POSIX nanosleep, a cancellation point) short with EINTR and
// what is left; any other wake is slept through. Another thread waits on the time alone.
static bool GuestSleep(u64 ns, u64* left, bool interruptible) {
    *left = 0;
    if (ns == 0) {
        // A sleep of nothing: a yield
        GameYield();
        return true;
    }
    const u64 deadline = GameDeadlineIn(ns);
    Pthread* curthread = g_curthread;
    if (!curthread) {
        GameWait(&host_sleep_ec, deadline, [] { return false; });
        return true;
    }
    while (curthread->SleepUntil(deadline)) {
        if (interruptible && curthread->ShouldCancel()) {
            const u64 now = GameNow();
            *left = deadline > now ? deadline - now : 0;
            return false;
        }
    }
    return true;
}

u64 GameRealtimeNs() {
    return boot_epoch_ns + GameNow();
}

void GameSleepUntil(u64 deadline) {
    if (deadline == 0) {
        // Forever
        GameWait(&host_sleep_ec, 0, [] { return false; });
        return;
    }
    const u64 now = GameNow();
    u64 left;
    if (deadline > now) {
        GuestSleep(deadline - now, &left, false);
    }
}

static s32 posix_nanosleep_impl(const OrbisKernelTimespec* rqtp, OrbisKernelTimespec* rmtp,
                                const bool interruptible) {
    if (!rqtp || rqtp->tv_sec < 0 || rqtp->tv_nsec < 0 || rqtp->tv_nsec >= 1'000'000'000) {
        SetPosixErrno(EINVAL);
        return -1;
    }
    u64 left;
    const bool uninterrupted = GuestSleep(
        static_cast<u64>(rqtp->tv_sec) * 1'000'000'000 + rqtp->tv_nsec, &left, interruptible);
    if (rmtp) {
        rmtp->tv_sec = left / 1'000'000'000;
        rmtp->tv_nsec = left % 1'000'000'000;
    }
    if (!uninterrupted) {
        SetPosixErrno(EINTR);
        return -1;
    }
    return 0;
}

s32 PS4_SYSV_ABI posix_nanosleep(const OrbisKernelTimespec* rqtp, OrbisKernelTimespec* rmtp) {
    return posix_nanosleep_impl(rqtp, rmtp, true);
}

s32 PS4_SYSV_ABI sceKernelNanosleep(const OrbisKernelTimespec* rqtp, OrbisKernelTimespec* rmtp) {
    if (const auto ret = posix_nanosleep_impl(rqtp, rmtp, false); ret < 0) {
        return ErrnoToSceKernelError(*__Error());
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI posix_usleep(u32 microseconds) {
    const OrbisKernelTimespec ts = {
        .tv_sec = microseconds / 1'000'000,
        .tv_nsec = (microseconds % 1'000'000) * 1'000,
    };
    return posix_nanosleep(&ts, nullptr);
}

s32 PS4_SYSV_ABI sceKernelUsleep(u32 microseconds) {
    const OrbisKernelTimespec ts = {
        .tv_sec = microseconds / 1'000'000,
        .tv_nsec = (microseconds % 1'000'000) * 1'000,
    };
    return sceKernelNanosleep(&ts, nullptr);
}

u32 PS4_SYSV_ABI posix_sleep(u32 seconds) {
    const OrbisKernelTimespec ts = {
        .tv_sec = seconds,
        .tv_nsec = 0,
    };
    OrbisKernelTimespec rm;
    if (const auto ret = posix_nanosleep(&ts, &rm); ret < 0) {
        return *__Error() == POSIX_EINTR ? rm.tv_sec + (rm.tv_nsec == 0 ? 0 : 1) : seconds;
    }
    return 0;
}

s32 PS4_SYSV_ABI sceKernelSleep(u32 seconds) {
    return sceKernelUsleep(seconds * 1'000'000);
}

s32 PS4_SYSV_ABI posix_clock_gettime(u32 clock_id, OrbisKernelTimespec* ts) {
    if (ts == nullptr) {
        SetPosixErrno(EFAULT);
        return -1;
    }

    switch (clock_id) {
    case ORBIS_CLOCK_PROCTIME:
    case ORBIS_CLOCK_UPTIME:
    case ORBIS_CLOCK_UPTIME_PRECISE:
    case ORBIS_CLOCK_MONOTONIC:
    case ORBIS_CLOCK_MONOTONIC_PRECISE:
    case ORBIS_CLOCK_UPTIME_FAST:
    case ORBIS_CLOCK_MONOTONIC_FAST:
        GameTimespec(0, ts);
        return 0;
    case ORBIS_CLOCK_REALTIME:
    case ORBIS_CLOCK_REALTIME_PRECISE:
    case ORBIS_CLOCK_SECOND:
    case ORBIS_CLOCK_REALTIME_FAST:
    case ORBIS_CLOCK_EXT_NETWORK:
    case ORBIS_CLOCK_EXT_DEBUG_NETWORK:
    case ORBIS_CLOCK_EXT_AD_NETWORK:
    case ORBIS_CLOCK_EXT_RAW_NETWORK:
        GameTimespec(boot_epoch_ns, ts);
        return 0;
    default:
        // The CPU time clocks: what the host's threads spent, not a time of day
        break;
    }

#ifdef _WIN32
    static const auto FileTimeTo100Ns = [](FILETIME& ft) { return *reinterpret_cast<u64*>(&ft); };
    switch (clock_id) {
    case ORBIS_CLOCK_REALTIME:
    case ORBIS_CLOCK_REALTIME_PRECISE: {
        FILETIME ft;
        GetSystemTimePreciseAsFileTime(&ft);
        static constexpr u64 DeltaEpochIn100ns = 116444736000000000ULL;
        const u64 ns = FileTimeTo100Ns(ft) - DeltaEpochIn100ns;
        ts->tv_sec = ns / 10'000'000;
        ts->tv_nsec = (ns % 10'000'000) * 100;
        return 0;
    }
    case ORBIS_CLOCK_SECOND:
    case ORBIS_CLOCK_REALTIME_FAST: {
        FILETIME ft;
        GetSystemTimeAsFileTime(&ft);
        static constexpr u64 DeltaEpochIn100ns = 116444736000000000ULL;
        const u64 ns = FileTimeTo100Ns(ft) - DeltaEpochIn100ns;
        ts->tv_sec = ns / 10'000'000;
        ts->tv_nsec = (ns % 10'000'000) * 100;
        return 0;
    }
    case ORBIS_CLOCK_UPTIME:
    case ORBIS_CLOCK_UPTIME_PRECISE:
    case ORBIS_CLOCK_MONOTONIC:
    case ORBIS_CLOCK_MONOTONIC_PRECISE:
    case ORBIS_CLOCK_UPTIME_FAST:
    case ORBIS_CLOCK_MONOTONIC_FAST: {
        static LARGE_INTEGER pf = [] {
            LARGE_INTEGER res{};
            QueryPerformanceFrequency(&res);
            return res;
        }();

        LARGE_INTEGER pc{};
        if (!QueryPerformanceCounter(&pc)) {
            SetPosixErrno(EFAULT);
            return -1;
        }
        ts->tv_sec = pc.QuadPart / pf.QuadPart;
        ts->tv_nsec = ((pc.QuadPart % pf.QuadPart) * 1000'000'000) / pf.QuadPart;
        return 0;
    }
    case ORBIS_CLOCK_THREAD_CPUTIME_ID: {
        FILETIME ct, et, kt, ut;
        if (!GetThreadTimes(GetCurrentThread(), &ct, &et, &kt, &ut)) {
            SetPosixErrno(EFAULT);
            return -1;
        }
        const u64 ns = FileTimeTo100Ns(ut) + FileTimeTo100Ns(kt);
        ts->tv_sec = ns / 10'000'000;
        ts->tv_nsec = (ns % 10'000'000) * 100;
        return 0;
    }
    case ORBIS_CLOCK_VIRTUAL: {
        FILETIME ct, et, kt, ut;
        if (!GetProcessTimes(GetCurrentProcess(), &ct, &et, &kt, &ut)) {
            SetPosixErrno(EFAULT);
            return -1;
        }
        const u64 ns = FileTimeTo100Ns(ut);
        ts->tv_sec = ns / 10'000'000;
        ts->tv_nsec = (ns % 10'000'000) * 100;
        return 0;
    }
    case ORBIS_CLOCK_PROF: {
        FILETIME ct, et, kt, ut;
        if (!GetProcessTimes(GetCurrentProcess(), &ct, &et, &kt, &ut)) {
            SetPosixErrno(EFAULT);
            return -1;
        }
        const u64 ns = FileTimeTo100Ns(kt);
        ts->tv_sec = ns / 10'000'000;
        ts->tv_nsec = (ns % 10'000'000) * 100;
        return 0;
    }
    default:
        SetPosixErrno(EFAULT);
        return -1;
    }
#else
    clockid_t pclock_id;
    switch (clock_id) {
    case ORBIS_CLOCK_REALTIME:
    case ORBIS_CLOCK_REALTIME_PRECISE:
        pclock_id = CLOCK_REALTIME;
        break;
    case ORBIS_CLOCK_SECOND:
    case ORBIS_CLOCK_REALTIME_FAST:
#ifdef CLOCK_REALTIME_COARSE
        pclock_id = CLOCK_REALTIME_COARSE;
#else
        pclock_id = CLOCK_REALTIME;
#endif
        break;
    case ORBIS_CLOCK_UPTIME:
    case ORBIS_CLOCK_UPTIME_PRECISE:
    case ORBIS_CLOCK_MONOTONIC:
    case ORBIS_CLOCK_MONOTONIC_PRECISE:
        pclock_id = CLOCK_MONOTONIC;
        break;
    case ORBIS_CLOCK_UPTIME_FAST:
    case ORBIS_CLOCK_MONOTONIC_FAST:
#ifdef CLOCK_MONOTONIC_COARSE
        pclock_id = CLOCK_MONOTONIC_COARSE;
#else
        pclock_id = CLOCK_MONOTONIC;
#endif
        break;
    case ORBIS_CLOCK_THREAD_CPUTIME_ID:
        pclock_id = CLOCK_THREAD_CPUTIME_ID;
        break;
    case ORBIS_CLOCK_VIRTUAL: {
        rusage ru;
        const auto res = getrusage(RUSAGE_SELF, &ru);
        if (res < 0) {
            SetPosixErrno(EFAULT);
            return -1;
        }
        ts->tv_sec = ru.ru_utime.tv_sec;
        ts->tv_nsec = ru.ru_utime.tv_usec * 1000;
        return 0;
    }
    case ORBIS_CLOCK_PROF: {
        rusage ru;
        const auto res = getrusage(RUSAGE_SELF, &ru);
        if (res < 0) {
            SetPosixErrno(EFAULT);
            return -1;
        }
        ts->tv_sec = ru.ru_stime.tv_sec;
        ts->tv_nsec = ru.ru_stime.tv_usec * 1000;
        return 0;
    }
    default:
        SetPosixErrno(EFAULT);
        return -1;
    }

    timespec t{};
    const auto result = clock_gettime(pclock_id, &t);
    ts->tv_sec = t.tv_sec;
    ts->tv_nsec = t.tv_nsec;
    if (result < 0) {
        SetPosixErrno(errno);
        return -1;
    }
    return 0;
#endif
}

s32 PS4_SYSV_ABI sceKernelClockGettime(const u32 clock_id, OrbisKernelTimespec* ts) {
    if (const auto ret = posix_clock_gettime(clock_id, ts); ret < 0) {
        return ErrnoToSceKernelError(*__Error());
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI posix_clock_getres(u32 clock_id, OrbisKernelTimespec* res) {
    if (res == nullptr) {
        SetPosixErrno(EFAULT);
        return -1;
    }

    if (clock_id == ORBIS_CLOCK_EXT_NETWORK || clock_id == ORBIS_CLOCK_EXT_DEBUG_NETWORK ||
        clock_id == ORBIS_CLOCK_EXT_AD_NETWORK || clock_id == ORBIS_CLOCK_EXT_RAW_NETWORK) {
        res->tv_sec = 0;
        res->tv_nsec = 1;
        return 0;
    }

#ifdef _WIN32
    switch (clock_id) {
    case ORBIS_CLOCK_SECOND:
    case ORBIS_CLOCK_REALTIME_FAST: {
        DWORD timeAdjustment;
        DWORD timeIncrement;
        BOOL isTimeAdjustmentDisabled;
        if (!GetSystemTimeAdjustment(&timeAdjustment, &timeIncrement, &isTimeAdjustmentDisabled)) {
            SetPosixErrno(EFAULT);
            return -1;
        }
        res->tv_sec = 0;
        res->tv_nsec = timeIncrement * 100;
        return 0;
    }
    case ORBIS_CLOCK_REALTIME:
    case ORBIS_CLOCK_REALTIME_PRECISE:
    case ORBIS_CLOCK_UPTIME:
    case ORBIS_CLOCK_UPTIME_PRECISE:
    case ORBIS_CLOCK_MONOTONIC:
    case ORBIS_CLOCK_MONOTONIC_PRECISE:
    case ORBIS_CLOCK_UPTIME_FAST:
    case ORBIS_CLOCK_MONOTONIC_FAST: {
        LARGE_INTEGER pf;
        if (!QueryPerformanceFrequency(&pf)) {
            SetPosixErrno(EFAULT);
            return -1;
        }
        res->tv_sec = 0;
        res->tv_nsec =
            std::max(static_cast<s32>((1000000000 + (pf.QuadPart >> 1)) / pf.QuadPart), 1);
        return 0;
    }
    default:
        UNREACHABLE();
    }
#else
    clockid_t pclock_id;
    switch (clock_id) {
    case ORBIS_CLOCK_REALTIME:
    case ORBIS_CLOCK_REALTIME_PRECISE:
        pclock_id = CLOCK_REALTIME;
        break;
    case ORBIS_CLOCK_SECOND:
    case ORBIS_CLOCK_REALTIME_FAST:
#ifdef CLOCK_REALTIME_COARSE
        pclock_id = CLOCK_REALTIME_COARSE;
#else
        pclock_id = CLOCK_REALTIME;
#endif
        break;
    case ORBIS_CLOCK_UPTIME:
    case ORBIS_CLOCK_UPTIME_PRECISE:
    case ORBIS_CLOCK_MONOTONIC:
    case ORBIS_CLOCK_MONOTONIC_PRECISE:
        pclock_id = CLOCK_MONOTONIC;
        break;
    case ORBIS_CLOCK_UPTIME_FAST:
    case ORBIS_CLOCK_MONOTONIC_FAST:
#ifdef CLOCK_MONOTONIC_COARSE
        pclock_id = CLOCK_MONOTONIC_COARSE;
#else
        pclock_id = CLOCK_MONOTONIC;
#endif
        break;
    default:
        UNREACHABLE();
    }

    timespec t{};
    const auto result = clock_getres(pclock_id, &t);
    res->tv_sec = t.tv_sec;
    res->tv_nsec = t.tv_nsec;
    if (result < 0) {
        SetPosixErrno(errno);
        return -1;
    }
    return 0;
#endif
}

s32 PS4_SYSV_ABI sceKernelClockGetres(const u32 clock_id, OrbisKernelTimespec* res) {
    if (const auto ret = posix_clock_getres(clock_id, res); ret < 0) {
        return ErrnoToSceKernelError(*__Error());
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI posix_gettimeofday(OrbisKernelTimeval* tp, OrbisKernelTimezone* tz) {
    // The date the game started at, plus the game's time
    if (tp) {
        const u64 us = (boot_epoch_ns + game_clock_read(&game_clock)) / 1000;
        tp->tv_sec = static_cast<s64>(us / 1'000'000);
        tp->tv_usec = static_cast<s64>(us % 1'000'000);
    }
#ifdef _WIN64
    if (tz) {
        static int tzflag = 0;
        if (!tzflag) {
            _tzset();
            tzflag++;
        }
        time_t t = 0;
        time(&t);
        struct tm tm_val;
        localtime_s(&tm_val, &t);
        tz->tz_minuteswest = -static_cast<s32>(_timezone / 60);
        tz->tz_dsttime = (tm_val.tm_isdst > 0) ? 60 : 0;
    }
    return 0;
#else
    if (tz) {
        time_t t =
            static_cast<time_t>((boot_epoch_ns + game_clock_now(&game_clock)) / 1'000'000'000);
        struct tm tm_val;
        localtime_r(&t, &tm_val);
        s32 is_dst = (tm_val.tm_isdst > 0) ? 1 : 0;
        s32 dst_sec_val = is_dst ? 3600 : 0;
        s32 standard_offset_sec = static_cast<s32>(tm_val.tm_gmtoff) - dst_sec_val;
        tz->tz_minuteswest = standard_offset_sec / 60;
        tz->tz_dsttime = dst_sec_val / 60;
    }
    return 0;
#endif
}

s32 PS4_SYSV_ABI sceKernelGettimeofday(OrbisKernelTimeval* tp) {
    if (const auto ret = posix_gettimeofday(tp, nullptr); ret < 0) {
        return ErrnoToSceKernelError(*__Error());
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceKernelGettimezone(OrbisKernelTimezone* tz) {
    if (const auto ret = posix_gettimeofday(nullptr, tz); ret < 0) {
        return ErrnoToSceKernelError(*__Error());
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceKernelConvertLocaltimeToUtc(time_t param_1, int64_t param_2, time_t* seconds,
                                                OrbisKernelTimezone* timezone, s32* dst_seconds) {
    if (timezone) {
        sceKernelGettimezone(timezone);
        param_1 -= (timezone->tz_minuteswest + timezone->tz_dsttime) * 60;
        if (seconds) {
            *seconds = param_1;
        }
        if (dst_seconds) {
            *dst_seconds = timezone->tz_dsttime * 60;
        }
    } else {
        return ORBIS_KERNEL_ERROR_EINVAL;
    }
    return ORBIS_OK;
}

namespace Dev {
u64& GetInitialPtc() {
    return initial_ptc;
}

Common::NativeClock* GetClock() {
    return clock.get();
}

} // namespace Dev

s32 PS4_SYSV_ABI sceKernelConvertUtcToLocaltime(time_t time, time_t* local_time,
                                                struct OrbisTimesec* st, u64* dst_sec) {
    LOG_TRACE(Kernel, "Called");
#ifdef _WIN32
    time_t t = time;
    struct tm tm_val;
    localtime_s(&tm_val, &t);
    s32 is_dst = (tm_val.tm_isdst > 0) ? 1 : 0;
    s32 dst_sec_val = is_dst ? 3600 : 0;
    _tzset();
    s32 west_sec_val = -static_cast<s32>(_timezone);

    if (local_time != nullptr) {
        *local_time = time + west_sec_val + dst_sec_val;
    }

    if (st != nullptr) {
        st->t = time;
        st->west_sec = west_sec_val;
        st->dst_sec = dst_sec_val;
    }

    if (dst_sec != nullptr) {
        *dst_sec = dst_sec_val;
    }
#else
    time_t t = time;
    struct tm tm_val;
    localtime_r(&t, &tm_val);
    s32 is_dst = (tm_val.tm_isdst > 0) ? 1 : 0;
    s32 dst_sec_val = is_dst ? 3600 : 0;
    s32 west_sec_val = static_cast<s32>(tm_val.tm_gmtoff) - dst_sec_val;

    if (local_time != nullptr) {
        *local_time = time + tm_val.tm_gmtoff;
    }

    if (st != nullptr) {
        st->t = time;
        st->west_sec = west_sec_val;
        st->dst_sec = dst_sec_val;
    }

    if (dst_sec != nullptr) {
        *dst_sec = dst_sec_val;
    }
#endif // _WIN32

    return ORBIS_OK;
}

s32 PS4_SYSV_ABI posix_clock_settime(s32 clock_id, OrbisKernelTimespec* tp) {
    LOG_ERROR(Lib_Kernel, "(STUBBED) called, clock_id: %d", clock_id);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI posix_settimeofday(OrbisKernelTimeval* _tv, OrbisKernelTimezone* _tz) {
    LOG_ERROR(Lib_Kernel, "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceKernelSettimeofday(OrbisKernelTimeval* _tv, OrbisKernelTimezone* _tz) {
    s32 ret = posix_settimeofday(_tv, _tz);
    if (ret < 0) {
        return ErrnoToSceKernelError(ret);
    }
    return ret;
}

void RegisterTime(Core::Loader::SymbolsResolver* sym) {
    clock = std::make_unique<Common::NativeClock>();
    initial_ptc = clock->GetUptime();
    // The date, once, at the start: from here on the game's own time is added to it
    boot_epoch_ns = static_cast<u64>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                         std::chrono::system_clock::now().time_since_epoch())
                                         .count());
    game_clock_init(&game_clock, 60, 1);
    retro_eventcount_init(&host_sleep_ec);

    // POSIX
    LIB_FUNCTION("NhpspxdjEKU", "libkernel", 1, "libkernel", posix_nanosleep);
    LIB_FUNCTION("NhpspxdjEKU", "libScePosix", 1, "libkernel", posix_nanosleep);
    LIB_FUNCTION("yS8U2TGCe1A", "libkernel", 1, "libkernel", posix_nanosleep);
    LIB_FUNCTION("yS8U2TGCe1A", "libScePosix", 1, "libkernel", posix_nanosleep);
    LIB_FUNCTION("QcteRwbsnV0", "libkernel", 1, "libkernel", posix_usleep);
    LIB_FUNCTION("QcteRwbsnV0", "libScePosix", 1, "libkernel", posix_usleep);
    LIB_FUNCTION("0wu33hunNdE", "libkernel", 1, "libkernel", posix_sleep);
    LIB_FUNCTION("0wu33hunNdE", "libScePosix", 1, "libkernel", posix_sleep);
    LIB_FUNCTION("lLMT9vJAck0", "libkernel", 1, "libkernel", posix_clock_gettime);
    LIB_FUNCTION("lLMT9vJAck0", "libScePosix", 1, "libkernel", posix_clock_gettime);
    LIB_FUNCTION("smIj7eqzZE8", "libkernel", 1, "libkernel", posix_clock_getres);
    LIB_FUNCTION("smIj7eqzZE8", "libScePosix", 1, "libkernel", posix_clock_getres);
    LIB_FUNCTION("n88vx3C5nW8", "libkernel", 1, "libkernel", posix_gettimeofday);
    LIB_FUNCTION("n88vx3C5nW8", "libScePosix", 1, "libkernel", posix_gettimeofday);
    LIB_FUNCTION("ChCOChPU-YM", "libkernel", 1, "libkernel", sceKernelSettimeofday);
    LIB_FUNCTION("VdXIDAbJ3tQ", "libScePosix", 1, "libkernel", posix_settimeofday);
    LIB_FUNCTION("d7nUj1LOdDU", "libScePosix", 1, "libkernel", posix_clock_settime);

    // Orbis
    LIB_FUNCTION("4J2sUJmuHZQ", "libkernel", 1, "libkernel", sceKernelGetProcessTime);
    LIB_FUNCTION("fgxnMeTNUtY", "libkernel", 1, "libkernel", sceKernelGetProcessTimeCounter);
    LIB_FUNCTION("BNowx2l588E", "libkernel", 1, "libkernel",
                 sceKernelGetProcessTimeCounterFrequency);
    LIB_FUNCTION("-2IRUCO--PM", "libkernel", 1, "libkernel", sceKernelReadTsc);
    LIB_FUNCTION("1j3S3n-tTW4", "libkernel", 1, "libkernel", sceKernelGetTscFrequency);
    LIB_FUNCTION("QvsZxomvUHs", "libkernel", 1, "libkernel", sceKernelNanosleep);
    LIB_FUNCTION("1jfXLRVzisc", "libkernel", 1, "libkernel", sceKernelUsleep);
    LIB_FUNCTION("-ZR+hG7aDHw", "libkernel", 1, "libkernel", sceKernelSleep);
    LIB_FUNCTION("QBi7HCK03hw", "libkernel", 1, "libkernel", sceKernelClockGettime);
    LIB_FUNCTION("wRYVA5Zolso", "libkernel", 1, "libkernel", sceKernelClockGetres);
    LIB_FUNCTION("ejekcaNQNq0", "libkernel", 1, "libkernel", sceKernelGettimeofday);
    LIB_FUNCTION("kOcnerypnQA", "libkernel", 1, "libkernel", sceKernelGettimezone);
    LIB_FUNCTION("0NTHN1NKONI", "libkernel", 1, "libkernel", sceKernelConvertLocaltimeToUtc);
    LIB_FUNCTION("-o5uEDpN+oY", "libkernel", 1, "libkernel", sceKernelConvertUtcToLocaltime);
}

} // namespace Libraries::Kernel
