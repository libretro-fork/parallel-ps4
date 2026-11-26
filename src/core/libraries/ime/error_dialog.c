/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* libSceErrorDialog. Drawn by the overlay (video_core/overlay/ovl_msgdialog.c), which
 * finishes it when OK is pressed; the status is the overlay's own. */

#include <stdio.h>
#include <string.h>

#include "common/c_bridge.h"
#include "common/enum_names.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/ime/error_dialog.h"
#include "core/libraries/system/commondialog_c.h"
#include "video_core/overlay/ovl_layers.h"

#define LOG_CLASS C_LOG_LIB_ERROR_DIALOG
#define DIALOG OVL_MSG_DIALOG_ERROR

static u32 dialog_status(void) {
    return (u32)ovl_msg_dialog_status(DIALOG);
}

static u32 status_logged(const char* func) {
    const u32 status = dialog_status();
#ifndef NDEBUG
    c_log(LOG_CLASS, C_LOG_LEVEL_TRACE, func, "called status=%s",
          enum_name(&enum_names_Libraries_CommonDialog_Status, (s64)status));
#else
    (void)func;
#endif
    return status;
}

u32 PS4_SYSV_ABI sceErrorDialogClose(void) {
    c_log(LOG_CLASS, C_LOG_LEVEL_DEBUG, "sceErrorDialogClose", "called");
    if (dialog_status() != COMMON_DIALOG_STATUS_RUNNING) {
        return COMMON_DIALOG_ERROR_NOT_RUNNING;
    }
    ovl_msg_dialog_close(DIALOG, OVL_MSG_BUTTON_INVALID, OVL_MSG_RESULT_OK);
    return COMMON_DIALOG_ERROR_OK;
}

u32 PS4_SYSV_ABI sceErrorDialogGetStatus(void) {
    return status_logged("sceErrorDialogGetStatus");
}

u32 PS4_SYSV_ABI sceErrorDialogInitialize(void) {
    c_log(LOG_CLASS, C_LOG_LEVEL_DEBUG, "sceErrorDialogInitialize", "called");
    if (dialog_status() != COMMON_DIALOG_STATUS_NONE) {
        return COMMON_DIALOG_ERROR_ALREADY_INITIALIZED;
    }
    ovl_msg_dialog_set_status(DIALOG, (int)COMMON_DIALOG_STATUS_INITIALIZED);
    return COMMON_DIALOG_ERROR_OK;
}

u32 PS4_SYSV_ABI sceErrorDialogOpen(const OrbisErrorDialogParam* param) {
    /* The text and the eight digits of a code */
    char message[48];
    ovl_msg_dialog_param_t overlay_param;
    const u32 status = dialog_status();
    if (status != COMMON_DIALOG_STATUS_INITIALIZED && status != COMMON_DIALOG_STATUS_FINISHED) {
        c_log(LOG_CLASS, C_LOG_LEVEL_INFO, "sceErrorDialogOpen", "called without initialize");
        return COMMON_DIALOG_ERROR_INVALID_STATE;
    }
    if (param == NULL) {
        c_log(LOG_CLASS, C_LOG_LEVEL_DEBUG, "sceErrorDialogOpen", "called param:(NULL)");
        return COMMON_DIALOG_ERROR_ARG_NULL;
    }
    c_log(LOG_CLASS, C_LOG_LEVEL_DEBUG, "sceErrorDialogOpen", "called param->errorCode = 0x%x",
          (u32)param->errorCode);
    C_ASSERT_MSG(param->size == (s32)sizeof(OrbisErrorDialogParam), "sceErrorDialogOpen",
                 "the parameter's size is not the structure's");

    /* Lent to the overlay for the call: it keeps a copy of its own */
    sprintf(message, "An error has occurred. \nCode: 0X%X", (u32)param->errorCode);
    memset(&overlay_param, 0, sizeof(overlay_param));
    overlay_param.mode = OVL_MSG_MODE_ERROR;
    overlay_param.msg = message;
    ovl_msg_dialog_open(DIALOG, &overlay_param);
    return COMMON_DIALOG_ERROR_OK;
}

int PS4_SYSV_ABI sceErrorDialogOpenDetail(void) {
    c_log(LOG_CLASS, C_LOG_LEVEL_ERROR, "sceErrorDialogOpenDetail", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceErrorDialogOpenWithReport(void) {
    c_log(LOG_CLASS, C_LOG_LEVEL_ERROR, "sceErrorDialogOpenWithReport", "(STUBBED) called");
    return ORBIS_OK;
}

u32 PS4_SYSV_ABI sceErrorDialogTerminate(void) {
    c_log(LOG_CLASS, C_LOG_LEVEL_DEBUG, "sceErrorDialogTerminate", "called");
    if (dialog_status() == COMMON_DIALOG_STATUS_RUNNING) {
        sceErrorDialogClose();
    }
    if (dialog_status() == COMMON_DIALOG_STATUS_NONE) {
        return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    }
    ovl_msg_dialog_set_status(DIALOG, (int)COMMON_DIALOG_STATUS_NONE);
    return COMMON_DIALOG_ERROR_OK;
}

u32 PS4_SYSV_ABI sceErrorDialogUpdateStatus(void) {
    return status_logged("sceErrorDialogUpdateStatus");
}
