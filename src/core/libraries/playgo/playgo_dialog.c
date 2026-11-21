/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stddef.h>

#include "common/c_bridge.h"
#include "core/libraries/playgo/playgo_dialog.h"

u32 PS4_SYSV_ABI scePlayGoDialogClose(void) {
    c_log(C_LOG_LIB_PLAYGO_DIALOG, C_LOG_LEVEL_ERROR, "scePlayGoDialogClose", "(DUMMY) called");
    return COMMON_DIALOG_ERROR_OK;
}

u32 PS4_SYSV_ABI scePlayGoDialogGetResult(OrbisPlayGoDialogResult* result) {
    c_log(C_LOG_LIB_PLAYGO_DIALOG, C_LOG_LEVEL_ERROR, "scePlayGoDialogGetResult", "(DUMMY) called");
    if (result == NULL) {
        return COMMON_DIALOG_ERROR_ARG_NULL;
    }
    /* Result value 3 allows games to proceed. */
    result->result = 3;
    return COMMON_DIALOG_ERROR_OK;
}

u32 PS4_SYSV_ABI scePlayGoDialogGetStatus(void) {
    c_log(C_LOG_LIB_PLAYGO_DIALOG, C_LOG_LEVEL_ERROR, "scePlayGoDialogGetStatus", "(DUMMY) called");
    return COMMON_DIALOG_STATUS_NONE;
}

u32 PS4_SYSV_ABI scePlayGoDialogInitialize(void) {
    c_log(C_LOG_LIB_PLAYGO_DIALOG, C_LOG_LEVEL_ERROR, "scePlayGoDialogInitialize",
          "(DUMMY) called");
    return COMMON_DIALOG_ERROR_OK;
}

u32 PS4_SYSV_ABI scePlayGoDialogOpen(const OrbisPlayGoDialogParam* param) {
    c_log(C_LOG_LIB_PLAYGO_DIALOG, C_LOG_LEVEL_ERROR, "scePlayGoDialogOpen", "(DUMMY) called");
    if (param == NULL) {
        return COMMON_DIALOG_ERROR_ARG_NULL;
    }
    C_ASSERT(param->size == sizeof(OrbisPlayGoDialogParam), "scePlayGoDialogOpen");
    C_ASSERT(param->baseParam.size == sizeof(common_dialog_base_param), "scePlayGoDialogOpen");
    return COMMON_DIALOG_ERROR_OK;
}

u32 PS4_SYSV_ABI scePlayGoDialogTerminate(void) {
    c_log(C_LOG_LIB_PLAYGO_DIALOG, C_LOG_LEVEL_ERROR, "scePlayGoDialogTerminate", "(DUMMY) called");
    return COMMON_DIALOG_ERROR_OK;
}

u32 PS4_SYSV_ABI scePlayGoDialogUpdateStatus(void) {
    c_log(C_LOG_LIB_PLAYGO_DIALOG, C_LOG_LEVEL_ERROR, "scePlayGoDialogUpdateStatus",
          "(DUMMY) called");
    return COMMON_DIALOG_STATUS_NONE;
}
