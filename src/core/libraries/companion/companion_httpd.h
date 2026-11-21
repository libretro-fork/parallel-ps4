/* SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_COMPANION_HTTPD_H
#define SHADPS4_LIBRARIES_COMPANION_HTTPD_H

/* libSceCompanionHttpd. Registered in libs.cpp. */

#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

/* OrbisCompanionHttpdEvent event codes */
#define ORBIS_COMPANION_HTTPD_EVENT_CONNECT 0x10000001
#define ORBIS_COMPANION_HTTPD_EVENT_DISCONNECT 0x10000002

typedef struct OrbisCompanionHttpdHeader {
    char* key;
    char* value;
    struct OrbisCompanionHttpdHeader* header;
} OrbisCompanionHttpdHeader;

typedef struct OrbisCompanionHttpdRequest {
    s32 method;
    char* url;
    OrbisCompanionHttpdHeader* header;
    char* body;
    u64 bodySize;
} OrbisCompanionHttpdRequest;

typedef struct OrbisCompanionHttpdResponse {
    s32 status;
    OrbisCompanionHttpdHeader* header;
    char* body;
    u64 bodySize;
} OrbisCompanionHttpdResponse;

typedef s32(PS4_SYSV_ABI* OrbisCompanionHttpdRequestBodyReceptionCallback)(
    s32 event, s32 userId, const OrbisCompanionHttpdRequest* httpRequest, void* param);

typedef s32(PS4_SYSV_ABI* OrbisCompanionHttpdRequestCallback)(
    s32 userId, const OrbisCompanionHttpdRequest* httpRequest,
    OrbisCompanionHttpdResponse* httpResponse, void* param);

/* The layout of OrbisNetSockaddrIn (net_types.h), 16 bytes */
typedef struct OrbisCompanionSockaddrIn {
    u8 sin_len;
    u8 sin_family;
    u16 sin_port;  /* network byte order */
    u32 sin_addr;  /* network byte order */
    u16 sin_vport; /* P2P virtual port, network byte order */
    char sin_zero[6];
} OrbisCompanionSockaddrIn;

typedef struct OrbisCompanionUtilDeviceInfo {
    s32 userId;
    OrbisCompanionSockaddrIn addr;
    char reserved[236];
} OrbisCompanionUtilDeviceInfo;

typedef struct OrbisCompanionHttpdEvent {
    s32 event;
    union {
        OrbisCompanionUtilDeviceInfo deviceInfo;
        s32 userId;
        char reserved[256];
    } data;
} OrbisCompanionHttpdEvent;

s32 PS4_SYSV_ABI sceCompanionHttpdAddHeader(const char* key, const char* value,
                                            OrbisCompanionHttpdResponse* response);
s32 PS4_SYSV_ABI sceCompanionHttpdGet2ndScreenStatus(s32 userId);
s32 PS4_SYSV_ABI sceCompanionHttpdGetEvent(OrbisCompanionHttpdEvent* pEvent);
s32 PS4_SYSV_ABI sceCompanionHttpdGetUserId(u32 addr, s32* userId);
s32 PS4_SYSV_ABI sceCompanionHttpdInitialize(void);
s32 PS4_SYSV_ABI sceCompanionHttpdInitialize2(void);
s32 PS4_SYSV_ABI sceCompanionHttpdOptParamInitialize(void);
s32 PS4_SYSV_ABI sceCompanionHttpdRegisterRequestBodyReceptionCallback(
    OrbisCompanionHttpdRequestBodyReceptionCallback function, void* param);
s32 PS4_SYSV_ABI
sceCompanionHttpdRegisterRequestCallback(OrbisCompanionHttpdRequestCallback function, void* param);
s32 PS4_SYSV_ABI
sceCompanionHttpdRegisterRequestCallback2(OrbisCompanionHttpdRequestCallback function, void* param);
s32 PS4_SYSV_ABI sceCompanionHttpdSetBody(const char* body, u64 bodySize,
                                          OrbisCompanionHttpdResponse* response);
s32 PS4_SYSV_ABI sceCompanionHttpdSetStatus(s32 status, OrbisCompanionHttpdResponse* response);
s32 PS4_SYSV_ABI sceCompanionHttpdStart(void);
s32 PS4_SYSV_ABI sceCompanionHttpdStop(void);
s32 PS4_SYSV_ABI sceCompanionHttpdTerminate(void);
s32 PS4_SYSV_ABI sceCompanionHttpdUnregisterRequestBodyReceptionCallback(void);
s32 PS4_SYSV_ABI sceCompanionHttpdUnregisterRequestCallback(void);

RETRO_END_DECLS

#endif
