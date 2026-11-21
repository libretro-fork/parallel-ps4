/* SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <boolean.h>
#include <stddef.h>
#include <string.h>

#include "common/c_bridge.h"
#include "core/libraries/companion/companion_error.h"
#include "core/libraries/companion/companion_httpd.h"
#include "core/libraries/error_codes.h"

s32 PS4_SYSV_ABI sceCompanionHttpdAddHeader(const char* key, const char* value,
                                            OrbisCompanionHttpdResponse* response) {
    c_log(C_LOG_LIB_COMPANION_HTTPD, C_LOG_LEVEL_ERROR, "sceCompanionHttpdAddHeader",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCompanionHttpdGet2ndScreenStatus(s32 userId) {
    c_log(C_LOG_LIB_COMPANION_HTTPD, C_LOG_LEVEL_ERROR, "sceCompanionHttpdGet2ndScreenStatus",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCompanionHttpdGetEvent(OrbisCompanionHttpdEvent* pEvent) {
    pEvent->event = ORBIS_COMPANION_HTTPD_EVENT_DISCONNECT; /* disconnected */
    c_log(C_LOG_LIB_COMPANION_HTTPD, C_LOG_LEVEL_DEBUG, "sceCompanionHttpdGetEvent",
          "device disconnected");
    return ORBIS_COMPANION_HTTPD_ERROR_NO_EVENT; /* No events to obtain */
}

s32 PS4_SYSV_ABI sceCompanionHttpdGetUserId(u32 addr, s32* userId) {
    c_log(C_LOG_LIB_COMPANION_HTTPD, C_LOG_LEVEL_ERROR, "sceCompanionHttpdGetUserId",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCompanionHttpdInitialize(void) {
    c_log(C_LOG_LIB_COMPANION_HTTPD, C_LOG_LEVEL_ERROR, "sceCompanionHttpdInitialize",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCompanionHttpdInitialize2(void) {
    c_log(C_LOG_LIB_COMPANION_HTTPD, C_LOG_LEVEL_ERROR, "sceCompanionHttpdInitialize2",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCompanionHttpdOptParamInitialize(void) {
    c_log(C_LOG_LIB_COMPANION_HTTPD, C_LOG_LEVEL_ERROR, "sceCompanionHttpdOptParamInitialize",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCompanionHttpdRegisterRequestBodyReceptionCallback(
    OrbisCompanionHttpdRequestBodyReceptionCallback function, void* param) {
    c_log(C_LOG_LIB_COMPANION_HTTPD, C_LOG_LEVEL_ERROR,
          "sceCompanionHttpdRegisterRequestBodyReceptionCallback", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI
sceCompanionHttpdRegisterRequestCallback(OrbisCompanionHttpdRequestCallback function, void* param) {
    c_log(C_LOG_LIB_COMPANION_HTTPD, C_LOG_LEVEL_ERROR, "sceCompanionHttpdRegisterRequestCallback",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCompanionHttpdRegisterRequestCallback2(
    OrbisCompanionHttpdRequestCallback function, void* param) {
    c_log(C_LOG_LIB_COMPANION_HTTPD, C_LOG_LEVEL_ERROR, "sceCompanionHttpdRegisterRequestCallback2",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCompanionHttpdSetBody(const char* body, u64 bodySize,
                                          OrbisCompanionHttpdResponse* response) {
    c_log(C_LOG_LIB_COMPANION_HTTPD, C_LOG_LEVEL_ERROR, "sceCompanionHttpdSetBody",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCompanionHttpdSetStatus(s32 status, OrbisCompanionHttpdResponse* response) {
    c_log(C_LOG_LIB_COMPANION_HTTPD, C_LOG_LEVEL_ERROR, "sceCompanionHttpdSetStatus",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCompanionHttpdStart(void) {
    c_log(C_LOG_LIB_COMPANION_HTTPD, C_LOG_LEVEL_ERROR, "sceCompanionHttpdStart",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCompanionHttpdStop(void) {
    c_log(C_LOG_LIB_COMPANION_HTTPD, C_LOG_LEVEL_ERROR, "sceCompanionHttpdStop",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCompanionHttpdTerminate(void) {
    c_log(C_LOG_LIB_COMPANION_HTTPD, C_LOG_LEVEL_ERROR, "sceCompanionHttpdTerminate",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCompanionHttpdUnregisterRequestBodyReceptionCallback(void) {
    c_log(C_LOG_LIB_COMPANION_HTTPD, C_LOG_LEVEL_ERROR,
          "sceCompanionHttpdUnregisterRequestBodyReceptionCallback", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCompanionHttpdUnregisterRequestCallback(void) {
    c_log(C_LOG_LIB_COMPANION_HTTPD, C_LOG_LEVEL_ERROR,
          "sceCompanionHttpdUnregisterRequestCallback", "(STUBBED) called");
    return ORBIS_OK;
}
