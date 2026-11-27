/* SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_NETWORK_HTTP2_H
#define SHADPS4_NETWORK_HTTP2_H

/* libSceHttp2, on libSceHttp: a request made with a URL has a connection of
 * its own, made with it and deleted with it */

#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

#define ORBIS_HTTP2_VERSION_1_0 1
#define ORBIS_HTTP2_VERSION_1_1 2
#define ORBIS_HTTP2_VERSION_2_0 3

/* TODO: Figure out the whole struct. What's here is based on libSceNpWebApi2 use. */
typedef struct OrbisHttp2PreSendCallbackData {
    void* unk;
    void* cert;
} OrbisHttp2PreSendCallbackData;
typedef s32(PS4_SYSV_ABI* OrbisHttp2PreSendCallback)(s32 request_id, s32 ssl_id,
                                                     OrbisHttp2PreSendCallbackData* data,
                                                     void* user_arg);

s32 PS4_SYSV_ABI sceHttp2AbortRequest(s32 req_id);
s32 PS4_SYSV_ABI sceHttp2AddRequestHeader(s32 template_or_req_id, const char* name,
                                          const char* value, u32 mode);
s32 PS4_SYSV_ABI sceHttp2CreateRequestWithURL(s32 tmpl_id, const char* method, const char* url,
                                              u64 content_length);
s32 PS4_SYSV_ABI sceHttp2CreateTemplate(s32 ctx_id, const char* user_agent, s32 http_ver,
                                        s32 auto_proxy_conf);
s32 PS4_SYSV_ABI sceHttp2DeleteRequest(s32 req_id);
s32 PS4_SYSV_ABI sceHttp2DeleteTemplate(s32 tmpl_id);
s32 PS4_SYSV_ABI sceHttp2GetAllResponseHeaders(s32 req_id, char** header, u64* header_size);
s32 PS4_SYSV_ABI sceHttp2GetResponseContentLength(s32 req_id, s32* result, u64* content_length);
s32 PS4_SYSV_ABI sceHttp2GetStatusCode(s32 request_id, s32* status_code);
s32 PS4_SYSV_ABI sceHttp2Init(s32 net_id, s32 ssl_id, u64 pool_size, s32 max_requests);
s32 PS4_SYSV_ABI sceHttp2ReadData(s32 req_id, void* data, u64 size);
s32 PS4_SYSV_ABI sceHttp2SendRequest(s32 req_id, const void* data, u64 size);
s32 PS4_SYSV_ABI sceHttp2SetRequestContentLength(s32 req_id, u64 content_length);
s32 PS4_SYSV_ABI sceHttp2Term(s32 ctx_id);

/* Not done yet: logged and answered with ORBIS_OK */
s32 PS4_SYSV_ABI sceHttp2AddCookie(void);
s32 PS4_SYSV_ABI sceHttp2AuthCacheFlush(void);
s32 PS4_SYSV_ABI sceHttp2CookieExport(void);
s32 PS4_SYSV_ABI sceHttp2CookieFlush(void);
s32 PS4_SYSV_ABI sceHttp2CookieImport(void);
s32 PS4_SYSV_ABI sceHttp2CreateCookieBox(void);
s32 PS4_SYSV_ABI sceHttp2DeleteCookieBox(void);
s32 PS4_SYSV_ABI sceHttp2GetAuthEnabled(void);
s32 PS4_SYSV_ABI sceHttp2GetAutoRedirect(void);
s32 PS4_SYSV_ABI sceHttp2GetCookie(void);
s32 PS4_SYSV_ABI sceHttp2GetCookieBox(void);
s32 PS4_SYSV_ABI sceHttp2GetCookieStats(void);
s32 PS4_SYSV_ABI sceHttp2GetMemoryPoolStats(void);
s32 PS4_SYSV_ABI sceHttp2ReadDataAsync(void);
s32 PS4_SYSV_ABI sceHttp2RedirectCacheFlush(void);
s32 PS4_SYSV_ABI sceHttp2RemoveRequestHeader(void);
s32 PS4_SYSV_ABI sceHttp2SendRequestAsync(void);
s32 PS4_SYSV_ABI sceHttp2SetAuthEnabled(void);
s32 PS4_SYSV_ABI sceHttp2SetAuthInfoCallback(void);
s32 PS4_SYSV_ABI sceHttp2SetAutoRedirect(void);
s32 PS4_SYSV_ABI sceHttp2SetConnectionWaitTimeOut(void);
s32 PS4_SYSV_ABI sceHttp2SetConnectTimeOut(void);
s32 PS4_SYSV_ABI sceHttp2SetCookieBox(void);
s32 PS4_SYSV_ABI sceHttp2SetCookieMaxNum(void);
s32 PS4_SYSV_ABI sceHttp2SetCookieMaxNumPerDomain(void);
s32 PS4_SYSV_ABI sceHttp2SetCookieMaxSize(void);
s32 PS4_SYSV_ABI sceHttp2SetCookieRecvCallback(void);
s32 PS4_SYSV_ABI sceHttp2SetCookieSendCallback(void);
s32 PS4_SYSV_ABI sceHttp2SetInflateGZIPEnabled(void);
s32 PS4_SYSV_ABI sceHttp2SetMinSslVersion(void);
s32 PS4_SYSV_ABI sceHttp2SetPreSendCallback(s32 template_id, OrbisHttp2PreSendCallback cb_func,
                                            void* user_arg);
s32 PS4_SYSV_ABI sceHttp2SetRecvTimeOut(void);
s32 PS4_SYSV_ABI sceHttp2SetRedirectCallback(void);
s32 PS4_SYSV_ABI sceHttp2SetResolveRetry(void);
s32 PS4_SYSV_ABI sceHttp2SetResolveTimeOut(void);
s32 PS4_SYSV_ABI sceHttp2SetSendTimeOut(void);
s32 PS4_SYSV_ABI sceHttp2SetSslCallback(void);
s32 PS4_SYSV_ABI sceHttp2SetTimeOut(void);
s32 PS4_SYSV_ABI sceHttp2SslDisableOption(void);
s32 PS4_SYSV_ABI sceHttp2SslEnableOption(void);
s32 PS4_SYSV_ABI sceHttp2WaitAsync(void);

RETRO_END_DECLS

#endif
