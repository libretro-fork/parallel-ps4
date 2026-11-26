/* SPDX-FileCopyrightText: Copyright 2025-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stddef.h>
#include <string.h>

#include "common/c_bridge.h"
#include "common/enum_names.h"
#include "core/libraries/np/np_profile_dialog/np_profile_dialog.h"
#include "video_core/overlay/ovl_layers.h"

/* The dialog is the overlay's (video_core/overlay/ovl_profile.c): its status and what the
 * player chose are kept there, among the game's thread and the overlay's. What the game gave
 * to be handed back with the result is the game's thread's alone */
static void* g_user_data = NULL;

static u32 profile_status(void) {
    return (u32)ovl_profile_dialog_status();
}

static void profile_set_status(u32 status) {
    ovl_profile_dialog_set_status((int)status);
}

u32 PS4_SYSV_ABI sceNpProfileDialogOpen(OrbisNpProfileDialogParam* param) {
    ovl_profile_dialog_param_t shown;
    u32 status = profile_status();
    if (status != COMMON_DIALOG_STATUS_INITIALIZED && status != COMMON_DIALOG_STATUS_FINISHED) {
        c_log(C_LOG_LIB_NP_PROFILE_DIALOG, C_LOG_LEVEL_ERROR, "sceNpProfileDialogOpen",
              "called without initialize");
        return COMMON_DIALOG_ERROR_INVALID_STATE;
    }
    if (param == NULL) {
        c_log(C_LOG_LIB_NP_PROFILE_DIALOG, C_LOG_LEVEL_ERROR, "sceNpProfileDialogOpen",
              "param is nullptr");
        return COMMON_DIALOG_ERROR_ARG_NULL;
    }

    c_log(C_LOG_LIB_NP_PROFILE_DIALOG, C_LOG_LEVEL_DEBUG, "sceNpProfileDialogOpen",
          "param details: mode=%d, userId=%d, targetOnlineId='%.16s', userData=%p",
          (int)param->mode, (int)param->userId, param->targetOnlineId.data, param->userData);

    g_user_data = param->userData;
    memset(&shown, 0, sizeof(shown));
    shown.mode = (int)param->mode;
    shown.online_id = param->targetOnlineId.data;
    ovl_profile_dialog_open(&shown);

    c_log(C_LOG_LIB_NP_PROFILE_DIALOG, C_LOG_LEVEL_INFO, "sceNpProfileDialogOpen",
          "dialog opened successfully for user %d", (int)param->userId);
    return COMMON_DIALOG_ERROR_OK;
}

u32 PS4_SYSV_ABI sceNpProfileDialogClose(void) {
    u32 status;
    c_log(C_LOG_LIB_NP_PROFILE_DIALOG, C_LOG_LEVEL_DEBUG, "sceNpProfileDialogClose", "called");
    status = profile_status();
    if (status == COMMON_DIALOG_STATUS_NONE) {
        return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    }
    /* Pressed on screen at the same moment: finished already, by the player */
    if (status != COMMON_DIALOG_STATUS_RUNNING ||
        !ovl_profile_dialog_close(OVL_PROFILE_RESULT_OK)) {
        return COMMON_DIALOG_ERROR_NOT_RUNNING;
    }
    return COMMON_DIALOG_ERROR_OK;
}

u32 PS4_SYSV_ABI sceNpProfileDialogGetResult(OrbisNpProfileDialogResult* result) {
    u32 status;
    c_log(C_LOG_LIB_NP_PROFILE_DIALOG, C_LOG_LEVEL_DEBUG, "sceNpProfileDialogGetResult", "called");

    status = profile_status();
    if (status == COMMON_DIALOG_STATUS_NONE) {
        c_log(C_LOG_LIB_NP_PROFILE_DIALOG, C_LOG_LEVEL_ERROR, "sceNpProfileDialogGetResult",
              "failed: not initialized (status=NONE)");
        return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    }
    if (result == NULL) {
        c_log(C_LOG_LIB_NP_PROFILE_DIALOG, C_LOG_LEVEL_ERROR, "sceNpProfileDialogGetResult",
              "failed: result pointer is nullptr");
        return COMMON_DIALOG_ERROR_ARG_NULL;
    }
    if (status != COMMON_DIALOG_STATUS_FINISHED) {
        c_log(C_LOG_LIB_NP_PROFILE_DIALOG, C_LOG_LEVEL_ERROR, "sceNpProfileDialogGetResult",
              "failed: dialog not finished (status=%d)", (int)status);
        return COMMON_DIALOG_ERROR_NOT_FINISHED;
    }

    memset(result, 0, sizeof(*result));
    result->result = 0; /* ORBIS_OK */
    result->userAction = (u32)ovl_profile_dialog_result();
    result->userData = g_user_data;

    c_log(C_LOG_LIB_NP_PROFILE_DIALOG, C_LOG_LEVEL_DEBUG, "sceNpProfileDialogGetResult",
          "result details: resultCode=%d, userAction=%d, userData=%p", (int)result->result,
          (int)result->userAction, result->userData);
    return COMMON_DIALOG_ERROR_OK;
}

u32 PS4_SYSV_ABI sceNpProfileDialogGetStatus(void) {
    u32 status = profile_status();
    c_log(C_LOG_LIB_NP_PROFILE_DIALOG, C_LOG_LEVEL_TRACE, "sceNpProfileDialogGetStatus",
          "called status=%s",
          enum_name(&enum_names_Libraries_CommonDialog_Status, (int64_t)status));
    return status;
}

u32 PS4_SYSV_ABI sceNpProfileDialogInitialize(void) {
    u32 status;
    if (!g_common_dialog_initialized) {
        c_log(C_LOG_LIB_NP_PROFILE_DIALOG, C_LOG_LEVEL_ERROR, "sceNpProfileDialogInitialize",
              "failed: system not initialized");
        return COMMON_DIALOG_ERROR_NOT_SYSTEM_INITIALIZED;
    }
    status = profile_status();
    if (status != COMMON_DIALOG_STATUS_NONE) {
        c_log(C_LOG_LIB_NP_PROFILE_DIALOG, C_LOG_LEVEL_ERROR, "sceNpProfileDialogInitialize",
              "failed: already initialized (status=%d)", (int)status);
        return COMMON_DIALOG_ERROR_ALREADY_INITIALIZED;
    }
    if (g_common_dialog_used) {
        c_log(C_LOG_LIB_NP_PROFILE_DIALOG, C_LOG_LEVEL_ERROR, "sceNpProfileDialogInitialize",
              "failed: dialog system busy (g_isUsed=true)");
        return COMMON_DIALOG_ERROR_BUSY;
    }

    profile_set_status(COMMON_DIALOG_STATUS_INITIALIZED);
    g_common_dialog_used = true;
    c_log(C_LOG_LIB_NP_PROFILE_DIALOG, C_LOG_LEVEL_INFO, "sceNpProfileDialogInitialize",
          "initialized successfully");
    return COMMON_DIALOG_ERROR_OK;
}

u32 PS4_SYSV_ABI sceNpProfileDialogOpenA(OrbisNpProfileDialogParamA* param) {
    ovl_profile_dialog_param_t shown;
    u32 status = profile_status();
    if (status != COMMON_DIALOG_STATUS_INITIALIZED && status != COMMON_DIALOG_STATUS_FINISHED) {
        c_log(C_LOG_LIB_NP_PROFILE_DIALOG, C_LOG_LEVEL_ERROR, "sceNpProfileDialogOpenA",
              "called without initialize");
        return COMMON_DIALOG_ERROR_INVALID_STATE;
    }
    if (param == NULL) {
        c_log(C_LOG_LIB_NP_PROFILE_DIALOG, C_LOG_LEVEL_ERROR, "sceNpProfileDialogOpenA",
              "failed: param is nullptr");
        return COMMON_DIALOG_ERROR_ARG_NULL;
    }

    c_log(C_LOG_LIB_NP_PROFILE_DIALOG, C_LOG_LEVEL_DEBUG, "sceNpProfileDialogOpenA",
          "param details: mode=%d, userId=%d, targetAccountId='%llu', userData=%p",
          (int)param->mode, (int)param->userId, (unsigned long long)param->targetAccountId,
          param->userData);
    c_log(C_LOG_LIB_NP_PROFILE_DIALOG, C_LOG_LEVEL_ERROR, "sceNpProfileDialogOpenA",
          "(STUBBED) called");

    g_user_data = param->userData;
    memset(&shown, 0, sizeof(shown));
    shown.mode = (int)param->mode;
    shown.has_account_id = 1;
    shown.account_id = param->targetAccountId;
    ovl_profile_dialog_open(&shown);

    c_log(C_LOG_LIB_NP_PROFILE_DIALOG, C_LOG_LEVEL_INFO, "sceNpProfileDialogOpenA",
          "dialog opened successfully for user %d (account ID variant)", (int)param->userId);
    return COMMON_DIALOG_ERROR_OK;
}

u32 PS4_SYSV_ABI sceNpProfileDialogTerminate(void) {
    c_log(C_LOG_LIB_NP_PROFILE_DIALOG, C_LOG_LEVEL_DEBUG, "sceNpProfileDialogTerminate",
          "called (status=%d)", (int)profile_status());

    if (profile_status() == COMMON_DIALOG_STATUS_RUNNING) {
        c_log(C_LOG_LIB_NP_PROFILE_DIALOG, C_LOG_LEVEL_ERROR, "sceNpProfileDialogTerminate",
              "dialog is running, closing it first");
        sceNpProfileDialogClose();
    }
    if (profile_status() == COMMON_DIALOG_STATUS_NONE) {
        c_log(C_LOG_LIB_NP_PROFILE_DIALOG, C_LOG_LEVEL_ERROR, "sceNpProfileDialogTerminate",
              "failed: not initialized (status=NONE)");
        return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    }

    profile_set_status(COMMON_DIALOG_STATUS_NONE);
    g_common_dialog_used = false;

    c_log(C_LOG_LIB_NP_PROFILE_DIALOG, C_LOG_LEVEL_INFO, "sceNpProfileDialogTerminate",
          "terminated successfully");
    return COMMON_DIALOG_ERROR_OK;
}

u32 PS4_SYSV_ABI sceNpProfileDialogUpdateStatus(void) {
    u32 status = profile_status();
    c_log(C_LOG_LIB_NP_PROFILE_DIALOG, C_LOG_LEVEL_TRACE, "sceNpProfileDialogUpdateStatus",
          "called status=%s",
          enum_name(&enum_names_Libraries_CommonDialog_Status, (int64_t)status));
    return status;
}
