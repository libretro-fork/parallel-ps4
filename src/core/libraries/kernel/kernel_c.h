/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_KERNEL_C_H
#define SHADPS4_LIBRARIES_KERNEL_C_H

/* The kernel's functions and the memory manager's queries the libraries written in C call.
 * Each is the C++ function of the same name, called as it is; defined beside it, in the kernel's
 * own files (memory.cpp, process.cpp, time.cpp, threads/pthread.cpp). */

#include <stddef.h>
#include <stdint.h>

#include <boolean.h>
#include <retro_atomic.h>
#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

/* Core::MemoryProt */
#define KERNEL_PROT_CPU_READ 1u
#define KERNEL_PROT_CPU_WRITE 2u
#define KERNEL_PROT_CPU_READ_WRITE 3u
#define KERNEL_PROT_GPU_READ 16u
#define KERNEL_PROT_GPU_WRITE 32u
#define KERNEL_PROT_GPU_READ_WRITE 48u

/* The firmware versions sceKernelGetCompiledSdkVersion gives (Common::ElfInfo::FW_*) */
#define KERNEL_FW_150 0x1500000
#define KERNEL_FW_200 0x2000000
#define KERNEL_FW_250 0x2500000
#define KERNEL_FW_300 0x3000000
#define KERNEL_FW_350 0x3500000
#define KERNEL_FW_450 0x4500000
#define KERNEL_FW_700 0x7000000
#define KERNEL_FW_750 0x7500000
#define KERNEL_FW_1150 0x11500000

/* The kernel's time structures, as OrbisKernelTimespec, OrbisKernelTimeval and
 * OrbisKernelTimezone (time.h) */
typedef struct kernel_timespec {
    s64 tv_sec;
    s64 tv_nsec;
} kernel_timespec_t;

typedef struct kernel_timeval {
    s64 tv_sec;
    s64 tv_usec;
} kernel_timeval_t;

typedef struct kernel_timezone {
    s32 tz_minuteswest;
    s32 tz_dsttime;
} kernel_timezone_t;

/* ORBIS_CLOCK_REALTIME */
#define KERNEL_CLOCK_REALTIME 0

/* sceKernelClockGettime */
s32 kernel_clock_gettime(u32 clock_id, kernel_timespec_t* tp);
/* sceKernelGettimezone */
s32 kernel_gettimezone(kernel_timezone_t* tz);
/* sceKernelConvertLocaltimeToUtc */
s32 kernel_convert_localtime_to_utc(s64 local_time, s64 param_2, s64* seconds,
                                    kernel_timezone_t* timezone, s32* dst_seconds);
/* sceKernelSettimeofday */
s32 kernel_settimeofday(kernel_timeval_t* tv, kernel_timezone_t* tz);
/* posix_clock_settime */
s32 kernel_posix_clock_settime(s32 clock_id, kernel_timespec_t* tp);
/* The kernel error of the calling thread's errno: ErrnoToSceKernelError(*__Error()) */
s32 kernel_errno_error(void);

/* The game's process time, in microseconds (sceKernelGetProcessTime); defined in time.cpp */
uint64_t kernel_process_time_usec(void);
/* sceKernelMapNamedSystemFlexibleMemory */
s32 kernel_map_named_system_flexible_memory(void** addr_in_out, u64 len, s32 prot, s32 flags,
                                            const char* name);
/* sceKernelGetCompiledSdkVersion */
s32 kernel_get_compiled_sdk_version(s32* ver);
/* The firmware version the game's ELF asks for (Common::ElfInfo::FirmwareVer) */
u32 kernel_firmware_version(void);
/* Core::Memory::IsValidGpuMapping */
bool kernel_is_valid_gpu_mapping(uintptr_t addr, u64 size);
/* The CPU wrote [@addr, @addr + @size) behind the GPU caches' backs
 * (Core::Memory::InvalidateMemory; memory.cpp) */
void kernel_invalidate_memory(uintptr_t addr, u64 size);

/* A stack of @size bytes from the threads' stack allocator (ThreadState::CreateStack), in
 * memory the GPU never tracks: NULL if none could be made (threads/stack.cpp) */
void* kernel_fiber_stack_create(u64 size);
/* Gives it back to the allocator's spare stacks */
void kernel_fiber_stack_free(void* stack, u64 size);
/* Core::Memory::VirtualQuery with no flags: 0 and the end of the area @addr is in, or the
 * kernel's error */
s32 kernel_virtual_query_end(uintptr_t addr, uintptr_t* end);

/* A service thread of the emulator's, as Kernel::Thread (threads.h) makes one: a thread of the
 * game's pthreads that runs host code, not counted by the game's time. Zeroed: not started. */
typedef struct kernel_service_thread {
    void* thread; /* the PthreadT, NULL while not started */
    void (*run)(void* arg);
    void* arg;
} kernel_service_thread_t;

/* Starts @run(@arg) on a thread of its own: false if none could be made, or one is started */
bool kernel_service_thread_start(kernel_service_thread_t* t, void (*run)(void* arg), void* arg);
/* Waits for the thread to end, if one was started; it can then be started again. It does not
 * ask the thread to stop: whoever made it has its own flag for that, and wakes it first. */
void kernel_service_thread_join(kernel_service_thread_t* t);

struct retro_eventcount;

/* The game's time now, in nanoseconds (Kernel::GameNow) */
u64 kernel_game_now(void);
/* A deadline @ns from now on the game's time, never 0 (Kernel::GameDeadlineIn) */
u64 kernel_game_deadline_in(u64 ns);
/* Waits on @ec until @ready(@ctx) is nonzero (true) or the game's time reaches @deadline
 * (false; 0: none), as the calling thread is counted (Kernel::GameWait) */
bool kernel_game_wait(struct retro_eventcount* ec, u64 deadline, int (*ready)(void* ctx),
                      void* ctx);
/* Before waking the game threads parked on an event whose epoch is @word, notes them for a
 * sound output thread's pace (Kernel::GameNoteWake) */
void kernel_game_note_wake(retro_atomic_int_t* word);

/* Libraries::Kernel::PthreadMutexT and PthreadMutexAttrT, the game's mutexes: pointers to the
 * kernel's objects, as the game holds them */
typedef struct kernel_pthread_mutex* kernel_pthread_mutex_t;
typedef struct kernel_pthread_mutexattr* kernel_pthread_mutexattr_t;
/* PthreadMutexType::Recursive */
#define KERNEL_PTHREAD_MUTEX_RECURSIVE 2u

/* posix_pthread_mutexattr_init, _settype and _destroy; scePthreadMutexInit (named);
 * posix_pthread_mutex_lock, _unlock and _destroy (threads/mutex.cpp) */
s32 kernel_pthread_mutexattr_init(kernel_pthread_mutexattr_t* attr);
s32 kernel_pthread_mutexattr_settype(kernel_pthread_mutexattr_t* attr, u32 type);
s32 kernel_pthread_mutexattr_destroy(kernel_pthread_mutexattr_t* attr);
s32 kernel_pthread_mutex_init_named(kernel_pthread_mutex_t* mutex,
                                    const kernel_pthread_mutexattr_t* attr, const char* name);
s32 kernel_pthread_mutex_lock(kernel_pthread_mutex_t* mutex);
s32 kernel_pthread_mutex_unlock(kernel_pthread_mutex_t* mutex);
s32 kernel_pthread_mutex_destroy(kernel_pthread_mutex_t* mutex);

/* The calling thread's errno, the game's (__Error; defined in time.cpp) */
s32* kernel_errno_ptr(void);

/* posix_open, posix_close, posix_lseek, sceKernelWrite and sceKernelRead (file_system.cpp) */
s32 kernel_posix_open(const char* path, s32 flags, u16 mode);
s32 kernel_posix_close(s32 fd);
s64 kernel_posix_lseek(s32 fd, s64 offset, s32 whence);
s64 kernel_write(s32 fd, const void* buf, u64 nbytes);
s64 kernel_read(s32 fd, void* buf, u64 nbytes);

/* sceKernelIsNeoMode: 1 for a PS4 Pro in its enhanced mode (process.cpp) */
s32 kernel_is_neo_mode(void);
/* The game's serial (Common::ElfInfo::GameSerial), "" if none (process.cpp) */
const char* kernel_game_serial(void);

/* ORBIS_DBG_MAX_NAME_LENGTH */
#define KERNEL_DBG_MAX_NAME_LENGTH 256

/* Libraries::Kernel::OrbisModuleInfoForUnwind */
typedef struct kernel_module_info_for_unwind {
    u64 st_size;
    char name[KERNEL_DBG_MAX_NAME_LENGTH];
    u64 eh_frame_hdr_addr;
    u64 eh_frame_addr;
    u64 eh_frame_size;
    u64 seg0_addr;
    u64 seg0_size;
} kernel_module_info_for_unwind_t;

/* sceKernelGetModuleInfoForUnwind (process.cpp) */
s32 kernel_get_module_info_for_unwind(u64 addr, s32 flags, kernel_module_info_for_unwind_t* info);

/* Libraries::Kernel::OrbisKernelEvent, an event as an event queue gives it */
typedef struct kernel_event {
    u64 ident;
    s16 filter;
    u16 flags;
    u32 fflags;
    u64 data;
    void* udata;
} kernel_event_t;

/* OrbisKernelEvent::Filter::VideoOut and OrbisKernelEvent::Flags::Add */
#define KERNEL_EVFILT_VIDEO_OUT ((s16) - 13)
#define KERNEL_EV_ADD 1u

/* An event queue's handle stands for one (Kernel::GetEqueue; equeue.cpp) */
bool kernel_equeue_exists(s64 eq);
/* EqueueInternal::AddEvent of @event, its EqueueEvent::data @data; false without the queue */
bool kernel_equeue_add_event(s64 eq, const kernel_event_t* event, void* data);
/* EqueueInternal::RemoveEvent and TriggerEvent; nothing without the queue */
void kernel_equeue_remove_event(s64 eq, u64 id, s16 filter);
void kernel_equeue_trigger_event(s64 eq, u64 ident, s16 filter, void* trigger_data);

/* Until @deadline on the game's time (Kernel::GameSleepUntil; 0: forever) (time.cpp) */
void kernel_game_sleep_until(u64 deadline);

/* The game's time, which the frontend's frames move (Kernel::GameClock; time.cpp) */
struct game_clock* kernel_game_clock(void);
/* The wakes the calling thread noted since it last asked, up to @max, into @words; marks it a
 * sound output thread (Kernel::GameTakeWakes; time.cpp) */
int kernel_game_take_wakes(retro_atomic_int_t** words, int max);

/* sceKernelGetProcessTime and sceKernelReadTsc (time.cpp) */
u64 kernel_get_process_time(void);
u64 kernel_read_tsc(void);

RETRO_END_DECLS

#endif
