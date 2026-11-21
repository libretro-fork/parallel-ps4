/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <boolean.h>

#include "common/c_bridge.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/signin_dialog/signindialog.h"
#include "core/libraries/signin_dialog/signindialog_error.h"

static bool g_lib_init = false;
static bool g_lib_open = false;

s32 PS4_SYSV_ABI sceSigninDialogInitialize(void) {
    c_log(C_LOG_LIB_SIGNIN_DIALOG, C_LOG_LEVEL_ERROR, "sceSigninDialogInitialize",
          "(STUBBED) called");
    g_lib_init = true;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSigninDialogOpen(void) {
    c_log(C_LOG_LIB_SIGNIN_DIALOG, C_LOG_LEVEL_ERROR, "sceSigninDialogOpen", "(STUBBED) called");
    g_lib_open = true;
    return ORBIS_OK;
}

u32 PS4_SYSV_ABI sceSigninDialogGetStatus(void) {
    if (!g_lib_init) {
        return ORBIS_SIGNIN_DIALOG_STATUS_NONE;
    }
    if (!g_lib_open) {
        return ORBIS_SIGNIN_DIALOG_STATUS_NONE;
    }
    c_log(C_LOG_LIB_SIGNIN_DIALOG, C_LOG_LEVEL_ERROR, "sceSigninDialogGetStatus",
          "(STUBBED) called, return 'finished' status");
    return ORBIS_SIGNIN_DIALOG_STATUS_FINISHED;
}

u32 PS4_SYSV_ABI sceSigninDialogUpdateStatus(void) {
    if (!g_lib_init) {
        return ORBIS_SIGNIN_DIALOG_STATUS_NONE;
    }
    if (!g_lib_open) {
        return ORBIS_SIGNIN_DIALOG_STATUS_NONE;
    }
    c_log(C_LOG_LIB_SIGNIN_DIALOG, C_LOG_LEVEL_ERROR, "sceSigninDialogUpdateStatus",
          "(STUBBED) called, return 'finished' status");
    return g_lib_open ? ORBIS_SIGNIN_DIALOG_STATUS_FINISHED : ORBIS_SIGNIN_DIALOG_STATUS_NONE;
}

s32 PS4_SYSV_ABI sceSigninDialogGetResult(OrbisSigninDialogResult* result) {
    if (!g_lib_init) {
        return (s32)ORBIS_SIGNIN_DIALOG_ERROR_NOT_INITIALIZED;
    }
    c_log(C_LOG_LIB_SIGNIN_DIALOG, C_LOG_LEVEL_ERROR, "sceSigninDialogGetResult",
          "(STUBBED) called");
    if (!result) {
        return (s32)ORBIS_SIGNIN_DIALOG_ERROR_PARAM_INVALID;
    }
    result->result = ORBIS_SIGNIN_DIALOG_RESULT_OK;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSigninDialogClose(void) {
    c_log(C_LOG_LIB_SIGNIN_DIALOG, C_LOG_LEVEL_ERROR, "sceSigninDialogClose", "(STUBBED) called");
    g_lib_open = false;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSigninDialogTerminate(void) {
    c_log(C_LOG_LIB_SIGNIN_DIALOG, C_LOG_LEVEL_ERROR, "sceSigninDialogTerminate",
          "(STUBBED) called");
    g_lib_open = false;
    g_lib_init = false;
    return ORBIS_OK;
}
