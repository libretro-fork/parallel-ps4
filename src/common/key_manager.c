/* SPDX-FileCopyrightText: Copyright 2025-2026 shadLauncher4 Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <string.h>

#include <file/file_path.h>
#include <formats/rjson.h>
#include <formats/rjson_helpers.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>

#include "common/c_bridge.h"
#include "common/json_util.h"
#include "common/key_manager.h"

#define KEY_MANAGER_PATH_MAX 4096

static struct key_manager_keys key_manager_state;

static bool key_manager_path(char* s, size_t len) {
    char dir[KEY_MANAGER_PATH_MAX];
    if (c_get_user_path(C_USER_PATH_USER, dir, sizeof(dir)) >= sizeof(dir))
        return false;
    return fill_pathname_join_special(s, dir, "keys.json", len) < len;
}

static int key_manager_hex_digit(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return -1;
}

static bool key_manager_parse_hex(struct key_manager_key* key, const char* hex, size_t len) {
    size_t i;
    if ((len & 1) || len / 2 > KEY_MANAGER_KEY_MAX)
        return false;
    for (i = 0; i < len; i += 2) {
        int hi = key_manager_hex_digit(hex[i]);
        int lo = key_manager_hex_digit(hex[i + 1]);
        if (hi < 0 || lo < 0)
            return false;
        key->data[i / 2] = (uint8_t)((hi << 4) | lo);
    }
    key->size = len / 2;
    return true;
}

static void key_manager_write_hex(rjsonwriter_t* writer, const struct key_manager_key* key) {
    static const char digits[] = "0123456789ABCDEF";
    char hex[KEY_MANAGER_KEY_MAX * 2];
    size_t i;
    for (i = 0; i < key->size; i++) {
        hex[i * 2] = digits[(key->data[i] >> 4) & 0xF];
        hex[i * 2 + 1] = digits[key->data[i] & 0xF];
    }
    rjsonwriter_add_string_len(writer, hex, (int)(key->size * 2));
}

/* Reads the members of the "TrophyKeySet" object */
static bool key_manager_read_trophy_set(rjson_t* json, struct key_manager_keys* keys) {
    for (;;) {
        size_t len;
        const char* name;
        enum rjson_type type = rjson_next(json);
        if (type == RJSON_OBJECT_END)
            return true;
        if (type != RJSON_STRING)
            return false;
        name = rjson_get_string(json, &len);
        if (string_is_equal(name, "ReleaseTrophyKey")) {
            const char* hex;
            if (rjson_next(json) != RJSON_STRING)
                return false;
            hex = rjson_get_string(json, &len);
            if (!key_manager_parse_hex(&keys->release_trophy_key, hex, len)) {
                c_log(C_LOG_KEY_MANAGER, C_LOG_LEVEL_ERROR, "key_manager_load",
                      "Invalid hex string for ReleaseTrophyKey");
                return false;
            }
        } else if (!json_util_skip(json, rjson_next(json)))
            return false;
    }
}

static bool key_manager_read(rjson_t* json, struct key_manager_keys* keys) {
    if (rjson_next(json) != RJSON_OBJECT)
        return false;
    for (;;) {
        size_t len;
        const char* name;
        enum rjson_type type = rjson_next(json);
        if (type == RJSON_OBJECT_END)
            return rjson_next(json) == RJSON_DONE;
        if (type != RJSON_STRING)
            return false;
        name = rjson_get_string(json, &len);
        type = rjson_next(json);
        if (string_is_equal(name, "TrophyKeySet") && type == RJSON_OBJECT) {
            if (!key_manager_read_trophy_set(json, keys))
                return false;
        } else if (!json_util_skip(json, type))
            return false;
    }
}

bool key_manager_load(void) {
    char path[KEY_MANAGER_PATH_MAX];
    struct key_manager_keys keys;
    json_util_reader_t reader;
    bool ok;

    key_manager_set_default_keys();
    if (!key_manager_path(path, sizeof(path)))
        return false;

    if (!path_is_valid(path))
        return key_manager_save();

    if (!json_util_reader_open(&reader, path)) {
        c_log(C_LOG_KEY_MANAGER, C_LOG_LEVEL_ERROR, "key_manager_load",
              "Could not open key file: %s", path);
        return false;
    }

    memset(&keys, 0, sizeof(keys));
    ok = key_manager_read(reader.json, &keys);
    if (ok)
        key_manager_state = keys;
    else
        c_log(C_LOG_KEY_MANAGER, C_LOG_LEVEL_ERROR, "key_manager_load",
              "Error loading keys, using defaults: %s",
              *rjson_get_error(reader.json) ? rjson_get_error(reader.json) : "bad value");
    json_util_reader_close(&reader);
    return ok;
}

bool key_manager_save(void) {
    char path[KEY_MANAGER_PATH_MAX];
    json_util_writer_t out;
    rjsonwriter_t* writer;

    if (!key_manager_path(path, sizeof(path)))
        return false;
    if (!json_util_writer_open(&out, path)) {
        c_log(C_LOG_KEY_MANAGER, C_LOG_LEVEL_ERROR, "key_manager_save",
              "Could not open key file for writing: %s", path);
        return false;
    }
    writer = out.writer;
    rjsonwriter_add_start_object(writer);
    json_util_write_key(writer, 1, "TrophyKeySet", true);
    rjsonwriter_add_start_object(writer);
    json_util_write_key(writer, 2, "ReleaseTrophyKey", true);
    key_manager_write_hex(writer, &key_manager_state.release_trophy_key);
    json_util_write_end(writer, 2, false, '}');
    json_util_write_end(writer, 1, false, '}');

    if (!json_util_writer_close(&out, path, true)) {
        c_log(C_LOG_KEY_MANAGER, C_LOG_LEVEL_ERROR, "key_manager_save",
              "Failed to write keys to: %s", path);
        return false;
    }
    return true;
}

const struct key_manager_keys* key_manager_get_keys(void) {
    return &key_manager_state;
}

void key_manager_set_keys(const struct key_manager_keys* keys) {
    key_manager_state = *keys;
}

void key_manager_set_default_keys(void) {
    memset(&key_manager_state, 0, sizeof(key_manager_state));
}

bool key_manager_has_keys(void) {
    return key_manager_state.release_trophy_key.size != 0;
}
