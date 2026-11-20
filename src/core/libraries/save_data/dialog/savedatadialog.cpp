// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/enum_names.h"

#include "common/elf_info.h"
#include "common/logging/log.h"
#include "core/libraries/libs.h"
#include "core/libraries/system/commondialog.h"
#include "savedatadialog.h"
#include "savedatadialog_ui.h"

namespace Libraries::SaveData::Dialog {

using CommonDialog::Error;
using CommonDialog::Result;
using CommonDialog::Status;

// The dialog is drawn by the overlay (video_core/overlay/ovl_savedialog.c), which keeps its
// status and result, and is lent what it shows from one of these, the one it says to build in
// next; the overlay never draws from that one, so building there waits for nothing.
static SaveDialogState g_states[3];
static int g_opened = -1; // the one the dialog opened last was built in
static SaveDialogResult g_result{};

static Status GetStatus() {
    return static_cast<Status>(ovl_save_dialog_status());
}

Error PS4_SYSV_ABI sceSaveDataDialogClose() {
    LOG_DEBUG(Lib_SaveDataDialog, "called");
    if (GetStatus() != Status::RUNNING) {
        return Error::NOT_RUNNING;
    }
    ovl_save_dialog_close(OVL_SAVE_BUTTON_INVALID, OVL_SAVE_RESULT_OK);
    return Error::OK;
}

Error PS4_SYSV_ABI sceSaveDataDialogGetResult(OrbisSaveDataDialogResult* result) {
    LOG_DEBUG(Lib_SaveDataDialog, "called");
    if (GetStatus() != Status::FINISHED) {
        return Error::NOT_FINISHED;
    }
    if (result == nullptr) {
        return Error::ARG_NULL;
    }
    if (g_opened >= 0) {
        int button, res, item;
        ovl_save_dialog_result(&button, &res, &item);
        g_states[g_opened].MakeResult(g_result, button, res, item);
    }
    g_result.CopyTo(*result);
    return Error::OK;
}

Status PS4_SYSV_ABI sceSaveDataDialogGetStatus() {
    const auto status = GetStatus();
    LOG_TRACE(Lib_SaveDataDialog, "called status=%s",
              enum_name(&enum_names_Libraries_CommonDialog_Status, static_cast<s64>(status)));
    return status;
}

Error PS4_SYSV_ABI sceSaveDataDialogInitialize() {
    LOG_DEBUG(Lib_SaveDataDialog, "called");
    if (!CommonDialog::g_isInitialized) {
        return Error::NOT_SYSTEM_INITIALIZED;
    }
    if (GetStatus() != Status::NONE) {
        return Error::ALREADY_INITIALIZED;
    }
    if (CommonDialog::g_isUsed) {
        return Error::BUSY;
    }
    ovl_save_dialog_set_status(static_cast<int>(Status::INITIALIZED));
    CommonDialog::g_isUsed = true;

    return Error::OK;
}

s32 PS4_SYSV_ABI sceSaveDataDialogIsReadyToDisplay() {
    return 1;
}

Error PS4_SYSV_ABI sceSaveDataDialogOpen(const OrbisSaveDataDialogParam* param) {
    const auto status = GetStatus();
    if (status != Status::INITIALIZED && status != Status::FINISHED) {
        LOG_INFO(Lib_SaveDataDialog, "called without initialize");
        return Error::INVALID_STATE;
    }
    if (param == nullptr) {
        LOG_DEBUG(Lib_SaveDataDialog, "called param:(NULL)");
        return Error::ARG_NULL;
    }
    LOG_DEBUG(Lib_SaveDataDialog, "called param->mode: %s",
              enum_name(&enum_names_Libraries_SaveData_Dialog_SaveDataDialogMode,
                        static_cast<s64>(param->mode)));
    ASSERT(param->size == sizeof(OrbisSaveDataDialogParam));
    ASSERT(param->baseParam.size == sizeof(CommonDialog::BaseParam));
    g_result = {};
    // The last dialog's icons go (it is finished, so no longer drawn), then what was built where
    // this one is built
    if (g_opened >= 0) {
        g_states[g_opened].ReleaseIcons();
    }
    const int back = ovl_save_dialog_back();
    g_states[back].ReleaseIcons();
    g_states[back] = SaveDialogState{*param};
    const auto open = g_states[back].MakeParam();
    g_opened = ovl_save_dialog_open(&open);
    return Error::OK;
}

Error PS4_SYSV_ABI sceSaveDataDialogProgressBarInc(OrbisSaveDataDialogProgressBarTarget target,
                                                   u32 delta) {
    LOG_DEBUG(Lib_SaveDataDialog, "called");
    if (GetStatus() != Status::RUNNING) {
        return Error::NOT_RUNNING;
    }
    if (g_states[g_opened].GetMode() != SaveDataDialogMode::PROGRESS_BAR) {
        return Error::NOT_SUPPORTED;
    }
    if (target != OrbisSaveDataDialogProgressBarTarget::DEFAULT) {
        return Error::PARAM_INVALID;
    }
    ovl_save_dialog_add_progress(delta);
    return Error::OK;
}

Error PS4_SYSV_ABI sceSaveDataDialogProgressBarSetValue(OrbisSaveDataDialogProgressBarTarget target,
                                                        u32 rate) {
    LOG_DEBUG(Lib_SaveDataDialog, "called");
    if (GetStatus() != Status::RUNNING) {
        return Error::NOT_RUNNING;
    }
    if (g_states[g_opened].GetMode() != SaveDataDialogMode::PROGRESS_BAR) {
        return Error::NOT_SUPPORTED;
    }
    if (target != OrbisSaveDataDialogProgressBarTarget::DEFAULT) {
        return Error::PARAM_INVALID;
    }
    ovl_save_dialog_set_progress(rate);
    return Error::OK;
}

Error PS4_SYSV_ABI sceSaveDataDialogTerminate() {
    LOG_DEBUG(Lib_SaveDataDialog, "called");
    if (GetStatus() == Status::RUNNING) {
        sceSaveDataDialogClose();
    }
    if (GetStatus() == Status::NONE) {
        return Error::NOT_INITIALIZED;
    }
    // Only the icons go: the overlay may still be drawing the last frame of the one it showed
    for (auto& state : g_states) {
        state.ReleaseIcons();
    }
    ovl_save_dialog_set_status(static_cast<int>(Status::NONE));
    CommonDialog::g_isUsed = false;
    return Error::OK;
}

Status PS4_SYSV_ABI sceSaveDataDialogUpdateStatus() {
    const auto status = GetStatus();
    LOG_TRACE(Lib_SaveDataDialog, "called status=%s",
              enum_name(&enum_names_Libraries_CommonDialog_Status, static_cast<s64>(status)));
    return status;
}

void RegisterLib(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("fH46Lag88XY", "libSceSaveDataDialog", 1, "libSceSaveDataDialog",
                 sceSaveDataDialogClose);
    LIB_FUNCTION("yEiJ-qqr6Cg", "libSceSaveDataDialog", 1, "libSceSaveDataDialog",
                 sceSaveDataDialogGetResult);
    LIB_FUNCTION("ERKzksauAJA", "libSceSaveDataDialog", 1, "libSceSaveDataDialog",
                 sceSaveDataDialogGetStatus);
    LIB_FUNCTION("s9e3+YpRnzw", "libSceSaveDataDialog", 1, "libSceSaveDataDialog",
                 sceSaveDataDialogInitialize);
    LIB_FUNCTION("en7gNVnh878", "libSceSaveDataDialog", 1, "libSceSaveDataDialog",
                 sceSaveDataDialogIsReadyToDisplay);
    LIB_FUNCTION("4tPhsP6FpDI", "libSceSaveDataDialog", 1, "libSceSaveDataDialog",
                 sceSaveDataDialogOpen);
    LIB_FUNCTION("V-uEeFKARJU", "libSceSaveDataDialog", 1, "libSceSaveDataDialog",
                 sceSaveDataDialogProgressBarInc);
    LIB_FUNCTION("hay1CfTmLyA", "libSceSaveDataDialog", 1, "libSceSaveDataDialog",
                 sceSaveDataDialogProgressBarSetValue);
    LIB_FUNCTION("YuH2FA7azqQ", "libSceSaveDataDialog", 1, "libSceSaveDataDialog",
                 sceSaveDataDialogTerminate);
    LIB_FUNCTION("KK3Bdg1RWK0", "libSceSaveDataDialog", 1, "libSceSaveDataDialog",
                 sceSaveDataDialogUpdateStatus);
};

} // namespace Libraries::SaveData::Dialog
