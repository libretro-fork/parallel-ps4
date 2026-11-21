/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_KEYBOARD_H
#define SHADPS4_LIBRARIES_KEYBOARD_H

/* libSceKeyboard. The core has no keyboard. Registered in libs.cpp. */

#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

#define ORBIS_KEYBOARD_ERROR_INVALID_ARG ((int32_t)0x80DA0001)
#define ORBIS_KEYBOARD_ERROR_INVALID_PORT ((int32_t)0x80DA0002)
#define ORBIS_KEYBOARD_ERROR_INVALID_HANDLE ((int32_t)0x80DA0003)
#define ORBIS_KEYBOARD_ERROR_ALREADY_OPENED ((int32_t)0x80DA0004)
#define ORBIS_KEYBOARD_ERROR_NOT_INITIALIZED ((int32_t)0x80DA0005)
#define ORBIS_KEYBOARD_ERROR_FATAL ((int32_t)0x80DA00FF)

typedef struct OrbisKeyboardData {
    s64 timestamp; /* microseconds */
    u8 padding[8];
    u32 unk1; /* always 1 */
    u32 nkeys;
    u32 locks; /* num lock, caps lock, scroll lock */
    u32 mods;  /* ctrl, shift, alt, meta */
    u16 keycodes[32];
} OrbisKeyboardData;

typedef struct OrbisKeyboardKey2Char {
    s32 ok;
    s32 ok2;
    s32 keycode;
    char unk[8]; /* zeros */
} OrbisKeyboardKey2Char;

s32 PS4_SYSV_ABI sceKeyboardClose(void);
s32 PS4_SYSV_ABI sceKeyboardConnectPort(void);
s32 PS4_SYSV_ABI sceKeyboardDebugGetDeviceId(void);
s32 PS4_SYSV_ABI sceKeyboardDeviceOpen(void);
s32 PS4_SYSV_ABI sceKeyboardDisconnectDevice(void);
s32 PS4_SYSV_ABI sceKeyboardDisconnectPort(void);
s32 PS4_SYSV_ABI sceKeyboardGetConnection(void);
s32 PS4_SYSV_ABI sceKeyboardGetDeviceInfo(void);
s32 PS4_SYSV_ABI sceKeyboardGetKey2Char(void);
s32 PS4_SYSV_ABI sceKeyboardInit(void);
s32 PS4_SYSV_ABI sceKeyboardMbusInit(void);
s32 PS4_SYSV_ABI sceKeyboardOpen(void);
s32 PS4_SYSV_ABI sceKeyboardRead(void);
s32 PS4_SYSV_ABI sceKeyboardReadState(s32 handle, OrbisKeyboardData* data);
s32 PS4_SYSV_ABI sceKeyboardSetProcessFocus(void);
s32 PS4_SYSV_ABI sceKeyboardSetProcessPrivilege(void);
s32 PS4_SYSV_ABI Keyboard_Func_97BEA3A6D1DF8B33(void);
s32 PS4_SYSV_ABI sceDbgKeyboardClose(void);
s32 PS4_SYSV_ABI sceDbgKeyboardGetKey2Char(void);
s32 PS4_SYSV_ABI sceDbgKeyboardInit(void);
s32 PS4_SYSV_ABI sceDbgKeyboardOpen(void);
s32 PS4_SYSV_ABI sceDbgKeyboardRead(void);
s32 PS4_SYSV_ABI sceDbgKeyboardReadState(void);

RETRO_END_DECLS

#endif
