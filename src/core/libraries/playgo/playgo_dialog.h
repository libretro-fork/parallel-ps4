/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_PLAYGO_DIALOG_H
#define SHADPS4_LIBRARIES_PLAYGO_DIALOG_H

/* libScePlayGoDialog: no dialog is shown; its result lets the game go on. Defined in
 * playgo_dialog.c, registered in libs.cpp. */

#include <retro_common_api.h>

#include "common/types.h"
#include "core/libraries/system/commondialog_c.h"

RETRO_BEGIN_DECLS

typedef struct OrbisPlayGoDialogParam {
    common_dialog_base_param baseParam;
    s32 size;
    u8 unk[0x30];
} OrbisPlayGoDialogParam;
typedef char playgo_dialog_param_size_check[sizeof(OrbisPlayGoDialogParam) == 0x68 ? 1 : -1];

typedef struct OrbisPlayGoDialogResult {
    u8 unk1[0x4];
    u32 result; /* COMMON_DIALOG_RESULT_* */
    u8 unk2[0x20];
} OrbisPlayGoDialogResult;
typedef char playgo_dialog_result_size_check[sizeof(OrbisPlayGoDialogResult) == 0x28 ? 1 : -1];

/* The errors are COMMON_DIALOG_ERROR_*, the statuses COMMON_DIALOG_STATUS_* */
u32 PS4_SYSV_ABI scePlayGoDialogClose(void);
u32 PS4_SYSV_ABI scePlayGoDialogGetResult(OrbisPlayGoDialogResult* result);
u32 PS4_SYSV_ABI scePlayGoDialogGetStatus(void);
u32 PS4_SYSV_ABI scePlayGoDialogInitialize(void);
u32 PS4_SYSV_ABI scePlayGoDialogOpen(const OrbisPlayGoDialogParam* param);
u32 PS4_SYSV_ABI scePlayGoDialogTerminate(void);
u32 PS4_SYSV_ABI scePlayGoDialogUpdateStatus(void);

RETRO_END_DECLS

#endif
