/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stddef.h>
#include <string.h>

#include <boolean.h>

#include "common/c_bridge.h"
#include "common/enum_names.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/invitation_dialog/invitation_dialog.h"
#include "core/libraries/np/np_handler/np_handler_c.h"
#include "video_core/overlay/ovl_layers.h"

/* An online id's text, terminated: up to its 16 characters */
typedef struct invdlg_online_id_text {
    char text[NP_ONLINEID_MAX_LENGTH + 1];
} invdlg_online_id_text;

/* An account id's text: up to 20 digits, terminated */
typedef struct invdlg_account_id_text {
    char text[21];
} invdlg_account_id_text;

/* What the overlay says was sent to: its npids, up to 31 characters */
typedef struct invdlg_sent_text {
    char text[32];
} invdlg_sent_text;

/* The dialog is the overlay's (video_core/overlay/ovl_invdialog.c): its status, what the player
 * chose and whether an invitation was sent are kept there, among the game's thread and the
 * overlay's. This is the game's thread's alone: what it opened the dialog with, and what
 * GetResult()/GetResultA() hand back, taken from the overlay once it is finished. The lists are
 * as long as a dialog's can be: nothing is allocated. */
typedef struct invdlg_state {
    s32 mode;
    u32 result;
    bool user_editable; /* USERENABLE recipient list (the player ticks friends) */
    void* callback_arg;
    s32 error_code;
    /* USERDISABLE fixed recipient lists resolved at Open. Only one is populated per dialog:
     * online IDs (non-A path) or account IDs (A accountIds path). */
    u32 online_id_count;
    np_online_id_t online_ids[ORBIS_INVITATION_DIALOG_ADDRESS_USER_LIST_MAX_SIZE];
    u32 account_id_count;
    np_account_id_t account_ids[ORBIS_INVITATION_DIALOG_ADDRESS_USER_LIST_MAX_SIZE];
    /* Recipients actually sent (SEND mode), echoed back by GetResult/GetResultA. Online IDs
     * come from the picker or the non-A fixed list; account IDs from the A fixed list. */
    u32 sent_online_id_count;
    invdlg_sent_text sent_online_ids[OVL_INVDLG_SENT];
    u32 sent_account_id_count;
    np_account_id_t sent_account_ids[ORBIS_INVITATION_DIALOG_ADDRESS_USER_LIST_MAX_SIZE];
} invdlg_state;

static invdlg_state g_state;

typedef char invdlg_sent_fits[OVL_INVDLG_SENT == ORBIS_INVITATION_DIALOG_ADDRESS_USER_LIST_MAX_SIZE
                                  ? 1
                                  : -1];

static u32 DialogStatus(void) {
    return (u32)ovl_invitation_dialog_status();
}

static const char* StatusName(void) {
    return enum_name(&enum_names_Libraries_CommonDialog_Status, (s64)DialogStatus());
}

static void ResetState(void) {
    memset(&g_state, 0, sizeof(g_state));
    g_state.mode = ORBIS_INVITATION_DIALOG_MODE_INVALID;
    g_state.result = COMMON_DIALOG_RESULT_OK;
}

static void OnlineIdText(const np_online_id_t* oid, char* out) {
    size_t n = 0;
    while (n < sizeof(oid->data) && oid->data[n] != '\0') {
        out[n] = oid->data[n];
        n++;
    }
    out[n] = '\0';
}

/* @text, cut to what @size holds, terminated */
static void CopyText(char* dst, size_t size, const char* src) {
    size_t n = 0;
    while (n + 1 < size && src[n] != '\0') {
        dst[n] = src[n];
        n++;
    }
    dst[n] = '\0';
}

/* std::to_string of an account id */
static void AccountIdText(np_account_id_t id, char* out) {
    char digits[21];
    size_t n = 0;
    size_t i;
    do {
        digits[n++] = (char)('0' + (int)(id % 10));
        id /= 10;
    } while (id != 0);
    for (i = 0; i < n; i++) {
        out[i] = digits[n - 1 - i];
    }
    out[n] = '\0';
}

/* The finished dialog's result and who it sent to, for GetResult()/GetResultA() */
static void TakeResult(void) {
    const char* picked[OVL_INVDLG_SENT];
    int n;
    u32 i;
    g_state.result = (u32)ovl_invitation_dialog_result();
    g_state.sent_online_id_count = 0;
    g_state.sent_account_id_count = 0;
    n = ovl_invitation_dialog_sent(picked, OVL_INVDLG_SENT);
    if (n < 0) {
        return; /* nothing sent */
    }
    if (g_state.user_editable) {
        for (i = 0; i < (u32)n; i++) {
            CopyText(g_state.sent_online_ids[i].text, sizeof(g_state.sent_online_ids[i].text),
                     picked[i]);
        }
        g_state.sent_online_id_count = (u32)n;
    } else {
        for (i = 0; i < g_state.online_id_count; i++) {
            OnlineIdText(&g_state.online_ids[i], g_state.sent_online_ids[i].text);
        }
        g_state.sent_online_id_count = g_state.online_id_count;
    }
    for (i = 0; i < g_state.account_id_count; i++) {
        g_state.sent_account_ids[i] = g_state.account_ids[i];
    }
    g_state.sent_account_id_count = g_state.account_id_count;
}

/* Opens the overlay's dialog with what g_state was just given, and for SEND the session and
 * message, for RECV the invitations waiting for the user */
static void OpenOverlayDialog(s32 user_id, const char* session_id, const char* message,
                              const ovl_invdlg_invite_t* invites, int invite_count) {
    ovl_invitation_dialog_param_t shown;
    /* The game's own recipients, as the server takes them: online ids, or account numbers */
    invdlg_online_id_text online_text[ORBIS_INVITATION_DIALOG_ADDRESS_USER_LIST_MAX_SIZE];
    invdlg_account_id_text account_text[ORBIS_INVITATION_DIALOG_ADDRESS_USER_LIST_MAX_SIZE];
    const char* fixed_to[OVL_INVDLG_FIXED];
    u32 i;
    memset(&shown, 0, sizeof(shown));
    shown.mode = g_state.mode;
    shown.user_id = user_id;
    shown.user_editable = g_state.user_editable;
    shown.session_id = session_id;
    shown.message = message;
    for (i = 0; i < g_state.online_id_count && shown.fixed_count < OVL_INVDLG_FIXED; i++) {
        OnlineIdText(&g_state.online_ids[i], online_text[i].text);
        fixed_to[shown.fixed_count++] = online_text[i].text;
    }
    for (i = 0; i < g_state.account_id_count && shown.fixed_count < OVL_INVDLG_FIXED; i++) {
        AccountIdText(g_state.account_ids[i], account_text[i].text);
        fixed_to[shown.fixed_count++] = account_text[i].text;
    }
    shown.fixed_to = fixed_to;
    shown.invites = invites;
    shown.invite_count = invite_count < OVL_INVDLG_INVITES ? invite_count : OVL_INVDLG_INVITES;
    ovl_invitation_dialog_open(&shown);
}

/* RECV: the dialog opened with the invitations waiting, which the NP handler lends */
typedef struct invdlg_recv_open {
    const char* open; /* "Open" or "OpenA", for the log */
    const char* func;
    s32 user_id;
} invdlg_recv_open;

static void OpenRecv(const ovl_invdlg_invite_t* invites, int count, size_t pending, void* ctx) {
    const invdlg_recv_open* r = (const invdlg_recv_open*)ctx;
    c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_INFO, r->func, "%s(RECV) user=%d pending=%u",
          r->open, (int)r->user_id, (unsigned)pending);
    OpenOverlayDialog(r->user_id, NULL, NULL, invites, count);
}

s32 PS4_SYSV_ABI sceInvitationDialogGetResultA(OrbisInvitationDialogResultA* result) {
    c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_DEBUG, "sceInvitationDialogGetResultA",
          "Getting invitation dialog result (async), result ptr=0x%llx",
          (unsigned long long)(size_t)result);

    if (DialogStatus() == COMMON_DIALOG_STATUS_NONE) {
        return (s32)COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    }
    if (DialogStatus() != COMMON_DIALOG_STATUS_FINISHED) {
        return (s32)COMMON_DIALOG_ERROR_NOT_FINISHED;
    }
    if (result == NULL) {
        return (s32)COMMON_DIALOG_ERROR_ARG_NULL;
    }

    TakeResult();
    if (g_state.error_code != 0) {
        c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_ERROR, "sceInvitationDialogGetResultA",
              "errorCode %d block all players", (int)g_state.error_code);
        return ORBIS_INVITATION_DIALOG_ERROR_BLOCKED_ALL_PLAYERS;
    }

    result->callbackArg = g_state.callback_arg;
    result->errorCode = g_state.error_code;
    result->result = g_state.result;
    /* sentUsers carries {onlineId, accountId} pairs; report whichever recipient form was sent --
     * online IDs (picker / non-A fixed list) or account IDs (A fixed list). Only one is
     * populated. */
    if (result->sentUsers != NULL) {
        u32 count = g_state.sent_online_id_count > g_state.sent_account_id_count
                        ? g_state.sent_online_id_count
                        : g_state.sent_account_id_count;
        u32 i;
        if (count > ORBIS_INVITATION_DIALOG_ADDRESS_USER_LIST_MAX_SIZE) {
            count = ORBIS_INVITATION_DIALOG_ADDRESS_USER_LIST_MAX_SIZE;
        }
        for (i = 0; i < count; i++) {
            memset(&result->sentUsers->users[i], 0, sizeof(result->sentUsers->users[i]));
            if (i < g_state.sent_online_id_count) {
                strncpy(result->sentUsers->users[i].onlineId.data, g_state.sent_online_ids[i].text,
                        sizeof(result->sentUsers->users[i].onlineId.data) - 1);
            }
            if (i < g_state.sent_account_id_count) {
                result->sentUsers->users[i].accountId = g_state.sent_account_ids[i];
            }
        }
        result->sentUsers->count = (g_state.result == COMMON_DIALOG_RESULT_OK) ? count : 0;
    }
    return (s32)g_state.result;
}

/* The game's data, logged as it opens the dialog */
static void LogSendInfo(const char* func, const char* user_message, const OrbisNpSessionId* sid,
                        s32 address_type) {
    c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_INFO, func, "SendInfo:");
    if (user_message) {
        c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_INFO, func, "  userMessage: '%s'",
              user_message);
    }
    if (sid) {
        c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_INFO, func, "  sessionId: %s", sid->data);
    }
    /* Log address parameters */
    c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_INFO, func, "  addressType=%d",
          (int)address_type);
}

u32 PS4_SYSV_ABI sceInvitationDialogOpenA(const OrbisInvitationDialogParamA* param) {
    const char* session_id = NULL;
    const char* message = NULL;
    c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_DEBUG, "sceInvitationDialogOpenA",
          "sceInvitationDialogOpenA() called (async version)");
    c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_INFO, "sceInvitationDialogOpenA",
          "Opening invitation dialog asynchronously, param ptr=0x%llx",
          (unsigned long long)(size_t)param);

    if (!param) {
        c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_ERROR, "sceInvitationDialogOpenA",
              "Parameter is NULL");
        return COMMON_DIALOG_ERROR_ARG_NULL;
    }

    c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_INFO, "sceInvitationDialogOpenA",
          "ParamA size=0x%x", (unsigned)param->size);
    c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_INFO, "sceInvitationDialogOpenA",
          "ParamA mode=%d", (int)param->mode);

    /* Validate mode */
    if (param->mode != ORBIS_INVITATION_DIALOG_MODE_SEND &&
        param->mode != ORBIS_INVITATION_DIALOG_MODE_RECV) {
        c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_ERROR, "sceInvitationDialogOpenA",
              "Invalid mode: %d (expected SEND=%d or RECV=%d)", (int)param->mode,
              ORBIS_INVITATION_DIALOG_MODE_SEND, ORBIS_INVITATION_DIALOG_MODE_RECV);
        return COMMON_DIALOG_ERROR_PARAM_INVALID;
    }

    c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_INFO, "sceInvitationDialogOpenA",
          "ParamA userId=0x%x", (unsigned)param->userId);
    c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_INFO, "sceInvitationDialogOpenA",
          "ParamA callbackArg=0x%llx", (unsigned long long)(size_t)param->callbackArg);
    c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_INFO, "sceInvitationDialogOpenA",
          "ParamA dataParam=0x%llx", (unsigned long long)(size_t)param->dataParam);

    /* Log data parameters if present */
    if (param->dataParam) {
        c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_INFO, "sceInvitationDialogOpenA",
              "DataParamA pointer: 0x%llx", (unsigned long long)(size_t)param->dataParam);
        if (param->mode == ORBIS_INVITATION_DIALOG_MODE_SEND) {
            const OrbisInvitationDialogAddressParamA* addressParam =
                &param->dataParam->SendInfo.addressParam;
            LogSendInfo("sceInvitationDialogOpenA", param->dataParam->SendInfo.userMessage,
                        param->dataParam->SendInfo.sessionId, addressParam->addressType);
            if (addressParam->addressType == ORBIS_INVITATION_DIALOG_ADDRESS_TYPE_USERDISABLE) {
                c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_INFO, "sceInvitationDialogOpenA",
                      "  UserSelectDisableAddress:");
                c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_INFO, "sceInvitationDialogOpenA",
                      "    accountIds=0x%llx",
                      (unsigned long long)(size_t)
                          addressParam->addressInfo.UserSelectDisableAddress.accountIds);
                c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_INFO, "sceInvitationDialogOpenA",
                      "    accountIdsCount=%u",
                      (unsigned)addressParam->addressInfo.UserSelectDisableAddress.accountIdsCount);
            } else if (addressParam->addressType ==
                       ORBIS_INVITATION_DIALOG_ADDRESS_TYPE_USERENABLE) {
                c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_INFO, "sceInvitationDialogOpenA",
                      "  UserSelectEnableAddress:");
                c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_INFO, "sceInvitationDialogOpenA",
                      "    userMaxCount=%u",
                      (unsigned)addressParam->addressInfo.UserSelectEnableAddress.userMaxCount);
            } else {
                c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_WARNING, "sceInvitationDialogOpenA",
                      "  Invalid addressType: %d", (int)addressParam->addressType);
            }
        } else if (param->mode == ORBIS_INVITATION_DIALOG_MODE_RECV) {
            c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_INFO, "sceInvitationDialogOpenA",
                  "RecvInfo (receive mode - no additional data)");
        }
    }

    /* Check initialization state */
    if (DialogStatus() != COMMON_DIALOG_STATUS_INITIALIZED &&
        DialogStatus() != COMMON_DIALOG_STATUS_FINISHED) {
        c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_INFO, "sceInvitationDialogOpenA",
              "Called without proper initialization (status=%s)", StatusName());
        return COMMON_DIALOG_ERROR_INVALID_STATE;
    }

    ResetState();
    g_state.mode = param->mode;
    g_state.callback_arg = param->callbackArg;

    if (param->mode == ORBIS_INVITATION_DIALOG_MODE_SEND) {
        if (param->dataParam == NULL) {
            return COMMON_DIALOG_ERROR_PARAM_INVALID;
        }
        if (param->dataParam->SendInfo.sessionId != NULL) {
            session_id = param->dataParam->SendInfo.sessionId->data;
        }
        if (param->dataParam->SendInfo.userMessage != NULL) {
            message = param->dataParam->SendInfo.userMessage;
        }
        /* USERDISABLE: the app fixes the recipient list up front (account IDs). USERENABLE: the
         * user picks them in the dialog via the searchable friend picker. */
        if (param->dataParam->SendInfo.addressParam.addressType ==
            ORBIS_INVITATION_DIALOG_ADDRESS_TYPE_USERENABLE) {
            g_state.user_editable = true;
        } else if (param->dataParam->SendInfo.addressParam.addressType ==
                   ORBIS_INVITATION_DIALOG_ADDRESS_TYPE_USERDISABLE) {
            const np_account_id_t* ids = param->dataParam->SendInfo.addressParam.addressInfo
                                             .UserSelectDisableAddress.accountIds;
            u32 count = param->dataParam->SendInfo.addressParam.addressInfo.UserSelectDisableAddress
                            .accountIdsCount;
            u32 i;
            if (count > ORBIS_INVITATION_DIALOG_ADDRESS_USER_LIST_MAX_SIZE) {
                count = ORBIS_INVITATION_DIALOG_ADDRESS_USER_LIST_MAX_SIZE;
            }
            for (i = 0; i < count && ids != NULL; i++) {
                g_state.account_ids[g_state.account_id_count++] = ids[i];
            }
        }
        c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_INFO, "sceInvitationDialogOpenA",
              "OpenA(SEND) user=%d sessionId='%s' recipients=%u", (int)param->userId,
              session_id ? session_id : "", (unsigned)g_state.account_id_count);
        OpenOverlayDialog(param->userId, session_id, message, NULL, 0);
    } else {
        invdlg_recv_open r;
        r.open = "OpenA";
        r.func = "sceInvitationDialogOpenA";
        r.user_id = param->userId;
        np_lend_pending_invitations(param->userId, OpenRecv, &r);
    }
    return COMMON_DIALOG_ERROR_OK;
}

u32 PS4_SYSV_ABI sceInvitationDialogOpen(const OrbisInvitationDialogParam* param) {
    const char* session_id = NULL;
    const char* message = NULL;
    c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_DEBUG, "sceInvitationDialogOpen",
          "Opening invitation dialog, param ptr=0x%llx", (unsigned long long)(size_t)param);

    if (DialogStatus() != COMMON_DIALOG_STATUS_INITIALIZED &&
        DialogStatus() != COMMON_DIALOG_STATUS_FINISHED) {
        c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_ERROR, "sceInvitationDialogOpen",
              "called without initialize");
        return COMMON_DIALOG_ERROR_INVALID_STATE;
    }

    if (!param) {
        c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_ERROR, "sceInvitationDialogOpen",
              "Parameter is NULL");
        return COMMON_DIALOG_ERROR_ARG_NULL;
    }

    c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_INFO, "sceInvitationDialogOpen",
          "size=0x%x mode=%d userId=%d Param callbackArg=0x%llx Param dataParam=0x%llx",
          (unsigned)param->size, (int)param->mode, (int)param->userId,
          (unsigned long long)(size_t)param->callbackArg,
          (unsigned long long)(size_t)param->dataParam);

    /* Validate mode */
    if (param->mode != ORBIS_INVITATION_DIALOG_MODE_SEND &&
        param->mode != ORBIS_INVITATION_DIALOG_MODE_RECV) {
        c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_ERROR, "sceInvitationDialogOpen",
              "Invalid mode: %d (expected SEND=%d or RECV=%d)", (int)param->mode,
              ORBIS_INVITATION_DIALOG_MODE_SEND, ORBIS_INVITATION_DIALOG_MODE_RECV);
        return COMMON_DIALOG_ERROR_PARAM_INVALID;
    }

    /* Log data parameters if present */
    if (param->dataParam) {
        c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_INFO, "sceInvitationDialogOpen",
              "DataParam pointer: 0x%llx", (unsigned long long)(size_t)param->dataParam);
        if (param->mode == ORBIS_INVITATION_DIALOG_MODE_SEND) {
            const OrbisInvitationDialogAddressParam* addressParam =
                &param->dataParam->SendInfo.addressParam;
            LogSendInfo("sceInvitationDialogOpen", param->dataParam->SendInfo.userMessage,
                        param->dataParam->SendInfo.sessionId, addressParam->addressType);
            if (addressParam->addressType == ORBIS_INVITATION_DIALOG_ADDRESS_TYPE_USERDISABLE) {
                c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_INFO, "sceInvitationDialogOpen",
                      "  UserSelectDisableAddress:");
                c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_INFO, "sceInvitationDialogOpen",
                      "    onlineIds=0x%llx",
                      (unsigned long long)(size_t)
                          addressParam->addressInfo.UserSelectDisableAddress.onlineIds);
                c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_INFO, "sceInvitationDialogOpen",
                      "    onlineIdsCount=%u",
                      (unsigned)addressParam->addressInfo.UserSelectDisableAddress.onlineIdsCount);
            } else if (addressParam->addressType ==
                       ORBIS_INVITATION_DIALOG_ADDRESS_TYPE_USERENABLE) {
                c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_INFO, "sceInvitationDialogOpen",
                      "  UserSelectEnableAddress:");
                c_log(
                    C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_INFO, "sceInvitationDialogOpen",
                    "    onlineIdsMaxCount=%u",
                    (unsigned)addressParam->addressInfo.UserSelectEnableAddress.onlineIdsMaxCount);
            } else {
                c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_WARNING, "sceInvitationDialogOpen",
                      "  Invalid addressType: %d", (int)addressParam->addressType);
            }
        } else if (param->mode == ORBIS_INVITATION_DIALOG_MODE_RECV) {
            c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_INFO, "sceInvitationDialogOpen",
                  "RecvInfo (receive mode - no additional data)");
        }
    }

    ResetState();
    g_state.mode = param->mode;
    g_state.callback_arg = param->callbackArg;

    if (param->mode == ORBIS_INVITATION_DIALOG_MODE_SEND) {
        if (param->dataParam == NULL) {
            return COMMON_DIALOG_ERROR_PARAM_INVALID;
        }
        if (param->dataParam->SendInfo.sessionId != NULL) {
            session_id = param->dataParam->SendInfo.sessionId->data;
        }
        if (param->dataParam->SendInfo.userMessage != NULL) {
            message = param->dataParam->SendInfo.userMessage;
        }
        /* USERDISABLE: the app fixes the recipient list up front. USERENABLE: the user picks
         * them in the dialog via the searchable friend picker. */
        if (param->dataParam->SendInfo.addressParam.addressType ==
            ORBIS_INVITATION_DIALOG_ADDRESS_TYPE_USERENABLE) {
            g_state.user_editable = true;
        } else if (param->dataParam->SendInfo.addressParam.addressType ==
                   ORBIS_INVITATION_DIALOG_ADDRESS_TYPE_USERDISABLE) {
            const np_online_id_t* ids = param->dataParam->SendInfo.addressParam.addressInfo
                                            .UserSelectDisableAddress.onlineIds;
            u32 count = param->dataParam->SendInfo.addressParam.addressInfo.UserSelectDisableAddress
                            .onlineIdsCount;
            if (count > ORBIS_INVITATION_DIALOG_ADDRESS_USER_LIST_MAX_SIZE) {
                count = ORBIS_INVITATION_DIALOG_ADDRESS_USER_LIST_MAX_SIZE;
            }
            if (count != 0 && ids != NULL) {
                memcpy(g_state.online_ids, ids, count * sizeof(ids[0]));
                g_state.online_id_count = count;
            }
        }
        c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_INFO, "sceInvitationDialogOpen",
              "Open(SEND) user=%d sessionId='%s' recipients=%u", (int)param->userId,
              session_id ? session_id : "", (unsigned)g_state.online_id_count);
        OpenOverlayDialog(param->userId, session_id, message, NULL, 0);
    } else {
        invdlg_recv_open r;
        r.open = "Open";
        r.func = "sceInvitationDialogOpen";
        r.user_id = param->userId;
        np_lend_pending_invitations(param->userId, OpenRecv, &r);
    }
    return COMMON_DIALOG_ERROR_OK;
}

s32 PS4_SYSV_ABI sceInvitationDialogGetResult(OrbisInvitationDialogResult* result) {
    c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_DEBUG, "sceInvitationDialogGetResult",
          "Getting invitation dialog result, result ptr=0x%llx",
          (unsigned long long)(size_t)result);

    if (DialogStatus() == COMMON_DIALOG_STATUS_NONE) {
        return (s32)COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    }
    if (DialogStatus() != COMMON_DIALOG_STATUS_FINISHED) {
        return (s32)COMMON_DIALOG_ERROR_NOT_FINISHED;
    }
    if (result == NULL) {
        return (s32)COMMON_DIALOG_ERROR_ARG_NULL;
    }

    TakeResult();
    if (g_state.error_code != 0) {
        c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_ERROR, "sceInvitationDialogGetResult",
              "errorCode %d block all players", (int)g_state.error_code);
        return ORBIS_INVITATION_DIALOG_ERROR_BLOCKED_ALL_PLAYERS;
    }

    /* Log result fields if they contain data */
    if (result->callbackArg) {
        c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_INFO, "sceInvitationDialogGetResult",
              "Result callbackArg=0x%llx", (unsigned long long)(size_t)result->callbackArg);
    }
    c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_INFO, "sceInvitationDialogGetResult",
          "Result errorCode=0x%x", (unsigned)result->errorCode);
    c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_INFO, "sceInvitationDialogGetResult",
          "Result status=%s",
          enum_name(&enum_names_Libraries_CommonDialog_Result, (s64)result->result));

    if (result->sentOnlineIds) {
        u32 i;
        c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_INFO, "sceInvitationDialogGetResult",
              "Result sentOnlineIds count=%u", (unsigned)result->sentOnlineIds->count);
        for (i = 0; i < result->sentOnlineIds->count &&
                    i < ORBIS_INVITATION_DIALOG_ADDRESS_USER_LIST_MAX_SIZE;
             i++) {
            c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_INFO, "sceInvitationDialogGetResult",
                  "  sentOnlineIds[%u]: %s", (unsigned)i, result->sentOnlineIds->onlineId[i].data);
        }
    }
    result->callbackArg = g_state.callback_arg;
    result->errorCode = g_state.error_code;
    result->result = g_state.result;
    if (result->sentOnlineIds != NULL) {
        u32 count = g_state.sent_online_id_count;
        u32 i;
        if (count > ORBIS_INVITATION_DIALOG_ADDRESS_USER_LIST_MAX_SIZE) {
            count = ORBIS_INVITATION_DIALOG_ADDRESS_USER_LIST_MAX_SIZE;
        }
        for (i = 0; i < count; i++) {
            np_online_id_t* oid = &result->sentOnlineIds->onlineId[i];
            memset(oid, 0, sizeof(*oid));
            strncpy(oid->data, g_state.sent_online_ids[i].text, sizeof(oid->data) - 1);
        }
        result->sentOnlineIds->count = (g_state.result == COMMON_DIALOG_RESULT_OK) ? count : 0;
    }
    return (s32)g_state.result;
}

u32 PS4_SYSV_ABI sceInvitationDialogUpdateStatus(void) {
    c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_DEBUG, "sceInvitationDialogUpdateStatus",
          "UpdateStatus called, current status=%s", StatusName());
    return DialogStatus();
}

u32 PS4_SYSV_ABI sceInvitationDialogClose(void) {
    c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_DEBUG, "sceInvitationDialogClose",
          "Closing invitation dialog, current status=%s", StatusName());

    if (DialogStatus() != COMMON_DIALOG_STATUS_RUNNING) {
        c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_WARNING, "sceInvitationDialogClose",
              "Cannot close dialog: not running (status=%s)", StatusName());
        return COMMON_DIALOG_ERROR_NOT_RUNNING;
    }

    /* Closed on screen at the same moment: finished already, by the player */
    if (!ovl_invitation_dialog_close(OVL_INVDLG_RESULT_OK)) {
        return COMMON_DIALOG_ERROR_NOT_RUNNING;
    }
    return COMMON_DIALOG_ERROR_OK;
}

u32 PS4_SYSV_ABI sceInvitationDialogTerminate(void) {
    c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_DEBUG, "sceInvitationDialogTerminate",
          "Terminating invitation dialog system, current status=%s", StatusName());

    if (DialogStatus() == COMMON_DIALOG_STATUS_RUNNING) {
        c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_DEBUG, "sceInvitationDialogTerminate",
              "Dialog is running, closing before termination");
        sceInvitationDialogClose();
    }

    if (DialogStatus() == COMMON_DIALOG_STATUS_NONE) {
        c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_ERROR, "sceInvitationDialogTerminate",
              "Cannot terminate: not initialized");
        return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    }

    ovl_invitation_dialog_set_status((int)COMMON_DIALOG_STATUS_NONE);
    g_common_dialog_used = false;
    c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_DEBUG, "sceInvitationDialogTerminate",
          "Invitation dialog terminated successfully");
    return COMMON_DIALOG_ERROR_OK;
}

u32 PS4_SYSV_ABI sceInvitationDialogGetStatus(void) {
    c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_DEBUG, "sceInvitationDialogGetStatus",
          "Getting invitation dialog status: %s", StatusName());
    return DialogStatus();
}

u32 PS4_SYSV_ABI sceInvitationDialogInitialize(void) {
    c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_DEBUG, "sceInvitationDialogInitialize",
          "called");
    if (!g_common_dialog_initialized) {
        c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_ERROR, "sceInvitationDialogInitialize",
              "CommonDialog system not initialized");
        return COMMON_DIALOG_ERROR_NOT_SYSTEM_INITIALIZED;
    }

    if (DialogStatus() != COMMON_DIALOG_STATUS_NONE) {
        c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_ERROR, "sceInvitationDialogInitialize",
              "Invitation dialog already initialized (status=%s)", StatusName());
        return COMMON_DIALOG_ERROR_ALREADY_INITIALIZED;
    }

    if (g_common_dialog_used) {
        c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_ERROR, "sceInvitationDialogInitialize",
              "CommonDialog already in use");
        return COMMON_DIALOG_ERROR_BUSY;
    }

    ovl_invitation_dialog_set_status((int)COMMON_DIALOG_STATUS_INITIALIZED);
    g_common_dialog_used = true;
    c_log(C_LOG_LIB_INVITATION_DIALOG, C_LOG_LEVEL_INFO, "sceInvitationDialogInitialize",
          "Invitation dialog initialized successfully");
    return COMMON_DIALOG_ERROR_OK;
}
