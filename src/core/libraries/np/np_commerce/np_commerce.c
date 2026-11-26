/* SPDX-FileCopyrightText: Copyright 2025-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stddef.h>
#include <string.h>

#include "common/c_bridge.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/np/np_commerce/np_commerce.h"
#include "core/libraries/system/userservice.h"
#include "video_core/overlay/ovl_layers.h"

/* The dialog is the overlay's (video_core/overlay/ovl_commerce.c): its status and what was
 * pressed are kept there, among the game's thread and the overlay's. These are the game's
 * thread's alone: the mode it opened, what it gave to be handed back with the result, and the
 * error of an open the dialog was never shown for (0: none) */
static s32 g_mode = ORBIS_NP_COMMERCE_DIALOG_MODE_CATEGORY;
static void* g_user_data = NULL;
static s32 g_param_error = 0;

static u32 dialog_status(void) {
    return (u32)ovl_commerce_dialog_status();
}

s32 PS4_SYSV_ABI sceNpCommerceDialogInitialize(void) {
    c_log(C_LOG_LIB_NP_COMMERCE, C_LOG_LEVEL_INFO, "sceNpCommerceDialogInitialize", "called");
    if (dialog_status() != COMMON_DIALOG_STATUS_NONE) {
        return (s32)COMMON_DIALOG_ERROR_ALREADY_INITIALIZED;
    }
    ovl_commerce_dialog_set_status((int)COMMON_DIALOG_STATUS_INITIALIZED);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNpCommerceDialogTerminate(void) {
    c_log(C_LOG_LIB_NP_COMMERCE, C_LOG_LEVEL_INFO, "sceNpCommerceDialogTerminate", "called");
    if (dialog_status() == COMMON_DIALOG_STATUS_NONE) {
        return (s32)COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    }
    ovl_commerce_dialog_close(OVL_COMMERCE_RESULT_OK);
    ovl_commerce_dialog_set_status((int)COMMON_DIALOG_STATUS_NONE);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNpCommerceDialogClose(void) {
    c_log(C_LOG_LIB_NP_COMMERCE, C_LOG_LEVEL_INFO, "sceNpCommerceDialogClose", "called");
    switch (dialog_status()) {
    case COMMON_DIALOG_STATUS_NONE:
        return (s32)COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    case COMMON_DIALOG_STATUS_INITIALIZED:
        return (s32)COMMON_DIALOG_ERROR_NOT_RUNNING;
    case COMMON_DIALOG_STATUS_FINISHED:
        return (s32)COMMON_DIALOG_ERROR_ALREADY_CLOSE;
    default:
        break;
    }
    /* Pressed on screen at the same moment: closed already, by the player */
    if (!ovl_commerce_dialog_close(OVL_COMMERCE_RESULT_OK)) {
        return (s32)COMMON_DIALOG_ERROR_ALREADY_CLOSE;
    }
    return ORBIS_OK;
}

s8 PS4_SYSV_ABI sceNpCommerceDialogGetStatus(void) {
    const u32 status = dialog_status();
    c_log(C_LOG_LIB_NP_COMMERCE, C_LOG_LEVEL_DEBUG, "sceNpCommerceDialogGetStatus",
          "called, status = %u", status);
    return (s8)status;
}

s32 PS4_SYSV_ABI sceNpCommerceDialogInitializeInternal(void) {
    c_log(C_LOG_LIB_NP_COMMERCE, C_LOG_LEVEL_INFO, "sceNpCommerceDialogInitializeInternal",
          "called");
    return sceNpCommerceDialogInitialize();
}

s32 PS4_SYSV_ABI sceNpCommerceDialogUpdateStatus(void) {
    const u32 status = dialog_status();
    c_log(C_LOG_LIB_NP_COMMERCE, C_LOG_LEVEL_DEBUG, "sceNpCommerceDialogUpdateStatus",
          "called, status = %u", status);
    return (s32)status;
}

s32 PS4_SYSV_ABI sceNpCommerceDialogGetResult(OrbisNpCommerceDialogResult* result) {
    s32 result_code;
    if (result == NULL) {
        return (s32)COMMON_DIALOG_ERROR_ARG_NULL;
    }
    if (dialog_status() != COMMON_DIALOG_STATUS_FINISHED) {
        return (s32)COMMON_DIALOG_ERROR_NOT_FINISHED;
    }
    result_code = g_param_error != 0 ? g_param_error : ovl_commerce_dialog_result();
    result->result = result_code;
    /* authorized is only meaningful in PLUS mode */
    result->authorized = (u8)(g_mode == ORBIS_NP_COMMERCE_DIALOG_MODE_PLUS &&
                              result_code == ORBIS_NP_COMMERCE_DIALOG_RESULT_PURCHASED);
    result->userData = g_user_data;
    c_log(C_LOG_LIB_NP_COMMERCE, C_LOG_LEVEL_INFO, "sceNpCommerceDialogGetResult",
          "result=%d authorized=%s", (int)result->result, result->authorized ? "true" : "false");
    return result_code;
}

s32 PS4_SYSV_ABI sceNpCommerceDialogOpen(const OrbisNpCommerceDialogParam* param) {
    ovl_commerce_dialog_param_t shown;
    char name_buf[ORBIS_USER_SERVICE_MAX_USER_NAME_LENGTH + 1];
    /* The labels the game gave, the null ones left out; the overlay copies them, each up to
     * NP_COMMERCE_MAX_LABEL_LEN bytes */
    const char* targets[ORBIS_NP_COMMERCE_DIALOG_NUM_TARGETS_MAX];
    u32 count;
    u32 i;
    u32 status;

    c_log(C_LOG_LIB_NP_COMMERCE, C_LOG_LEVEL_INFO, "sceNpCommerceDialogOpen", "called, param = %p",
          (const void*)param);
    status = dialog_status();
    if (status != COMMON_DIALOG_STATUS_INITIALIZED && status != COMMON_DIALOG_STATUS_FINISHED) {
        c_log(C_LOG_LIB_NP_COMMERCE, C_LOG_LEVEL_WARNING, "sceNpCommerceDialogOpen",
              "Open in bad state %u", status);
        return (s32)COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    }
    if (param == NULL) {
        return (s32)COMMON_DIALOG_ERROR_ARG_NULL;
    }
    if (param->mode < ORBIS_NP_COMMERCE_DIALOG_MODE_CATEGORY ||
        param->mode > ORBIS_NP_COMMERCE_DIALOG_MODE_PLUS) {
        c_log(C_LOG_LIB_NP_COMMERCE, C_LOG_LEVEL_ERROR, "sceNpCommerceDialogOpen",
              "SceNpCommerceDialog Parameter Error (mode=%d)", (int)param->mode);
        g_param_error = (s32)COMMON_DIALOG_ERROR_PARAM_INVALID;
        ovl_commerce_dialog_set_status((int)COMMON_DIALOG_STATUS_FINISHED);
        return (s32)COMMON_DIALOG_ERROR_PARAM_INVALID;
    }

    g_param_error = 0;
    g_mode = param->mode;
    g_user_data = param->userData;

    memset(&shown, 0, sizeof(shown));
    shown.mode = param->mode;
    shown.user_id = param->userId;
    shown.service_label = param->serviceLabel;
    /* features is only meaningful in PLUS mode */
    shown.features = param->mode == ORBIS_NP_COMMERCE_DIALOG_MODE_PLUS ? param->features : 0;

    /* Resolve the calling user's name for display */
    memset(name_buf, 0, sizeof(name_buf));
    if (sceUserServiceGetUserName(param->userId, name_buf, sizeof(name_buf)) >= 0) {
        shown.username = name_buf;
    }

    if (param->numTargets > ORBIS_NP_COMMERCE_DIALOG_NUM_TARGETS_MAX) {
        c_log(C_LOG_LIB_NP_COMMERCE, C_LOG_LEVEL_WARNING, "sceNpCommerceDialogOpen",
              "numTargets %u exceeds max %u, clamping", (unsigned)param->numTargets,
              (unsigned)ORBIS_NP_COMMERCE_DIALOG_NUM_TARGETS_MAX);
    }
    count = param->numTargets < ORBIS_NP_COMMERCE_DIALOG_NUM_TARGETS_MAX
                ? param->numTargets
                : ORBIS_NP_COMMERCE_DIALOG_NUM_TARGETS_MAX;
    if (count > 0 && param->targets != NULL) {
        for (i = 0; i < count; ++i) {
            const char* label = param->targets[i];
            if (label != NULL) {
                targets[shown.target_count++] = label;
            }
        }
    }
    shown.targets = targets;
    c_log(C_LOG_LIB_NP_COMMERCE, C_LOG_LEVEL_INFO, "sceNpCommerceDialogOpen",
          "open: mode=%d user_id=%d serviceLabel=0x%x numTargets=%u features=0x%llx first='%s'",
          (int)param->mode, shown.user_id, shown.service_label, (unsigned)param->numTargets,
          (unsigned long long)shown.features, shown.target_count == 0 ? "" : targets[0]);

    ovl_commerce_dialog_open(&shown);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNpCommerceHidePsStoreIcon(void) {
    c_log(C_LOG_LIB_NP_COMMERCE, C_LOG_LEVEL_INFO, "sceNpCommerceHidePsStoreIcon", "called");
    ovl_store_icon_hide();
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNpCommerceSetPsStoreIconLayout(s32 layout) {
    c_log(C_LOG_LIB_NP_COMMERCE, C_LOG_LEVEL_INFO, "sceNpCommerceSetPsStoreIconLayout",
          "called, layout = %d", (int)layout);
    if (layout < 0 || layout > 2) {
        c_log(C_LOG_LIB_NP_COMMERCE, C_LOG_LEVEL_WARNING, "sceNpCommerceSetPsStoreIconLayout",
              "invalid layout %d, ignoring", (int)layout);
        return ORBIS_OK;
    }
    ovl_store_icon_set_layout(layout);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNpCommerceShowPsStoreIcon(s32 pos) {
    c_log(C_LOG_LIB_NP_COMMERCE, C_LOG_LEVEL_INFO, "sceNpCommerceShowPsStoreIcon",
          "called, pos = %d", (int)pos);
    ovl_store_icon_show(pos >= 0 && pos <= 2 ? pos : OVL_STORE_ICON_CENTER);
    return ORBIS_OK;
}
