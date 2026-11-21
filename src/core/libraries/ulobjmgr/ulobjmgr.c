/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stddef.h>

#include "core/libraries/error_codes.h"
#include "core/libraries/kernel/posix_error.h"
#include "core/libraries/ulobjmgr/ulobjmgr.h"

s32 PS4_SYSV_ABI _sceUlobjmgrRegisterObject(u64 arg0, s32 arg1, u32* arg2) {
    if (arg0 == 0 || arg1 == 0 || arg2 == NULL) {
        return POSIX_EINVAL;
    }
    *arg2 = 0;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI _sceUlobjmgrUnregisterObject(u32 arg0) {
    if (arg0 >= 0x4000) {
        return POSIX_EINVAL;
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI Ulobjmgr_Func_1D9F50D9CFB8054E(void) {
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI Ulobjmgr_Func_4B07893BBB77A649(u64 arg0) {
    if (arg0 == 0) {
        return POSIX_EINVAL;
    }
    return ORBIS_OK;
}
