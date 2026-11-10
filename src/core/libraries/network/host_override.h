/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_NETWORK_HOST_OVERRIDE_H
#define SHADPS4_NETWORK_HOST_OVERRIDE_H

/* HTTP host overrides, read from host_overrides.json in the user directory
 * (or the file named by SHADPS4_HTTP_HOST_OVERRIDES_JSON). The file is an
 * object mapping a key to a replacement:
 *
 *   key:   "scheme://host:port"  that exact endpoint
 *          "host:port"           host and port on any scheme
 *          "host"                host on any scheme and port
 *          "*"                   anything else
 *   value: "host", "host:port", "http://host", "https://host:port", ...
 *
 * Keys starting with '_' are comments. A scheme in the value replaces the
 * connection's; without one it is kept. Without a port the connection's is
 * kept, or swapped between 80 and 443 when the scheme changes. */

#include <stddef.h>

#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

#define HOST_OVERRIDE_MAX_ENTRIES 64
#define HOST_OVERRIDE_HOST_MAX 256

struct host_override_result {
    /* Points into the override table, or at the scheme passed in */
    const char* scheme;
    const char* host;
    unsigned port;
    bool is_secure;
};

/* Reads the override file. Call once, before host_override_apply. */
void host_override_load(void);

/* Looks up scheme://host:port. Returns false when no override applies;
 * otherwise fills out with the endpoint to connect to. */
bool host_override_apply(const char* scheme, const char* host, unsigned port, bool is_secure,
                         struct host_override_result* out);

RETRO_END_DECLS

#endif
