/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_KERNEL_C_H
#define SHADPS4_LIBRARIES_KERNEL_C_H

/* The kernel's functions and the memory manager's queries the libraries written in C call.
 * Each is the C++ function of the same name, called as it is; defined beside it, in the kernel's
 * own files (memory.cpp, process.cpp, time.cpp). */

#include <stddef.h>
#include <stdint.h>

#include <boolean.h>
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
#define KERNEL_FW_250 0x2500000
#define KERNEL_FW_300 0x3000000
#define KERNEL_FW_350 0x3500000
#define KERNEL_FW_450 0x4500000

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
/* Core::Memory::IsValidGpuMapping */
bool kernel_is_valid_gpu_mapping(uintptr_t addr, u64 size);
/* Core::Memory::VirtualQuery with no flags: 0 and the end of the area @addr is in, or the
 * kernel's error */
s32 kernel_virtual_query_end(uintptr_t addr, uintptr_t* end);

RETRO_END_DECLS

#endif
