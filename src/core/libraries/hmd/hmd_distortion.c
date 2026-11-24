/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <boolean.h>

#include "common/c_bridge.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/hmd/hmd.h"
#include "core/libraries/hmd/hmd_error.h"

static bool g_library_initialized = false;

s32 PS4_SYSV_ABI sceHmdDistortionInitialize(void* reserved) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdDistortionInitialize", "(STUBBED) called");
    g_library_initialized = true;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdDistortionGet2dVrCommand(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdDistortionGet2dVrCommand", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdDistortionGetCompoundEyeCorrectionCommand(void) {
    /* Stubbed on real hardware. */
    return ORBIS_HMD_ERROR_PARAMETER_INVALID;
}

s32 PS4_SYSV_ABI sceHmdDistortionGetCorrectionCommand(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdDistortionGetCorrectionCommand",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdDistortionGetWideNearCorrectionCommand(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdDistortionGetWideNearCorrectionCommand",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdDistortionGetWorkMemoryAlign(void) {
    return 0x400;
}

s32 PS4_SYSV_ABI sceHmdDistortionGetWorkMemorySize(void) {
    return 0x20000;
}

s32 PS4_SYSV_ABI sceHmdDistortionSetOutputMinColor(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdDistortionSetOutputMinColor",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI Hmd_Func_B26430EA74FC3DC0(void) {
    /* Stubbed on real hardware. */
    return ORBIS_HMD_ERROR_PARAMETER_INVALID;
}

s32 PS4_SYSV_ABI sceHmdGetDistortionParams(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdGetDistortionParams", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdDistortionTerminate(void) {
    /* Internal (non-exported) library function for terminating the distortion sub-library. */
    if (!g_library_initialized) {
        return ORBIS_HMD_ERROR_NOT_INITIALIZED;
    }
    g_library_initialized = false;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI Hmd_Func_B614F290B67FB59B(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "Func_B614F290B67FB59B", "(STUBBED) called");
    return ORBIS_OK;
}
