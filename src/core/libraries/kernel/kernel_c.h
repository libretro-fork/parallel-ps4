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
