/* SPDX-FileCopyrightText: Copyright 2025-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* libSceNpParty: no one is in a party here. The functions that ask about one say so; the rest
 * do nothing. */

#ifndef SHADPS4_LIBRARIES_NP_PARTY_H
#define SHADPS4_LIBRARIES_NP_PARTY_H

#include "common/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* OrbisNpPartyState */
#define ORBIS_NP_PARTY_STATE_NOT_IN_PARTY 2
#define ORBIS_NP_PARTY_STATE_IN_PRIVATE_PARTY 3
typedef u16 OrbisNpPartyState;

s32 PS4_SYSV_ABI sceNpPartyCheckCallback(void);
s32 PS4_SYSV_ABI sceNpPartyCreate(void);
s32 PS4_SYSV_ABI sceNpPartyCreateA(void);
s32 PS4_SYSV_ABI sceNpPartyGetId(void);
s32 PS4_SYSV_ABI sceNpPartyGetMemberInfo(void);
s32 PS4_SYSV_ABI sceNpPartyGetMemberInfoA(void);
s32 PS4_SYSV_ABI sceNpPartyGetMembers(void);
s32 PS4_SYSV_ABI sceNpPartyGetMembersA(void);
s32 PS4_SYSV_ABI sceNpPartyGetMemberSessionInfo(void);
s32 PS4_SYSV_ABI sceNpPartyGetMemberVoiceInfo(void);
s32 PS4_SYSV_ABI sceNpPartyGetState(OrbisNpPartyState* state);
s32 PS4_SYSV_ABI sceNpPartyGetStateAsUser(void);
s32 PS4_SYSV_ABI sceNpPartyGetStateAsUserA(void);
s32 PS4_SYSV_ABI sceNpPartyGetVoiceChatPriority(void);
s32 PS4_SYSV_ABI sceNpPartyInitialize(void);
s32 PS4_SYSV_ABI sceNpPartyJoin(void);
s32 PS4_SYSV_ABI sceNpPartyLeave(void);
s32 PS4_SYSV_ABI sceNpPartyRegisterHandler(void);
s32 PS4_SYSV_ABI sceNpPartyRegisterHandlerA(void);
s32 PS4_SYSV_ABI sceNpPartyRegisterPrivateHandler(void);
s32 PS4_SYSV_ABI sceNpPartySendBinaryMessage(void);
s32 PS4_SYSV_ABI sceNpPartySetVoiceChatPriority(void);
s32 PS4_SYSV_ABI sceNpPartyShowInvitationList(void);
s32 PS4_SYSV_ABI sceNpPartyShowInvitationListA(void);
s32 PS4_SYSV_ABI sceNpPartyTerminate(void);
s32 PS4_SYSV_ABI sceNpPartyUnregisterPrivateHandler(void);

#ifdef __cplusplus
}
#endif

#endif
