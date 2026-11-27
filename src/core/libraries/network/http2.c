/* SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <retro_atomic.h>

#include "common/c_bridge.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/network/http2.h"
#include "core/libraries/network/http_c.h"

/* The connection each request made with a URL has (sceHttp2CreateRequestWithURL), deleted
 * with it. Lock-free: an entry is claimed with a compare-and-swap, filled, then published;
 * a delete takes a published entry back with another */
#define HTTP2_TRACKED_MAX 256
enum { ENTRY_FREE = 0, ENTRY_FILLING, ENTRY_READY, ENTRY_TAKING };

typedef struct http2_request_entry {
    s32 req_id;
    s32 conn_id;
    retro_atomic_int_t state;
} http2_request_entry_t;

static http2_request_entry_t http2_requests[HTTP2_TRACKED_MAX];

static int http2_track(s32 req_id, s32 conn_id) {
    int i;
    for (i = 0; i < HTTP2_TRACKED_MAX; i++) {
        http2_request_entry_t* const e = &http2_requests[i];
        if (retro_atomic_cas_int(&e->state, ENTRY_FREE, ENTRY_FILLING)) {
            e->req_id = req_id;
            e->conn_id = conn_id;
            retro_atomic_store_release_int(&e->state, ENTRY_READY);
            return 1;
        }
    }
    return 0;
}

/* The connection of @req_id, its entry freed; -1 when none is tracked. Taking an entry is
 * a compare-and-swap from published, so two deletes of a request take it once. */
static s32 http2_untrack(s32 req_id) {
    int i;
    for (i = 0; i < HTTP2_TRACKED_MAX; i++) {
        http2_request_entry_t* const e = &http2_requests[i];
        if (retro_atomic_load_acquire_int(&e->state) == ENTRY_READY && e->req_id == req_id &&
            retro_atomic_cas_int(&e->state, ENTRY_READY, ENTRY_TAKING)) {
            const s32 conn_id = e->conn_id;
            if (e->req_id != req_id) {
                /* Freed and filled for another request between the look and the swap */
                retro_atomic_store_release_int(&e->state, ENTRY_READY);
                continue;
            }
            retro_atomic_store_release_int(&e->state, ENTRY_FREE);
            return conn_id;
        }
    }
    return -1;
}

/* @result, logged as @what's failure when it is one */
static s32 http2_checked(s32 result, const char* func, const char* what) {
    if (result < 0) {
        c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, func, "Failed to %s, error = 0x%x", what,
              (unsigned)result);
    }
    return result;
}

s32 PS4_SYSV_ABI sceHttp2AbortRequest(s32 req_id) {
    c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2AbortRequest", "(STUBBED) called");
    return http2_checked(http_abort_request(req_id), "sceHttp2AbortRequest",
                         "get abort HTTP request");
}

s32 PS4_SYSV_ABI sceHttp2AddRequestHeader(s32 template_or_req_id, const char* name,
                                          const char* value, u32 mode) {
    c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2AddRequestHeader", "(STUBBED) called");
    return http2_checked(http_add_request_header(template_or_req_id, name, value, (s32)mode),
                         "sceHttp2AddRequestHeader", "add HTTP response header");
}

s32 PS4_SYSV_ABI sceHttp2CreateRequestWithURL(s32 tmpl_id, const char* method, const char* url,
                                              u64 content_length) {
    s32 conn_id;
    s32 req_id;
    c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2CreateRequestWithURL", "(STUBBED) called");
    /* Http1 has a connection thing to go through first */
    conn_id = http_create_connection_with_url(tmpl_id, url, 1);
    if (conn_id < 0) {
        c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2CreateRequestWithURL",
              "Failed to create HTTP connection, error = 0x%x", (unsigned)conn_id);
        return conn_id;
    }
    req_id = http_create_request_with_url2(conn_id, method, url, content_length);
    if (req_id < 0) {
        const s32 del_result = http_delete_connection(conn_id);
        c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2CreateRequestWithURL",
              "Failed to create HTTP request, error = 0x%x", (unsigned)req_id);
        if (del_result < 0) {
            c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2CreateRequestWithURL",
                  "Failed to clean up HTTP connection %d, error = 0x%x", conn_id,
                  (unsigned)del_result);
        }
        return req_id;
    }
    if (!http2_track(req_id, conn_id)) {
        c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2CreateRequestWithURL",
              "More than %d requests at once: connection %d of request %d not tracked",
              HTTP2_TRACKED_MAX, conn_id, req_id);
    }
    return req_id;
}

s32 PS4_SYSV_ABI sceHttp2CreateTemplate(s32 ctx_id, const char* user_agent, s32 http_ver,
                                        s32 auto_proxy_conf) {
    c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2CreateTemplate", "(STUBBED) called");
    return http2_checked(http_create_template(ctx_id, user_agent, http_ver, auto_proxy_conf),
                         "sceHttp2CreateTemplate", "create HTTP template");
}

s32 PS4_SYSV_ABI sceHttp2DeleteRequest(s32 req_id) {
    s32 result;
    s32 conn_id;
    c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2DeleteRequest", "(STUBBED) called");
    result = http_delete_request(req_id);
    if (result < 0) {
        c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2DeleteRequest",
              "Failed to delete HTTP request, error = 0x%x", (unsigned)result);
        return result;
    }
    conn_id = http2_untrack(req_id);
    if (conn_id < 0) {
        c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_DEBUG, "sceHttp2DeleteRequest",
              "No connection tracked for request %d", req_id);
        return ORBIS_OK;
    }
    result = http_delete_connection(conn_id);
    if (result < 0) {
        c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2DeleteRequest",
              "Failed to delete HTTP connection, error = 0x%x", (unsigned)result);
        return result;
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHttp2DeleteTemplate(s32 tmpl_id) {
    c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2DeleteTemplate", "(STUBBED) called");
    return http2_checked(http_delete_template(tmpl_id), "sceHttp2DeleteTemplate",
                         "delete HTTP template");
}

s32 PS4_SYSV_ABI sceHttp2GetAllResponseHeaders(s32 req_id, char** header, u64* header_size) {
    return http2_checked(http_get_all_response_headers(req_id, header, header_size),
                         "sceHttp2GetAllResponseHeaders", "get HTTP response headers");
}

s32 PS4_SYSV_ABI sceHttp2GetResponseContentLength(s32 req_id, s32* result, u64* content_length) {
    return http2_checked(http_get_response_content_length(req_id, result, content_length),
                         "sceHttp2GetResponseContentLength", "get HTTP response content length");
}

s32 PS4_SYSV_ABI sceHttp2GetStatusCode(s32 req_id, s32* status_code) {
    return http2_checked(http_get_status_code(req_id, status_code), "sceHttp2GetStatusCode",
                         "get HTTP status code");
}

s32 PS4_SYSV_ABI sceHttp2Init(s32 net_id, s32 ssl_id, u64 pool_size, s32 max_requests) {
    (void)max_requests;
    c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2Init", "(DUMMY) called");
    return http2_checked(http_init(net_id, ssl_id, pool_size), "sceHttp2Init", "init HTTP context");
}

s32 PS4_SYSV_ABI sceHttp2ReadData(s32 req_id, void* data, u64 size) {
    return http2_checked(http_read_data(req_id, data, size), "sceHttp2ReadData", "read HTTP data");
}

s32 PS4_SYSV_ABI sceHttp2SendRequest(s32 req_id, const void* data, u64 size) {
    return http2_checked(http_send_request(req_id, data, size), "sceHttp2SendRequest",
                         "send HTTP request");
}

s32 PS4_SYSV_ABI sceHttp2SetRequestContentLength(s32 req_id, u64 content_length) {
    return http2_checked(http_set_request_content_length(req_id, content_length),
                         "sceHttp2SetRequestContentLength", "set HTTP request content length");
}

s32 PS4_SYSV_ABI sceHttp2Term(s32 ctx_id) {
    s32 result;
    c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2Term", "(STUBBED) called ctx_id=%d", ctx_id);
    result = http_term(ctx_id);
    if (result < 0) {
        c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2Term",
              "Failed to terminate HTTP context %d, error = 0x%x", ctx_id, (unsigned)result);
    }
    return result;
}

s32 PS4_SYSV_ABI sceHttp2AddCookie(void) {
    c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2AddCookie", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHttp2AuthCacheFlush(void) {
    c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2AuthCacheFlush", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHttp2CookieExport(void) {
    c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2CookieExport", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHttp2CookieFlush(void) {
    c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2CookieFlush", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHttp2CookieImport(void) {
    c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2CookieImport", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHttp2CreateCookieBox(void) {
    c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2CreateCookieBox", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHttp2DeleteCookieBox(void) {
    c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2DeleteCookieBox", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHttp2GetAuthEnabled(void) {
    c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2GetAuthEnabled", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHttp2GetAutoRedirect(void) {
    c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2GetAutoRedirect", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHttp2GetCookie(void) {
    c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2GetCookie", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHttp2GetCookieBox(void) {
    c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2GetCookieBox", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHttp2GetCookieStats(void) {
    c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2GetCookieStats", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHttp2GetMemoryPoolStats(void) {
    c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2GetMemoryPoolStats", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHttp2ReadDataAsync(void) {
    c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2ReadDataAsync", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHttp2RedirectCacheFlush(void) {
    c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2RedirectCacheFlush", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHttp2RemoveRequestHeader(void) {
    c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2RemoveRequestHeader", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHttp2SendRequestAsync(void) {
    c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2SendRequestAsync", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHttp2SetAuthEnabled(void) {
    c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2SetAuthEnabled", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHttp2SetAuthInfoCallback(void) {
    c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2SetAuthInfoCallback", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHttp2SetAutoRedirect(void) {
    c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2SetAutoRedirect", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHttp2SetConnectionWaitTimeOut(void) {
    c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2SetConnectionWaitTimeOut",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHttp2SetConnectTimeOut(void) {
    c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2SetConnectTimeOut", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHttp2SetCookieBox(void) {
    c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2SetCookieBox", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHttp2SetCookieMaxNum(void) {
    c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2SetCookieMaxNum", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHttp2SetCookieMaxNumPerDomain(void) {
    c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2SetCookieMaxNumPerDomain",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHttp2SetCookieMaxSize(void) {
    c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2SetCookieMaxSize", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHttp2SetCookieRecvCallback(void) {
    c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2SetCookieRecvCallback", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHttp2SetCookieSendCallback(void) {
    c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2SetCookieSendCallback", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHttp2SetInflateGZIPEnabled(void) {
    c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2SetInflateGZIPEnabled", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHttp2SetMinSslVersion(void) {
    c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2SetMinSslVersion", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHttp2SetPreSendCallback(s32 template_id, OrbisHttp2PreSendCallback cb_func,
                                            void* user_arg) {
    (void)template_id;
    (void)cb_func;
    (void)user_arg;
    c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2SetPreSendCallback", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHttp2SetRecvTimeOut(void) {
    c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2SetRecvTimeOut", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHttp2SetRedirectCallback(void) {
    c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2SetRedirectCallback", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHttp2SetResolveRetry(void) {
    c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2SetResolveRetry", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHttp2SetResolveTimeOut(void) {
    c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2SetResolveTimeOut", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHttp2SetSendTimeOut(void) {
    c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2SetSendTimeOut", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHttp2SetSslCallback(void) {
    c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2SetSslCallback", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHttp2SetTimeOut(void) {
    c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2SetTimeOut", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHttp2SslDisableOption(void) {
    c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2SslDisableOption", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHttp2SslEnableOption(void) {
    c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2SslEnableOption", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceHttp2WaitAsync(void) {
    c_log(C_LOG_LIB_HTTP2, C_LOG_LEVEL_ERROR, "sceHttp2WaitAsync", "(STUBBED) called");
    return ORBIS_OK;
}
