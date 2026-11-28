/* SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_NETWORK_SSL2_H
#define SHADPS4_NETWORK_SSL2_H

/* libSceSsl2: only what a game needs to go on without a network. A context is a number, the
 * CA certificates one dummy certificate, and everything else is logged and answered with
 * ORBIS_OK */

#include <stddef.h>

#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

typedef struct OrbisSslData {
    char* ptr;
    u64 size;
} OrbisSslData;

typedef struct OrbisSslCaCerts {
    OrbisSslData* certs;
    u64 num;
    void* pool;
} OrbisSslCaCerts;

s32 PS4_SYSV_ABI sceSsl2FreeCaCerts(s32 ssl_ctx_id, OrbisSslCaCerts* certs);
s32 PS4_SYSV_ABI sceSsl2GetCaCerts(s32 ssl_ctx_id, OrbisSslCaCerts* certs);
s32 PS4_SYSV_ABI sceSsl2Init(size_t pool_size);

/* Not done yet: logged and answered with ORBIS_OK */
s32 PS4_SYSV_ABI ssl2_CA_MGMT_extractKeyBlobEx(void);
s32 PS4_SYSV_ABI ssl2_CA_MGMT_extractPublicKeyInfo(void);
s32 PS4_SYSV_ABI ssl2_CA_MGMT_freeKeyBlob(void);
s32 PS4_SYSV_ABI ssl2_CRYPTO_initAsymmetricKey(void);
s32 PS4_SYSV_ABI ssl2_CRYPTO_uninitAsymmetricKey(void);
s32 PS4_SYSV_ABI ssl2_RSA_verifySignature(void);
s32 PS4_SYSV_ABI sceSsl2CheckRecvPending(void);
s32 PS4_SYSV_ABI sceSsl2Close(void);
s32 PS4_SYSV_ABI sceSsl2Connect(void);
s32 PS4_SYSV_ABI sceSsl2CreateConnection(void);
s32 PS4_SYSV_ABI sceSsl2CreateSslConnection(void);
s32 PS4_SYSV_ABI sceSsl2DeleteConnection(void);
s32 PS4_SYSV_ABI sceSsl2DeleteSslConnection(void);
s32 PS4_SYSV_ABI sceSsl2DisableOption(void);
s32 PS4_SYSV_ABI sceSsl2DisableOptionInternal(void);
s32 PS4_SYSV_ABI sceSsl2DisableOptionInternalInsecure(void);
s32 PS4_SYSV_ABI sceSsl2DisableVerifyOption(void);
s32 PS4_SYSV_ABI sceSsl2EnableOption(void);
s32 PS4_SYSV_ABI sceSsl2EnableOptionInternal(void);
s32 PS4_SYSV_ABI sceSsl2EnableVerifyOption(void);
s32 PS4_SYSV_ABI sceSsl2FreeCaList(void);
s32 PS4_SYSV_ABI sceSsl2FreeSslCertName(void);
s32 PS4_SYSV_ABI sceSsl2GetAlpnSelected(void);
s32 PS4_SYSV_ABI sceSsl2GetCaList(void);
s32 PS4_SYSV_ABI sceSsl2GetFingerprint(void);
s32 PS4_SYSV_ABI sceSsl2GetIssuerName(void);
s32 PS4_SYSV_ABI sceSsl2GetMemoryPoolStats(void);
s32 PS4_SYSV_ABI sceSsl2GetNameEntryCount(void);
s32 PS4_SYSV_ABI sceSsl2GetNameEntryInfo(void);
s32 PS4_SYSV_ABI sceSsl2GetNanoSSLModuleId(void);
s32 PS4_SYSV_ABI sceSsl2GetNotAfter(void);
s32 PS4_SYSV_ABI sceSsl2GetNotBefore(void);
s32 PS4_SYSV_ABI sceSsl2GetPeerCert(void);
s32 PS4_SYSV_ABI sceSsl2GetPem(void);
s32 PS4_SYSV_ABI sceSsl2GetSerialNumber(void);
s32 PS4_SYSV_ABI sceSsl2GetSslError(void);
s32 PS4_SYSV_ABI sceSsl2GetSubjectName(void);
s32 PS4_SYSV_ABI sceSsl2LoadCert(void);
s32 PS4_SYSV_ABI sceSsl2LoadRootCACert(void);
s32 PS4_SYSV_ABI sceSsl2Read(void);
s32 PS4_SYSV_ABI sceSsl2Recv(void);
s32 PS4_SYSV_ABI sceSsl2ReuseConnection(void);
s32 PS4_SYSV_ABI sceSsl2Send(void);
s32 PS4_SYSV_ABI sceSsl2SetAlpn(void);
s32 PS4_SYSV_ABI sceSsl2SetMinSslVersion(void);
s32 PS4_SYSV_ABI sceSsl2SetSslVersion(void);
s32 PS4_SYSV_ABI sceSsl2SetVerifyCallback(void);
s32 PS4_SYSV_ABI sceSsl2Term(void);
s32 PS4_SYSV_ABI sceSsl2UnloadCert(void);
s32 PS4_SYSV_ABI sceSsl2Write(void);
s32 PS4_SYSV_ABI ssl2_VLONG_freeVlongQueue(void);
s32 PS4_SYSV_ABI ssl2_Func_22E76E60BC0587D7(void);
s32 PS4_SYSV_ABI ssl2_Func_28F8791A771D39C7(void);

RETRO_END_DECLS

#endif
