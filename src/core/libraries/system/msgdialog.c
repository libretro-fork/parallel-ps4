/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* libSceMsgDialog. The status and the result are the overlay's
 * (video_core/overlay/ovl_msgdialog.c), which draws the dialog and finishes it when a button is
 * pressed; the mode is the game's thread's own. The game's texts are lent to the overlay for
 * the call that opens the dialog: it keeps copies of its own. */

#include <string.h>

#include "common/c_bridge.h"
#include "common/enum_names.h"
#include "core/libraries/system/msgdialog.h"
#include "video_core/overlay/ovl_layers.h"

#define GAME OVL_MSG_DIALOG_GAME

static u32 g_mode;

static u32 dialog_status(void) {
    return (u32)ovl_msg_dialog_status(GAME);
}

static void log_called(enum c_log_level level, const char* func, const char* msg) {
    c_log(C_LOG_LIB_MSG_DLG, level, func, "%s", msg);
}

u32 PS4_SYSV_ABI sceMsgDialogClose(void) {
    log_called(C_LOG_LEVEL_DEBUG, "sceMsgDialogClose", "called");
    if (dialog_status() != COMMON_DIALOG_STATUS_RUNNING) {
        return COMMON_DIALOG_ERROR_NOT_RUNNING;
    }
    ovl_msg_dialog_close(GAME, OVL_MSG_BUTTON_INVALID, OVL_MSG_RESULT_OK);
    return COMMON_DIALOG_ERROR_OK;
}

u32 PS4_SYSV_ABI sceMsgDialogGetResult(OrbisMsgDialogResult* result) {
    int button, r;
    log_called(C_LOG_LEVEL_DEBUG, "sceMsgDialogGetResult", "called");
    if (dialog_status() != COMMON_DIALOG_STATUS_FINISHED) {
        return COMMON_DIALOG_ERROR_NOT_FINISHED;
    }
    if (result == NULL) {
        return COMMON_DIALOG_ERROR_ARG_NULL;
    }
    ovl_msg_dialog_result(GAME, &button, &r);
    memset(result, 0, sizeof(*result));
    result->result = (u32)r;
    result->buttonId = (u32)button;
    return COMMON_DIALOG_ERROR_OK;
}

static u32 status_logged(const char* func) {
    const u32 status = dialog_status();
#ifndef NDEBUG
    c_log(C_LOG_LIB_MSG_DLG, C_LOG_LEVEL_TRACE, func, "called status=%s",
          enum_name(&enum_names_Libraries_CommonDialog_Status, (s64)status));
#else
    (void)func;
#endif
    return status;
}

u32 PS4_SYSV_ABI sceMsgDialogGetStatus(void) {
    return status_logged("sceMsgDialogGetStatus");
}

u32 PS4_SYSV_ABI sceMsgDialogInitialize(void) {
    log_called(C_LOG_LEVEL_DEBUG, "sceMsgDialogInitialize", "called");
    if (!g_common_dialog_initialized) {
        return COMMON_DIALOG_ERROR_NOT_SYSTEM_INITIALIZED;
    }
    if (dialog_status() != COMMON_DIALOG_STATUS_NONE) {
        return COMMON_DIALOG_ERROR_ALREADY_INITIALIZED;
    }
    if (g_common_dialog_used) {
        return COMMON_DIALOG_ERROR_BUSY;
    }
    ovl_msg_dialog_set_status(GAME, (int)COMMON_DIALOG_STATUS_INITIALIZED);
    g_common_dialog_used = true;
    return COMMON_DIALOG_ERROR_OK;
}

u32 PS4_SYSV_ABI sceMsgDialogOpen(const OrbisMsgDialogParam* param) {
    static const char func[] = "sceMsgDialogOpen";
    const u32 status = dialog_status();
    ovl_msg_dialog_param_t p;
    if (status != COMMON_DIALOG_STATUS_INITIALIZED && status != COMMON_DIALOG_STATUS_FINISHED) {
        log_called(C_LOG_LEVEL_INFO, func, "called without initialize");
        return COMMON_DIALOG_ERROR_INVALID_STATE;
    }
    if (param == NULL) {
        log_called(C_LOG_LEVEL_DEBUG, func, "called param:(NULL)");
        return COMMON_DIALOG_ERROR_ARG_NULL;
    }
    c_log(C_LOG_LIB_MSG_DLG, C_LOG_LEVEL_DEBUG, func, "called param->mode: %s",
          enum_name(&enum_names_MsgDialogMode, (s64)param->mode));
    C_ASSERT_MSG(param->size == sizeof(OrbisMsgDialogParam), func,
                 "param->size == sizeof(OrbisParam)");
    C_ASSERT_MSG(param->baseParam.size == sizeof(common_dialog_base_param), func,
                 "param->baseParam.size == sizeof(CommonDialog::BaseParam)");
    memset(&p, 0, sizeof(p));
    p.mode = (int)param->mode;
    switch (param->mode) {
    case MSG_DIALOG_MODE_USER_MSG: {
        const OrbisMsgDialogUserMessageParam* v = param->userMsgParam;
        C_ASSERT_MSG(v != NULL, func, "param.userMsgParam");
        C_ASSERT_MSG(v->buttonType <= MSG_DIALOG_BUTTON_TYPE_TWO_BUTTONS, func,
                     "user.type <= ButtonType::TWO_BUTTONS");
        p.button_type = (int)v->buttonType;
        p.msg = v->msg;
        if (v->buttonType == MSG_DIALOG_BUTTON_TYPE_TWO_BUTTONS) {
            C_ASSERT_MSG(v->buttonsParam != NULL, func, "v.buttonsParam");
            p.button1 = v->buttonsParam->msg1;
            p.button2 = v->buttonsParam->msg2;
        }
        break;
    }
    case MSG_DIALOG_MODE_PROGRESS_BAR: {
        const OrbisMsgDialogProgressBarParam* v = param->progBarParam;
        C_ASSERT_MSG(v != NULL, func, "param.progBarParam");
        p.bar_type = (int)v->barType;
        p.msg = v->msg;
        break;
    }
    case MSG_DIALOG_MODE_SYSTEM_MSG: {
        const OrbisMsgDialogSystemMessageParam* v = param->sysMsgParam;
        C_ASSERT_MSG(v != NULL, func, "param.sysMsgParam");
        C_ASSERT_MSG(v->sysMsgType <= MSG_DIALOG_SYSMSG_WARNING_PROFILE_PICTURE_AND_NAME_NOT_SHARED,
                     func,
                     "sys.type <= SystemMessageType::WARNING_PROFILE_PICTURE_AND_NAME_NOT_SHARED");
        p.sys_type = (int)v->sysMsgType;
        break;
    }
    default:
        c_unreachable(__FILE__, __LINE__, func, "Unknown dialog mode");
    }
    g_mode = param->mode;
    ovl_msg_dialog_open(GAME, &p);
    return COMMON_DIALOG_ERROR_OK;
}

static u32 check_progress_bar(u32 target) {
    if (dialog_status() != COMMON_DIALOG_STATUS_RUNNING) {
        return COMMON_DIALOG_ERROR_NOT_RUNNING;
    }
    if (g_mode != MSG_DIALOG_MODE_PROGRESS_BAR) {
        return COMMON_DIALOG_ERROR_NOT_SUPPORTED;
    }
    if (target != MSG_DIALOG_PROGRESS_BAR_TARGET_DEFAULT) {
        return COMMON_DIALOG_ERROR_PARAM_INVALID;
    }
    return COMMON_DIALOG_ERROR_OK;
}

u32 PS4_SYSV_ABI sceMsgDialogProgressBarInc(u32 target, u32 delta) {
    u32 error;
    log_called(C_LOG_LEVEL_DEBUG, "sceMsgDialogProgressBarInc", "called");
    error = check_progress_bar(target);
    if (error == COMMON_DIALOG_ERROR_OK) {
        ovl_msg_dialog_add_progress(GAME, delta);
    }
    return error;
}

u32 PS4_SYSV_ABI sceMsgDialogProgressBarSetMsg(u32 target, const char* msg) {
    u32 error;
    log_called(C_LOG_LEVEL_DEBUG, "sceMsgDialogProgressBarSetMsg", "called");
    error = check_progress_bar(target);
    if (error == COMMON_DIALOG_ERROR_OK) {
        ovl_msg_dialog_set_message(GAME, msg);
    }
    return error;
}

u32 PS4_SYSV_ABI sceMsgDialogProgressBarSetValue(u32 target, u32 value) {
    u32 error;
    log_called(C_LOG_LEVEL_DEBUG, "sceMsgDialogProgressBarSetValue", "called");
    error = check_progress_bar(target);
    if (error == COMMON_DIALOG_ERROR_OK) {
        ovl_msg_dialog_set_progress(GAME, value);
    }
    return error;
}

u32 PS4_SYSV_ABI sceMsgDialogTerminate(void) {
    log_called(C_LOG_LEVEL_DEBUG, "sceMsgDialogTerminate", "called");
    if (dialog_status() == COMMON_DIALOG_STATUS_RUNNING) {
        sceMsgDialogClose();
    }
    if (dialog_status() == COMMON_DIALOG_STATUS_NONE) {
        return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    }
    ovl_msg_dialog_set_status(GAME, (int)COMMON_DIALOG_STATUS_NONE);
    g_common_dialog_used = false;
    return COMMON_DIALOG_ERROR_OK;
}

u32 PS4_SYSV_ABI sceMsgDialogUpdateStatus(void) {
    return status_logged("sceMsgDialogUpdateStatus");
}
