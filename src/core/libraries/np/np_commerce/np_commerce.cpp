// SPDX-FileCopyrightText: Copyright 2025-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <cstddef>
#include <cstring>
#include "common/logging/log.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/libs.h"
#include "core/libraries/system/commondialog.h"
#include "core/libraries/system/userservice.h"
#include "np_commerce.h"
#include "video_core/overlay/ovl_layers.h"

namespace Libraries::Np::NpCommerce {

// The dialog is the overlay's (video_core/overlay/ovl_commerce.c): its status and what was
// pressed are kept there, among the game's thread and the overlay's. These are the game's
// thread's alone: the mode it opened, what it gave to be handed back with the result, and the
// error of an open the dialog was never shown for (0: none)
static s32 g_mode = ORBIS_NP_COMMERCE_DIALOG_MODE_CATEGORY;
static void* g_user_data = nullptr;
static s32 g_param_error = 0;

static Status DialogStatus() {
    return static_cast<Status>(ovl_commerce_dialog_status());
}

s32 PS4_SYSV_ABI sceNpCommerceDialogInitialize() {
    LOG_INFO(Lib_NpCommerce, "called");
    if (DialogStatus() != Status::NONE) {
        return static_cast<s32>(Error::ALREADY_INITIALIZED);
    }
    ovl_commerce_dialog_set_status(static_cast<int>(Status::INITIALIZED));
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNpCommerceDialogTerminate() {
    LOG_INFO(Lib_NpCommerce, "called");
    if (DialogStatus() == Status::NONE) {
        return static_cast<s32>(Error::NOT_INITIALIZED);
    }
    ovl_commerce_dialog_close(OVL_COMMERCE_RESULT_OK);
    ovl_commerce_dialog_set_status(static_cast<int>(Status::NONE));
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNpCommerceDialogClose() {
    LOG_INFO(Lib_NpCommerce, "called");
    switch (DialogStatus()) {
    case Status::NONE:
        return static_cast<s32>(Error::NOT_INITIALIZED);
    case Status::INITIALIZED:
        return static_cast<s32>(Error::NOT_RUNNING);
    case Status::FINISHED:
        return static_cast<s32>(Error::ALREADY_CLOSE);
    default:
        break;
    }
    // Pressed on screen at the same moment: closed already, by the player
    if (!ovl_commerce_dialog_close(OVL_COMMERCE_RESULT_OK)) {
        return static_cast<s32>(Error::ALREADY_CLOSE);
    }
    return ORBIS_OK;
}

s8 PS4_SYSV_ABI sceNpCommerceDialogGetStatus() {
    const Status status = DialogStatus();
    LOG_DEBUG(Lib_NpCommerce, "called, status = %u", static_cast<u32>(status));
    return static_cast<s8>(status);
}

s32 PS4_SYSV_ABI sceNpCommerceDialogInitializeInternal() {
    LOG_INFO(Lib_NpCommerce, "called");
    return sceNpCommerceDialogInitialize();
}

s32 PS4_SYSV_ABI sceNpCommerceDialogUpdateStatus() {
    const Status status = DialogStatus();
    LOG_DEBUG(Lib_NpCommerce, "called, status = %u", static_cast<u32>(status));
    return static_cast<s32>(status);
}

s32 PS4_SYSV_ABI sceNpCommerceDialogGetResult(OrbisNpCommerceDialogResult* result) {
    if (result == nullptr) {
        return static_cast<s32>(Error::ARG_NULL);
    }
    if (DialogStatus() != Status::FINISHED) {
        return static_cast<s32>(Error::NOT_FINISHED);
    }
    const s32 g_result_code = g_param_error != 0 ? g_param_error : ovl_commerce_dialog_result();
    result->result = g_result_code;
    // authorized is only meaningful in PLUS mode
    result->authorized = g_mode == ORBIS_NP_COMMERCE_DIALOG_MODE_PLUS &&
                         g_result_code == ORBIS_NP_COMMERCE_DIALOG_RESULT_PURCHASED;
    result->userData = g_user_data;
    LOG_INFO(Lib_NpCommerce, "result=%d authorized=%s", result->result,
             (result->authorized) ? "true" : "false");
    return g_result_code;
}

s32 PS4_SYSV_ABI sceNpCommerceDialogOpen(const OrbisNpCommerceDialogParam* param) {
    LOG_INFO(Lib_NpCommerce, "called, param = 0x%llx",
             static_cast<unsigned long long>(
                 reinterpret_cast<uintptr_t>(static_cast<const void*>(param))));
    if (const Status status = DialogStatus();
        status != Status::INITIALIZED && status != Status::FINISHED) {
        LOG_WARNING(Lib_NpCommerce, "Open in bad state %u", static_cast<u32>(status));
        return static_cast<s32>(Error::NOT_INITIALIZED);
    }
    if (param == nullptr) {
        return static_cast<s32>(Error::ARG_NULL);
    }
    if (param->mode < ORBIS_NP_COMMERCE_DIALOG_MODE_CATEGORY ||
        param->mode > ORBIS_NP_COMMERCE_DIALOG_MODE_PLUS) {
        LOG_ERROR(Lib_NpCommerce, "SceNpCommerceDialog Parameter Error (mode=%d)", param->mode);
        g_param_error = static_cast<s32>(Error::PARAM_INVALID);
        ovl_commerce_dialog_set_status(static_cast<int>(Status::FINISHED));
        return static_cast<s32>(Error::PARAM_INVALID);
    }

    g_param_error = 0;
    g_mode = param->mode;
    g_user_data = param->userData;

    ovl_commerce_dialog_param_t shown{};
    shown.mode = param->mode;
    shown.user_id = param->userId;
    shown.service_label = param->serviceLabel;
    // features is only meaningful in PLUS mode
    shown.features = param->mode == ORBIS_NP_COMMERCE_DIALOG_MODE_PLUS ? param->features : 0;

    // Resolve the calling user's name for display
    char name_buf[ORBIS_USER_SERVICE_MAX_USER_NAME_LENGTH + 1] = {};
    if (sceUserServiceGetUserName(param->userId, name_buf, sizeof(name_buf)) >= 0) {
        shown.username = name_buf;
    }

    if (param->numTargets > ORBIS_NP_COMMERCE_DIALOG_NUM_TARGETS_MAX) {
        LOG_WARNING(Lib_NpCommerce, "numTargets %u exceeds max %u, clamping", param->numTargets,
                    ORBIS_NP_COMMERCE_DIALOG_NUM_TARGETS_MAX);
    }
    // The labels the game gave, the null ones left out; the overlay copies them, each up to
    // MAX_LABEL_LEN bytes
    const char* targets[ORBIS_NP_COMMERCE_DIALOG_NUM_TARGETS_MAX];
    const u32 count = std::min(param->numTargets, ORBIS_NP_COMMERCE_DIALOG_NUM_TARGETS_MAX);
    if (count > 0 && param->targets != nullptr) {
        for (u32 i = 0; i < count; ++i) {
            if (const char* label = param->targets[i]; label != nullptr) {
                targets[shown.target_count++] = label;
            }
        }
    }
    shown.targets = targets;
    LOG_INFO(Lib_NpCommerce,
             "open: mode=%d user_id=%d serviceLabel=0x%x numTargets=%u features=0x%" PRIx64
             " first='%s'",
             param->mode, shown.user_id, shown.service_label, param->numTargets, shown.features,
             shown.target_count == 0 ? "" : targets[0]);

    ovl_commerce_dialog_open(&shown);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNpCommerceHidePsStoreIcon() {
    LOG_INFO(Lib_NpCommerce, "called");
    ovl_store_icon_hide();
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNpCommerceSetPsStoreIconLayout(s32 layout) {
    LOG_INFO(Lib_NpCommerce, "called, layout = %d", layout);
    if (layout < 0 || layout > 2) {
        LOG_WARNING(Lib_NpCommerce, "invalid layout %d, ignoring", layout);
        return ORBIS_OK;
    }
    ovl_store_icon_set_layout(layout);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNpCommerceShowPsStoreIcon(s32 pos) {
    LOG_INFO(Lib_NpCommerce, "called, pos = %d", pos);
    ovl_store_icon_show(pos >= 0 && pos <= 2 ? pos : OVL_STORE_ICON_CENTER);
    return ORBIS_OK;
}

void RegisterLib(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("NU3ckGHMFXo", "libSceNpCommerce", 1, "libSceNpCommerce",
                 sceNpCommerceDialogClose);
    LIB_FUNCTION("r42bWcQbtZY", "libSceNpCommerce", 1, "libSceNpCommerce",
                 sceNpCommerceDialogGetResult);
    LIB_FUNCTION("CCbC+lqqvF0", "libSceNpCommerce", 1, "libSceNpCommerce",
                 sceNpCommerceDialogGetStatus);
    LIB_FUNCTION("0aR2aWmQal4", "libSceNpCommerce", 1, "libSceNpCommerce",
                 sceNpCommerceDialogInitialize);
    LIB_FUNCTION("9ZiLXAGG5rg", "libSceNpCommerce", 1, "libSceNpCommerce",
                 sceNpCommerceDialogInitializeInternal);
    LIB_FUNCTION("DfSCDRA3EjY", "libSceNpCommerce", 1, "libSceNpCommerce", sceNpCommerceDialogOpen);
    LIB_FUNCTION("m-I92Ab50W8", "libSceNpCommerce", 1, "libSceNpCommerce",
                 sceNpCommerceDialogTerminate);
    LIB_FUNCTION("LR5cwFMMCVE", "libSceNpCommerce", 1, "libSceNpCommerce",
                 sceNpCommerceDialogUpdateStatus);
    LIB_FUNCTION("dsqCVsNM0Zg", "libSceNpCommerce", 1, "libSceNpCommerce",
                 sceNpCommerceHidePsStoreIcon);
    LIB_FUNCTION("uKTDW8hk-ts", "libSceNpCommerce", 1, "libSceNpCommerce",
                 sceNpCommerceSetPsStoreIconLayout);
    LIB_FUNCTION("DHmwsa6S8Tc", "libSceNpCommerce", 1, "libSceNpCommerce",
                 sceNpCommerceShowPsStoreIcon);
};

} // namespace Libraries::Np::NpCommerce
