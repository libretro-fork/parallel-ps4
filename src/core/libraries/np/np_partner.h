/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* libSceNpPartner001: EA Access. No one is signed in to it here, so no one has a
 * subscription. */

#ifndef SHADPS4_LIBRARIES_NP_PARTNER_H
#define SHADPS4_LIBRARIES_NP_PARTNER_H

#include <boolean.h>

#include "common/types.h"
#include "core/libraries/system/userservice.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ORBIS_NP_PARTNER_ERROR_NOT_INITIALIZED ((s32)0x819d0001)
#define ORBIS_NP_PARTNER_ERROR_INVALID_ARGUMENT ((s32)0x819d0002)

s32 PS4_SYSV_ABI sceNpEAAccessInitialize(void);
s32 PS4_SYSV_ABI sceNpEAAccessTerminate(void);
s32 PS4_SYSV_ABI sceNpHasEAAccessSubscription(OrbisUserServiceUserId user_id, bool* result);
s32 PS4_SYSV_ABI sceNpHasEAAccessSubscriptionAbortRequest(void);

#ifdef __cplusplus
}
#endif

#endif
