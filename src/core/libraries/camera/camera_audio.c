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

s32 PS4_SYSV_ABI sceCameraAudioClose(void) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_DEBUG, "sceCameraAudioClose", "called");
    /* Returns ORBIS_OK while internally failing */
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCameraAudioGetData(void* data) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_DEBUG, "sceCameraAudioGetData", "called");
    if (data == NULL) {
        return ORBIS_CAMERA_ERROR_PARAM;
    }
    /* Returns ORBIS_OK while internally failing */
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCameraAudioGetData2(void* data) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_DEBUG, "sceCameraAudioGetData2", "called");
    if (data == NULL) {
        return ORBIS_CAMERA_ERROR_PARAM;
    }
    /* Returns ORBIS_OK while internally failing */
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCameraAudioOpen(void) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_DEBUG, "sceCameraAudioOpen", "called");
    /* Returns error fatal as a side effect of not having a camera attached. */
    return ORBIS_CAMERA_ERROR_FATAL;
}

s32 PS4_SYSV_ABI sceCameraAudioReset(void) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_DEBUG, "sceCameraAudioReset", "called");
    /* Returns ORBIS_OK while internally failing */
    return ORBIS_OK;
}
