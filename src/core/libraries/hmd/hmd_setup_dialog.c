/* SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "common/c_bridge.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/hmd/hmd_setup_dialog.h"
#include "core/libraries/system/commondialog_c.h"

s32 PS4_SYSV_ABI sceHmdSetupDialogInitialize(void) {
    c_log(C_LOG_LIB_HMD_SETUP_DIALOG, C_LOG_LEVEL_ERROR, "sceHmdSetupDialogInitialize",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdSetupDialogClose(void) {
    c_log(C_LOG_LIB_HMD_SETUP_DIALOG, C_LOG_LEVEL_ERROR, "sceHmdSetupDialogClose",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdSetupDialogOpen(const OrbisHmdSetupDialogParam* param) {
    c_log(C_LOG_LIB_HMD_SETUP_DIALOG, C_LOG_LEVEL_ERROR, "sceHmdSetupDialogOpen",
          "(STUBBED) called");
    /* On real hardware, a dialog would show up telling the user to connect a PSVR headset. */
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHmdSetupDialogGetResult(OrbisHmdSetupDialogResult* result) {
    c_log(C_LOG_LIB_HMD_SETUP_DIALOG, C_LOG_LEVEL_ERROR, "sceHmdSetupDialogGetResult",
          "(STUBBED) called");
    /* Simulates behavior of user pressing circle to cancel the dialog. */
    /* Result::OK would mean a headset was connected. */
    result->result = COMMON_DIALOG_RESULT_USER_CANCELED;
    return ORBIS_OK;
}

u32 PS4_SYSV_ABI sceHmdSetupDialogUpdateStatus(void) {
    c_log(C_LOG_LIB_HMD_SETUP_DIALOG, C_LOG_LEVEL_ERROR, "sceHmdSetupDialogUpdateStatus",
          "(STUBBED) called");
    return COMMON_DIALOG_STATUS_FINISHED;
}

u32 PS4_SYSV_ABI sceHmdSetupDialogGetStatus(void) {
    c_log(C_LOG_LIB_HMD_SETUP_DIALOG, C_LOG_LEVEL_ERROR, "sceHmdSetupDialogGetStatus",
          "(STUBBED) called");
    return COMMON_DIALOG_STATUS_FINISHED;
}

s32 PS4_SYSV_ABI sceHmdSetupDialogTerminate(void) {
    c_log(C_LOG_LIB_HMD_SETUP_DIALOG, C_LOG_LEVEL_ERROR, "sceHmdSetupDialogTerminate",
          "(STUBBED) called");
    return ORBIS_OK;
}
