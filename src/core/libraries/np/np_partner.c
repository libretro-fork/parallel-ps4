/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <retro_atomic.h>

#include "common/c_bridge.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/np/np_partner.h"

#define LOG_CLASS C_LOG_LIB_NP_PARTNER

/* Set by Initialize, cleared by Terminate; read from any of the game's threads */
static retro_atomic_int_t library_init;

#define NP_PARTNER_STUB_LOG(func) c_log(LOG_CLASS, C_LOG_LEVEL_ERROR, func, "(STUBBED) called")

s32 PS4_SYSV_ABI sceNpEAAccessTerminate(void) {
    NP_PARTNER_STUB_LOG("sceNpEAAccessTerminate");
    if (!retro_atomic_exchange_int(&library_init, 0)) {
        return ORBIS_NP_PARTNER_ERROR_NOT_INITIALIZED;
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNpHasEAAccessSubscriptionAbortRequest(void) {
    NP_PARTNER_STUB_LOG("sceNpHasEAAccessSubscriptionAbortRequest");
    if (!retro_atomic_load_acquire_int(&library_init)) {
        return ORBIS_NP_PARTNER_ERROR_NOT_INITIALIZED;
    }
    /* No request is ever made, so there is none to abort */
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNpEAAccessInitialize(void) {
    NP_PARTNER_STUB_LOG("sceNpEAAccessInitialize");
    /* The console's also sends the SDK version the game was built with to the server */
    retro_atomic_store_release_int(&library_init, 1);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNpHasEAAccessSubscription(OrbisUserServiceUserId user_id, bool* result) {
    (void)user_id;
    NP_PARTNER_STUB_LOG("sceNpHasEAAccessSubscription");
    if (!retro_atomic_load_acquire_int(&library_init)) {
        return ORBIS_NP_PARTNER_ERROR_NOT_INITIALIZED;
    }
    if (!result) {
        return ORBIS_NP_PARTNER_ERROR_INVALID_ARGUMENT;
    }
    /* The console's asks the server and waits for its answer. Signed out of the network, it
     * likely returns an error whose code is not known: no subscription it is. */
    *result = false;
    return ORBIS_OK;
}
