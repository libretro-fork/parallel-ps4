/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_SIGNIN_DIALOG_H
#define SHADPS4_LIBRARIES_SIGNIN_DIALOG_H

/* libSceSigninDialog. Registered in libs.cpp. */

#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

/* The dialog's status (u32) */
enum OrbisSigninDialogStatus {
    ORBIS_SIGNIN_DIALOG_STATUS_NONE = 0,
    ORBIS_SIGNIN_DIALOG_STATUS_INITIALIZED = 1,
    ORBIS_SIGNIN_DIALOG_STATUS_RUNNING = 2,
    ORBIS_SIGNIN_DIALOG_STATUS_FINISHED = 3
};

enum OrbisSigninDialogResultType {
    ORBIS_SIGNIN_DIALOG_RESULT_OK = 0,
    ORBIS_SIGNIN_DIALOG_RESULT_USER_CANCELED = 1
};

typedef struct OrbisSigninDialogResult {
    s32 result; /* OrbisSigninDialogResultType */
    s32 reserved[3];
} OrbisSigninDialogResult;

s32 PS4_SYSV_ABI sceSigninDialogInitialize(void);
s32 PS4_SYSV_ABI sceSigninDialogOpen(void);
u32 PS4_SYSV_ABI sceSigninDialogGetStatus(void);
u32 PS4_SYSV_ABI sceSigninDialogUpdateStatus(void);
s32 PS4_SYSV_ABI sceSigninDialogGetResult(OrbisSigninDialogResult* result);
s32 PS4_SYSV_ABI sceSigninDialogClose(void);
s32 PS4_SYSV_ABI sceSigninDialogTerminate(void);

RETRO_END_DECLS

#endif
