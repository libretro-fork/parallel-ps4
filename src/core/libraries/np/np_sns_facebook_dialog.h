/* SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* libSceNpSnsFacebookDialog: never opened here, so its status is always none
 * (system/commondialog_c.h). */

#ifndef SHADPS4_LIBRARIES_NP_SNS_FACEBOOK_DIALOG_H
#define SHADPS4_LIBRARIES_NP_SNS_FACEBOOK_DIALOG_H

#include "common/types.h"

#ifdef __cplusplus
extern "C" {
#endif

u32 PS4_SYSV_ABI sceNpSnsFacebookDialogUpdateStatus(void);

#ifdef __cplusplus
}
#endif

#endif
