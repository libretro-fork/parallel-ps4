/* SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_SYSTEM_GESTURE_H
#define SHADPS4_LIBRARIES_SYSTEM_GESTURE_H

/* libSceSystemGesture. Registered in sysmodule_internal.cpp. */

#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

int PS4_SYSV_ABI sceSystemGestureAppendTouchRecognizer(void);
int PS4_SYSV_ABI sceSystemGestureClose(void);
int PS4_SYSV_ABI sceSystemGestureCreateTouchRecognizer(void);
int PS4_SYSV_ABI sceSystemGestureFinalizePrimitiveTouchRecognizer(void);
int PS4_SYSV_ABI sceSystemGestureGetPrimitiveTouchEventByIndex(void);
int PS4_SYSV_ABI sceSystemGestureGetPrimitiveTouchEventByPrimitiveID(void);
int PS4_SYSV_ABI sceSystemGestureGetPrimitiveTouchEvents(void);
int PS4_SYSV_ABI sceSystemGestureGetPrimitiveTouchEventsCount(void);
int PS4_SYSV_ABI sceSystemGestureGetTouchEventByEventID(void);
int PS4_SYSV_ABI sceSystemGestureGetTouchEventByIndex(void);
int PS4_SYSV_ABI sceSystemGestureGetTouchEvents(void);
int PS4_SYSV_ABI sceSystemGestureGetTouchEventsCount(void);
int PS4_SYSV_ABI sceSystemGestureGetTouchRecognizerInformation(void);
int PS4_SYSV_ABI sceSystemGestureInitializePrimitiveTouchRecognizer(void);
int PS4_SYSV_ABI sceSystemGestureOpen(void);
int PS4_SYSV_ABI sceSystemGestureRemoveTouchRecognizer(void);
int PS4_SYSV_ABI sceSystemGestureResetPrimitiveTouchRecognizer(void);
int PS4_SYSV_ABI sceSystemGestureResetTouchRecognizer(void);
int PS4_SYSV_ABI sceSystemGestureUpdateAllTouchRecognizer(void);
int PS4_SYSV_ABI sceSystemGestureUpdatePrimitiveTouchRecognizer(void);
int PS4_SYSV_ABI sceSystemGestureUpdateTouchRecognizer(void);
int PS4_SYSV_ABI sceSystemGestureUpdateTouchRecognizerRectangle(void);

RETRO_END_DECLS

#endif
