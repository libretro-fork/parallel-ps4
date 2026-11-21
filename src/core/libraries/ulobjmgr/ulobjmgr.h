/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_ULOBJMGR_H
#define SHADPS4_LIBRARIES_ULOBJMGR_H

/* ulobjmgr. Registered in libs.cpp. */

#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

s32 PS4_SYSV_ABI _sceUlobjmgrRegisterObject(u64 arg0, s32 arg1, u32* arg2);
s32 PS4_SYSV_ABI _sceUlobjmgrUnregisterObject(u32 arg0);
s32 PS4_SYSV_ABI Ulobjmgr_Func_1D9F50D9CFB8054E(void);
s32 PS4_SYSV_ABI Ulobjmgr_Func_4B07893BBB77A649(u64 arg0);

RETRO_END_DECLS

#endif
