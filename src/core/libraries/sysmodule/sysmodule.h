/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_SYSMODULE_H
#define SHADPS4_LIBRARIES_SYSMODULE_H

/* libSceSysmodule: the system modules a game asks for, loaded from the game, from the user's
 * sys_modules or as HLE, with their dependencies. Defined in sysmodule.c and
 * sysmodule_internal.c, registered in libs.cpp. */

#include <retro_common_api.h>

#include "common/types.h"
#include "core/libraries/kernel/kernel_c.h"

RETRO_BEGIN_DECLS

typedef u16 OrbisSysModule;
typedef u32 OrbisSysModuleInternal;

s32 PS4_SYSV_ABI sceSysmoduleGetModuleHandleInternal(OrbisSysModuleInternal id, s32* handle);
s32 PS4_SYSV_ABI sceSysmoduleGetModuleInfoForUnwind(u64 addr, s32 flags,
                                                    kernel_module_info_for_unwind_t* info);
s32 PS4_SYSV_ABI sceSysmoduleIsCalledFromSysModule(void);
s32 PS4_SYSV_ABI sceSysmoduleIsCameraPreloaded(void);
s32 PS4_SYSV_ABI sceSysmoduleIsLoaded(OrbisSysModule id);
s32 PS4_SYSV_ABI sceSysmoduleIsLoadedInternal(OrbisSysModuleInternal id);
s32 PS4_SYSV_ABI sceSysmoduleLoadModule(OrbisSysModule id);
s32 PS4_SYSV_ABI sceSysmoduleLoadModuleByNameInternal(void);
s32 PS4_SYSV_ABI sceSysmoduleLoadModuleInternal(OrbisSysModuleInternal id);
s32 PS4_SYSV_ABI sceSysmoduleLoadModuleInternalWithArg(OrbisSysModuleInternal id, s32 argc,
                                                       const void* argv, u64 unk, s32* res_out);
s32 PS4_SYSV_ABI sceSysmoduleMapLibcForLibkernel(void);
s32 PS4_SYSV_ABI sceSysmodulePreloadModuleForLibkernel(void);
s32 PS4_SYSV_ABI sceSysmoduleUnloadModule(OrbisSysModule id);
s32 PS4_SYSV_ABI sceSysmoduleUnloadModuleByNameInternal(void);
s32 PS4_SYSV_ABI sceSysmoduleUnloadModuleInternal(void);
s32 PS4_SYSV_ABI sceSysmoduleUnloadModuleInternalWithArg(void);

/* Makes the library's state, once, before any call: libs.cpp's registration calls it */
void sysmodule_library_init(void);

RETRO_END_DECLS

#endif
