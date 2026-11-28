/* SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <retro_atomic.h>

#include "common/c_bridge.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/network/ssl2.h"
#include "core/libraries/network/ssl2_error.h"

/* The one CA certificate there is: lent to every caller, never made or freed */
static char ssl2_dummy_data[] = "dummy";
static OrbisSslData ssl2_dummy_cert = {ssl2_dummy_data, sizeof(ssl2_dummy_data)};

/* The last context number given: the first is 1 */
static retro_atomic_int_t ssl2_last_id;

s32 PS4_SYSV_ABI sceSsl2FreeCaCerts(s32 ssl_ctx_id, OrbisSslCaCerts* certs) {
    (void)ssl_ctx_id;
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "sceSslFreeCaCerts", "(DUMMY) called");
    if (certs == NULL) {
        return ORBIS_SSL_ERROR_INVALID_ARGUMENT;
    }
    /* Lent, so nothing to give back */
    certs->certs = NULL;
    certs->num = 0;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSsl2GetCaCerts(s32 ssl_ctx_id, OrbisSslCaCerts* certs) {
    (void)ssl_ctx_id;
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "sceSslGetCaCerts", "(DUMMY) called");
    if (certs == NULL) {
        return ORBIS_SSL_ERROR_INVALID_ARGUMENT;
    }
    certs->certs = &ssl2_dummy_cert;
    certs->num = 1;
    certs->pool = NULL;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSsl2Init(size_t pool_size) {
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "sceSslInit", "(DUMMY) called poolSize = %lu",
          (unsigned long)pool_size);
    return (s32)(retro_atomic_fetch_add_int(&ssl2_last_id, 1) + 1);
}

s32 PS4_SYSV_ABI ssl2_CA_MGMT_extractKeyBlobEx(void) {
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "CA_MGMT_extractKeyBlobEx", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI ssl2_CA_MGMT_extractPublicKeyInfo(void) {
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "CA_MGMT_extractPublicKeyInfo", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI ssl2_CA_MGMT_freeKeyBlob(void) {
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "CA_MGMT_freeKeyBlob", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI ssl2_CRYPTO_initAsymmetricKey(void) {
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "CRYPTO_initAsymmetricKey", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI ssl2_CRYPTO_uninitAsymmetricKey(void) {
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "CRYPTO_uninitAsymmetricKey", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI ssl2_RSA_verifySignature(void) {
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "RSA_verifySignature", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSsl2CheckRecvPending(void) {
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "sceSslCheckRecvPending", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSsl2Close(void) {
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "sceSslClose", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSsl2Connect(void) {
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "sceSslConnect", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSsl2CreateConnection(void) {
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "sceSslCreateConnection", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSsl2CreateSslConnection(void) {
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "sceSslCreateSslConnection", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSsl2DeleteConnection(void) {
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "sceSslDeleteConnection", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSsl2DeleteSslConnection(void) {
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "sceSslDeleteSslConnection", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSsl2DisableOption(void) {
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "sceSslDisableOption", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSsl2DisableOptionInternal(void) {
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "sceSslDisableOptionInternal", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSsl2DisableOptionInternalInsecure(void) {
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "sceSslDisableOptionInternalInsecure",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSsl2DisableVerifyOption(void) {
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "sceSslDisableVerifyOption", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSsl2EnableOption(void) {
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "sceSslEnableOption", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSsl2EnableOptionInternal(void) {
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "sceSslEnableOptionInternal", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSsl2EnableVerifyOption(void) {
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "sceSslEnableVerifyOption", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSsl2FreeCaList(void) {
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "sceSslFreeCaList", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSsl2FreeSslCertName(void) {
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "sceSslFreeSslCertName", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSsl2GetAlpnSelected(void) {
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "sceSslGetAlpnSelected", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSsl2GetCaList(void) {
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "sceSslGetCaList", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSsl2GetFingerprint(void) {
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "sceSslGetFingerprint", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSsl2GetIssuerName(void) {
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "sceSslGetIssuerName", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSsl2GetMemoryPoolStats(void) {
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "sceSslGetMemoryPoolStats", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSsl2GetNameEntryCount(void) {
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "sceSslGetNameEntryCount", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSsl2GetNameEntryInfo(void) {
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "sceSslGetNameEntryInfo", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSsl2GetNanoSSLModuleId(void) {
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "sceSslGetNanoSSLModuleId", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSsl2GetNotAfter(void) {
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "sceSslGetNotAfter", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSsl2GetNotBefore(void) {
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "sceSslGetNotBefore", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSsl2GetPeerCert(void) {
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "sceSslGetPeerCert", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSsl2GetPem(void) {
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "sceSslGetPem", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSsl2GetSerialNumber(void) {
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "sceSslGetSerialNumber", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSsl2GetSslError(void) {
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "sceSslGetSslError", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSsl2GetSubjectName(void) {
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "sceSslGetSubjectName", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSsl2LoadCert(void) {
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "sceSslLoadCert", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSsl2LoadRootCACert(void) {
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "sceSslLoadRootCACert", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSsl2Read(void) {
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "sceSslRead", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSsl2Recv(void) {
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "sceSslRecv", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSsl2ReuseConnection(void) {
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "sceSslReuseConnection", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSsl2Send(void) {
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "sceSslSend", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSsl2SetAlpn(void) {
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "sceSslSetAlpn", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSsl2SetMinSslVersion(void) {
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "sceSslSetMinSslVersion", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSsl2SetSslVersion(void) {
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "sceSslSetSslVersion", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSsl2SetVerifyCallback(void) {
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "sceSslSetVerifyCallback", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSsl2Term(void) {
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "sceSslTerm", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSsl2UnloadCert(void) {
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "sceSslUnloadCert", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSsl2Write(void) {
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "sceSslWrite", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI ssl2_VLONG_freeVlongQueue(void) {
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "VLONG_freeVlongQueue", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI ssl2_Func_22E76E60BC0587D7(void) {
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "Func_22E76E60BC0587D7", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI ssl2_Func_28F8791A771D39C7(void) {
    c_log(C_LOG_LIB_SSL2, C_LOG_LEVEL_ERROR, "Func_28F8791A771D39C7", "(STUBBED) called");
    return ORBIS_OK;
}
