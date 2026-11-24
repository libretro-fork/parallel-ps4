/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <string.h>

#include <boolean.h>

#include "common/c_bridge.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/hmd/hmd.h"
#include "core/libraries/hmd/hmd_error.h"
#include "core/libraries/kernel/kernel_c.h"

static bool g_library_initialized = false;
static s32 g_firmware_version = 0;
static s32 g_internal_handle = 0;
static s32 g_user_id = -1;

s32 PS4_SYSV_ABI sceHmdInitialize(const OrbisHmdInitializeParam* param) {
    if (g_library_initialized) {
        return ORBIS_HMD_ERROR_ALREADY_INITIALIZED;
    }
    if (param == NULL) {
        return ORBIS_HMD_ERROR_PARAMETER_NULL;
    }
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_WARNING, "sceHmdInitialize",
          "PSVR headsets are not supported yet");
    if (param->reserved0 != NULL) {
        sceHmdDistortionInitialize(param->reserved0);
    }
    g_library_initialized = true;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInitialize315(const OrbisHmdInitializeParam* param) {
    if (g_library_initialized) {
        return ORBIS_HMD_ERROR_ALREADY_INITIALIZED;
    }
    if (param == NULL) {
        return ORBIS_HMD_ERROR_PARAMETER_NULL;
    }
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_WARNING, "sceHmdInitialize315",
          "PSVR headsets are not supported yet");
    g_library_initialized = true;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdOpen(s32 user_id, s32 type, s32 index, OrbisHmdOpenParam* param) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_DEBUG, "sceHmdOpen", "called");
    if (!g_library_initialized) {
        return ORBIS_HMD_ERROR_NOT_INITIALIZED;
    }
    if (type != 0 || index != 0 || param != NULL) {
        return ORBIS_HMD_ERROR_PARAMETER_INVALID;
    }
    if (g_internal_handle != 0) {
        return ORBIS_HMD_ERROR_ALREADY_OPENED;
    }
    if (user_id == HMD_USER_ID_INVALID || user_id == HMD_USER_ID_SYSTEM) {
        return ORBIS_HMD_ERROR_PARAMETER_INVALID;
    }

    /* Return positive value representing handle. */
    /* Internal libSceVrTracker logic requires this handle to be different from other devices. */
    g_user_id = user_id;
    g_internal_handle = 0xf000000;
    return g_internal_handle;
}

s32 PS4_SYSV_ABI sceHmdGet2DEyeOffset(s32 handle, OrbisHmdEyeOffset* left_offset,
                                      OrbisHmdEyeOffset* right_offset) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_DEBUG, "sceHmdGet2DEyeOffset", "called");
    if (!g_library_initialized) {
        return ORBIS_HMD_ERROR_NOT_INITIALIZED;
    }
    if (handle != g_internal_handle) {
        return ORBIS_HMD_ERROR_HANDLE_INVALID;
    }
    if (g_firmware_version >= KERNEL_FW_450) {
        /* Due to some faulty in-library checks, a missing headset results in this error */
        /* instead of the expected ORBIS_HMD_ERROR_DEVICE_DISCONNECTED error. */
        return ORBIS_HMD_ERROR_HANDLE_INVALID;
    }
    if (left_offset == NULL || right_offset == NULL) {
        return ORBIS_HMD_ERROR_PARAMETER_NULL;
    }

    /* Return default values */
    left_offset->offset_x = -0.0315;
    left_offset->offset_y = 0;
    left_offset->offset_z = 0;
    right_offset->offset_x = 0.0315;
    right_offset->offset_y = 0;
    right_offset->offset_z = 0;

    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdGetAssyError(void* data) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_DEBUG, "sceHmdGetAssyError", "called");
    if (data == NULL) {
        return ORBIS_HMD_ERROR_PARAMETER_NULL;
    }
    if (!g_library_initialized) {
        return ORBIS_HMD_ERROR_NOT_INITIALIZED;
    }

    return ORBIS_HMD_ERROR_DEVICE_DISCONNECTED;
}

s32 PS4_SYSV_ABI sceHmdGetDeviceInformation(OrbisHmdDeviceInformation* info) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_DEBUG, "sceHmdGetDeviceInformation", "called");
    if (info == NULL) {
        return ORBIS_HMD_ERROR_PARAMETER_NULL;
    }
    if (!g_library_initialized) {
        return ORBIS_HMD_ERROR_NOT_INITIALIZED;
    }

    memset(info, 0, sizeof(OrbisHmdDeviceInformation));
    info->status = ORBIS_HMD_DEVICE_STATUS_NOT_DETECTED;
    info->user_id = g_user_id;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdGetDeviceInformationByHandle(s32 handle, OrbisHmdDeviceInformation* info) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_DEBUG, "sceHmdGetDeviceInformationByHandle", "called");
    if (handle != g_internal_handle) {
        return ORBIS_HMD_ERROR_HANDLE_INVALID;
    }
    if (g_firmware_version >= KERNEL_FW_450) {
        /* Due to some faulty in-library checks, a missing headset results in this error */
        /* instead of the expected ORBIS_HMD_ERROR_DEVICE_DISCONNECTED error. */
        return ORBIS_HMD_ERROR_HANDLE_INVALID;
    }
    if (info == NULL) {
        return ORBIS_HMD_ERROR_PARAMETER_NULL;
    }
    if (!g_library_initialized) {
        return ORBIS_HMD_ERROR_NOT_INITIALIZED;
    }

    memset(info, 0, sizeof(OrbisHmdDeviceInformation));
    info->status = ORBIS_HMD_DEVICE_STATUS_NOT_DETECTED;
    info->user_id = g_user_id;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdGetFieldOfView(s32 handle, OrbisHmdFieldOfView* field_of_view) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_DEBUG, "sceHmdGetFieldOfView", "called");
    if (field_of_view == NULL) {
        return ORBIS_HMD_ERROR_PARAMETER_NULL;
    }
    if (handle != g_internal_handle) {
        return ORBIS_HMD_ERROR_HANDLE_INVALID;
    }
    if (g_firmware_version >= KERNEL_FW_450) {
        /* Due to some faulty in-library checks, a missing headset results in this error */
        /* instead of the expected ORBIS_HMD_ERROR_DEVICE_DISCONNECTED error. */
        return ORBIS_HMD_ERROR_HANDLE_INVALID;
    }
    if (!g_library_initialized) {
        return ORBIS_HMD_ERROR_NOT_INITIALIZED;
    }

    /* These values are a hardcoded return when a headset is connected. */
    /* Leaving this here for future developers. */
    /* field_of_view->tan_out = 1.20743; */
    /* field_of_view->tan_in = 1.181346; */
    /* field_of_view->tan_top = 1.262872; */
    /* field_of_view->tan_bottom = 1.262872; */

    /* Fails internally due to some internal library checks that break without a connected headset.
     */
    return ORBIS_HMD_ERROR_HANDLE_INVALID;
}

s32 PS4_SYSV_ABI sceHmdGetInertialSensorData(s32 handle, void* data, s32 unk) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdGetInertialSensorData", "(STUBBED) called");
    if (!g_library_initialized) {
        return ORBIS_HMD_ERROR_NOT_INITIALIZED;
    }
    if (handle != g_internal_handle) {
        return ORBIS_HMD_ERROR_HANDLE_INVALID;
    }
    if (g_firmware_version >= KERNEL_FW_450) {
        /* Due to some faulty in-library checks, a missing headset results in this error */
        /* instead of the expected ORBIS_HMD_ERROR_DEVICE_DISCONNECTED error. */
        return ORBIS_HMD_ERROR_HANDLE_INVALID;
    }

    return ORBIS_HMD_ERROR_DEVICE_DISCONNECTED;
}

s32 PS4_SYSV_ABI sceHmdClose(s32 handle) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_DEBUG, "sceHmdClose", "called");
    if (!g_library_initialized) {
        return ORBIS_HMD_ERROR_NOT_INITIALIZED;
    }
    if (handle != g_internal_handle) {
        return ORBIS_HMD_ERROR_HANDLE_INVALID;
    }

    g_internal_handle = 0;
    g_user_id = -1;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdTerminate(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_DEBUG, "sceHmdTerminate", "called");
    if (!g_library_initialized) {
        return ORBIS_HMD_ERROR_NOT_INITIALIZED;
    }
    sceHmdDistortionTerminate();
    g_library_initialized = false;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternal3dAudioClose(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternal3dAudioClose", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternal3dAudioOpen(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternal3dAudioOpen", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternal3dAudioSendData(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternal3dAudioSendData", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalAnotherScreenClose(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalAnotherScreenClose", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalAnotherScreenGetAudioStatus(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalAnotherScreenGetAudioStatus",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalAnotherScreenGetFadeState(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalAnotherScreenGetFadeState",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalAnotherScreenGetVideoStatus(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalAnotherScreenGetVideoStatus",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalAnotherScreenOpen(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalAnotherScreenOpen", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalAnotherScreenSendAudio(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalAnotherScreenSendAudio",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalAnotherScreenSendVideo(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalAnotherScreenSendVideo",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalAnotherScreenSetFadeAndSwitch(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalAnotherScreenSetFadeAndSwitch",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalBindDeviceWithUserId(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalBindDeviceWithUserId",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalCheckDeviceModelMk3(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalCheckDeviceModelMk3",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalCheckS3dPassModeAvailable(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalCheckS3dPassModeAvailable",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalCrashReportCancel(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalCrashReportCancel", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalCrashReportClose(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalCrashReportClose", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalCrashReportOpen(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalCrashReportOpen", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalCrashReportReadData(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalCrashReportReadData",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalCrashReportReadDataSize(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalCrashReportReadDataSize",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalCreateSharedMemory(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalCreateSharedMemory", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalDfuCheckAfterPvt(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalDfuCheckAfterPvt", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalDfuCheckPartialUpdateAvailable(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalDfuCheckPartialUpdateAvailable",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalDfuClose(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalDfuClose", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalDfuGetStatus(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalDfuGetStatus", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalDfuOpen(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalDfuOpen", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalDfuReset(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalDfuReset", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalDfuSend(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalDfuSend", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalDfuSendSize(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalDfuSendSize", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalDfuSetMode(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalDfuSetMode", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalDfuStart(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalDfuStart", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalEventInitialize(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalEventInitialize", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalGetBrightness(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalGetBrightness", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalGetCrashDumpInfo(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalGetCrashDumpInfo", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalGetDebugMode(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalGetDebugMode", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalGetDebugSocialScreenMode(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalGetDebugSocialScreenMode",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalGetDebugTextMode(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalGetDebugTextMode", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalGetDefaultLedData(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalGetDefaultLedData", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalGetDemoMode(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalGetDemoMode", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalGetDeviceInformation(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalGetDeviceInformation",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalGetDeviceInformationByHandle(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalGetDeviceInformationByHandle",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalGetDeviceStatus(OrbisHmdDeviceStatus* status) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_DEBUG, "sceHmdInternalGetDeviceStatus", "called");
    if (status == NULL) {
        return ORBIS_HMD_ERROR_PARAMETER_NULL;
    }
    /* Internal function fails with error DEVICE_DISCONNECTED */
    *status = ORBIS_HMD_DEVICE_STATUS_NOT_DETECTED;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalGetEyeStatus(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalGetEyeStatus", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalGetHmuOpticalParam(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalGetHmuOpticalParam", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalGetHmuPowerStatusForDebug(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalGetHmuPowerStatusForDebug",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalGetHmuSerialNumber(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalGetHmuSerialNumber", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalGetIPD(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalGetIPD", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalGetIpdSettingEnableForSystemService(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalGetIpdSettingEnableForSystemService",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalGetPuBuildNumber(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalGetPuBuildNumber", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalGetPuPositionParam(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalGetPuPositionParam", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalGetPuRevision(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalGetPuRevision", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalGetPUSerialNumber(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalGetPUSerialNumber", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalGetPUVersion(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalGetPUVersion", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalGetRequiredPUPVersion(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalGetRequiredPUPVersion",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalGetStatusReport(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalGetStatusReport", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalGetTv4kCapability(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalGetTv4kCapability", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalGetVirtualDisplayDepth(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalGetVirtualDisplayDepth",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalGetVirtualDisplayHeight(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalGetVirtualDisplayHeight",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalGetVirtualDisplaySize(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalGetVirtualDisplaySize",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalGetVr2dData(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalGetVr2dData", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalIsCommonDlgMiniAppVr2d(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalIsCommonDlgMiniAppVr2d",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalIsCommonDlgVr2d(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalIsCommonDlgVr2d", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalIsGameVr2d(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalIsGameVr2d", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalIsMiniAppVr2d(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalIsMiniAppVr2d", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalMapSharedMemory(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalMapSharedMemory", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalMirroringModeSetAspect(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalMirroringModeSetAspect",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalMirroringModeSetAspectDebug(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalMirroringModeSetAspectDebug",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalMmapGetCount(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalMmapGetCount", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalMmapGetModeId(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalMmapGetModeId", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalMmapGetSensorCalibrationData(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalMmapGetSensorCalibrationData",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalMmapIsConnect(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_DEBUG, "sceHmdInternalMmapIsConnect", "called");
    /* Returns 0 when device is disconnected. */
    return 0;
}

s32 PS4_SYSV_ABI sceHmdInternalPushVr2dData(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalPushVr2dData", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalRegisterEventCallback(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalRegisterEventCallback",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalResetInertialSensor(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalResetInertialSensor",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalResetLedForVrTracker(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalResetLedForVrTracker",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalResetLedForVsh(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalResetLedForVsh", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalSeparateModeClose(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalSeparateModeClose", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalSeparateModeGetAudioStatus(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalSeparateModeGetAudioStatus",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalSeparateModeGetVideoStatus(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalSeparateModeGetVideoStatus",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalSeparateModeOpen(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalSeparateModeOpen", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalSeparateModeSendAudio(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalSeparateModeSendAudio",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalSeparateModeSendVideo(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalSeparateModeSendVideo",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalSetBrightness(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalSetBrightness", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalSetCrashReportCommand(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalSetCrashReportCommand",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalSetDebugGpo(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalSetDebugGpo", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalSetDebugMode(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalSetDebugMode", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalSetDebugSocialScreenMode(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalSetDebugSocialScreenMode",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalSetDebugTextMode(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalSetDebugTextMode", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalSetDefaultLedData(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalSetDefaultLedData", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalSetDemoMode(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalSetDemoMode", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalSetDeviceConnection(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalSetDeviceConnection",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalSetForcedCrash(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalSetForcedCrash", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalSetHmuPowerControl(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalSetHmuPowerControl", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalSetHmuPowerControlForDebug(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalSetHmuPowerControlForDebug",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalSetIPD(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalSetIPD", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalSetIpdSettingEnableForSystemService(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalSetIpdSettingEnableForSystemService",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalSetLedOn(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalSetLedOn", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalSetM2LedBrightness(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalSetM2LedBrightness", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalSetM2LedOn(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalSetM2LedOn", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalSetPortConnection(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalSetPortConnection", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalSetPortStatus(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalSetPortStatus", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalSetS3dPassMode(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalSetS3dPassMode", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalSetSidetone(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalSetSidetone", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalSetUserType(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalSetUserType", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalSetVirtualDisplayDepth(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalSetVirtualDisplayDepth",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalSetVirtualDisplayHeight(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalSetVirtualDisplayHeight",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalSetVirtualDisplaySize(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalSetVirtualDisplaySize",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalSetVRMode(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalSetVRMode", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalSocialScreenGetFadeState(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalSocialScreenGetFadeState",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalSocialScreenSetFadeAndSwitch(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalSocialScreenSetFadeAndSwitch",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdInternalSocialScreenSetOutput(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "sceHmdInternalSocialScreenSetOutput",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI Hmd_Func_202D0D1A687FCD2F(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "Func_202D0D1A687FCD2F", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI Hmd_Func_358DBF818A3D8A12(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "Func_358DBF818A3D8A12", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI Hmd_Func_5CCBADA76FE8F40E(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "Func_5CCBADA76FE8F40E", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI Hmd_Func_63D403167DC08CF0(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "Func_63D403167DC08CF0", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI Hmd_Func_69383B2B4E3AEABF(s32 handle, void* data, s32 unk) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "Func_69383B2B4E3AEABF", "(STUBBED) called");
    if (!g_library_initialized) {
        return ORBIS_HMD_ERROR_NOT_INITIALIZED;
    }
    if (handle != g_internal_handle) {
        return ORBIS_HMD_ERROR_HANDLE_INVALID;
    }
    if (g_firmware_version >= KERNEL_FW_450) {
        /* Due to some faulty in-library checks, a missing headset results in this error */
        /* instead of the expected ORBIS_HMD_ERROR_DEVICE_DISCONNECTED error. */
        return ORBIS_HMD_ERROR_HANDLE_INVALID;
    }

    return ORBIS_HMD_ERROR_DEVICE_DISCONNECTED;
}

s32 PS4_SYSV_ABI Hmd_Func_791560C32F4F6D68(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "Func_791560C32F4F6D68", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI Hmd_Func_7C955961EA85B6D3(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "Func_7C955961EA85B6D3", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI Hmd_Func_9952277839236BA7(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "Func_9952277839236BA7", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI Hmd_Func_9A276E739E54EEAF(void) {
    /* Stubbed on real hardware. */
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI Hmd_Func_9E501994E289CBE7(void) {
    /* Stubbed on real hardware. */
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI Hmd_Func_A31F4DA8B3BD2E12(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "Func_A31F4DA8B3BD2E12", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI Hmd_Func_A92D7C23AC364993(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "Func_A92D7C23AC364993", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI Hmd_Func_ADCCC25CB876FDBE(void) {
    /* Stubbed on real hardware. */
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI Hmd_Func_B16652641FE69F0E(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "Func_B16652641FE69F0E", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI Hmd_Func_FC193BD653F2AF2E(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "Func_FC193BD653F2AF2E", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI Hmd_Func_FF2E0E53015FE231(void) {
    c_log(C_LOG_LIB_HMD, C_LOG_LEVEL_ERROR, "Func_FF2E0E53015FE231", "(STUBBED) called");
    return ORBIS_OK;
}

void hmd_library_init(void) {
    kernel_get_compiled_sdk_version(&g_firmware_version);
}
