/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "common/c_bridge.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/screenshot/screenshot.h"

int PS4_SYSV_ABI ScreenShot_dummy(void) {
    c_log(C_LOG_LIB_SCREENSHOT, C_LOG_LEVEL_ERROR, "ScreenShot_dummy", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceScreenShotCapture(void) {
    c_log(C_LOG_LIB_SCREENSHOT, C_LOG_LEVEL_ERROR, "sceScreenShotCapture", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceScreenShotDisable(void) {
    c_log(C_LOG_LIB_SCREENSHOT, C_LOG_LEVEL_ERROR, "sceScreenShotDisable", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceScreenShotDisableNotification(void) {
    c_log(C_LOG_LIB_SCREENSHOT, C_LOG_LEVEL_ERROR, "sceScreenShotDisableNotification",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceScreenShotEnable(void) {
    c_log(C_LOG_LIB_SCREENSHOT, C_LOG_LEVEL_ERROR, "sceScreenShotEnable", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceScreenShotEnableNotification(void) {
    c_log(C_LOG_LIB_SCREENSHOT, C_LOG_LEVEL_ERROR, "sceScreenShotEnableNotification",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceScreenShotGetAppInfo(void) {
    c_log(C_LOG_LIB_SCREENSHOT, C_LOG_LEVEL_ERROR, "sceScreenShotGetAppInfo", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceScreenShotGetDrcParam(void) {
    c_log(C_LOG_LIB_SCREENSHOT, C_LOG_LEVEL_ERROR, "sceScreenShotGetDrcParam", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceScreenShotIsDisabled(void) {
    c_log(C_LOG_LIB_SCREENSHOT, C_LOG_LEVEL_ERROR, "sceScreenShotIsDisabled", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceScreenShotIsVshScreenCaptureDisabled(void) {
    c_log(C_LOG_LIB_SCREENSHOT, C_LOG_LEVEL_ERROR, "sceScreenShotIsVshScreenCaptureDisabled",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceScreenShotSetOverlayImage(void) {
    c_log(C_LOG_LIB_SCREENSHOT, C_LOG_LEVEL_ERROR, "sceScreenShotSetOverlayImage",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceScreenShotSetOverlayImageWithOrigin(void) {
    c_log(C_LOG_LIB_SCREENSHOT, C_LOG_LEVEL_ERROR, "sceScreenShotSetOverlayImageWithOrigin",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceScreenShotSetParam(void) {
    c_log(C_LOG_LIB_SCREENSHOT, C_LOG_LEVEL_ERROR, "sceScreenShotSetParam", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceScreenShotSetDrcParam(void) {
    c_log(C_LOG_LIB_SCREENSHOT, C_LOG_LEVEL_ERROR, "sceScreenShotSetDrcParam", "(STUBBED) called");
    return ORBIS_OK;
}
