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

s32 PS4_SYSV_ABI sceCameraDeviceOpen(void) {
    /* Stubbed on real hardware */
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCameraGetDeviceConfig(s32 handle, OrbisCameraConfig* config) {
    if (handle < 1 || config == NULL || config->sizeThis != sizeof(OrbisCameraConfig)) {
        return ORBIS_CAMERA_ERROR_PARAM;
    }
    if (!g_library_opened) {
        return ORBIS_CAMERA_ERROR_NOT_OPEN;
    }

    memset(config, 0, sizeof(OrbisCameraConfig));
    config->sizeThis = sizeof(OrbisCameraConfig);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCameraGetDeviceConfigWithoutHandle(OrbisCameraConfig* config) {
    if (config == NULL || config->sizeThis != sizeof(OrbisCameraConfig)) {
        return ORBIS_CAMERA_ERROR_PARAM;
    }
    if (!g_library_opened) {
        return ORBIS_CAMERA_ERROR_NOT_OPEN;
    }

    memset(config, 0, sizeof(OrbisCameraConfig));
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCameraGetDeviceID(void) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_ERROR, "sceCameraGetDeviceID", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCameraGetDeviceIDWithoutOpen(void) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_ERROR, "sceCameraGetDeviceIDWithoutOpen",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCameraGetDeviceInfo(s32 reserved, OrbisCameraDeviceInfo* device_info) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_DEBUG, "sceCameraGetDeviceInfo", "called");
    if (reserved != 0 || device_info == NULL ||
        device_info->sizeThis != sizeof(OrbisCameraDeviceInfo) || device_info->infoRevision != 1) {
        return ORBIS_CAMERA_ERROR_PARAM;
    }
    if (!g_library_opened) {
        return ORBIS_CAMERA_ERROR_NOT_INIT;
    }

    return ORBIS_CAMERA_ERROR_NOT_CONNECTED;
}
