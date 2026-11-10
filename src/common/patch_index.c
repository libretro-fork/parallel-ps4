/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <string.h>

#include <formats/rjson.h>

#include "common/c_bridge.h"
#include "common/json_util.h"
#include "common/patch_index.h"

static bool patch_index_is_serial(rjson_t* json, const char* serial, size_t serial_len) {
    size_t len;
    const char* str = rjson_get_string(json, &len);
    return len == serial_len && memcmp(str, serial, len) == 0;
}

/* Reads one member's value; sets *found when it lists serial. */
static bool patch_index_read_serials(rjson_t* json, enum rjson_type type, const char* serial,
                                     bool* found) {
    size_t serial_len = strlen(serial);
    *found = false;
    if (type == RJSON_STRING) {
        *found = patch_index_is_serial(json, serial, serial_len);
        return true;
    }
    if (type != RJSON_ARRAY)
        return json_util_skip(json, type);
    for (;;) {
        type = rjson_next(json);
        if (type == RJSON_ARRAY_END)
            return true;
        if (type == RJSON_STRING) {
            if (!*found && patch_index_is_serial(json, serial, serial_len))
                *found = true;
        } else if (!json_util_skip(json, type))
            return false;
    }
}

static bool patch_index_read(rjson_t* json, const char* serial, char* s, size_t len) {
    if (rjson_next(json) != RJSON_OBJECT)
        return false;
    for (;;) {
        char name[1024];
        bool found;
        enum rjson_type type = rjson_next(json);
        if (type == RJSON_OBJECT_END)
            return true;
        if (type != RJSON_STRING)
            return false;
        json_util_get_string(json, name, sizeof(name));
        if (!patch_index_read_serials(json, rjson_next(json), serial, &found))
            return false;
        if (found && !s[0] && strlen(name) < len)
            memcpy(s, name, strlen(name) + 1);
    }
}

bool patch_index_find(const char* index_path, const char* serial, char* s, size_t len) {
    json_util_reader_t reader;
    bool ok;

    if (!len)
        return false;
    s[0] = '\0';
    if (!json_util_reader_open(&reader, index_path)) {
        c_log(C_LOG_LOADER, C_LOG_LEVEL_ERROR, "patch_index_find", "Could not open patch index: %s",
              index_path);
        return false;
    }
    ok = patch_index_read(reader.json, serial, s, len);
    if (!ok) {
        c_log(C_LOG_LOADER, C_LOG_LEVEL_ERROR, "patch_index_find",
              "Could not parse patch index %s: %s", index_path, rjson_get_error(reader.json));
        s[0] = '\0';
    }
    json_util_reader_close(&reader);
    return ok && s[0] != '\0';
}
