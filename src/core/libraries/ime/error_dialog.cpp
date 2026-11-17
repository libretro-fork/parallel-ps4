// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <utility>
#include <fmt/format.h>
#include "common/enum_names.h"

#include "common/assert.h"
#include "common/logging/log.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/libs.h"
#include "core/libraries/system/commondialog.h"
#include "error_dialog.h"
#include "video_core/overlay/ovl_layers.h"

// Drawn by the overlay (video_core/overlay/ovl_msgdialog.c), which finishes it when OK is
// pressed; the status is its own

namespace Libraries::ErrorDialog {

using CommonDialog::Error;
using CommonDialog::Result;
using CommonDialog::Status;

static constexpr int DIALOG = OVL_MSG_DIALOG_ERROR;

static Status GetStatus() {
    return static_cast<Status>(ovl_msg_dialog_status(DIALOG));
}

struct Param {
    s32 size;
    s32 errorCode;
    OrbisUserServiceUserId userId;
    s32 _reserved;
};

Error PS4_SYSV_ABI sceErrorDialogClose() {
    LOG_DEBUG(Lib_ErrorDialog, "called");
    if (GetStatus() != Status::RUNNING) {
        return Error::NOT_RUNNING;
    }
    ovl_msg_dialog_close(DIALOG, OVL_MSG_BUTTON_INVALID, OVL_MSG_RESULT_OK);
    return Error::OK;
}

Status PS4_SYSV_ABI sceErrorDialogGetStatus() {
    const Status status = GetStatus();
    LOG_TRACE(Lib_ErrorDialog, "called status={}",
              enum_name(&enum_names_Libraries_CommonDialog_Status, static_cast<s64>(status)));
    return status;
}

Error PS4_SYSV_ABI sceErrorDialogInitialize() {
    LOG_DEBUG(Lib_ErrorDialog, "called");
    if (GetStatus() != Status::NONE) {
        return Error::ALREADY_INITIALIZED;
    }
    ovl_msg_dialog_set_status(DIALOG, static_cast<int>(Status::INITIALIZED));
    return Error::OK;
}

Error PS4_SYSV_ABI sceErrorDialogOpen(const Param* param) {
    const Status status = GetStatus();
    if (status != Status::INITIALIZED && status != Status::FINISHED) {
        LOG_INFO(Lib_ErrorDialog, "called without initialize");
        return Error::INVALID_STATE;
    }
    if (param == nullptr) {
        LOG_DEBUG(Lib_ErrorDialog, "called param:(NULL)");
        return Error::ARG_NULL;
    }
    const auto err = static_cast<u32>(param->errorCode);
    LOG_DEBUG(Lib_ErrorDialog, "called param->errorCode = {:#x}", err);
    ASSERT(param->size == sizeof(Param));

    const std::string err_message = fmt::format("An error has occurred. \nCode: {:#X}", err);
    ovl_msg_dialog_param_t overlay_param{};
    overlay_param.mode = OVL_MSG_MODE_ERROR;
    overlay_param.msg = err_message.c_str();
    ovl_msg_dialog_open(DIALOG, &overlay_param);
    return Error::OK;
}

int PS4_SYSV_ABI sceErrorDialogOpenDetail() {
    LOG_ERROR(Lib_ErrorDialog, "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceErrorDialogOpenWithReport() {
    LOG_ERROR(Lib_ErrorDialog, "(STUBBED) called");
    return ORBIS_OK;
}

Error PS4_SYSV_ABI sceErrorDialogTerminate() {
    LOG_DEBUG(Lib_ErrorDialog, "called");
    if (GetStatus() == Status::RUNNING) {
        sceErrorDialogClose();
    }
    if (GetStatus() == Status::NONE) {
        return Error::NOT_INITIALIZED;
    }
    ovl_msg_dialog_set_status(DIALOG, static_cast<int>(Status::NONE));
    return Error::OK;
}

Status PS4_SYSV_ABI sceErrorDialogUpdateStatus() {
    const Status status = GetStatus();
    LOG_TRACE(Lib_ErrorDialog, "called status={}",
              enum_name(&enum_names_Libraries_CommonDialog_Status, static_cast<s64>(status)));
    return status;
}

void RegisterLib(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("ekXHb1kDBl0", "libSceErrorDialog", 1, "libSceErrorDialog", sceErrorDialogClose);
    LIB_FUNCTION("t2FvHRXzgqk", "libSceErrorDialog", 1, "libSceErrorDialog",
                 sceErrorDialogGetStatus);
    LIB_FUNCTION("I88KChlynSs", "libSceErrorDialog", 1, "libSceErrorDialog",
                 sceErrorDialogInitialize);
    LIB_FUNCTION("M2ZF-ClLhgY", "libSceErrorDialog", 1, "libSceErrorDialog", sceErrorDialogOpen);
    LIB_FUNCTION("jrpnVQfJYgQ", "libSceErrorDialog", 1, "libSceErrorDialog",
                 sceErrorDialogOpenDetail);
    LIB_FUNCTION("wktCiyWoDTI", "libSceErrorDialog", 1, "libSceErrorDialog",
                 sceErrorDialogOpenWithReport);
    LIB_FUNCTION("9XAxK2PMwk8", "libSceErrorDialog", 1, "libSceErrorDialog",
                 sceErrorDialogTerminate);
    LIB_FUNCTION("WWiGuh9XfgQ", "libSceErrorDialog", 1, "libSceErrorDialog",
                 sceErrorDialogUpdateStatus);
};

} // namespace Libraries::ErrorDialog