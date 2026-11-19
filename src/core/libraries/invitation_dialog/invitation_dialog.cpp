// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>
#include <fmt/format.h>
#include "common/enum_names.h"
#include "common/unique_function.h"

#include "common/logging/log.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/invitation_dialog/invitation_dialog.h"
#include "core/libraries/libs.h"
#include "core/libraries/np/np_handler/np_handler.h"
#include "core/libraries/system/commondialog.h"
#include "video_core/overlay/ovl_layers.h"

namespace Libraries::InvitationDialog {

// The dialog is the overlay's (video_core/overlay/ovl_invdialog.c): its status, what the player
// chose and whether an invitation was sent are kept there, among the game's thread and the
// overlay's. This is the game's thread's alone: what it opened the dialog with, and what
// GetResult()/GetResultA() hand back, taken from the overlay once it is finished.
struct DialogState {
    s32 mode = ORBIS_INVITATION_DIALOG_MODE_INVALID;
    CommonDialog::Result result = CommonDialog::Result::OK;
    bool user_editable = false; // USERENABLE recipient list (the player ticks friends)
    void* callback_arg = nullptr;
    s32 error_code = 0;
    // USERDISABLE fixed recipient lists resolved at Open. Only one is populated per dialog:
    // online IDs (non-A path) or account IDs (A accountIds path).
    std::vector<Libraries::Np::OrbisNpOnlineId> online_ids;
    std::vector<Libraries::Np::OrbisNpAccountId> account_ids;
    // Recipients actually sent (SEND mode), echoed back by GetResult/GetResultA. Online IDs come
    // from the picker or the non-A fixed list; account IDs from the A fixed list.
    std::vector<std::string> sent_online_ids;
    std::vector<Libraries::Np::OrbisNpAccountId> sent_account_ids;
};
static DialogState g_state;

static Libraries::CommonDialog::Status DialogStatus() {
    return static_cast<Libraries::CommonDialog::Status>(ovl_invitation_dialog_status());
}

static std::string OnlineIdText(const Libraries::Np::OrbisNpOnlineId& oid) {
    size_t n = 0;
    while (n < sizeof(oid.data) && oid.data[n] != '\0') {
        n++;
    }
    return std::string(oid.data, n);
}

// The finished dialog's result and who it sent to, for GetResult()/GetResultA()
static void TakeResult() {
    g_state.result = static_cast<CommonDialog::Result>(ovl_invitation_dialog_result());
    g_state.sent_online_ids.clear();
    g_state.sent_account_ids.clear();
    const char* picked[OVL_INVDLG_SENT];
    const int n = ovl_invitation_dialog_sent(picked, OVL_INVDLG_SENT);
    if (n < 0) {
        return; // nothing sent
    }
    if (g_state.user_editable) {
        g_state.sent_online_ids.assign(picked, picked + n);
    } else {
        for (const auto& oid : g_state.online_ids) {
            g_state.sent_online_ids.push_back(OnlineIdText(oid));
        }
    }
    g_state.sent_account_ids = g_state.account_ids;
}

// Opens the overlay's dialog with what g_state was just given, and for SEND the session and
// message, for RECV the invitations waiting for the user
static void OpenOverlayDialog(
    s32 user_id, const char* session_id, const char* message,
    const std::vector<Libraries::Np::NpHandler::PendingInvitation>& pend) {
    ovl_invitation_dialog_param_t shown{};
    shown.mode = g_state.mode;
    shown.user_id = user_id;
    shown.user_editable = g_state.user_editable;
    shown.session_id = session_id;
    shown.message = message;
    // The game's own recipients, as the server takes them: online ids, or account numbers
    std::vector<std::string> fixed;
    for (const auto& oid : g_state.online_ids) {
        fixed.push_back(OnlineIdText(oid));
    }
    for (const auto id : g_state.account_ids) {
        fixed.push_back(std::to_string(id));
    }
    const char* fixed_to[OVL_INVDLG_FIXED];
    for (;
         shown.fixed_count < static_cast<int>(fixed.size()) && shown.fixed_count < OVL_INVDLG_FIXED;
         shown.fixed_count++) {
        fixed_to[shown.fixed_count] = fixed[shown.fixed_count].c_str();
    }
    shown.fixed_to = fixed_to;
    ovl_invdlg_invite_t invites[OVL_INVDLG_INVITES];
    for (; shown.invite_count < static_cast<int>(pend.size()) &&
           shown.invite_count < OVL_INVDLG_INVITES;
         shown.invite_count++) {
        invites[shown.invite_count].invitation_id = pend[shown.invite_count].invitation_id.c_str();
        invites[shown.invite_count].from_npid = pend[shown.invite_count].from_npid.c_str();
    }
    shown.invites = invites;
    ovl_invitation_dialog_open(&shown);
}

s32 PS4_SYSV_ABI sceInvitationDialogGetResultA(OrbisInvitationDialogResultA* result) {
    LOG_DEBUG(Lib_InvitationDialog, "Getting invitation dialog result (async), result ptr={}",
              fmt::ptr(result));

    if (DialogStatus() == Libraries::CommonDialog::Status::NONE) {
        return static_cast<s32>(Libraries::CommonDialog::Error::NOT_INITIALIZED);
    }
    if (DialogStatus() != Libraries::CommonDialog::Status::FINISHED) {
        return static_cast<s32>(Libraries::CommonDialog::Error::NOT_FINISHED);
    }
    if (result == nullptr) {
        return static_cast<s32>(Libraries::CommonDialog::Error::ARG_NULL);
    }

    TakeResult();
    if (g_state.error_code != 0) {
        LOG_ERROR(Lib_InvitationDialog, "errorCode {} block all players", g_state.error_code);
        return ORBIS_INVITATION_DIALOG_ERROR_BLOCKED_ALL_PLAYERS;
    }

    result->callbackArg = g_state.callback_arg;
    result->errorCode = g_state.error_code;
    result->result = g_state.result;
    // sentUsers carries {onlineId, accountId} pairs; report whichever recipient form was sent --
    // online IDs (picker / non-A fixed list) or account IDs (A fixed list). Only one is populated.
    if (result->sentUsers != nullptr) {
        u32 count =
            static_cast<u32>(g_state.sent_online_ids.size() > g_state.sent_account_ids.size()
                                 ? g_state.sent_online_ids.size()
                                 : g_state.sent_account_ids.size());
        if (count > ORBIS_INVITATION_DIALOG_ADDRESS_USER_LIST_MAX_SIZE) {
            count = ORBIS_INVITATION_DIALOG_ADDRESS_USER_LIST_MAX_SIZE;
        }
        for (u32 i = 0; i < count; i++) {
            auto& u = result->sentUsers->users[i];
            std::memset(&u, 0, sizeof(u));
            if (i < g_state.sent_online_ids.size()) {
                std::strncpy(u.onlineId.data, g_state.sent_online_ids[i].c_str(),
                             sizeof(u.onlineId.data) - 1);
            }
            if (i < g_state.sent_account_ids.size()) {
                u.accountId = g_state.sent_account_ids[i];
            }
        }
        result->sentUsers->count =
            (g_state.result == Libraries::CommonDialog::Result::OK) ? count : 0;
    }
    return static_cast<s32>(g_state.result);
}

Libraries::CommonDialog::Error PS4_SYSV_ABI
sceInvitationDialogOpenA(const OrbisInvitationDialogParamA* param) {
    LOG_DEBUG(Lib_InvitationDialog, "sceInvitationDialogOpenA() called (async version)");
    LOG_INFO(Lib_InvitationDialog, "Opening invitation dialog asynchronously, param ptr={}",
             fmt::ptr(param));

    if (!param) {
        LOG_ERROR(Lib_InvitationDialog, "Parameter is NULL");
        return Libraries::CommonDialog::Error::ARG_NULL;
    }

    LOG_INFO(Lib_InvitationDialog, "ParamA size={:#x}", param->size);
    LOG_INFO(Lib_InvitationDialog, "ParamA mode={}", param->mode);

    // Validate mode
    if (param->mode != ORBIS_INVITATION_DIALOG_MODE_SEND &&
        param->mode != ORBIS_INVITATION_DIALOG_MODE_RECV) {
        LOG_ERROR(Lib_InvitationDialog, "Invalid mode: {} (expected SEND={} or RECV={})",
                  param->mode, ORBIS_INVITATION_DIALOG_MODE_SEND,
                  ORBIS_INVITATION_DIALOG_MODE_RECV);
        return Libraries::CommonDialog::Error::PARAM_INVALID;
    }

    LOG_INFO(Lib_InvitationDialog, "ParamA userId={:#x}", param->userId);
    LOG_INFO(Lib_InvitationDialog, "ParamA callbackArg={}", fmt::ptr(param->callbackArg));
    LOG_INFO(Lib_InvitationDialog, "ParamA dataParam={}", fmt::ptr(param->dataParam));

    // Log data parameters if present
    if (param->dataParam) {
        LOG_INFO(Lib_InvitationDialog, "DataParamA pointer: {}", fmt::ptr(param->dataParam));
        if (param->mode == ORBIS_INVITATION_DIALOG_MODE_SEND) {
            LOG_INFO(Lib_InvitationDialog, "SendInfo:");
            if (param->dataParam->SendInfo.userMessage) {
                LOG_INFO(Lib_InvitationDialog, "  userMessage: '{}'",
                         param->dataParam->SendInfo.userMessage);
            }
            if (param->dataParam->SendInfo.sessionId) {
                LOG_INFO(Lib_InvitationDialog, "  sessionId: {}",
                         param->dataParam->SendInfo.sessionId->data);
            }

            // Log address parameters
            const auto& addressParam = param->dataParam->SendInfo.addressParam;
            LOG_INFO(Lib_InvitationDialog, "  addressType={}", addressParam.addressType);
            if (addressParam.addressType == ORBIS_INVITATION_DIALOG_ADDRESS_TYPE_USERDISABLE) {
                LOG_INFO(Lib_InvitationDialog, "  UserSelectDisableAddress:");
                LOG_INFO(Lib_InvitationDialog, "    accountIds={}",
                         fmt::ptr(addressParam.addressInfo.UserSelectDisableAddress.accountIds));
                LOG_INFO(Lib_InvitationDialog, "    accountIdsCount={}",
                         addressParam.addressInfo.UserSelectDisableAddress.accountIdsCount);
            } else if (addressParam.addressType ==
                       ORBIS_INVITATION_DIALOG_ADDRESS_TYPE_USERENABLE) {
                LOG_INFO(Lib_InvitationDialog, "  UserSelectEnableAddress:");
                LOG_INFO(Lib_InvitationDialog, "    userMaxCount={}",
                         addressParam.addressInfo.UserSelectEnableAddress.userMaxCount);
            } else {
                LOG_WARNING(Lib_InvitationDialog, "  Invalid addressType: {}",
                            addressParam.addressType);
            }
        } else if (param->mode == ORBIS_INVITATION_DIALOG_MODE_RECV) {
            LOG_INFO(Lib_InvitationDialog, "RecvInfo (receive mode - no additional data)");
        }
    }

    // Check initialization state
    if (DialogStatus() != Libraries::CommonDialog::Status::INITIALIZED &&
        DialogStatus() != Libraries::CommonDialog::Status::FINISHED) {
        LOG_INFO(
            Lib_InvitationDialog, "Called without proper initialization (status={})",
            enum_name(&enum_names_Libraries_CommonDialog_Status, static_cast<s64>(DialogStatus())));
        return Libraries::CommonDialog::Error::INVALID_STATE;
    }

    const char* session_id = nullptr;
    const char* message = nullptr;
    std::vector<Libraries::Np::NpHandler::PendingInvitation> pend;
    g_state = DialogState{};
    g_state.mode = param->mode;
    g_state.callback_arg = param->callbackArg;

    if (param->mode == ORBIS_INVITATION_DIALOG_MODE_SEND) {
        if (param->dataParam == nullptr) {
            return Libraries::CommonDialog::Error::PARAM_INVALID;
        }
        const auto& send = param->dataParam->SendInfo;
        if (send.sessionId != nullptr) {
            session_id = send.sessionId->data;
        }
        if (send.userMessage != nullptr) {
            message = send.userMessage;
        }
        // USERDISABLE: the app fixes the recipient list up front (account IDs). USERENABLE: the
        // user picks them in the dialog via the searchable friend picker.
        if (send.addressParam.addressType == ORBIS_INVITATION_DIALOG_ADDRESS_TYPE_USERENABLE) {
            g_state.user_editable = true;
        } else if (send.addressParam.addressType ==
                   ORBIS_INVITATION_DIALOG_ADDRESS_TYPE_USERDISABLE) {
            const auto& addr = send.addressParam.addressInfo.UserSelectDisableAddress;
            u32 count = addr.accountIdsCount;
            if (count > ORBIS_INVITATION_DIALOG_ADDRESS_USER_LIST_MAX_SIZE) {
                count = ORBIS_INVITATION_DIALOG_ADDRESS_USER_LIST_MAX_SIZE;
            }
            for (u32 i = 0; i < count && addr.accountIds != nullptr; i++) {
                g_state.account_ids.push_back(addr.accountIds[i]);
            }
        }
        LOG_INFO(Lib_InvitationDialog, "OpenA(SEND) user={} sessionId='{}' recipients={}",
                 param->userId, session_id ? session_id : "", g_state.account_ids.size());
    } else {
        pend = Libraries::Np::NpHandler::GetInstance().GetPendingInvitations(param->userId);
        LOG_INFO(Lib_InvitationDialog, "OpenA(RECV) user={} pending={}", param->userId,
                 pend.size());
    }

    OpenOverlayDialog(param->userId, session_id, message, pend);
    return Libraries::CommonDialog::Error::OK;
}

Libraries::CommonDialog::Error PS4_SYSV_ABI
sceInvitationDialogOpen(const OrbisInvitationDialogParam* param) {
    LOG_DEBUG(Lib_InvitationDialog, "Opening invitation dialog, param ptr={}", fmt::ptr(param));

    if (DialogStatus() != Libraries::CommonDialog::Status::INITIALIZED &&
        DialogStatus() != Libraries::CommonDialog::Status::FINISHED) {
        LOG_ERROR(Lib_InvitationDialog, "called without initialize");
        return Libraries::CommonDialog::Error::INVALID_STATE;
    }

    if (!param) {
        LOG_ERROR(Lib_InvitationDialog, "Parameter is NULL");
        return Libraries::CommonDialog::Error::ARG_NULL;
    }

    LOG_INFO(Lib_InvitationDialog,
             "size={:#x} mode={} userId={} Param callbackArg={} Param dataParam={}", param->size,
             param->mode, param->userId, fmt::ptr(param->callbackArg), fmt::ptr(param->dataParam));

    // Validate mode
    if (param->mode != ORBIS_INVITATION_DIALOG_MODE_SEND &&
        param->mode != ORBIS_INVITATION_DIALOG_MODE_RECV) {
        LOG_ERROR(Lib_InvitationDialog, "Invalid mode: {} (expected SEND={} or RECV={})",
                  param->mode, ORBIS_INVITATION_DIALOG_MODE_SEND,
                  ORBIS_INVITATION_DIALOG_MODE_RECV);
        return Libraries::CommonDialog::Error::PARAM_INVALID;
    }

    // Log data parameters if present
    if (param->dataParam) {
        LOG_INFO(Lib_InvitationDialog, "DataParam pointer: {}", fmt::ptr(param->dataParam));
        if (param->mode == ORBIS_INVITATION_DIALOG_MODE_SEND) {
            LOG_INFO(Lib_InvitationDialog, "SendInfo:");
            if (param->dataParam->SendInfo.userMessage) {
                LOG_INFO(Lib_InvitationDialog, "  userMessage: '{}'",
                         param->dataParam->SendInfo.userMessage);
            }
            if (param->dataParam->SendInfo.sessionId) {
                LOG_INFO(Lib_InvitationDialog, "  sessionId: {}",
                         param->dataParam->SendInfo.sessionId->data);
            }

            // Log address parameters
            const auto& addressParam = param->dataParam->SendInfo.addressParam;
            LOG_INFO(Lib_InvitationDialog, "  addressType={}", addressParam.addressType);
            if (addressParam.addressType == ORBIS_INVITATION_DIALOG_ADDRESS_TYPE_USERDISABLE) {
                LOG_INFO(Lib_InvitationDialog, "  UserSelectDisableAddress:");
                LOG_INFO(Lib_InvitationDialog, "    onlineIds={}",
                         fmt::ptr(addressParam.addressInfo.UserSelectDisableAddress.onlineIds));
                LOG_INFO(Lib_InvitationDialog, "    onlineIdsCount={}",
                         addressParam.addressInfo.UserSelectDisableAddress.onlineIdsCount);
            } else if (addressParam.addressType ==
                       ORBIS_INVITATION_DIALOG_ADDRESS_TYPE_USERENABLE) {
                LOG_INFO(Lib_InvitationDialog, "  UserSelectEnableAddress:");
                LOG_INFO(Lib_InvitationDialog, "    onlineIdsMaxCount={}",
                         addressParam.addressInfo.UserSelectEnableAddress.onlineIdsMaxCount);
            } else {
                LOG_WARNING(Lib_InvitationDialog, "  Invalid addressType: {}",
                            addressParam.addressType);
            }
        } else if (param->mode == ORBIS_INVITATION_DIALOG_MODE_RECV) {
            LOG_INFO(Lib_InvitationDialog, "RecvInfo (receive mode - no additional data)");
        }
    }

    const char* session_id = nullptr;
    const char* message = nullptr;
    std::vector<Libraries::Np::NpHandler::PendingInvitation> pend;
    g_state = DialogState{};
    g_state.mode = param->mode;
    g_state.callback_arg = param->callbackArg;

    if (param->mode == ORBIS_INVITATION_DIALOG_MODE_SEND) {
        if (param->dataParam == nullptr) {
            return Libraries::CommonDialog::Error::PARAM_INVALID;
        }
        const auto& send = param->dataParam->SendInfo;
        if (send.sessionId != nullptr) {
            session_id = send.sessionId->data;
        }
        if (send.userMessage != nullptr) {
            message = send.userMessage;
        }
        // USERDISABLE: the app fixes the recipient list up front. USERENABLE: the user picks them
        // in the dialog via the searchable friend picker.
        if (send.addressParam.addressType == ORBIS_INVITATION_DIALOG_ADDRESS_TYPE_USERENABLE) {
            g_state.user_editable = true;
        } else if (send.addressParam.addressType ==
                   ORBIS_INVITATION_DIALOG_ADDRESS_TYPE_USERDISABLE) {
            const auto& addr = send.addressParam.addressInfo.UserSelectDisableAddress;
            u32 count = addr.onlineIdsCount;
            if (count > ORBIS_INVITATION_DIALOG_ADDRESS_USER_LIST_MAX_SIZE) {
                count = ORBIS_INVITATION_DIALOG_ADDRESS_USER_LIST_MAX_SIZE;
            }
            if (count != 0 && addr.onlineIds != nullptr) {
                g_state.online_ids.assign(addr.onlineIds, addr.onlineIds + count);
            }
        }
        LOG_INFO(Lib_InvitationDialog, "Open(SEND) user={} sessionId='{}' recipients={}",
                 param->userId, session_id ? session_id : "", g_state.online_ids.size());
    } else {
        pend = Libraries::Np::NpHandler::GetInstance().GetPendingInvitations(param->userId);
        LOG_INFO(Lib_InvitationDialog, "Open(RECV) user={} pending={}", param->userId, pend.size());
    }

    OpenOverlayDialog(param->userId, session_id, message, pend);
    return Libraries::CommonDialog::Error::OK;
}

s32 PS4_SYSV_ABI sceInvitationDialogGetResult(OrbisInvitationDialogResult* result) {
    LOG_DEBUG(Lib_InvitationDialog, "Getting invitation dialog result, result ptr={}",
              fmt::ptr(result));

    if (DialogStatus() == Libraries::CommonDialog::Status::NONE) {
        return static_cast<s32>(Libraries::CommonDialog::Error::NOT_INITIALIZED);
    }
    if (DialogStatus() != Libraries::CommonDialog::Status::FINISHED) {
        return static_cast<s32>(Libraries::CommonDialog::Error::NOT_FINISHED);
    }
    if (result == nullptr) {
        return static_cast<s32>(Libraries::CommonDialog::Error::ARG_NULL);
    }

    TakeResult();
    if (g_state.error_code != 0) {
        LOG_ERROR(Lib_InvitationDialog, "errorCode {} block all players", g_state.error_code);
        return ORBIS_INVITATION_DIALOG_ERROR_BLOCKED_ALL_PLAYERS;
    }

    // Log result fields if they contain data
    if (result->callbackArg) {
        LOG_INFO(Lib_InvitationDialog, "Result callbackArg={}", fmt::ptr(result->callbackArg));
    }
    LOG_INFO(Lib_InvitationDialog, "Result errorCode={:#x}", result->errorCode);
    LOG_INFO(
        Lib_InvitationDialog, "Result status={}",
        enum_name(&enum_names_Libraries_CommonDialog_Result, static_cast<s64>(result->result)));

    if (result->sentOnlineIds) {
        LOG_INFO(Lib_InvitationDialog, "Result sentOnlineIds count={}",
                 result->sentOnlineIds->count);
        for (u32 i = 0; i < result->sentOnlineIds->count &&
                        i < ORBIS_INVITATION_DIALOG_ADDRESS_USER_LIST_MAX_SIZE;
             i++) {
            LOG_INFO(Lib_InvitationDialog, "  sentOnlineIds[{}]: {}", i,
                     result->sentOnlineIds->onlineId[i].data);
        }
    }
    result->callbackArg = g_state.callback_arg;
    result->errorCode = g_state.error_code;
    result->result = g_state.result;
    if (result->sentOnlineIds != nullptr) {
        u32 count = static_cast<u32>(g_state.sent_online_ids.size());
        if (count > ORBIS_INVITATION_DIALOG_ADDRESS_USER_LIST_MAX_SIZE) {
            count = ORBIS_INVITATION_DIALOG_ADDRESS_USER_LIST_MAX_SIZE;
        }
        for (u32 i = 0; i < count; i++) {
            auto& oid = result->sentOnlineIds->onlineId[i];
            std::memset(&oid, 0, sizeof(oid));
            std::strncpy(oid.data, g_state.sent_online_ids[i].c_str(), sizeof(oid.data) - 1);
        }
        result->sentOnlineIds->count =
            (g_state.result == Libraries::CommonDialog::Result::OK) ? count : 0;
    }
    return static_cast<s32>(g_state.result);
}

Libraries::CommonDialog::Status PS4_SYSV_ABI sceInvitationDialogUpdateStatus() {
    LOG_DEBUG(
        Lib_InvitationDialog, "UpdateStatus called, current status={}",
        enum_name(&enum_names_Libraries_CommonDialog_Status, static_cast<s64>(DialogStatus())));
    return DialogStatus();
}

Libraries::CommonDialog::Error PS4_SYSV_ABI sceInvitationDialogClose() {
    LOG_DEBUG(
        Lib_InvitationDialog, "Closing invitation dialog, current status={}",
        enum_name(&enum_names_Libraries_CommonDialog_Status, static_cast<s64>(DialogStatus())));

    if (DialogStatus() != Libraries::CommonDialog::Status::RUNNING) {
        LOG_WARNING(
            Lib_InvitationDialog, "Cannot close dialog: not running (status={})",
            enum_name(&enum_names_Libraries_CommonDialog_Status, static_cast<s64>(DialogStatus())));
        return Libraries::CommonDialog::Error::NOT_RUNNING;
    }

    // Closed on screen at the same moment: finished already, by the player
    if (!ovl_invitation_dialog_close(OVL_INVDLG_RESULT_OK)) {
        return Libraries::CommonDialog::Error::NOT_RUNNING;
    }
    return Libraries::CommonDialog::Error::OK;
}

Libraries::CommonDialog::Error PS4_SYSV_ABI sceInvitationDialogTerminate() {
    LOG_DEBUG(
        Lib_InvitationDialog, "Terminating invitation dialog system, current status={}",
        enum_name(&enum_names_Libraries_CommonDialog_Status, static_cast<s64>(DialogStatus())));

    if (DialogStatus() == Libraries::CommonDialog::Status::RUNNING) {
        LOG_DEBUG(Lib_InvitationDialog, "Dialog is running, closing before termination");
        sceInvitationDialogClose();
    }

    if (DialogStatus() == Libraries::CommonDialog::Status::NONE) {
        LOG_ERROR(Lib_InvitationDialog, "Cannot terminate: not initialized");
        return Libraries::CommonDialog::Error::NOT_INITIALIZED;
    }

    ovl_invitation_dialog_set_status(static_cast<int>(Libraries::CommonDialog::Status::NONE));
    CommonDialog::g_isUsed = false;
    LOG_DEBUG(Lib_InvitationDialog, "Invitation dialog terminated successfully");
    return Libraries::CommonDialog::Error::OK;
}

Libraries::CommonDialog::Status PS4_SYSV_ABI sceInvitationDialogGetStatus() {
    LOG_DEBUG(
        Lib_InvitationDialog, "Getting invitation dialog status: {}",
        enum_name(&enum_names_Libraries_CommonDialog_Status, static_cast<s64>(DialogStatus())));
    return DialogStatus();
}

Libraries::CommonDialog::Error PS4_SYSV_ABI sceInvitationDialogInitialize() {
    LOG_DEBUG(Lib_InvitationDialog, "called");
    if (!CommonDialog::g_isInitialized) {
        LOG_ERROR(Lib_InvitationDialog, "CommonDialog system not initialized");
        return Libraries::CommonDialog::Error::NOT_SYSTEM_INITIALIZED;
    }

    if (DialogStatus() != Libraries::CommonDialog::Status::NONE) {
        LOG_ERROR(
            Lib_InvitationDialog, "Invitation dialog already initialized (status={})",
            enum_name(&enum_names_Libraries_CommonDialog_Status, static_cast<s64>(DialogStatus())));
        return Libraries::CommonDialog::Error::ALREADY_INITIALIZED;
    }

    if (CommonDialog::g_isUsed) {
        LOG_ERROR(Lib_InvitationDialog, "CommonDialog already in use");
        return Libraries::CommonDialog::Error::BUSY;
    }

    ovl_invitation_dialog_set_status(
        static_cast<int>(Libraries::CommonDialog::Status::INITIALIZED));
    CommonDialog::g_isUsed = true;
    LOG_INFO(Lib_InvitationDialog, "Invitation dialog initialized successfully");
    return Libraries::CommonDialog::Error::OK;
}

} // namespace Libraries::InvitationDialog

// What the overlay's dialog asks of the core, on the overlay's thread. Sending, accepting and
// declining hit the shadNet server (blocking HTTP inside NpHandler), so each runs on a thread
// of its own; the dialog shows itself busy until a send or an acceptance says how it went.
extern "C" int ovl_invitation_dialog_friends(int user_id, ovl_invdlg_friend_t* out, int max) {
    const auto list = Libraries::Np::NpHandler::GetInstance().GetFriendList(user_id);
    int n = 0;
    for (const auto& f : list.friends) {
        if (n == max) {
            break;
        }
        const size_t len = std::min(f.npid.size(), sizeof(out[n].npid) - 1);
        std::memcpy(out[n].npid, f.npid.data(), len);
        out[n].npid[len] = '\0';
        out[n].online = f.online;
        n++;
    }
    return n;
}

extern "C" void ovl_invitation_dialog_send(int user_id, const char* session_id, const char* message,
                                           const char* const* to, int count, unsigned token) {
    Common::DetachThread([user_id, session = std::string{session_id}, msg = std::string{message},
                          recipients = std::vector<std::string>(to, to + count), token] {
        const bool ok = Libraries::Np::NpHandler::GetInstance().SendSessionInvitation(
            user_id, session, recipients, msg);
        ovl_invitation_dialog_request_done(token, ok);
    });
}

extern "C" void ovl_invitation_dialog_accept(int user_id, const char* invitation_id,
                                             unsigned token) {
    Common::DetachThread([user_id, id = std::string{invitation_id}, token] {
        const bool ok =
            Libraries::Np::NpHandler::GetInstance().AcceptSessionInvitation(user_id, id);
        ovl_invitation_dialog_request_done(token, ok);
    });
}

extern "C" void ovl_invitation_dialog_decline(int user_id, const char* invitation_id) {
    Common::DetachThread([user_id, id = std::string{invitation_id}] {
        Libraries::Np::NpHandler::GetInstance().DeclineSessionInvitation(user_id, id);
    });
}

namespace Libraries::InvitationDialog {

void RegisterLib(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("WWtCL5lzi7Y", "libSceInvitationDialog", 1, "libSceInvitationDialog",
                 sceInvitationDialogClose);
    LIB_FUNCTION("8XKR6wa64iQ", "libSceInvitationDialog", 1, "libSceInvitationDialog",
                 sceInvitationDialogGetResult);
    LIB_FUNCTION("WuuUhuKOxwQ", "libSceInvitationDialog", 1, "libSceInvitationDialog",
                 sceInvitationDialogGetResultA);
    LIB_FUNCTION("EiF92YDNHRA", "libSceInvitationDialog", 1, "libSceInvitationDialog",
                 sceInvitationDialogGetStatus);
    LIB_FUNCTION("XvA5KS56wcs", "libSceInvitationDialog", 1, "libSceInvitationDialog",
                 sceInvitationDialogInitialize);
    LIB_FUNCTION("0zU0G+wiVLA", "libSceInvitationDialog", 1, "libSceInvitationDialog",
                 sceInvitationDialogOpen);
    LIB_FUNCTION("sAxbHhAWMXM", "libSceInvitationDialog", 1, "libSceInvitationDialog",
                 sceInvitationDialogOpenA);
    LIB_FUNCTION("B6HVJtDYxEE", "libSceInvitationDialog", 1, "libSceInvitationDialog",
                 sceInvitationDialogTerminate);
    LIB_FUNCTION("9+g9iOq+7kg", "libSceInvitationDialog", 1, "libSceInvitationDialog",
                 sceInvitationDialogUpdateStatus);
    LIB_FUNCTION("8XKR6wa64iQ", "libSceInvitationDialogCompat", 1, "libSceInvitationDialog",
                 sceInvitationDialogGetResult);
    LIB_FUNCTION("0zU0G+wiVLA", "libSceInvitationDialogCompat", 1, "libSceInvitationDialog",
                 sceInvitationDialogOpen);
};

} // namespace Libraries::InvitationDialog