/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_MOVE_H
#define SHADPS4_LIBRARIES_MOVE_H

/* libSceMove. No Move controller is ever connected. Registered in libs.cpp. */

#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

typedef struct OrbisMoveDeviceInfo {
    float sphere_radius;
    float accelerometer_offset[3];
} OrbisMoveDeviceInfo;

typedef struct OrbisMoveButtonData {
    u16 button_data;
    u16 trigger_data;
} OrbisMoveButtonData;

typedef struct OrbisMoveExtensionPortData {
    u16 status;
    u16 digital0;
    u16 digital1;
    u16 analog_right_x;
    u16 analog_right_y;
    u16 analog_left_x;
    u16 analog_left_y;
    unsigned char custom[5];
} OrbisMoveExtensionPortData;

typedef struct OrbisMoveData {
    float accelerometer[3];
    float gyro[3];
    OrbisMoveButtonData button_data;
    OrbisMoveExtensionPortData extension_data;
    s64 timestamp;
    s32 count;
    float temperature;
} OrbisMoveData;

s32 PS4_SYSV_ABI sceMoveInit(void);
s32 PS4_SYSV_ABI sceMoveOpen(s32 user_id, s32 type, s32 index);
s32 PS4_SYSV_ABI sceMoveGetDeviceInfo(s32 handle, OrbisMoveDeviceInfo* info);
s32 PS4_SYSV_ABI sceMoveReadStateLatest(s32 handle, OrbisMoveData* data);
s32 PS4_SYSV_ABI sceMoveReadStateRecent(s32 handle, s64 timestamp, OrbisMoveData* data,
                                        s32* out_count);
s32 PS4_SYSV_ABI sceMoveGetExtensionPortInfo(s32 handle, void* data);
s32 PS4_SYSV_ABI sceMoveSetVibration(s32 handle, u8 intensity);
s32 PS4_SYSV_ABI sceMoveSetLightSphere(s32 handle, u8 red, u8 green, u8 blue);
s32 PS4_SYSV_ABI sceMoveResetLightSphere(s32 handle);
s32 PS4_SYSV_ABI sceMoveClose(s32 handle);
s32 PS4_SYSV_ABI sceMoveTerm(void);

RETRO_END_DECLS

#endif
