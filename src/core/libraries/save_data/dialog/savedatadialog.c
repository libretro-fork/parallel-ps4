/* SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* libSceSaveDataDialog. The dialog is drawn by the overlay (video_core/overlay/
 * ovl_savedialog.c), which keeps its status and result, and is lent what it shows from one of
 * three states, the one it says to build in next; the overlay never draws from that one, so
 * building there waits for nothing. */

#include <string.h>

#include "common/c_bridge.h"
#include "common/enum_names.h"
#include "core/libraries/save_data/dialog/savedatadialog.h"
#include "core/libraries/save_data/dialog/savedatadialog_ui.h"
#include "core/libraries/system/commondialog_c.h"
#include "video_core/overlay/ovl_layers.h"

static save_dialog_state_t g_states[3];
static int g_opened = -1; /* the one the dialog opened last was built in */
static save_dialog_result_t g_result;

static void sdd_log(enum c_log_level level, const char* func, const char* msg) {
    c_log(C_LOG_LIB_SAVE_DATA_DIALOG, level, func, "%s", msg);
}

static u32 sdd_status(void) {
    return (u32)ovl_save_dialog_status();
}

u32 PS4_SYSV_ABI sceSaveDataDialogClose(void) {
    sdd_log(C_LOG_LEVEL_DEBUG, "sceSaveDataDialogClose", "called");
    if (sdd_status() != COMMON_DIALOG_STATUS_RUNNING) {
        return COMMON_DIALOG_ERROR_NOT_RUNNING;
    }
    ovl_save_dialog_close(OVL_SAVE_BUTTON_INVALID, OVL_SAVE_RESULT_OK);
    return COMMON_DIALOG_ERROR_OK;
}

u32 PS4_SYSV_ABI sceSaveDataDialogGetResult(OrbisSaveDataDialogResult* result) {
    sdd_log(C_LOG_LEVEL_DEBUG, "sceSaveDataDialogGetResult", "called");
    if (sdd_status() != COMMON_DIALOG_STATUS_FINISHED) {
        return COMMON_DIALOG_ERROR_NOT_FINISHED;
    }
    if (result == NULL) {
        return COMMON_DIALOG_ERROR_ARG_NULL;
    }
    if (g_opened >= 0) {
        int button, res, item;
        ovl_save_dialog_result(&button, &res, &item);
        save_dialog_state_make_result(&g_states[g_opened], &g_result, button, res, item);
    }
    save_dialog_result_copy_to(&g_result, result);
    return COMMON_DIALOG_ERROR_OK;
}

static u32 sdd_status_logged(const char* func) {
    const u32 status = sdd_status();
#ifndef NDEBUG
    c_log(C_LOG_LIB_SAVE_DATA_DIALOG, C_LOG_LEVEL_TRACE, func, "called status=%s",
          enum_name(&enum_names_Libraries_CommonDialog_Status, (s64)status));
#else
    (void)func;
#endif
    return status;
}

u32 PS4_SYSV_ABI sceSaveDataDialogGetStatus(void) {
    return sdd_status_logged("sceSaveDataDialogGetStatus");
}

u32 PS4_SYSV_ABI sceSaveDataDialogInitialize(void) {
    sdd_log(C_LOG_LEVEL_DEBUG, "sceSaveDataDialogInitialize", "called");
    if (!g_common_dialog_initialized) {
        return COMMON_DIALOG_ERROR_NOT_SYSTEM_INITIALIZED;
    }
    if (sdd_status() != COMMON_DIALOG_STATUS_NONE) {
        return COMMON_DIALOG_ERROR_ALREADY_INITIALIZED;
    }
    if (g_common_dialog_used) {
        return COMMON_DIALOG_ERROR_BUSY;
    }
    ovl_save_dialog_set_status((int)COMMON_DIALOG_STATUS_INITIALIZED);
    g_common_dialog_used = true;
    return COMMON_DIALOG_ERROR_OK;
}

s32 PS4_SYSV_ABI sceSaveDataDialogIsReadyToDisplay(void) {
    return 1;
}

u32 PS4_SYSV_ABI sceSaveDataDialogOpen(const OrbisSaveDataDialogParam* param) {
    const u32 status = sdd_status();
    ovl_save_dialog_param_t open;
    int back;
    if (status != COMMON_DIALOG_STATUS_INITIALIZED && status != COMMON_DIALOG_STATUS_FINISHED) {
        sdd_log(C_LOG_LEVEL_INFO, "sceSaveDataDialogOpen", "called without initialize");
        return COMMON_DIALOG_ERROR_INVALID_STATE;
    }
    if (param == NULL) {
        sdd_log(C_LOG_LEVEL_DEBUG, "sceSaveDataDialogOpen", "called param:(NULL)");
        return COMMON_DIALOG_ERROR_ARG_NULL;
    }
    c_log(C_LOG_LIB_SAVE_DATA_DIALOG, C_LOG_LEVEL_DEBUG, "sceSaveDataDialogOpen",
          "called param->mode: %s", enum_name(&enum_names_SaveDataDialogMode, (s64)param->mode));
    C_ASSERT_MSG(param->size == (s32)sizeof(OrbisSaveDataDialogParam), "sceSaveDataDialogOpen",
                 "param->size == (s32)sizeof(OrbisSaveDataDialogParam)");
    C_ASSERT_MSG(param->baseParam.size == sizeof(common_dialog_base_param), "sceSaveDataDialogOpen",
                 "param->baseParam.size == sizeof(common_dialog_base_param)");
    memset(&g_result, 0, sizeof(g_result));
    /* The last dialog's icons go (it is finished, so no longer drawn), then what was built where
     * this one is built */
    if (g_opened >= 0) {
        save_dialog_state_release_icons(&g_states[g_opened]);
    }
    back = ovl_save_dialog_back();
    save_dialog_state_release_icons(&g_states[back]);
    save_dialog_state_build(&g_states[back], param);
    open = save_dialog_state_make_param(&g_states[back]);
    g_opened = ovl_save_dialog_open(&open);
    return COMMON_DIALOG_ERROR_OK;
}

static u32 sdd_progress_check(u32 target) {
    if (sdd_status() != COMMON_DIALOG_STATUS_RUNNING) {
        return COMMON_DIALOG_ERROR_NOT_RUNNING;
    }
    if (g_opened < 0 || g_states[g_opened].mode != SAVE_DATA_DIALOG_MODE_PROGRESS_BAR) {
        return COMMON_DIALOG_ERROR_NOT_SUPPORTED;
    }
    if (target != SAVE_DATA_DIALOG_PROGRESS_BAR_TARGET_DEFAULT) {
        return COMMON_DIALOG_ERROR_PARAM_INVALID;
    }
    return COMMON_DIALOG_ERROR_OK;
}

u32 PS4_SYSV_ABI sceSaveDataDialogProgressBarInc(u32 target, u32 delta) {
    u32 error;
    sdd_log(C_LOG_LEVEL_DEBUG, "sceSaveDataDialogProgressBarInc", "called");
    error = sdd_progress_check(target);
    if (error != COMMON_DIALOG_ERROR_OK) {
        return error;
    }
    ovl_save_dialog_add_progress(delta);
    return COMMON_DIALOG_ERROR_OK;
}

u32 PS4_SYSV_ABI sceSaveDataDialogProgressBarSetValue(u32 target, u32 rate) {
    u32 error;
    sdd_log(C_LOG_LEVEL_DEBUG, "sceSaveDataDialogProgressBarSetValue", "called");
    error = sdd_progress_check(target);
    if (error != COMMON_DIALOG_ERROR_OK) {
        return error;
    }
    ovl_save_dialog_set_progress(rate);
    return COMMON_DIALOG_ERROR_OK;
}

u32 PS4_SYSV_ABI sceSaveDataDialogTerminate(void) {
    int i;
    sdd_log(C_LOG_LEVEL_DEBUG, "sceSaveDataDialogTerminate", "called");
    if (sdd_status() == COMMON_DIALOG_STATUS_RUNNING) {
        sceSaveDataDialogClose();
    }
    if (sdd_status() == COMMON_DIALOG_STATUS_NONE) {
        return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    }
    /* Only the icons go: the overlay may still be drawing the last frame of the one it showed */
    for (i = 0; i < 3; i++) {
        save_dialog_state_release_icons(&g_states[i]);
    }
    ovl_save_dialog_set_status((int)COMMON_DIALOG_STATUS_NONE);
    g_common_dialog_used = false;
    return COMMON_DIALOG_ERROR_OK;
}

u32 PS4_SYSV_ABI sceSaveDataDialogUpdateStatus(void) {
    return sdd_status_logged("sceSaveDataDialogUpdateStatus");
}
