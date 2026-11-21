/* SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "common/c_bridge.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/keyboard/keyboard.h"

s32 PS4_SYSV_ABI sceKeyboardClose(void) {
    c_log(C_LOG_LIB_KEYBOARD, C_LOG_LEVEL_ERROR, "sceKeyboardClose", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceKeyboardConnectPort(void) {
    c_log(C_LOG_LIB_KEYBOARD, C_LOG_LEVEL_ERROR, "sceKeyboardConnectPort", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceKeyboardDebugGetDeviceId(void) {
    c_log(C_LOG_LIB_KEYBOARD, C_LOG_LEVEL_ERROR, "sceKeyboardDebugGetDeviceId", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceKeyboardDeviceOpen(void) {
    c_log(C_LOG_LIB_KEYBOARD, C_LOG_LEVEL_ERROR, "sceKeyboardDeviceOpen", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceKeyboardDisconnectDevice(void) {
    c_log(C_LOG_LIB_KEYBOARD, C_LOG_LEVEL_ERROR, "sceKeyboardDisconnectDevice", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceKeyboardDisconnectPort(void) {
    c_log(C_LOG_LIB_KEYBOARD, C_LOG_LEVEL_ERROR, "sceKeyboardDisconnectPort", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceKeyboardGetConnection(void) {
    c_log(C_LOG_LIB_KEYBOARD, C_LOG_LEVEL_ERROR, "sceKeyboardGetConnection", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceKeyboardGetDeviceInfo(void) {
    c_log(C_LOG_LIB_KEYBOARD, C_LOG_LEVEL_ERROR, "sceKeyboardGetDeviceInfo", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceKeyboardGetKey2Char(void) {
    c_log(C_LOG_LIB_KEYBOARD, C_LOG_LEVEL_ERROR, "sceKeyboardGetKey2Char", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceKeyboardInit(void) {
    c_log(C_LOG_LIB_KEYBOARD, C_LOG_LEVEL_ERROR, "sceKeyboardInit", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceKeyboardMbusInit(void) {
    c_log(C_LOG_LIB_KEYBOARD, C_LOG_LEVEL_ERROR, "sceKeyboardMbusInit", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceKeyboardOpen(void) {
    c_log(C_LOG_LIB_KEYBOARD, C_LOG_LEVEL_ERROR, "sceKeyboardOpen", "(STUBBED) called");
    return 1;
}

s32 PS4_SYSV_ABI sceKeyboardRead(void) {
    c_log(C_LOG_LIB_KEYBOARD, C_LOG_LEVEL_ERROR, "sceKeyboardRead", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceKeyboardReadState(s32 handle, OrbisKeyboardData* data) {
    c_log(C_LOG_LIB_KEYBOARD, C_LOG_LEVEL_DEBUG, "sceKeyboardReadState", "(DUMMY) called");
    if (handle != 1) {
        return ORBIS_KEYBOARD_ERROR_INVALID_HANDLE;
    }
    /* The core has no keyboard: no key is ever down. */
    data->nkeys = 0;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceKeyboardSetProcessFocus(void) {
    c_log(C_LOG_LIB_KEYBOARD, C_LOG_LEVEL_ERROR, "sceKeyboardSetProcessFocus", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceKeyboardSetProcessPrivilege(void) {
    c_log(C_LOG_LIB_KEYBOARD, C_LOG_LEVEL_ERROR, "sceKeyboardSetProcessPrivilege",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI Keyboard_Func_97BEA3A6D1DF8B33(void) {
    c_log(C_LOG_LIB_KEYBOARD, C_LOG_LEVEL_ERROR, "Keyboard_Func_97BEA3A6D1DF8B33",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceDbgKeyboardClose(void) {
    c_log(C_LOG_LIB_KEYBOARD, C_LOG_LEVEL_ERROR, "sceDbgKeyboardClose", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceDbgKeyboardGetKey2Char(void) {
    c_log(C_LOG_LIB_KEYBOARD, C_LOG_LEVEL_ERROR, "sceDbgKeyboardGetKey2Char", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceDbgKeyboardInit(void) {
    c_log(C_LOG_LIB_KEYBOARD, C_LOG_LEVEL_ERROR, "sceDbgKeyboardInit", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceDbgKeyboardOpen(void) {
    c_log(C_LOG_LIB_KEYBOARD, C_LOG_LEVEL_ERROR, "sceDbgKeyboardOpen", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceDbgKeyboardRead(void) {
    c_log(C_LOG_LIB_KEYBOARD, C_LOG_LEVEL_ERROR, "sceDbgKeyboardRead", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceDbgKeyboardReadState(void) {
    c_log(C_LOG_LIB_KEYBOARD, C_LOG_LEVEL_ERROR, "sceDbgKeyboardReadState", "(STUBBED) called");
    return ORBIS_OK;
}
