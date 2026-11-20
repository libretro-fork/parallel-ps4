// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/enum_names.h"

#include "common/assert.h"
#include "common/logging/log.h"
#include "core/libraries/libs.h"
#include "core/libraries/system/msgdialog.h"
#include "msgdialog_ui.h"

namespace Libraries::MsgDialog {

using CommonDialog::Error;
using CommonDialog::Result;
using CommonDialog::Status;

// The status and result are the overlay's (video_core/overlay/ovl_msgdialog.c), which draws the
// dialog and finishes it when a button is pressed; the mode is the game's thread's own
static constexpr int GAME = OVL_MSG_DIALOG_GAME;
static MsgDialogMode g_mode{};

static Status GetStatus() {
    return static_cast<Status>(ovl_msg_dialog_status(GAME));
}

Error PS4_SYSV_ABI sceMsgDialogClose() {
    LOG_DEBUG(Lib_MsgDlg, "called");
    if (GetStatus() != Status::RUNNING) {
        return Error::NOT_RUNNING;
    }
    ovl_msg_dialog_close(GAME, OVL_MSG_BUTTON_INVALID, OVL_MSG_RESULT_OK);
    return Error::OK;
}

Error PS4_SYSV_ABI sceMsgDialogGetResult(DialogResult* result) {
    LOG_DEBUG(Lib_MsgDlg, "called");
    if (GetStatus() != Status::FINISHED) {
        return Error::NOT_FINISHED;
    }
    if (result == nullptr) {
        return Error::ARG_NULL;
    }
    int button, r;
    ovl_msg_dialog_result(GAME, &button, &r);
    DialogResult out{};
    out.result = static_cast<Result>(r);
    out.buttonId = static_cast<ButtonId>(button);
    *result = out;
    return Error::OK;
}

Status PS4_SYSV_ABI sceMsgDialogGetStatus() {
    const Status status = GetStatus();
    LOG_TRACE(Lib_MsgDlg, "called status=%s",
              enum_name(&enum_names_Libraries_CommonDialog_Status, static_cast<s64>(status)));
    return status;
}

Error PS4_SYSV_ABI sceMsgDialogInitialize() {
    LOG_DEBUG(Lib_MsgDlg, "called");
    if (!CommonDialog::g_isInitialized) {
        return Error::NOT_SYSTEM_INITIALIZED;
    }
    if (GetStatus() != Status::NONE) {
        return Error::ALREADY_INITIALIZED;
    }
    if (CommonDialog::g_isUsed) {
        return Error::BUSY;
    }
    ovl_msg_dialog_set_status(GAME, static_cast<int>(Status::INITIALIZED));
    CommonDialog::g_isUsed = true;

    return Error::OK;
}

Error PS4_SYSV_ABI sceMsgDialogOpen(const OrbisParam* param) {
    const Status status = GetStatus();
    if (status != Status::INITIALIZED && status != Status::FINISHED) {
        LOG_INFO(Lib_MsgDlg, "called without initialize");
        return Error::INVALID_STATE;
    }
    if (param == nullptr) {
        LOG_DEBUG(Lib_MsgDlg, "called param:(NULL)");
        return Error::ARG_NULL;
    }
    LOG_DEBUG(
        Lib_MsgDlg, "called param->mode: %s",
        enum_name(&enum_names_Libraries_MsgDialog_MsgDialogMode, static_cast<s64>(param->mode)));
    ASSERT(param->size == sizeof(OrbisParam));
    ASSERT(param->baseParam.size == sizeof(CommonDialog::BaseParam));
    MsgDialogState state{*param};
    g_mode = state.GetMode();
    const ovl_msg_dialog_param_t overlay_param = MakeOverlayParam(state);
    ovl_msg_dialog_open(GAME, &overlay_param);
    return Error::OK;
}

static Error CheckProgressBar(OrbisMsgDialogProgressBarTarget target) {
    if (GetStatus() != Status::RUNNING) {
        return Error::NOT_RUNNING;
    }
    if (g_mode != MsgDialogMode::PROGRESS_BAR) {
        return Error::NOT_SUPPORTED;
    }
    if (target != OrbisMsgDialogProgressBarTarget::DEFAULT) {
        return Error::PARAM_INVALID;
    }
    return Error::OK;
}

Error PS4_SYSV_ABI sceMsgDialogProgressBarInc(OrbisMsgDialogProgressBarTarget target, u32 delta) {
    LOG_DEBUG(Lib_MsgDlg, "called");
    const Error error = CheckProgressBar(target);
    if (error == Error::OK) {
        ovl_msg_dialog_add_progress(GAME, delta);
    }
    return error;
}

Error PS4_SYSV_ABI sceMsgDialogProgressBarSetMsg(OrbisMsgDialogProgressBarTarget target,
                                                 const char* msg) {
    LOG_DEBUG(Lib_MsgDlg, "called");
    const Error error = CheckProgressBar(target);
    if (error == Error::OK) {
        ovl_msg_dialog_set_message(GAME, msg);
    }
    return error;
}

Error PS4_SYSV_ABI sceMsgDialogProgressBarSetValue(OrbisMsgDialogProgressBarTarget target,
                                                   u32 value) {
    LOG_DEBUG(Lib_MsgDlg, "called");
    const Error error = CheckProgressBar(target);
    if (error == Error::OK) {
        ovl_msg_dialog_set_progress(GAME, value);
    }
    return error;
}

Error PS4_SYSV_ABI sceMsgDialogTerminate() {
    LOG_DEBUG(Lib_MsgDlg, "called");
    if (GetStatus() == Status::RUNNING) {
        sceMsgDialogClose();
    }
    if (GetStatus() == Status::NONE) {
        return Error::NOT_INITIALIZED;
    }
    ovl_msg_dialog_set_status(GAME, static_cast<int>(Status::NONE));
    CommonDialog::g_isUsed = false;
    return Error::OK;
}

Status PS4_SYSV_ABI sceMsgDialogUpdateStatus() {
    const Status status = GetStatus();
    LOG_TRACE(Lib_MsgDlg, "called status=%s",
              enum_name(&enum_names_Libraries_CommonDialog_Status, static_cast<s64>(status)));
    return status;
}

void RegisterLib(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("HTrcDKlFKuM", "libSceMsgDialog", 1, "libSceMsgDialog", sceMsgDialogClose);
    LIB_FUNCTION("Lr8ovHH9l6A", "libSceMsgDialog", 1, "libSceMsgDialog", sceMsgDialogGetResult);
    LIB_FUNCTION("CWVW78Qc3fI", "libSceMsgDialog", 1, "libSceMsgDialog", sceMsgDialogGetStatus);
    LIB_FUNCTION("lDqxaY1UbEo", "libSceMsgDialog", 1, "libSceMsgDialog", sceMsgDialogInitialize);
    LIB_FUNCTION("b06Hh0DPEaE", "libSceMsgDialog", 1, "libSceMsgDialog", sceMsgDialogOpen);
    LIB_FUNCTION("Gc5k1qcK4fs", "libSceMsgDialog", 1, "libSceMsgDialog",
                 sceMsgDialogProgressBarInc);
    LIB_FUNCTION("6H-71OdrpXM", "libSceMsgDialog", 1, "libSceMsgDialog",
                 sceMsgDialogProgressBarSetMsg);
    LIB_FUNCTION("wTpfglkmv34", "libSceMsgDialog", 1, "libSceMsgDialog",
                 sceMsgDialogProgressBarSetValue);
    LIB_FUNCTION("ePw-kqZmelo", "libSceMsgDialog", 1, "libSceMsgDialog", sceMsgDialogTerminate);
    LIB_FUNCTION("6fIC3XKt2k0", "libSceMsgDialog", 1, "libSceMsgDialog", sceMsgDialogUpdateStatus);
};

} // namespace Libraries::MsgDialog
