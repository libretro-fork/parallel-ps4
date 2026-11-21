/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_INVITATION_DIALOG_H
#define SHADPS4_LIBRARIES_INVITATION_DIALOG_H

/* libSceInvitationDialog: the dialog is the overlay's (video_core/overlay/ovl_invdialog.c).
 * Defined in invitation_dialog.c, registered in libs.cpp; what the overlay asks of the NP
 * handler is in np_handler_invitation.cpp. */

#include <stddef.h>

#include <retro_common_api.h>

#include "common/types.h"
#include "core/libraries/np/np_types_c.h"
#include "core/libraries/system/commondialog_c.h"

RETRO_BEGIN_DECLS

#define ORBIS_INVITATION_DIALOG_ADDRESS_USER_LIST_MAX_SIZE 16
#define ORBIS_INVITATION_DIALOG_MAX_USER_MSG_SIZE 512
#define ORBIS_NP_SESSION_ID_MAX_SIZE 45

typedef struct OrbisInvitationDialogUserList {
    u32 count;
    struct {
        np_online_id_t onlineId;
        np_account_id_t accountId;
    } users[ORBIS_INVITATION_DIALOG_ADDRESS_USER_LIST_MAX_SIZE];
} OrbisInvitationDialogUserList;

typedef struct SceInvitationDialogOnlineIdList {
    u32 count;
    np_online_id_t onlineId[ORBIS_INVITATION_DIALOG_ADDRESS_USER_LIST_MAX_SIZE];
} SceInvitationDialogOnlineIdList;

/* if errorCode != 0 */
#define ORBIS_INVITATION_DIALOG_ERROR_BLOCKED_ALL_PLAYERS ((s32)0x810A0001)

typedef struct OrbisInvitationDialogResultA {
    void* callbackArg;
    s32 errorCode;
    u32 result; /* COMMON_DIALOG_RESULT_* */
    OrbisInvitationDialogUserList* sentUsers;
    u8 reserved[32];
} OrbisInvitationDialogResultA;

typedef struct OrbisInvitationDialogResult {
    void* callbackArg;
    s32 errorCode;
    u32 result; /* COMMON_DIALOG_RESULT_* */
    SceInvitationDialogOnlineIdList* sentOnlineIds;
    u8 reserved[32];
} OrbisInvitationDialogResult;

/* dialog mode */
#define ORBIS_INVITATION_DIALOG_MODE_INVALID 0 /* Invalid mode / initial value */
#define ORBIS_INVITATION_DIALOG_MODE_SEND 1    /* Send mode */
#define ORBIS_INVITATION_DIALOG_MODE_RECV 2    /* Receive mode */

typedef union OrbisInvitationDialogAddressInfoA {
    struct {
        const np_account_id_t* accountIds;
        u32 accountIdsCount;
        u32 pad;
    } UserSelectDisableAddress;
    struct {
        u32 userMaxCount;
    } UserSelectEnableAddress;
} OrbisInvitationDialogAddressInfoA;

typedef union OrbisInvitationDialogAddressInfo {
    struct {
        const np_online_id_t* onlineIds;
        u32 onlineIdsCount;
    } UserSelectDisableAddress;
    struct {
        u32 onlineIdsMaxCount;
    } UserSelectEnableAddress;
} OrbisInvitationDialogAddressInfo;

/* addressType */
#define ORBIS_INVITATION_DIALOG_ADDRESS_TYPE_INVALID 0     /* Invalid mode / initial value */
#define ORBIS_INVITATION_DIALOG_ADDRESS_TYPE_USERDISABLE 1 /* Recipient list editing disabled */
#define ORBIS_INVITATION_DIALOG_ADDRESS_TYPE_USERENABLE 2  /* Recipient list editing enabled */

typedef struct OrbisInvitationDialogAddressParam {
    s32 addressType;
    OrbisInvitationDialogAddressInfo addressInfo;
} OrbisInvitationDialogAddressParam;

typedef struct OrbisInvitationDialogAddressParamA {
    s32 addressType;
    u32 pad;
    OrbisInvitationDialogAddressInfoA addressInfo;
} OrbisInvitationDialogAddressParamA;

typedef struct OrbisNpSessionId {
    char data[ORBIS_NP_SESSION_ID_MAX_SIZE];
    char term;
    char padding[2];
} OrbisNpSessionId;

#define ORBIS_NP_INVITATION_ID_SIZE 60

typedef struct OrbisNpInvitationId {
    char data[ORBIS_NP_INVITATION_ID_SIZE];
    char term;
} OrbisNpInvitationId;

typedef s32 OrbisNpSessionInvitationEventFlag;
#define ORBIS_NP_SESSION_INVITATION_EVENT_FLAG_INVITATION 0x01

/* Delivered via SceSystemServiceEvent::data::param on ORBIS_SYSTEM_SERVICE_EVENT_SESSION_INVITATION
 */
typedef struct OrbisNpSessionInvitationEventParam {
    OrbisNpSessionId sessionId;
    OrbisNpInvitationId invitationId;
    OrbisNpSessionInvitationEventFlag flag;
    char padding[4];
    np_online_id_t onlineId;           /* accepting (local) user */
    s32 userId;                        /* accepting (local) user: OrbisUserServiceUserId */
    np_online_id_t referralOnlineId;   /* inviter */
    np_account_id_t referralAccountId; /* inviter */
} OrbisNpSessionInvitationEventParam;

typedef char invitation_event_flag_at_112[offsetof(OrbisNpSessionInvitationEventParam, flag) == 112
                                              ? 1
                                              : -1];
typedef char invitation_event_online_id_at_120
    [offsetof(OrbisNpSessionInvitationEventParam, onlineId) == 120 ? 1 : -1];
typedef char invitation_event_user_id_at_140
    [offsetof(OrbisNpSessionInvitationEventParam, userId) == 140 ? 1 : -1];
typedef char invitation_event_referral_online_id_at_144
    [offsetof(OrbisNpSessionInvitationEventParam, referralOnlineId) == 144 ? 1 : -1];
typedef char invitation_event_referral_account_id_at_168
    [offsetof(OrbisNpSessionInvitationEventParam, referralAccountId) == 168 ? 1 : -1];
/* SESSION_INVITATION param must be 176 bytes */
typedef char
    invitation_event_size_check[sizeof(OrbisNpSessionInvitationEventParam) == 176 ? 1 : -1];

typedef union OrbisInvitationDialogDataParam {
    struct {
        const char* userMessage;
        const OrbisNpSessionId* sessionId;
        OrbisInvitationDialogAddressParam addressParam;
    } SendInfo;
    struct {
        u8 reserved[64];
    } RecvInfo;
} OrbisInvitationDialogDataParam;

typedef union OrbisInvitationDialogDataParamA {
    struct {
        const char* userMessage;
        const OrbisNpSessionId* sessionId;
        OrbisInvitationDialogAddressParamA addressParam;
    } SendInfo;
    struct {
        u8 reserved[64];
    } RecvInfo;
} OrbisInvitationDialogDataParamA;

typedef struct OrbisInvitationDialogParam {
    common_dialog_base_param baseParam;
    u32 size;
    s32 mode;
    s32 userId; /* OrbisUserServiceUserId */
    void* callbackArg;
    OrbisInvitationDialogDataParam* dataParam;
    u8 reserved[64];
} OrbisInvitationDialogParam;

typedef struct OrbisInvitationDialogParamA {
    common_dialog_base_param baseParam;
    u32 size;
    s32 mode;
    s32 userId; /* OrbisUserServiceUserId */
    u32 pad;
    void* callbackArg;
    const OrbisInvitationDialogDataParamA* dataParam;
    u8 reserved[64];
} OrbisInvitationDialogParamA;

/* The errors are COMMON_DIALOG_ERROR_*, the statuses COMMON_DIALOG_STATUS_* */
u32 PS4_SYSV_ABI sceInvitationDialogClose(void);
s32 PS4_SYSV_ABI sceInvitationDialogGetResult(OrbisInvitationDialogResult* result);
s32 PS4_SYSV_ABI sceInvitationDialogGetResultA(OrbisInvitationDialogResultA* result);
u32 PS4_SYSV_ABI sceInvitationDialogGetStatus(void);
u32 PS4_SYSV_ABI sceInvitationDialogInitialize(void);
u32 PS4_SYSV_ABI sceInvitationDialogOpen(const OrbisInvitationDialogParam* param);
u32 PS4_SYSV_ABI sceInvitationDialogOpenA(const OrbisInvitationDialogParamA* param);
u32 PS4_SYSV_ABI sceInvitationDialogTerminate(void);
u32 PS4_SYSV_ABI sceInvitationDialogUpdateStatus(void);

RETRO_END_DECLS

#endif
