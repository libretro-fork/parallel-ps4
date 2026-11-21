/* SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stddef.h>
#include <string.h>

#include "common/c_bridge.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/kernel/kernel_c.h"
#include "core/libraries/vr_tracker/vr_tracker.h"
#include "core/libraries/vr_tracker/vr_tracker_error.h"

static bool g_library_initialized = false;

/* Internal memory */
static void* g_garlic_memory_pointer = NULL;
static u32 g_garlic_size = 0;
static void* g_onion_memory_pointer = NULL;
static u32 g_onion_size = 0;
static void* g_work_memory_pointer = NULL;
static u32 g_work_size = 0;

/* Registered handles */
static s32 g_pad_handle = -1;
static s32 g_move_handle = -1;
static s32 g_gun_handle = -1;
static s32 g_hmd_handle = -1;

s32 PS4_SYSV_ABI sceVrTrackerQueryMemory(const OrbisVrTrackerQueryMemoryParam* param,
                                         OrbisVrTrackerQueryMemoryResult* result) {
    u32 required_onion_size;
    c_log(C_LOG_LIB_VR_TRACKER, C_LOG_LEVEL_DEBUG, "sceVrTrackerQueryMemory", "called");
    if (param == NULL || result == NULL || param->size != sizeof(OrbisVrTrackerQueryMemoryParam) ||
        (param->profile != ORBIS_VR_TRACKER_PROFILE_000 &&
         param->profile != ORBIS_VR_TRACKER_PROFILE_100) ||
        param->calibration_settings.pad_position > ORBIS_VR_TRACKER_CALIBRATION_AUTO ||
        /* Hmd doesn't support auto calibration */
        param->calibration_settings.hmd_position > ORBIS_VR_TRACKER_CALIBRATION_MANUAL ||
        param->calibration_settings.move_position > ORBIS_VR_TRACKER_CALIBRATION_AUTO ||
        param->calibration_settings.gun_position > ORBIS_VR_TRACKER_CALIBRATION_AUTO) {
        return ORBIS_VR_TRACKER_ERROR_ARGUMENT_INVALID;
    }

    /* Setting move_position to ORBIS_VR_TRACKER_CALIBRATION_AUTO doubles required onion memory. */
    required_onion_size = ORBIS_VR_TRACKER_BASE_ONION_SIZE;
    if (param->calibration_settings.move_position == ORBIS_VR_TRACKER_CALIBRATION_AUTO) {
        required_onion_size *= 2;
    }

    result->direct_memory_onion_size = required_onion_size;
    result->direct_memory_onion_alignment = ORBIS_VR_TRACKER_MEMORY_ALIGNMENT;
    result->direct_memory_garlic_size = ORBIS_VR_TRACKER_GARLIC_SIZE;
    result->direct_memory_garlic_alignment = ORBIS_VR_TRACKER_MEMORY_ALIGNMENT;
    result->work_memory_size = ORBIS_VR_TRACKER_WORK_SIZE;
    result->work_memory_alignment = ORBIS_VR_TRACKER_MEMORY_ALIGNMENT;

    return ORBIS_OK;
}

/* The GPU's compute pipes a game may use, and queues on each (AmdGpu::Liverpool) */
#define VR_TRACKER_COMPUTE_PIPES 7u
#define VR_TRACKER_QUEUES_PER_PIPE 8u

/* The mapping at @addr runs on for at least @size bytes */
static bool mapping_holds(const void* addr, u32 size) {
    uintptr_t end = 0;
    return kernel_virtual_query_end((uintptr_t)addr, &end) == 0 && end - (uintptr_t)addr >= size;
}

s32 PS4_SYSV_ABI sceVrTrackerInit(const OrbisVrTrackerInitParam* param) {
    u32 required_onion_size;
    if (g_library_initialized) {
        return ORBIS_VR_TRACKER_ERROR_ALREADY_INITIALIZED;
    }

    /* Calculate correct onion size for parameter checks */
    required_onion_size = ORBIS_VR_TRACKER_BASE_ONION_SIZE;
    if (param->calibration_settings.move_position == ORBIS_VR_TRACKER_CALIBRATION_AUTO) {
        required_onion_size *= 2;
    }

    /* Parameter checks are fairly thorough here. */
    if (param->size != sizeof(OrbisVrTrackerInitParam) ||
        /* Check garlic memory parameters */
        param->direct_memory_garlic == NULL ||
        param->direct_memory_garlic_alignment != ORBIS_VR_TRACKER_MEMORY_ALIGNMENT ||
        param->direct_memory_garlic_size != ORBIS_VR_TRACKER_GARLIC_SIZE ||
        /* Check onion memory parameters */
        param->direct_memory_onion == NULL ||
        param->direct_memory_onion_alignment != ORBIS_VR_TRACKER_MEMORY_ALIGNMENT ||
        param->direct_memory_onion_size != required_onion_size ||
        /* Check work memory parameters */
        param->work_memory == NULL ||
        param->work_memory_alignment != ORBIS_VR_TRACKER_MEMORY_ALIGNMENT ||
        param->work_memory_size != ORBIS_VR_TRACKER_WORK_SIZE ||
        /* Check compute queue parameters */
        param->gpu_pipe_id >= VR_TRACKER_COMPUTE_PIPES ||
        param->gpu_queue_id >= VR_TRACKER_QUEUES_PER_PIPE ||
        /* Check calibration settings */
        param->calibration_settings.pad_position > ORBIS_VR_TRACKER_CALIBRATION_AUTO ||
        param->calibration_settings.hmd_position > ORBIS_VR_TRACKER_CALIBRATION_MANUAL ||
        param->calibration_settings.move_position > ORBIS_VR_TRACKER_CALIBRATION_AUTO ||
        param->calibration_settings.gun_position > ORBIS_VR_TRACKER_CALIBRATION_AUTO) {
        return ORBIS_VR_TRACKER_ERROR_ARGUMENT_INVALID;
    }

    /* Real hardware will segfault if any of the supplied mappings aren't long enough, */
    /* Validate each of them to ensure nothing weird can occur when this library is implemented. */
    C_ASSERT_MSG(mapping_holds(param->direct_memory_garlic, param->direct_memory_garlic_size),
                 "sceVrTrackerInit", "Insufficient garlic memory provided");
    g_garlic_memory_pointer = param->direct_memory_garlic;
    g_garlic_size = param->direct_memory_garlic_size;

    C_ASSERT_MSG(mapping_holds(param->direct_memory_onion, param->direct_memory_onion_size),
                 "sceVrTrackerInit", "Insufficient onion memory provided");
    g_onion_memory_pointer = param->direct_memory_onion;
    g_onion_size = param->direct_memory_onion_size;

    C_ASSERT_MSG(mapping_holds(param->work_memory, param->work_memory_size), "sceVrTrackerInit",
                 "Insufficient work memory provided");
    g_work_memory_pointer = param->work_memory;
    g_work_size = param->work_memory_size;

    /* All initialization checks passed. */
    c_log(C_LOG_LIB_VR_TRACKER, C_LOG_LEVEL_WARNING, "sceVrTrackerInit",
          "PSVR headsets are not supported yet");
    g_library_initialized = true;

    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceVrTrackerRegisterDevice(const OrbisVrTrackerDeviceType device_type,
                                            const s32 handle) {
    c_log(C_LOG_LIB_VR_TRACKER, C_LOG_LEVEL_TRACE, "sceVrTrackerRegisterDevice",
          "redirected to sceVrTrackerRegisterDeviceInternal");
    return sceVrTrackerRegisterDeviceInternal(device_type, handle, -1, 0);
}

s32 PS4_SYSV_ABI sceVrTrackerRegisterDevice2(const OrbisVrTrackerDeviceType device_type,
                                             const s32 handle) {
    c_log(C_LOG_LIB_VR_TRACKER, C_LOG_LEVEL_TRACE, "sceVrTrackerRegisterDevice2",
          "redirected to sceVrTrackerRegisterDeviceInternal");
    return sceVrTrackerRegisterDeviceInternal(device_type, handle, -1, 1);
}

s32 PS4_SYSV_ABI sceVrTrackerRegisterDeviceInternal(const OrbisVrTrackerDeviceType device_type,
                                                    const s32 handle, s32 unk0, s32 unk1) {
    c_log(C_LOG_LIB_VR_TRACKER, C_LOG_LEVEL_WARNING, "sceVrTrackerRegisterDeviceInternal",
          "(STUBBED) called, device_type = %u, handle = %d", ((u32)(device_type)), handle);
    if (!g_library_initialized) {
        return ORBIS_VR_TRACKER_ERROR_NOT_INIT;
    }
    if (device_type > ORBIS_VR_TRACKER_DEVICE_GUN || unk0 > 4) {
        return ORBIS_VR_TRACKER_ERROR_ARGUMENT_INVALID;
    }

    /* Ignore handle handle validation for now, since most of that logic isn't really handled. */
    switch (device_type) {
    case ORBIS_VR_TRACKER_DEVICE_HMD: {
        if (g_hmd_handle != -1) {
            return ORBIS_VR_TRACKER_ERROR_DEVICE_ALREADY_REGISTERED;
        }
        g_hmd_handle = handle;
        break;
    }
    case ORBIS_VR_TRACKER_DEVICE_DUALSHOCK4: {
        if (g_pad_handle != -1) {
            return ORBIS_VR_TRACKER_ERROR_DEVICE_ALREADY_REGISTERED;
        }
        g_pad_handle = handle;
        break;
    }
    case ORBIS_VR_TRACKER_DEVICE_MOVE: {
        if (g_move_handle != -1) {
            return ORBIS_VR_TRACKER_ERROR_DEVICE_ALREADY_REGISTERED;
        }
        g_move_handle = handle;
        break;
    }
    case ORBIS_VR_TRACKER_DEVICE_GUN: {
        if (g_gun_handle != -1) {
            return ORBIS_VR_TRACKER_ERROR_DEVICE_ALREADY_REGISTERED;
        }
        g_gun_handle = handle;
        break;
    }
    default: {
        /* Shouldn't be possible to hit this. */
        c_unreachable(__FILE__, __LINE__, "sceVrTrackerRegisterDeviceInternal",
                      "Unreachable code!");
    }
    }

    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceVrTrackerCpuProcess(const OrbisVrTrackerCpuProcessParam* param) {
    c_log(C_LOG_LIB_VR_TRACKER, C_LOG_LEVEL_ERROR, "sceVrTrackerCpuProcess", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceVrTrackerGetPlayAreaWarningInfo(OrbisVrTrackerPlayAreaWarningInfo* info) {
    c_log(C_LOG_LIB_VR_TRACKER, C_LOG_LEVEL_ERROR, "sceVrTrackerGetPlayAreaWarningInfo",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceVrTrackerGetResult(const OrbisVrTrackerGetResultParam* param,
                                       OrbisVrTrackerResultData* result) {
    c_log(C_LOG_LIB_VR_TRACKER, C_LOG_LEVEL_ERROR, "sceVrTrackerGetResult", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceVrTrackerGetTime(u64* time) {
    c_log(C_LOG_LIB_VR_TRACKER, C_LOG_LEVEL_TRACE, "sceVrTrackerGetTime", "called");
    if (!g_library_initialized) {
        return ORBIS_VR_TRACKER_ERROR_NOT_INIT;
    }
    if (time == NULL) {
        return ORBIS_VR_TRACKER_ERROR_ARGUMENT_INVALID;
    }
    *time = kernel_process_time_usec();
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceVrTrackerGpuSubmit(const OrbisVrTrackerGpuSubmitParam* param) {
    c_log(C_LOG_LIB_VR_TRACKER, C_LOG_LEVEL_ERROR, "sceVrTrackerGpuSubmit", "(STUBBED) called");
    if (!g_library_initialized) {
        return ORBIS_VR_TRACKER_ERROR_NOT_INIT;
    }

    /* Impossible to submit valid data here since sceCameraGetFrameData returns an error. */
    return ORBIS_VR_TRACKER_ERROR_ARGUMENT_INVALID;
}

s32 PS4_SYSV_ABI sceVrTrackerGpuWait(const OrbisVrTrackerGpuWaitParam* param) {
    c_log(C_LOG_LIB_VR_TRACKER, C_LOG_LEVEL_ERROR, "sceVrTrackerGpuWait", "(STUBBED) called");
    if (!g_library_initialized) {
        return ORBIS_VR_TRACKER_ERROR_NOT_INIT;
    }
    if (param == NULL || param->size != sizeof(OrbisVrTrackerGpuWaitParam)) {
        return ORBIS_VR_TRACKER_ERROR_ARGUMENT_INVALID;
    }

    /* Impossible to perform GPU submits */
    return ORBIS_VR_TRACKER_ERROR_NOT_EXECUTE_GPU_SUBMIT;
}

s32 PS4_SYSV_ABI sceVrTrackerGpuWaitAndCpuProcess(void) {
    c_log(C_LOG_LIB_VR_TRACKER, C_LOG_LEVEL_ERROR, "sceVrTrackerGpuWaitAndCpuProcess",
          "(STUBBED) called");
    if (!g_library_initialized) {
        return ORBIS_VR_TRACKER_ERROR_NOT_INIT;
    }

    /* Impossible to perform GPU submits */
    return ORBIS_VR_TRACKER_ERROR_NOT_EXECUTE_GPU_SUBMIT;
}

s32 PS4_SYSV_ABI
sceVrTrackerNotifyEndOfCpuProcess(const OrbisVrTrackerNotifyEndOfCpuProcessParam* param) {
    c_log(C_LOG_LIB_VR_TRACKER, C_LOG_LEVEL_ERROR, "sceVrTrackerNotifyEndOfCpuProcess",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceVrTrackerRecalibrate(const OrbisVrTrackerRecalibrateParam* param) {
    OrbisVrTrackerDeviceType device_type;
    c_log(C_LOG_LIB_VR_TRACKER, C_LOG_LEVEL_ERROR, "sceVrTrackerRecalibrate", "(STUBBED) called");
    if (!g_library_initialized) {
        return ORBIS_VR_TRACKER_ERROR_NOT_INIT;
    }
    if (param == NULL || param->size != sizeof(OrbisVrTrackerRecalibrateParam) ||
        param->device_type > ORBIS_VR_TRACKER_DEVICE_GUN) {
        return ORBIS_VR_TRACKER_ERROR_ARGUMENT_INVALID;
    }

    device_type = param->device_type;
    switch (device_type) {
    case ORBIS_VR_TRACKER_DEVICE_HMD: {
        /* Seems like the lack of a connected hmd results in this? */
        return ORBIS_VR_TRACKER_ERROR_DEVICE_NOT_REGISTERED;
        break;
    }
    case ORBIS_VR_TRACKER_DEVICE_DUALSHOCK4: {
        if (g_pad_handle == -1) {
            return ORBIS_VR_TRACKER_ERROR_DEVICE_NOT_REGISTERED;
        }
        break;
    }
    case ORBIS_VR_TRACKER_DEVICE_MOVE: {
        if (g_move_handle == -1) {
            return ORBIS_VR_TRACKER_ERROR_DEVICE_NOT_REGISTERED;
        }
        break;
    }
    case ORBIS_VR_TRACKER_DEVICE_GUN: {
        if (g_gun_handle == -1) {
            return ORBIS_VR_TRACKER_ERROR_DEVICE_NOT_REGISTERED;
        }
        break;
    }
    default: {
        /* Shouldn't be possible to hit this. */
        c_unreachable(__FILE__, __LINE__, "sceVrTrackerRecalibrate", "Unreachable code!");
    }
    }

    /* TODO: handle internal recalibration behaviors. */
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceVrTrackerResetAll(void) {
    c_log(C_LOG_LIB_VR_TRACKER, C_LOG_LEVEL_ERROR, "sceVrTrackerResetAll", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceVrTrackerResetOrientationRelative(const OrbisVrTrackerDeviceType device_type,
                                                      const s32 handle) {
    c_log(C_LOG_LIB_VR_TRACKER, C_LOG_LEVEL_ERROR, "sceVrTrackerResetOrientationRelative",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceVrTrackerSaveInternalBuffers(void) {
    c_log(C_LOG_LIB_VR_TRACKER, C_LOG_LEVEL_ERROR, "sceVrTrackerSaveInternalBuffers",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceVrTrackerSetDurationUntilStatusNotTracking(
    const OrbisVrTrackerDeviceType device_type, const u32 duration_camera_frames) {
    c_log(C_LOG_LIB_VR_TRACKER, C_LOG_LEVEL_ERROR, "sceVrTrackerSetDurationUntilStatusNotTracking",
          "(STUBBED) called");
    if (!g_library_initialized) {
        return ORBIS_VR_TRACKER_ERROR_NOT_INIT;
    }
    if (device_type > ORBIS_VR_TRACKER_DEVICE_GUN) {
        return ORBIS_VR_TRACKER_ERROR_ARGUMENT_INVALID;
    }

    /* Seems to unconditionally return 0 when parameters are valid. */
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceVrTrackerSetExtendedMode(void) {
    c_log(C_LOG_LIB_VR_TRACKER, C_LOG_LEVEL_ERROR, "sceVrTrackerSetExtendedMode",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceVrTrackerSetLEDBrightness(void) {
    c_log(C_LOG_LIB_VR_TRACKER, C_LOG_LEVEL_ERROR, "sceVrTrackerSetLEDBrightness",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceVrTrackerSetRestingMode(void) {
    c_log(C_LOG_LIB_VR_TRACKER, C_LOG_LEVEL_ERROR, "sceVrTrackerSetRestingMode",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI
sceVrTrackerUpdateMotionSensorData(const OrbisVrTrackerUpdateMotionSensorDataParam* param) {
    c_log(C_LOG_LIB_VR_TRACKER, C_LOG_LEVEL_ERROR, "sceVrTrackerUpdateMotionSensorData",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI VrTracker_Func_0FA4C949F8D3024E(void) {
    c_log(C_LOG_LIB_VR_TRACKER, C_LOG_LEVEL_ERROR, "VrTracker_Func_0FA4C949F8D3024E",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI VrTracker_Func_285C6AFC09C42F7E(void) {
    c_log(C_LOG_LIB_VR_TRACKER, C_LOG_LEVEL_ERROR, "VrTracker_Func_285C6AFC09C42F7E",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI VrTracker_Func_9A6CDB2103664F8A(void) {
    c_log(C_LOG_LIB_VR_TRACKER, C_LOG_LEVEL_ERROR, "VrTracker_Func_9A6CDB2103664F8A",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI VrTracker_Func_B4D26B7D8B18DF06(void) {
    c_log(C_LOG_LIB_VR_TRACKER, C_LOG_LEVEL_ERROR, "VrTracker_Func_B4D26B7D8B18DF06",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceVrTrackerSetDeviceRejection(void) {
    c_log(C_LOG_LIB_VR_TRACKER, C_LOG_LEVEL_ERROR, "sceVrTrackerSetDeviceRejection",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI VrTracker_Func_1119B0BE399F37E7(void) {
    c_log(C_LOG_LIB_VR_TRACKER, C_LOG_LEVEL_ERROR, "VrTracker_Func_1119B0BE399F37E7",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI VrTracker_Func_4928B43816BC440D(void) {
    c_log(C_LOG_LIB_VR_TRACKER, C_LOG_LEVEL_ERROR, "VrTracker_Func_4928B43816BC440D",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI VrTracker_Func_863EF32EFCB0FA9C(void) {
    c_log(C_LOG_LIB_VR_TRACKER, C_LOG_LEVEL_ERROR, "VrTracker_Func_863EF32EFCB0FA9C",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI VrTracker_Func_E6E726CBC85C48F9(void) {
    c_log(C_LOG_LIB_VR_TRACKER, C_LOG_LEVEL_ERROR, "VrTracker_Func_E6E726CBC85C48F9",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI VrTracker_Func_F6407E46C66DF383(void) {
    c_log(C_LOG_LIB_VR_TRACKER, C_LOG_LEVEL_ERROR, "VrTracker_Func_F6407E46C66DF383",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceVrTrackerCpuPopMarker(void) {
    c_log(C_LOG_LIB_VR_TRACKER, C_LOG_LEVEL_ERROR, "sceVrTrackerCpuPopMarker", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceVrTrackerCpuPushMarker(void) {
    c_log(C_LOG_LIB_VR_TRACKER, C_LOG_LEVEL_ERROR, "sceVrTrackerCpuPushMarker", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceVrTrackerGetLiveCaptureId(void) {
    c_log(C_LOG_LIB_VR_TRACKER, C_LOG_LEVEL_ERROR, "sceVrTrackerGetLiveCaptureId",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceVrTrackerStartLiveCapture(void) {
    c_log(C_LOG_LIB_VR_TRACKER, C_LOG_LEVEL_ERROR, "sceVrTrackerStartLiveCapture",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceVrTrackerStopLiveCapture(void) {
    c_log(C_LOG_LIB_VR_TRACKER, C_LOG_LEVEL_ERROR, "sceVrTrackerStopLiveCapture",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceVrTrackerUnregisterDevice(const s32 handle) {
    c_log(C_LOG_LIB_VR_TRACKER, C_LOG_LEVEL_DEBUG, "sceVrTrackerUnregisterDevice", "called");
    if (!g_library_initialized) {
        return ORBIS_VR_TRACKER_ERROR_NOT_INIT;
    }
    if (handle < 0) {
        return ORBIS_VR_TRACKER_ERROR_ARGUMENT_INVALID;
    }
    /* Since this function only takes a handle, compare the handle to registered handles. */
    if (handle == g_hmd_handle) {
        g_hmd_handle = -1;
    } else if (handle == g_pad_handle) {
        g_pad_handle = -1;
    } else if (handle == g_move_handle) {
        g_move_handle = -1;
    } else if (handle == g_gun_handle) {
        g_gun_handle = -1;
    } else {
        /* If none of the handles match up, then return an error. */
        return ORBIS_VR_TRACKER_ERROR_ARGUMENT_INVALID;
    }

    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceVrTrackerTerm(void) {
    c_log(C_LOG_LIB_VR_TRACKER, C_LOG_LEVEL_DEBUG, "sceVrTrackerTerm", "called");
    if (!g_library_initialized) {
        return ORBIS_VR_TRACKER_ERROR_NOT_INIT;
    }
    g_library_initialized = false;
    return ORBIS_OK;
}
