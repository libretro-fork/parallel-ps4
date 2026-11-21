/* SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <boolean.h>
#include <stddef.h>
#include <string.h>

#include "common/c_bridge.h"
#include "core/libraries/companion/companion_error.h"
#include "core/libraries/companion/companion_util.h"
#include "core/libraries/error_codes.h"

s32 PS4_SYSV_ABI sceCompanionUtilGetEvent(sceCompanionUtilEvent* outEvent) {
    u32 ret = ORBIS_COMPANION_UTIL_NO_EVENT;

    c_log(C_LOG_LIB_COMPANION_UTIL, C_LOG_LEVEL_DEBUG, "sceCompanionUtilGetEvent",
          "(STUBBED) called ret: 0x%x", ret);
    return ret;
}

s32 PS4_SYSV_ABI sceCompanionUtilGetRemoteOskEvent(void) {
    c_log(C_LOG_LIB_COMPANION_UTIL, C_LOG_LEVEL_ERROR, "sceCompanionUtilGetRemoteOskEvent",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCompanionUtilInitialize(void) {
    c_log(C_LOG_LIB_COMPANION_UTIL, C_LOG_LEVEL_ERROR, "sceCompanionUtilInitialize",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCompanionUtilOptParamInitialize(void) {
    c_log(C_LOG_LIB_COMPANION_UTIL, C_LOG_LEVEL_ERROR, "sceCompanionUtilOptParamInitialize",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCompanionUtilTerminate(void) {
    c_log(C_LOG_LIB_COMPANION_UTIL, C_LOG_LEVEL_ERROR, "sceCompanionUtilTerminate",
          "(STUBBED) called");
    return ORBIS_OK;
}
