/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <boolean.h>

#include "common/c_bridge.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/hmd/hmd.h"
#include "core/libraries/hmd/hmd_error.h"

s32 PS4_SYSV_ABI sceHmdReprojectionStartMultilayer(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdReprojectionStartMultilayer",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdReprojectionAddDisplayBuffer(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdReprojectionAddDisplayBuffer",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdReprojectionClearUserEventEnd(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdReprojectionClearUserEventEnd",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdReprojectionClearUserEventStart(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdReprojectionClearUserEventStart",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdReprojectionDebugGetLastInfo(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdReprojectionDebugGetLastInfo",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdReprojectionDebugGetLastInfoMultilayer(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdReprojectionDebugGetLastInfoMultilayer",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdReprojectionFinalize(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdReprojectionFinalize", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdReprojectionFinalizeCapture(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdReprojectionFinalizeCapture",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdReprojectionInitialize(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdReprojectionInitialize", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdReprojectionInitializeCapture(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdReprojectionInitializeCapture",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdReprojectionQueryGarlicBuffAlign(void) {
    return 0x100;
}

s32 PS4_SYSV_ABI sceHmdReprojectionQueryGarlicBuffSize(void) {
    return 0x100000;
}

s32 PS4_SYSV_ABI sceHmdReprojectionQueryOnionBuffAlign(void) {
    return 0x100;
}

s32 PS4_SYSV_ABI sceHmdReprojectionQueryOnionBuffSize(void) {
    return 0x810;
}

s32 PS4_SYSV_ABI sceHmdReprojectionSetCallback(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdReprojectionSetCallback", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdReprojectionSetDisplayBuffers(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdReprojectionSetDisplayBuffers",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdReprojectionSetOutputMinColor(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdReprojectionSetOutputMinColor",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdReprojectionSetUserEventEnd(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdReprojectionSetUserEventEnd",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdReprojectionSetUserEventStart(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdReprojectionSetUserEventStart",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdReprojectionStart(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdReprojectionStart", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdReprojectionStart2dVr(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdReprojectionStart2dVr", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdReprojectionStartCapture(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdReprojectionStartCapture", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdReprojectionStartLiveCapture(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdReprojectionStartLiveCapture",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdReprojectionStartMultilayer2(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdReprojectionStartMultilayer2",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdReprojectionStartWideNear(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdReprojectionStartWideNear", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdReprojectionStartWideNearWithOverlay(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdReprojectionStartWideNearWithOverlay",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdReprojectionStartWithOverlay(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdReprojectionStartWithOverlay",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdReprojectionStop(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdReprojectionStop", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdReprojectionStopCapture(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdReprojectionStopCapture", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdReprojectionStopLiveCapture(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdReprojectionStopLiveCapture",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdReprojectionUnsetCallback(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdReprojectionUnsetCallback", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdReprojectionUnsetDisplayBuffers(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdReprojectionUnsetDisplayBuffers",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI Hmd_Func_A31A0320D80EAD99(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "Func_A31A0320D80EAD99", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI Hmd_Func_B9A6FA0735EC7E49(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "Func_B9A6FA0735EC7E49", "(STUBBED) called");
    return ORBIS_OK;
}
