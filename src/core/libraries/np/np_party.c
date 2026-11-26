/* SPDX-FileCopyrightText: Copyright 2025-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "common/c_bridge.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/np/np_party.h"

/* np_error.h's ORBIS_NP_PARTY_ERROR_NOT_IN_PARTY, which is C++ */
#define NP_PARTY_ERROR_NOT_IN_PARTY ((s32)0x80552506)

#define NP_PARTY_STUB(name, result)                                                                \
    s32 PS4_SYSV_ABI name(void) {                                                                  \
        c_log(C_LOG_LIB_NP_PARTY, C_LOG_LEVEL_ERROR, #name, "(STUBBED) called");                   \
        return result;                                                                             \
    }

NP_PARTY_STUB(sceNpPartyCheckCallback, ORBIS_OK)
NP_PARTY_STUB(sceNpPartyCreate, ORBIS_OK)
NP_PARTY_STUB(sceNpPartyCreateA, ORBIS_OK)
NP_PARTY_STUB(sceNpPartyGetId, ORBIS_OK)
NP_PARTY_STUB(sceNpPartyGetMemberInfo, NP_PARTY_ERROR_NOT_IN_PARTY)
NP_PARTY_STUB(sceNpPartyGetMemberInfoA, NP_PARTY_ERROR_NOT_IN_PARTY)
NP_PARTY_STUB(sceNpPartyGetMembers, NP_PARTY_ERROR_NOT_IN_PARTY)
NP_PARTY_STUB(sceNpPartyGetMembersA, NP_PARTY_ERROR_NOT_IN_PARTY)
NP_PARTY_STUB(sceNpPartyGetMemberSessionInfo, ORBIS_OK)
NP_PARTY_STUB(sceNpPartyGetMemberVoiceInfo, ORBIS_OK)

s32 PS4_SYSV_ABI sceNpPartyGetState(OrbisNpPartyState* state) {
    c_log(C_LOG_LIB_NP_PARTY, C_LOG_LEVEL_ERROR, "sceNpPartyGetState", "(STUBBED) called");
    *state = ORBIS_NP_PARTY_STATE_NOT_IN_PARTY;
    return ORBIS_OK;
}

NP_PARTY_STUB(sceNpPartyGetStateAsUser, ORBIS_OK)
NP_PARTY_STUB(sceNpPartyGetStateAsUserA, ORBIS_OK)
NP_PARTY_STUB(sceNpPartyGetVoiceChatPriority, ORBIS_OK)
NP_PARTY_STUB(sceNpPartyInitialize, ORBIS_OK)
NP_PARTY_STUB(sceNpPartyJoin, ORBIS_OK)
NP_PARTY_STUB(sceNpPartyLeave, ORBIS_OK)
NP_PARTY_STUB(sceNpPartyRegisterHandler, ORBIS_OK)
NP_PARTY_STUB(sceNpPartyRegisterHandlerA, ORBIS_OK)
NP_PARTY_STUB(sceNpPartyRegisterPrivateHandler, ORBIS_OK)
NP_PARTY_STUB(sceNpPartySendBinaryMessage, ORBIS_OK)
NP_PARTY_STUB(sceNpPartySetVoiceChatPriority, ORBIS_OK)
NP_PARTY_STUB(sceNpPartyShowInvitationList, ORBIS_OK)
NP_PARTY_STUB(sceNpPartyShowInvitationListA, ORBIS_OK)
NP_PARTY_STUB(sceNpPartyTerminate, ORBIS_OK)
NP_PARTY_STUB(sceNpPartyUnregisterPrivateHandler, ORBIS_OK)
