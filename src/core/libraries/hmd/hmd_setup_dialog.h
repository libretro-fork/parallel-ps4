/* SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_HMD_SETUP_DIALOG_H
#define SHADPS4_HMD_SETUP_DIALOG_H

/* libSceHmdSetupDialog: no headset, so the dialog is cancelled at once. MSVC C89. */

#include <boolean.h>
#include <retro_common_api.h>

#include "common/types.h"
#include "core/libraries/system/commondialog_c.h"

RETRO_BEGIN_DECLS

typedef struct OrbisHmdSetupDialogParam {
    common_dialog_base_param base_param;
    u64 size;
    s32 user_id;
    bool disable_handover_screen;
    u8 reserved[40];
} OrbisHmdSetupDialogParam;

typedef struct OrbisHmdSetupDialogResult {
    u32 result; /* COMMON_DIALOG_RESULT_* */
    u8 reserved[32];
} OrbisHmdSetupDialogResult;

s32 PS4_SYSV_ABI sceHmdSetupDialogClose(void);
s32 PS4_SYSV_ABI sceHmdSetupDialogGetResult(OrbisHmdSetupDialogResult* result);
u32 PS4_SYSV_ABI sceHmdSetupDialogGetStatus(void);
s32 PS4_SYSV_ABI sceHmdSetupDialogInitialize(void);
s32 PS4_SYSV_ABI sceHmdSetupDialogOpen(const OrbisHmdSetupDialogParam* param);
s32 PS4_SYSV_ABI sceHmdSetupDialogTerminate(void);
u32 PS4_SYSV_ABI sceHmdSetupDialogUpdateStatus(void);

RETRO_END_DECLS

#endif
