/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <compat/strl.h>
#include <file/file_path.h>
#include <formats/rjson.h>
#include <string/stdstring.h>

#include "common/c_bridge.h"
#include "common/json_util.h"
#include "core/libraries/network/host_override.h"

#define HOST_OVERRIDE_PATH_MAX 4096
#define HOST_OVERRIDE_SCHEME_MAX 8
/* "scheme://" + host + ":65535" */
#define HOST_OVERRIDE_KEY_MAX (HOST_OVERRIDE_SCHEME_MAX + 3 + HOST_OVERRIDE_HOST_MAX + 6)

struct host_override_entry {
    char key[HOST_OVERRIDE_KEY_MAX];
    /* "http", "https", or empty to keep the connection's */
    char scheme[HOST_OVERRIDE_SCHEME_MAX];
    char host[HOST_OVERRIDE_HOST_MAX];
    /* 0 keeps the connection's */
    unsigned port;
};

static struct host_override_entry host_override_entries[HOST_OVERRIDE_MAX_ENTRIES];
static size_t host_override_count;

/* Parses a replacement value: "host", "host:port", "scheme://host" or
 * "scheme://host:port". Unknown schemes and bad ports are dropped. */
static void host_override_parse_target(struct host_override_entry* entry, const char* value,
                                       size_t len) {
    const char* end = value + len;
    const char* sep = NULL;
    const char* colon;
    const char* p;

    for (p = value; p + 3 <= end; p++) {
        if (p[0] == ':' && p[1] == '/' && p[2] == '/') {
            sep = p;
            break;
        }
    }
    entry->scheme[0] = '\0';
    if (sep) {
        size_t n = (size_t)(sep - value);
        if (n < sizeof(entry->scheme)) {
            size_t i;
            for (i = 0; i < n; i++)
                entry->scheme[i] = (char)tolower((unsigned char)value[i]);
            entry->scheme[n] = '\0';
            if (!string_is_equal(entry->scheme, "http") && !string_is_equal(entry->scheme, "https"))
                entry->scheme[0] = '\0';
        }
        value = sep + 3;
    }

    entry->port = 0;
    colon = (const char*)memchr(value, ':', (size_t)(end - value));
    if (colon) {
        char digits[16];
        size_t n = (size_t)(end - colon - 1);
        if (n < sizeof(digits)) {
            unsigned long port;
            char* stop;
            memcpy(digits, colon + 1, n);
            digits[n] = '\0';
            port = strtoul(digits, &stop, 10);
            if (stop != digits && port > 0 && port <= 65535)
                entry->port = (unsigned)port;
        }
        end = colon;
    }
    {
        size_t n = (size_t)(end - value);
        if (n >= sizeof(entry->host))
            n = sizeof(entry->host) - 1;
        memcpy(entry->host, value, n);
        entry->host[n] = '\0';
    }
}

static struct host_override_entry* host_override_slot(const char* key) {
    size_t i;
    for (i = 0; i < host_override_count; i++)
        if (string_is_equal(host_override_entries[i].key, key))
            return &host_override_entries[i];
    if (host_override_count == HOST_OVERRIDE_MAX_ENTRIES)
        return NULL;
    return &host_override_entries[host_override_count++];
}

static bool host_override_read(rjson_t* json) {
    if (rjson_next(json) != RJSON_OBJECT) {
        c_log(C_LOG_LIB_HTTP, C_LOG_LEVEL_ERROR, "host_override_load",
              "host overrides JSON root must be an object");
        return false;
    }
    for (;;) {
        char key[HOST_OVERRIDE_KEY_MAX];
        size_t key_len;
        size_t len;
        const char* value;
        struct host_override_entry* entry;
        enum rjson_type type = rjson_next(json);

        if (type == RJSON_OBJECT_END)
            return true;
        if (type != RJSON_STRING)
            return false;
        key_len = json_util_get_string(json, key, sizeof(key));
        type = rjson_next(json);
        if (key[0] == '_') {
            if (!json_util_skip(json, type))
                return false;
            continue;
        }
        if (type != RJSON_STRING) {
            c_log(C_LOG_LIB_HTTP, C_LOG_LEVEL_ERROR, "host_override_load",
                  "host overrides JSON: value for '%s' is not a string; skipped", key);
            if (!json_util_skip(json, type))
                return false;
            continue;
        }
        value = rjson_get_string(json, &len);
        if (len == 0) {
            c_log(C_LOG_LIB_HTTP, C_LOG_LEVEL_ERROR, "host_override_load",
                  "host overrides JSON: value for '%s' is empty; skipped", key);
            continue;
        }
        if (key_len >= sizeof(key)) {
            c_log(C_LOG_LIB_HTTP, C_LOG_LEVEL_ERROR, "host_override_load",
                  "host overrides JSON: key '%s...' is too long; skipped", key);
            continue;
        }
        entry = host_override_slot(key);
        if (!entry) {
            c_log(C_LOG_LIB_HTTP, C_LOG_LEVEL_ERROR, "host_override_load",
                  "host overrides JSON: more than %d entries; '%s' skipped",
                  HOST_OVERRIDE_MAX_ENTRIES, key);
            continue;
        }
        strlcpy(entry->key, key, sizeof(entry->key));
        host_override_parse_target(entry, value, len);
    }
}

void host_override_load(void) {
    char path[HOST_OVERRIDE_PATH_MAX];
    const char* env = getenv("SHADPS4_HTTP_HOST_OVERRIDES_JSON");
    json_util_reader_t reader;

    host_override_count = 0;
    if (env && env[0]) {
        /* Explicit path, for development and testing */
        strlcpy(path, env, sizeof(path));
    } else {
        char dir[HOST_OVERRIDE_PATH_MAX];
        if (c_get_user_path(C_USER_PATH_USER, dir, sizeof(dir)) >= sizeof(dir))
            return;
        fill_pathname_join_special(path, dir, "host_overrides.json", sizeof(path));
    }
    if (!json_util_reader_open(&reader, path))
        return;
    if (!host_override_read(reader.json)) {
        c_log(C_LOG_LIB_HTTP, C_LOG_LEVEL_ERROR, "host_override_load",
              "host overrides JSON parse failed: %s", rjson_get_error(reader.json));
        host_override_count = 0;
    }
    json_util_reader_close(&reader);
    c_log(C_LOG_LIB_HTTP, C_LOG_LEVEL_INFO, "host_override_load",
          "loaded %u host override entries from %s", (unsigned)host_override_count, path);
}

static const struct host_override_entry* host_override_find(const char* key) {
    size_t i;
    for (i = 0; i < host_override_count; i++)
        if (string_is_equal(host_override_entries[i].key, key))
            return &host_override_entries[i];
    return NULL;
}

static void host_override_append_port(char* s, size_t len, unsigned port) {
    char digits[8];
    int i = (int)sizeof(digits);
    digits[--i] = '\0';
    do {
        digits[--i] = (char)('0' + port % 10);
        port /= 10;
    } while (port && i > 0);
    strlcat(s, ":", len);
    strlcat(s, digits + i, len);
}

bool host_override_apply(const char* scheme, const char* host, unsigned port, bool is_secure,
                         struct host_override_result* out) {
    char key[HOST_OVERRIDE_KEY_MAX];
    const struct host_override_entry* entry;

    if (host_override_count == 0)
        return false;

    /* Most specific first: scheme://host:port, host:port, host, then "*" */
    strlcpy(key, scheme, sizeof(key));
    strlcat(key, "://", sizeof(key));
    strlcat(key, host, sizeof(key));
    host_override_append_port(key, sizeof(key), port);
    entry = host_override_find(key);
    if (!entry) {
        strlcpy(key, host, sizeof(key));
        host_override_append_port(key, sizeof(key), port);
        entry = host_override_find(key);
    }
    if (!entry)
        entry = host_override_find(host);
    if (!entry)
        entry = host_override_find("*");
    if (!entry)
        return false;

    out->host = entry->host;
    out->scheme = scheme;
    out->is_secure = is_secure;
    out->port = port;
    if (entry->scheme[0]) {
        out->scheme = entry->scheme;
        out->is_secure = string_is_equal(entry->scheme, "https");
    }
    if (entry->port != 0)
        out->port = entry->port;
    else if (entry->scheme[0] && !string_is_equal(entry->scheme, scheme)) {
        if (string_is_equal(scheme, "https") && port == 443)
            out->port = 80;
        else if (string_is_equal(scheme, "http") && port == 80)
            out->port = 443;
    }

    c_log(C_LOG_LIB_HTTP, C_LOG_LEVEL_INFO, "host_override_apply",
          "host override active: %s://%s:%u -> %s://%s:%u (matched key '%s')", scheme, host, port,
          out->scheme, out->host, out->port, entry->key);
    return true;
}
