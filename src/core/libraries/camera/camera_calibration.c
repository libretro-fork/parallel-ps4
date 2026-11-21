/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stddef.h>
#include <string.h>

#include "common/c_bridge.h"
#include "core/libraries/camera/camera.h"
#include "core/libraries/camera/camera_backend.h"
#include "core/libraries/camera/camera_helpers.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/kernel/kernel_c.h"

s32 PS4_SYSV_ABI sceCameraGetCalibData(s32 id, void* calib_data, void* maybe_reserved) {
    static const u64 dumped_calib_data1[32] = {
        0x000000000b0b0200, 0x00000000bc128639, 0x000000003de79766, 0x00000000be6e2869,
        0x3ca0ce1f3e30521c, 0x4452d55e3d0160b6, 0x3cf7fdaa4453abf1, 0x000000003ce3f40f,
        0x000000003cdbcaa6, 0x000000003d635f4e, 0x00000000be299439, 0xbb98ddc23e1159b4,
        0x44523b6c3c87d83c, 0x3c8d1c46445308a2, 0xbbb83fe03c9bcc40, 0xbab0ed65b8fe93b0,
        0xba677004bb598091, 0x3c0eb61e3b49d173, 0xba1bac9f3c67af0b, 0x00000000b9d0e64d,
        0x0000000000000000, 0x0000000000000000, 0xb97653e400000000, 0x00000000ba95460e,
        0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000,
        0x0000000000000000, 0x0000000000000000, 0xbab5330000000000, 0x000001013b07a47f,
    };
    static const u64 dumped_calib_data2[32] = {
        0x002d000000ff0009, 0x002d0ff70ffd0f14, 0x002d0efe00010008, 0x002f0ffe0f020008,
        0x002f000800060105, 0x002e00fc0001000a, 0x0000000000000000, 0x0000000000000000,
        0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000,
        0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000,
        0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000,
        0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000,
        0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000,
        0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000,
    };
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_WARNING, "sceCameraGetCalibData", "(DUMMY) called, id: %d",
          id);
    if (calib_data == NULL) {
        return ORBIS_CAMERA_ERROR_PARAM;
    }
    if (id != 1 && id != 2) {
        return ORBIS_OK;
    }
    memcpy(calib_data, id == 1 ? dumped_calib_data1 : dumped_calib_data2,
           sizeof(dumped_calib_data1));
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCameraGetCalibDataFromDevice(void) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_ERROR, "sceCameraGetCalibDataFromDevice",
          "(STUBBED) called");
    return ORBIS_CAMERA_ERROR_NOT_CONNECTED;
}

s32 PS4_SYSV_ABI sceCameraGetCalibrationData(const OrbisCameraGetCalibrationDataParameter* param,
                                             OrbisCameraCalibrationData* calibration_data) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_DEBUG, "sceCameraGetCalibrationData", "called");
    if (param == NULL || calibration_data == NULL ||
        param->size != sizeof(OrbisCameraGetCalibrationDataParameter) || param->format_type != 0 ||
        param->function_type < ORBIS_CAMERA_CALIBRATION_DATA_FUNCTION_TYPE_IMAGE_RECTIFICATION ||
        param->function_type >
            ORBIS_CAMERA_CALIBRATION_DATA_FUNCTION_TYPE_IMAGE_INVERSE_RECTIFICATION) {
        return ORBIS_CAMERA_ERROR_PARAM;
    }
    if (!camera_get_backend()->is_attached()) {
        return ORBIS_CAMERA_ERROR_NOT_CONNECTED;
    }

    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCameraInitializeRegistryCalibData(void) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_ERROR, "sceCameraInitializeRegistryCalibData",
          "(STUBBED) called");
    if (!g_library_opened) {
        return ORBIS_CAMERA_ERROR_NOT_INIT;
    }
    return ORBIS_OK;
}
