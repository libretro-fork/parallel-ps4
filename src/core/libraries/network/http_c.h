/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_NETWORK_HTTP_C_H
#define SHADPS4_NETWORK_HTTP_C_H

/* libSceHttp's calls that libSceHttp2 (http2.c) makes, for C: each is the
 * sceHttp function of its name (http.cpp) */

#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

int http_abort_request(int req_id);
int http_add_request_header(int id, const char* name, const char* value, s32 mode);
int http_create_connection_with_url(int tmpl_id, const char* url, int keepalive);
int http_create_request_with_url2(int conn_id, const char* method, const char* url,
                                  u64 content_length);
int http_create_template(int ctx_id, const char* user_agent, int http_ver, int auto_proxy_conf);
int http_delete_connection(int conn_id);
int http_delete_request(int req_id);
int http_delete_template(int tmpl_id);
int http_get_all_response_headers(int req_id, char** header, u64* header_size);
int http_get_response_content_length(int req_id, int* result, u64* content_length);
int http_get_status_code(int req_id, int* status_code);
int http_init(int net_id, int ssl_id, u64 pool_size);
int http_read_data(int req_id, void* data, u64 size);
int http_send_request(int req_id, const void* data, u64 size);
int http_set_request_content_length(int req_id, u64 content_length);
int http_term(int ctx_id);

RETRO_END_DECLS

#endif
