/* SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_SAVE_DATA_DIALOG_SAVEDATADIALOG_H
#define SHADPS4_LIBRARIES_SAVE_DATA_DIALOG_SAVEDATADIALOG_H

/* libSceSaveDataDialog. Defined in savedatadialog.c, registered in libs.cpp. */

#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

struct OrbisSaveDataDialogParam;
struct OrbisSaveDataDialogResult;

u32 PS4_SYSV_ABI sceSaveDataDialogClose(void);
u32 PS4_SYSV_ABI sceSaveDataDialogGetResult(struct OrbisSaveDataDialogResult* result);
u32 PS4_SYSV_ABI sceSaveDataDialogGetStatus(void);
u32 PS4_SYSV_ABI sceSaveDataDialogInitialize(void);
s32 PS4_SYSV_ABI sceSaveDataDialogIsReadyToDisplay(void);
u32 PS4_SYSV_ABI sceSaveDataDialogOpen(const struct OrbisSaveDataDialogParam* param);
u32 PS4_SYSV_ABI sceSaveDataDialogProgressBarInc(u32 target, u32 delta);
u32 PS4_SYSV_ABI sceSaveDataDialogProgressBarSetValue(u32 target, u32 rate);
u32 PS4_SYSV_ABI sceSaveDataDialogTerminate(void);
u32 PS4_SYSV_ABI sceSaveDataDialogUpdateStatus(void);

RETRO_END_DECLS

#endif
