/* SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "common/c_bridge.h"
#include "core/libraries/np/np_sns_facebook_dialog.h"
#include "core/libraries/system/commondialog_c.h"

u32 PS4_SYSV_ABI sceNpSnsFacebookDialogUpdateStatus(void) {
    c_log(C_LOG_LIB_NP_SNS_FACEBOOK_DIALOG, C_LOG_LEVEL_ERROR, "sceNpSnsFacebookDialogUpdateStatus",
          "(STUBBED) called");
    return COMMON_DIALOG_STATUS_NONE;
}
