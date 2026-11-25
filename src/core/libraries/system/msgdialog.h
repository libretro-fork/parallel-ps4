/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_SYSTEM_MSGDIALOG_H
#define SHADPS4_LIBRARIES_SYSTEM_MSGDIALOG_H

/* libSceMsgDialog: a message, a progress bar or a system message over the game, drawn by the
 * overlay (video_core/overlay/ovl_msgdialog.c). Defined in msgdialog.c, registered in
 * libs.cpp. */

#include <stddef.h>

#include <retro_common_api.h>

#include "common/types.h"
#include "core/libraries/system/commondialog_c.h"

RETRO_BEGIN_DECLS

typedef enum MsgDialogMode {
    MSG_DIALOG_MODE_USER_MSG = 1,
    MSG_DIALOG_MODE_PROGRESS_BAR = 2,
    MSG_DIALOG_MODE_SYSTEM_MSG = 3
} MsgDialogMode;

/* ButtonId: OK and YES are 1, NO is 2; of two buttons, the first and the second */
#define MSG_DIALOG_BUTTON_ID_INVALID 0u
#define MSG_DIALOG_BUTTON_ID_OK 1u
#define MSG_DIALOG_BUTTON_ID_NO 2u

/* ButtonType */
#define MSG_DIALOG_BUTTON_TYPE_OK 0u
#define MSG_DIALOG_BUTTON_TYPE_YESNO 1u
#define MSG_DIALOG_BUTTON_TYPE_NONE 2u
#define MSG_DIALOG_BUTTON_TYPE_OK_CANCEL 3u
#define MSG_DIALOG_BUTTON_TYPE_WAIT 5u
#define MSG_DIALOG_BUTTON_TYPE_WAIT_CANCEL 6u
#define MSG_DIALOG_BUTTON_TYPE_YESNO_FOCUS_NO 7u
#define MSG_DIALOG_BUTTON_TYPE_OK_CANCEL_FOCUS_CANCEL 8u
#define MSG_DIALOG_BUTTON_TYPE_TWO_BUTTONS 9u

/* ProgressBarType */
#define MSG_DIALOG_PROGRESS_BAR_PERCENTAGE 0u
#define MSG_DIALOG_PROGRESS_BAR_PERCENTAGE_CANCEL 1u

/* SystemMessageType */
#define MSG_DIALOG_SYSMSG_TRC_EMPTY_STORE 0u
#define MSG_DIALOG_SYSMSG_TRC_PSN_CHAT_RESTRICTION 1u
#define MSG_DIALOG_SYSMSG_TRC_PSN_UGC_RESTRICTION 2u
#define MSG_DIALOG_SYSMSG_CAMERA_NOT_CONNECTED 4u
#define MSG_DIALOG_SYSMSG_WARNING_PROFILE_PICTURE_AND_NAME_NOT_SHARED 5u

/* OrbisMsgDialogProgressBarTarget */
#define MSG_DIALOG_PROGRESS_BAR_TARGET_DEFAULT 0u

typedef struct OrbisMsgDialogButtonsParam {
    const char* msg1;
    const char* msg2;
    char reserved[32];
} OrbisMsgDialogButtonsParam;

typedef struct OrbisMsgDialogUserMessageParam {
    u32 buttonType;
    s32 pad;
    const char* msg;
    OrbisMsgDialogButtonsParam* buttonsParam;
    char reserved[24];
} OrbisMsgDialogUserMessageParam;

typedef struct OrbisMsgDialogProgressBarParam {
    u32 barType;
    s32 pad;
    const char* msg;
    char reserved[64];
} OrbisMsgDialogProgressBarParam;

typedef struct OrbisMsgDialogSystemMessageParam {
    u32 sysMsgType;
    char reserved[32];
} OrbisMsgDialogSystemMessageParam;

typedef struct OrbisMsgDialogParam {
    common_dialog_base_param baseParam;
    size_t size;
    u32 mode;
    s32 pad;
    OrbisMsgDialogUserMessageParam* userMsgParam;
    OrbisMsgDialogProgressBarParam* progBarParam;
    OrbisMsgDialogSystemMessageParam* sysMsgParam;
    s32 userId;
    char reserved[40];
    s32 pad2;
} OrbisMsgDialogParam;

typedef struct OrbisMsgDialogResult {
    u32 mode; /* always 0 */
    u32 result;
    u32 buttonId;
    char reserved[32];
} OrbisMsgDialogResult;

u32 PS4_SYSV_ABI sceMsgDialogClose(void);
u32 PS4_SYSV_ABI sceMsgDialogGetResult(OrbisMsgDialogResult* result);
u32 PS4_SYSV_ABI sceMsgDialogGetStatus(void);
u32 PS4_SYSV_ABI sceMsgDialogInitialize(void);
u32 PS4_SYSV_ABI sceMsgDialogOpen(const OrbisMsgDialogParam* param);
u32 PS4_SYSV_ABI sceMsgDialogProgressBarInc(u32 target, u32 delta);
u32 PS4_SYSV_ABI sceMsgDialogProgressBarSetMsg(u32 target, const char* msg);
u32 PS4_SYSV_ABI sceMsgDialogProgressBarSetValue(u32 target, u32 value);
u32 PS4_SYSV_ABI sceMsgDialogTerminate(void);
u32 PS4_SYSV_ABI sceMsgDialogUpdateStatus(void);

RETRO_END_DECLS

#endif
