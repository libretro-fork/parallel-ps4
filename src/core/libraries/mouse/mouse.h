/* SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_MOUSE_H
#define SHADPS4_LIBRARIES_MOUSE_H

/* libSceMouse. The core has no mouse: the library opens, and a mouse reads as not connected.
 * Registered in libs.cpp. */

#include <boolean.h>
#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

typedef struct OrbisMouseData {
    u64 timestamp;
    bool connected;
    u32 buttons;
    s32 x_axis;
    s32 y_axis;
    s32 wheel;
    s32 tilt;
    u8 reserve[8];
} OrbisMouseData;

/* OrbisMouseOpenParam.flag */
#define ORBIS_MOUSE_OPEN_NORMAL 0
#define ORBIS_MOUSE_OPEN_MERGED 1

typedef struct OrbisMouseOpenParam {
    u8 flag;
    u8 reserve[7];
} OrbisMouseOpenParam;

int PS4_SYSV_ABI sceMouseClose(s32 handle);
int PS4_SYSV_ABI sceMouseConnectPort(void);
int PS4_SYSV_ABI sceMouseDebugGetDeviceId(void);
int PS4_SYSV_ABI sceMouseDeviceOpen(void);
int PS4_SYSV_ABI sceMouseDisconnectDevice(void);
int PS4_SYSV_ABI sceMouseDisconnectPort(void);
int PS4_SYSV_ABI sceMouseGetDeviceInfo(void);
int PS4_SYSV_ABI sceMouseInit(void);
int PS4_SYSV_ABI sceMouseMbusInit(void);
int PS4_SYSV_ABI sceMouseOpen(s32 userId, s32 type, s32 index, OrbisMouseOpenParam* pParam);
int PS4_SYSV_ABI sceMouseRead(s32 handle, OrbisMouseData* pData, s32 num);
int PS4_SYSV_ABI sceMouseSetHandType(void);
int PS4_SYSV_ABI sceMouseSetPointerSpeed(void);
int PS4_SYSV_ABI sceMouseSetProcessPrivilege(void);

RETRO_END_DECLS

#endif
