/* SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_WEBBROWSERDIALOG_H
#define SHADPS4_LIBRARIES_WEBBROWSERDIALOG_H

/* libSceWebBrowserDialog: the dialog's states, with no browser shown; one opened is finished
 * at its next status update. Defined in webbrowserdialog.c, registered in libs.cpp. */

#include <retro_common_api.h>

#include "common/types.h"
#include "core/libraries/system/commondialog_c.h"

RETRO_BEGIN_DECLS

/* The errors are COMMON_DIALOG_ERROR_*, the statuses COMMON_DIALOG_STATUS_* */
u32 PS4_SYSV_ABI sceWebBrowserDialogClose(void);
s32 PS4_SYSV_ABI sceWebBrowserDialogGetEvent(void);
s32 PS4_SYSV_ABI sceWebBrowserDialogGetResult(void);
u32 PS4_SYSV_ABI sceWebBrowserDialogGetStatus(void);
u32 PS4_SYSV_ABI sceWebBrowserDialogInitialize(void);
s32 PS4_SYSV_ABI sceWebBrowserDialogNavigate(void);
u32 PS4_SYSV_ABI sceWebBrowserDialogOpen(void);
s32 PS4_SYSV_ABI sceWebBrowserDialogOpenForPredeterminedContent(void);
s32 PS4_SYSV_ABI sceWebBrowserDialogResetCookie(void);
s32 PS4_SYSV_ABI sceWebBrowserDialogSetCookie(void);
s32 PS4_SYSV_ABI sceWebBrowserDialogSetZoom(void);
u32 PS4_SYSV_ABI sceWebBrowserDialogTerminate(void);
u32 PS4_SYSV_ABI sceWebBrowserDialogUpdateStatus(void);
s32 PS4_SYSV_ABI WebBrowserDialog_Func_F2BE042771625F8C(void);

RETRO_END_DECLS

#endif
