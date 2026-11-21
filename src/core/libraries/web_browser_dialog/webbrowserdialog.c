/* SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "common/c_bridge.h"
#include "common/enum_names.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/web_browser_dialog/webbrowserdialog.h"

static u32 g_status = COMMON_DIALOG_STATUS_NONE;

u32 PS4_SYSV_ABI sceWebBrowserDialogClose(void) {
    c_log(C_LOG_LIB_WEB_BROWSER_DIALOG, C_LOG_LEVEL_ERROR, "sceWebBrowserDialogClose",
          "(STUBBED) called");
    if (g_status != COMMON_DIALOG_STATUS_RUNNING) {
        return COMMON_DIALOG_ERROR_NOT_RUNNING;
    }
    /* TODO close Ui dialog */
    c_log(C_LOG_LIB_NP_PROFILE_DIALOG, C_LOG_LEVEL_INFO, "sceWebBrowserDialogClose",
          "TODO: close npprofile ui dialog");
    return COMMON_DIALOG_ERROR_OK;
}

s32 PS4_SYSV_ABI sceWebBrowserDialogGetEvent(void) {
    c_log(C_LOG_LIB_WEB_BROWSER_DIALOG, C_LOG_LEVEL_ERROR, "sceWebBrowserDialogGetEvent",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceWebBrowserDialogGetResult(void) {
    c_log(C_LOG_LIB_WEB_BROWSER_DIALOG, C_LOG_LEVEL_ERROR, "sceWebBrowserDialogGetResult",
          "(STUBBED) called");
    return ORBIS_OK;
}

u32 PS4_SYSV_ABI sceWebBrowserDialogGetStatus(void) {
    c_log(C_LOG_LIB_WEB_BROWSER_DIALOG, C_LOG_LEVEL_TRACE, "sceWebBrowserDialogGetStatus",
          "called status=%s", enum_name(&enum_names_Libraries_CommonDialog_Status, (s64)g_status));
    return g_status;
}

u32 PS4_SYSV_ABI sceWebBrowserDialogInitialize(void) {
    if (!g_common_dialog_initialized) {
        return COMMON_DIALOG_ERROR_NOT_SYSTEM_INITIALIZED;
    }
    if (g_status != COMMON_DIALOG_STATUS_NONE) {
        c_log(C_LOG_LIB_WEB_BROWSER_DIALOG, C_LOG_LEVEL_ERROR, "sceWebBrowserDialogInitialize",
              "already initialized");
        return COMMON_DIALOG_ERROR_ALREADY_INITIALIZED;
    }
    if (g_common_dialog_used) {
        return COMMON_DIALOG_ERROR_BUSY;
    }
    g_status = COMMON_DIALOG_STATUS_INITIALIZED;
    g_common_dialog_used = true;
    return COMMON_DIALOG_ERROR_OK;
}

s32 PS4_SYSV_ABI sceWebBrowserDialogNavigate(void) {
    c_log(C_LOG_LIB_WEB_BROWSER_DIALOG, C_LOG_LEVEL_ERROR, "sceWebBrowserDialogNavigate",
          "(STUBBED) called");
    return ORBIS_OK;
}

u32 PS4_SYSV_ABI sceWebBrowserDialogOpen(void) {
    if (g_status != COMMON_DIALOG_STATUS_INITIALIZED && g_status != COMMON_DIALOG_STATUS_FINISHED) {
        c_log(C_LOG_LIB_WEB_BROWSER_DIALOG, C_LOG_LEVEL_INFO, "sceWebBrowserDialogOpen",
              "called without initialize");
        return COMMON_DIALOG_ERROR_INVALID_STATE;
    }
    /* TODO open ui dialog */
    c_log(C_LOG_LIB_WEB_BROWSER_DIALOG, C_LOG_LEVEL_ERROR, "sceWebBrowserDialogOpen",
          "(STUBBED) called");
    g_status = COMMON_DIALOG_STATUS_RUNNING;
    return COMMON_DIALOG_ERROR_OK;
}

s32 PS4_SYSV_ABI sceWebBrowserDialogOpenForPredeterminedContent(void) {
    c_log(C_LOG_LIB_WEB_BROWSER_DIALOG, C_LOG_LEVEL_ERROR,
          "sceWebBrowserDialogOpenForPredeterminedContent", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceWebBrowserDialogResetCookie(void) {
    c_log(C_LOG_LIB_WEB_BROWSER_DIALOG, C_LOG_LEVEL_ERROR, "sceWebBrowserDialogResetCookie",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceWebBrowserDialogSetCookie(void) {
    c_log(C_LOG_LIB_WEB_BROWSER_DIALOG, C_LOG_LEVEL_ERROR, "sceWebBrowserDialogSetCookie",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceWebBrowserDialogSetZoom(void) {
    c_log(C_LOG_LIB_WEB_BROWSER_DIALOG, C_LOG_LEVEL_ERROR, "sceWebBrowserDialogSetZoom",
          "(STUBBED) called");
    return ORBIS_OK;
}

u32 PS4_SYSV_ABI sceWebBrowserDialogTerminate(void) {
    if (g_status == COMMON_DIALOG_STATUS_RUNNING) {
        sceWebBrowserDialogClose();
    }
    if (g_status == COMMON_DIALOG_STATUS_NONE) {
        return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    }
    g_status = COMMON_DIALOG_STATUS_NONE;
    g_common_dialog_used = false;
    return COMMON_DIALOG_ERROR_OK;
}

u32 PS4_SYSV_ABI sceWebBrowserDialogUpdateStatus(void) {
    c_log(C_LOG_LIB_WEB_BROWSER_DIALOG, C_LOG_LEVEL_TRACE, "sceWebBrowserDialogUpdateStatus",
          "called status=%s", enum_name(&enum_names_Libraries_CommonDialog_Status, (s64)g_status));
    if (g_status == COMMON_DIALOG_STATUS_RUNNING) {
        /* TODO removed it when implementing real dialog */
        g_status = COMMON_DIALOG_STATUS_FINISHED;
    }
    return g_status;
}

s32 PS4_SYSV_ABI WebBrowserDialog_Func_F2BE042771625F8C(void) {
    c_log(C_LOG_LIB_WEB_BROWSER_DIALOG, C_LOG_LEVEL_ERROR, "Func_F2BE042771625F8C",
          "(STUBBED) called");
    return ORBIS_OK;
}
