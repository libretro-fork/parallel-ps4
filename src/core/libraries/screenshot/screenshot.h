/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_SCREENSHOT_H
#define SHADPS4_LIBRARIES_SCREENSHOT_H

/* libSceScreenShot. Registered in libs.cpp. */

#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

int PS4_SYSV_ABI ScreenShot_dummy(void);
int PS4_SYSV_ABI sceScreenShotCapture(void);
int PS4_SYSV_ABI sceScreenShotDisable(void);
int PS4_SYSV_ABI sceScreenShotDisableNotification(void);
int PS4_SYSV_ABI sceScreenShotEnable(void);
int PS4_SYSV_ABI sceScreenShotEnableNotification(void);
int PS4_SYSV_ABI sceScreenShotGetAppInfo(void);
int PS4_SYSV_ABI sceScreenShotGetDrcParam(void);
int PS4_SYSV_ABI sceScreenShotIsDisabled(void);
int PS4_SYSV_ABI sceScreenShotIsVshScreenCaptureDisabled(void);
int PS4_SYSV_ABI sceScreenShotSetOverlayImage(void);
int PS4_SYSV_ABI sceScreenShotSetOverlayImageWithOrigin(void);
int PS4_SYSV_ABI sceScreenShotSetParam(void);
int PS4_SYSV_ABI sceScreenShotSetDrcParam(void);

RETRO_END_DECLS

#endif
