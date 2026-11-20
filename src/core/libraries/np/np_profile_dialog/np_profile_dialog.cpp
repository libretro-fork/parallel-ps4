// SPDX-FileCopyrightText: Copyright 2025-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstring>
#include "common/enum_names.h"
#include "common/logging/log.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/libs.h"
#include "core/libraries/system/commondialog.h"
#include "np_profile_dialog.h"
#include "video_core/overlay/ovl_layers.h"

namespace Libraries::Np::NpProfileDialog {

// The dialog is the overlay's (video_core/overlay/ovl_profile.c): its status and what the
// player chose are kept there, among the game's thread and the overlay's. What the game gave
// to be handed back with the result is the game's thread's alone
static void* g_user_data = nullptr;

static Libraries::CommonDialog::Status Status() {
    return static_cast<Libraries::CommonDialog::Status>(ovl_profile_dialog_status());
}

static void SetStatus(Libraries::CommonDialog::Status status) {
    ovl_profile_dialog_set_status(static_cast<int>(status));
}

Libraries::CommonDialog::Error sceNpProfileDialogOpen(OrbisNpProfileDialogParam* param) {
    if (const auto status = Status(); status != Libraries::CommonDialog::Status::INITIALIZED &&
                                      status != Libraries::CommonDialog::Status::FINISHED) {
        LOG_ERROR(Lib_NpProfileDialog, "called without initialize");
        return Libraries::CommonDialog::Error::INVALID_STATE;
    }
    if (param == nullptr) {
        LOG_ERROR(Lib_NpProfileDialog, "param is nullptr");
        return Libraries::CommonDialog::Error::ARG_NULL;
    }

    LOG_DEBUG(Lib_NpProfileDialog,
              "param details: mode=%d, userId=%d, targetOnlineId='%s', userData=0x%llx",
              static_cast<int>(param->mode), param->userId, param->targetOnlineId.data,
              static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(param->userData)));

    g_user_data = param->userData;
    ovl_profile_dialog_param_t shown{};
    shown.mode = static_cast<int>(param->mode);
    shown.online_id = param->targetOnlineId.data;
    ovl_profile_dialog_open(&shown);

    LOG_INFO(Lib_NpProfileDialog, "dialog opened successfully for user %d", param->userId);
    return Libraries::CommonDialog::Error::OK;
}

Libraries::CommonDialog::Error PS4_SYSV_ABI sceNpProfileDialogClose() {
    LOG_DEBUG(Lib_NpProfileDialog, "called");
    const auto status = Status();
    if (status == Libraries::CommonDialog::Status::NONE) {
        return Libraries::CommonDialog::Error::NOT_INITIALIZED;
    }
    // Pressed on screen at the same moment: finished already, by the player
    if (status != Libraries::CommonDialog::Status::RUNNING ||
        !ovl_profile_dialog_close(OVL_PROFILE_RESULT_OK)) {
        return Libraries::CommonDialog::Error::NOT_RUNNING;
    }
    return Libraries::CommonDialog::Error::OK;
}

Libraries::CommonDialog::Error PS4_SYSV_ABI
sceNpProfileDialogGetResult(OrbisNpProfileDialogResult* result) {
    LOG_DEBUG(Lib_NpProfileDialog, "called");

    const auto status = Status();
    if (status == Libraries::CommonDialog::Status::NONE) {
        LOG_ERROR(Lib_NpProfileDialog, "failed: not initialized (status=NONE)");
        return Libraries::CommonDialog::Error::NOT_INITIALIZED;
    }
    if (result == nullptr) {
        LOG_ERROR(Lib_NpProfileDialog, "failed: result pointer is nullptr");
        return Libraries::CommonDialog::Error::ARG_NULL;
    }
    if (status != Libraries::CommonDialog::Status::FINISHED) {
        LOG_ERROR(Lib_NpProfileDialog, "failed: dialog not finished (status=%d)",
                  static_cast<int>(status));
        return Libraries::CommonDialog::Error::NOT_FINISHED;
    }

    *result = {};
    result->result = 0; // ORBIS_OK
    result->userAction = static_cast<Libraries::CommonDialog::Result>(ovl_profile_dialog_result());
    result->userData = g_user_data;

    LOG_DEBUG(Lib_NpProfileDialog, "result details: resultCode=%d, userAction=%d, userData=0x%llx",
              result->result, static_cast<int>(result->userAction),
              static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(result->userData)));
    return Libraries::CommonDialog::Error::OK;
}

Libraries::CommonDialog::Status PS4_SYSV_ABI sceNpProfileDialogGetStatus() {
    const auto status = Status();
    LOG_TRACE(Lib_NpProfileDialog, "called status=%s",
              enum_name(&enum_names_Libraries_CommonDialog_Status, static_cast<s64>(status)));
    return status;
}

Libraries::CommonDialog::Error PS4_SYSV_ABI sceNpProfileDialogInitialize() {
    if (!CommonDialog::g_isInitialized) {
        LOG_ERROR(Lib_NpProfileDialog, "failed: system not initialized");
        return Libraries::CommonDialog::Error::NOT_SYSTEM_INITIALIZED;
    }
    if (const auto status = Status(); status != Libraries::CommonDialog::Status::NONE) {
        LOG_ERROR(Lib_NpProfileDialog, "failed: already initialized (status=%d)",
                  static_cast<int>(status));
        return Libraries::CommonDialog::Error::ALREADY_INITIALIZED;
    }
    if (CommonDialog::g_isUsed) {
        LOG_ERROR(Lib_NpProfileDialog, "failed: dialog system busy (g_isUsed=true)");
        return Libraries::CommonDialog::Error::BUSY;
    }

    SetStatus(Libraries::CommonDialog::Status::INITIALIZED);
    CommonDialog::g_isUsed = true;
    LOG_INFO(Lib_NpProfileDialog, "initialized successfully");
    return Libraries::CommonDialog::Error::OK;
}

Libraries::CommonDialog::Error PS4_SYSV_ABI
sceNpProfileDialogOpenA(OrbisNpProfileDialogParamA* param) {
    if (const auto status = Status(); status != Libraries::CommonDialog::Status::INITIALIZED &&
                                      status != Libraries::CommonDialog::Status::FINISHED) {
        LOG_ERROR(Lib_NpProfileDialog, "called without initialize");
        return Libraries::CommonDialog::Error::INVALID_STATE;
    }
    if (param == nullptr) {
        LOG_ERROR(Lib_NpProfileDialog, "failed: param is nullptr");
        return Libraries::CommonDialog::Error::ARG_NULL;
    }

    LOG_DEBUG(Lib_NpProfileDialog,
              "param details: mode=%d, userId=%d, targetAccountId='%" PRIu64 "', userData=0x%llx",
              static_cast<int>(param->mode), param->userId, param->targetAccountId,
              static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(param->userData)));
    LOG_ERROR(Lib_NpProfileDialog, "(STUBBED) called");

    g_user_data = param->userData;
    ovl_profile_dialog_param_t shown{};
    shown.mode = static_cast<int>(param->mode);
    shown.has_account_id = 1;
    shown.account_id = param->targetAccountId;
    ovl_profile_dialog_open(&shown);

    LOG_INFO(Lib_NpProfileDialog, "dialog opened successfully for user %d (account ID variant)",
             param->userId);
    return Libraries::CommonDialog::Error::OK;
}

Libraries::CommonDialog::Error PS4_SYSV_ABI sceNpProfileDialogTerminate() {
    LOG_DEBUG(Lib_NpProfileDialog, "called (status=%d)", static_cast<int>(Status()));

    if (Status() == Libraries::CommonDialog::Status::RUNNING) {
        LOG_ERROR(Lib_NpProfileDialog, "dialog is running, closing it first");
        sceNpProfileDialogClose();
    }
    if (Status() == Libraries::CommonDialog::Status::NONE) {
        LOG_ERROR(Lib_NpProfileDialog, "failed: not initialized (status=NONE)");
        return Libraries::CommonDialog::Error::NOT_INITIALIZED;
    }

    SetStatus(Libraries::CommonDialog::Status::NONE);
    CommonDialog::g_isUsed = false;

    LOG_INFO(Lib_NpProfileDialog, "terminated successfully");
    return Libraries::CommonDialog::Error::OK;
}

Libraries::CommonDialog::Status PS4_SYSV_ABI sceNpProfileDialogUpdateStatus() {
    const auto status = Status();
    LOG_TRACE(Lib_NpProfileDialog, "called status=%s",
              enum_name(&enum_names_Libraries_CommonDialog_Status, static_cast<s64>(status)));
    return status;
}

void RegisterLib(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("uj9Cz7Tk0cc", "libSceNpProfileDialogCompat", 1, "libSceNpProfileDialog",
                 sceNpProfileDialogOpen);
    LIB_FUNCTION("wkwjz0Xdo2A", "libSceNpProfileDialog", 1, "libSceNpProfileDialog",
                 sceNpProfileDialogClose);
    LIB_FUNCTION("8rhLl1-0W-o", "libSceNpProfileDialog", 1, "libSceNpProfileDialog",
                 sceNpProfileDialogGetResult);
    LIB_FUNCTION("3BqoiFOjSsk", "libSceNpProfileDialog", 1, "libSceNpProfileDialog",
                 sceNpProfileDialogGetStatus);
    LIB_FUNCTION("Lg+NCE6pTwQ", "libSceNpProfileDialog", 1, "libSceNpProfileDialog",
                 sceNpProfileDialogInitialize);
    LIB_FUNCTION("uj9Cz7Tk0cc", "libSceNpProfileDialog", 1, "libSceNpProfileDialog",
                 sceNpProfileDialogOpen);
    LIB_FUNCTION("nrQRlLKzdwE", "libSceNpProfileDialog", 1, "libSceNpProfileDialog",
                 sceNpProfileDialogOpenA);
    LIB_FUNCTION("0Sp9vJcB1-w", "libSceNpProfileDialog", 1, "libSceNpProfileDialog",
                 sceNpProfileDialogTerminate);
    LIB_FUNCTION("haVZE9FgKqE", "libSceNpProfileDialog", 1, "libSceNpProfileDialog",
                 sceNpProfileDialogUpdateStatus);
};

} // namespace Libraries::Np::NpProfileDialog
