// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// The message dialog's state, from the game's parameters; drawn by the overlay
// (video_core/overlay/ovl_msgdialog.c)

#include <utility>

#include "common/assert.h"
#include "msgdialog_ui.h"

using namespace Libraries::CommonDialog;
using namespace Libraries::MsgDialog;

MsgDialogState::MsgDialogState(const OrbisParam& param) {
    this->mode = param.mode;
    switch (mode) {
    case MsgDialogMode::USER_MSG: {
        ASSERT(param.userMsgParam);
        const auto& v = *param.userMsgParam;
        auto state = UserState{
            .type = v.buttonType,
            .msg = std::string(v.msg),
        };
        if (v.buttonType == ButtonType::TWO_BUTTONS) {
            ASSERT(v.buttonsParam);
            state.btn_param1 = std::string(v.buttonsParam->msg1);
            state.btn_param2 = std::string(v.buttonsParam->msg2);
        }
        this->state = state;
    } break;
    case MsgDialogMode::PROGRESS_BAR: {
        ASSERT(param.progBarParam);
        const auto& v = *param.progBarParam;
        this->state = ProgressState{
            .type = v.barType,
            .msg = std::string(v.msg),
            .progress = 0,
        };
    } break;
    case MsgDialogMode::SYSTEM_MSG: {
        ASSERT(param.sysMsgParam);
        const auto& v = *param.sysMsgParam;
        this->state = SystemState{
            .type = v.sysMsgType,
        };
    } break;
    default:
        UNREACHABLE_MSG("Unknown dialog mode");
    }
}

MsgDialogState::MsgDialogState(UserState mode) {
    this->mode = MsgDialogMode::USER_MSG;
    this->state = mode;
}

MsgDialogState::MsgDialogState(ProgressState mode) {
    this->mode = MsgDialogMode::PROGRESS_BAR;
    this->state = mode;
}

MsgDialogState::MsgDialogState(SystemState mode) {
    this->mode = MsgDialogMode::SYSTEM_MSG;
    this->state = mode;
}

ovl_msg_dialog_param_t Libraries::MsgDialog::MakeOverlayParam(MsgDialogState& state) {
    ovl_msg_dialog_param_t p{};
    p.mode = static_cast<int>(state.GetMode());
    switch (state.GetMode()) {
    case MsgDialogMode::USER_MSG: {
        const auto& user = state.GetState<MsgDialogState::UserState>();
        // Checked where it was drawn before
        ASSERT(user.type <= ButtonType::TWO_BUTTONS);
        p.button_type = static_cast<int>(user.type);
        p.msg = user.msg.c_str();
        p.button1 = user.btn_param1.c_str();
        p.button2 = user.btn_param2.c_str();
    } break;
    case MsgDialogMode::PROGRESS_BAR: {
        const auto& bar = state.GetState<MsgDialogState::ProgressState>();
        p.bar_type = static_cast<int>(bar.type);
        p.msg = bar.msg.c_str();
    } break;
    case MsgDialogMode::SYSTEM_MSG: {
        const auto& sys = state.GetState<MsgDialogState::SystemState>();
        ASSERT(sys.type <= SystemMessageType::WARNING_PROFILE_PICTURE_AND_NAME_NOT_SHARED);
        p.sys_type = static_cast<int>(sys.type);
    } break;
    }
    return p;
}

// The emulator's own, waiting its turn after the game's
DialogResult Libraries::MsgDialog::ShowMsgDialog(MsgDialogState state, bool block) {
    const ovl_msg_dialog_param_t param = MakeOverlayParam(state);
    ovl_msg_dialog_open(OVL_MSG_DIALOG_EMULATOR, &param);
    if (block) {
        ovl_msg_dialog_wait(OVL_MSG_DIALOG_EMULATOR);
    }
    DialogResult result{};
    if (ovl_msg_dialog_status(OVL_MSG_DIALOG_EMULATOR) == static_cast<int>(Status::FINISHED)) {
        int button, r;
        ovl_msg_dialog_result(OVL_MSG_DIALOG_EMULATOR, &button, &r);
        result.result = static_cast<Result>(r);
        result.buttonId = static_cast<ButtonId>(button);
    }
    return result;
}
