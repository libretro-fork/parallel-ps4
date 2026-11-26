/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* libSceErrorDialog. The functions return the common dialog's errors and statuses
 * (system/commondialog_c.h). */

#ifndef SHADPS4_LIBRARIES_ERROR_DIALOG_H
#define SHADPS4_LIBRARIES_ERROR_DIALOG_H

#include "common/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OrbisErrorDialogParam {
    s32 size;
    s32 errorCode;
    s32 userId; /* OrbisUserServiceUserId */
    s32 _reserved;
} OrbisErrorDialogParam;

u32 PS4_SYSV_ABI sceErrorDialogClose(void);
u32 PS4_SYSV_ABI sceErrorDialogGetStatus(void);
u32 PS4_SYSV_ABI sceErrorDialogInitialize(void);
u32 PS4_SYSV_ABI sceErrorDialogOpen(const OrbisErrorDialogParam* param);
int PS4_SYSV_ABI sceErrorDialogOpenDetail(void);
int PS4_SYSV_ABI sceErrorDialogOpenWithReport(void);
u32 PS4_SYSV_ABI sceErrorDialogTerminate(void);
u32 PS4_SYSV_ABI sceErrorDialogUpdateStatus(void);

#ifdef __cplusplus
}
#endif

#endif
