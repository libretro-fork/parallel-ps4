/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stddef.h>
#include <string.h>

#include <compat/strl.h>
#include <file/file_path.h>
#include <formats/rjson.h>
#include <formats/rjson_helpers.h>
#include <string/stdstring.h>

#include "common/c_bridge.h"
#include "common/json_util.h"
#include "core/emulator_settings.h"
#include "core/user_manager.h"

#define USER_MANAGER_PATH_MAX (EMU_SETTINGS_PATH_MAX + 64)

enum user_field_type {
    USER_FIELD_S32 = 0,
    USER_FIELD_U32,
    USER_FIELD_INT,
    USER_FIELD_U8,
    USER_FIELD_BOOL,
    USER_FIELD_STRING
};

struct user_field {
    const char* key;
    enum user_field_type type;
    size_t offset;
    size_t size;
};

#define USER_FIELD(name, type)                                                                     \
    {#name, USER_FIELD_##type, offsetof(struct user, name), sizeof(((struct user*)0)->name)}

/* What users.json holds per user, in the order it is written */
static const struct user_field user_fields[] = {USER_FIELD(user_id, S32),
                                                USER_FIELD(user_color, U32),
                                                USER_FIELD(user_name, STRING),
                                                USER_FIELD(player_index, INT),
                                                USER_FIELD(shadnet_npid, STRING),
                                                USER_FIELD(shadnet_password, STRING),
                                                USER_FIELD(shadnet_token, STRING),
                                                USER_FIELD(shadnet_email, STRING),
                                                USER_FIELD(shadnet_enabled, BOOL),
                                                USER_FIELD(np_country, STRING),
                                                USER_FIELD(np_language, STRING),
                                                USER_FIELD(np_age, U8),
                                                USER_FIELD(np_date_of_birth, STRING)};

#define USER_FIELD_COUNT (sizeof(user_fields) / sizeof(user_fields[0]))

static struct {
    struct user users[USER_MANAGER_MAX_USERS];
    size_t count;
    char commit_hash[64];
    /* indices into users + 1, 0 for none */
    unsigned char logged_in[USER_MANAGER_MAX_LOGGED_IN];
} user_manager;

/* A user as a default-constructed one used to be */
static void user_manager_init_user(struct user* user) {
    memset(user, 0, sizeof(*user));
    user->user_id = -1;
    user->user_color = 1;
    strlcpy(user->np_country, "us", sizeof(user->np_country));
    strlcpy(user->np_language, "en", sizeof(user->np_language));
    user->np_age = 30;
    strlcpy(user->np_date_of_birth, "1994-01-01", sizeof(user->np_date_of_birth));
}

static bool user_manager_home_dir(const struct user* user, char* s, size_t len) {
    char home[EMU_SETTINGS_PATH_MAX];
    char id[16];
    unsigned v = (unsigned)user->user_id;
    int i = (int)sizeof(id);
    if (emu_settings_get_home_dir(home, sizeof(home)) >= sizeof(home))
        return false;
    id[--i] = '\0';
    if (user->user_id < 0)
        v = 0u - v;
    do {
        id[--i] = (char)('0' + v % 10);
        v /= 10;
    } while (v);
    if (user->user_id < 0)
        id[--i] = '-';
    return fill_pathname_join_special(s, home, id + i, len) < len;
}

static void user_manager_make_home_dir(const struct user* user) {
    static const char* const subdirs[] = {"savedata", "trophy", "inputs"};
    char dir[USER_MANAGER_PATH_MAX];
    char sub[USER_MANAGER_PATH_MAX];
    size_t i;
    if (!user_manager_home_dir(user, dir, sizeof(dir)) || path_is_directory(dir))
        return;
    path_mkdir(dir);
    for (i = 0; i < sizeof(subdirs) / sizeof(subdirs[0]); i++) {
        fill_pathname_join_special(sub, dir, subdirs[i], sizeof(sub));
        path_mkdir(sub);
    }
}

static void user_manager_set_defaults(void) {
    static const char* const names[] = {"shadPS4", "shadPS4-2", "shadPS4-3", "shadPS4-4"};
    size_t i;
    user_manager.count = 4;
    for (i = 0; i < 4; i++) {
        struct user* user = &user_manager.users[i];
        user_manager_init_user(user);
        user->user_id = 1000 + (int32_t)i;
        strlcpy(user->user_name, names[i], sizeof(user->user_name));
        user->user_color = (uint32_t)i + 1;
        user->player_index = (int)i + 1;
        user_manager_make_home_dir(user);
    }
    user_manager.commit_hash[0] = '\0';
}

/* ---- Reading ---- */

static bool user_manager_read_field(rjson_t* json, enum rjson_type type,
                                    const struct user_field* field, struct user* user) {
    char* dst = (char*)user + field->offset;
    bool ok = false;
    int i;
    unsigned u;
    switch (field->type) {
    case USER_FIELD_S32:
        if ((ok = json_util_get_int(json, type, &i)))
            *(int32_t*)dst = (int32_t)i;
        break;
    case USER_FIELD_INT:
        ok = json_util_get_int(json, type, (int*)dst);
        break;
    case USER_FIELD_U32:
        if ((ok = json_util_get_uint(json, type, &u)))
            *(uint32_t*)dst = (uint32_t)u;
        break;
    case USER_FIELD_U8:
        if ((ok = json_util_get_uint(json, type, &u) && u <= 255))
            *(uint8_t*)dst = (uint8_t)u;
        break;
    case USER_FIELD_BOOL:
        ok = json_util_get_bool(type, (bool*)dst);
        break;
    case USER_FIELD_STRING:
        if ((ok = (type == RJSON_STRING)))
            json_util_get_string(json, dst, field->size);
        break;
    }
    if (!ok) {
        c_log(C_LOG_CONFIG, C_LOG_LEVEL_ERROR, "user_manager_load",
              "User setting %s has the wrong type; ignored", field->key);
        return json_util_skip(json, type);
    }
    return true;
}

static bool user_manager_read_user(rjson_t* json, struct user* user) {
    user_manager_init_user(user);
    for (;;) {
        size_t len, i;
        const char* name;
        enum rjson_type type = rjson_next(json);
        if (type == RJSON_OBJECT_END)
            return true;
        if (type != RJSON_STRING)
            return false;
        name = rjson_get_string(json, &len);
        for (i = 0; i < USER_FIELD_COUNT; i++)
            if (strlen(user_fields[i].key) == len && memcmp(user_fields[i].key, name, len) == 0)
                break;
        type = rjson_next(json);
        if (i == USER_FIELD_COUNT) {
            if (!json_util_skip(json, type))
                return false;
        } else if (!user_manager_read_field(json, type, &user_fields[i], user))
            return false;
    }
}

static bool user_manager_read_users(rjson_t* json, enum rjson_type type) {
    if (type != RJSON_ARRAY)
        return json_util_skip(json, type);
    user_manager.count = 0;
    for (;;) {
        type = rjson_next(json);
        if (type == RJSON_ARRAY_END)
            return true;
        if (type != RJSON_OBJECT) {
            if (!json_util_skip(json, type))
                return false;
            continue;
        }
        if (user_manager.count == USER_MANAGER_MAX_USERS) {
            c_log(C_LOG_CONFIG, C_LOG_LEVEL_ERROR, "user_manager_load",
                  "More than %d users; the rest are left out", USER_MANAGER_MAX_USERS);
            if (!json_util_skip(json, type))
                return false;
            continue;
        }
        if (!user_manager_read_user(json, &user_manager.users[user_manager.count]))
            return false;
        user_manager.count++;
    }
}

static bool user_manager_read(rjson_t* json) {
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
        if (!(len == 5 && memcmp(name, "Users", 5) == 0 && type == RJSON_OBJECT)) {
            if (!json_util_skip(json, type))
                return false;
            continue;
        }
        for (;;) {
            type = rjson_next(json);
            if (type == RJSON_OBJECT_END)
                break;
            if (type != RJSON_STRING)
                return false;
            name = rjson_get_string(json, &len);
            type = rjson_next(json);
            if (len == 4 && memcmp(name, "user", 4) == 0) {
                if (!user_manager_read_users(json, type))
                    return false;
            } else if (len == 11 && memcmp(name, "commit_hash", 11) == 0 && type == RJSON_STRING)
                json_util_get_string(json, user_manager.commit_hash,
                                     sizeof(user_manager.commit_hash));
            else if (!json_util_skip(json, type))
                return false;
        }
    }
}

static bool user_manager_path(char* s, size_t len) {
    char dir[EMU_SETTINGS_PATH_MAX];
    if (c_get_user_path(C_USER_PATH_USER, dir, sizeof(dir)) >= sizeof(dir))
        return false;
    return fill_pathname_join_special(s, dir, "users.json", len) < len;
}

bool user_manager_load(void) {
    char path[USER_MANAGER_PATH_MAX];
    json_util_reader_t reader;
    bool ok;

    memset(user_manager.logged_in, 0, sizeof(user_manager.logged_in));
    if (!user_manager_path(path, sizeof(path)))
        return false;
    if (!json_util_reader_open(&reader, path)) {
        if (user_manager.count == 0)
            user_manager_set_defaults();
        user_manager_save();
        return false;
    }
    user_manager_set_defaults();
    ok = user_manager_read(reader.json);
    if (!ok) {
        c_log(C_LOG_CONFIG, C_LOG_LEVEL_ERROR, "user_manager_load",
              "Error loading user settings from %s: %s", path, rjson_get_error(reader.json));
        user_manager_set_defaults();
    }
    json_util_reader_close(&reader);
    if (!string_is_equal(user_manager.commit_hash, c_get_scm_rev()))
        user_manager_save();
    return ok;
}

/* ---- Writing ---- */

static void user_manager_write_field(rjsonwriter_t* writer, const struct user_field* field,
                                     const struct user* user) {
    const char* src = (const char*)user + field->offset;
    switch (field->type) {
    case USER_FIELD_S32:
        rjsonwriter_add_int(writer, (int)*(const int32_t*)src);
        break;
    case USER_FIELD_INT:
        rjsonwriter_add_int(writer, *(const int*)src);
        break;
    case USER_FIELD_U32:
        rjsonwriter_add_unsigned(writer, (unsigned)*(const uint32_t*)src);
        break;
    case USER_FIELD_U8:
        rjsonwriter_add_unsigned(writer, (unsigned)*(const uint8_t*)src);
        break;
    case USER_FIELD_BOOL:
        rjsonwriter_add_bool(writer, *(const bool*)src);
        break;
    case USER_FIELD_STRING:
        rjsonwriter_add_string(writer, src);
        break;
    }
}

/* Writes the members of "Users" this build knows, at indent 2 */
static void user_manager_write_users(rjsonwriter_t* writer) {
    size_t i, f;
    json_util_write_key(writer, 2, "user", true);
    rjsonwriter_add_start_array(writer);
    for (i = 0; i < user_manager.count; i++) {
        if (i)
            rjsonwriter_add_comma(writer);
        rjsonwriter_add_newline(writer);
        rjsonwriter_add_spaces(writer, 6);
        rjsonwriter_add_start_object(writer);
        for (f = 0; f < USER_FIELD_COUNT; f++) {
            json_util_write_key(writer, 4, user_fields[f].key, f == 0);
            user_manager_write_field(writer, &user_fields[f], &user_manager.users[i]);
        }
        json_util_write_end(writer, 4, false, '}');
    }
    json_util_write_end(writer, 3, user_manager.count == 0, ']');
    json_util_write_key(writer, 2, "commit_hash", false);
    rjsonwriter_add_string(writer, c_get_scm_rev());
}

/* Writes users.json, keeping what existing (when not NULL) has that this
 * build doesn't write. */
static bool user_manager_write(rjsonwriter_t* writer, rjson_t* existing) {
    bool first = true;
    bool written = false;

    rjsonwriter_add_start_object(writer);
    if (existing) {
        if (rjson_next(existing) != RJSON_OBJECT)
            return false;
        for (;;) {
            size_t len;
            const char* name;
            char key[USER_MANAGER_STRING_MAX];
            enum rjson_type type = rjson_next(existing);
            bool is_users;

            if (type == RJSON_OBJECT_END)
                break;
            if (type != RJSON_STRING)
                return false;
            name = rjson_get_string(existing, &len);
            is_users = (len == 5 && memcmp(name, "Users", 5) == 0);
            if (len >= sizeof(key) || (is_users && written)) {
                if (!json_util_skip(existing, rjson_next(existing)))
                    return false;
                continue;
            }
            memcpy(key, name, len);
            key[len] = '\0';
            json_util_write_key(writer, 1, key, first);
            first = false;
            type = rjson_next(existing);
            if (!is_users) {
                if (!json_util_copy_value(existing, type, writer, 1))
                    return false;
                continue;
            }
            rjsonwriter_add_start_object(writer);
            user_manager_write_users(writer);
            written = true;
            if (type != RJSON_OBJECT) {
                if (!json_util_skip(existing, type))
                    return false;
            } else
                for (;;) {
                    type = rjson_next(existing);
                    if (type == RJSON_OBJECT_END)
                        break;
                    if (type != RJSON_STRING)
                        return false;
                    name = rjson_get_string(existing, &len);
                    if ((len == 4 && memcmp(name, "user", 4) == 0) ||
                        (len == 11 && memcmp(name, "commit_hash", 11) == 0) || len >= sizeof(key)) {
                        if (!json_util_skip(existing, rjson_next(existing)))
                            return false;
                        continue;
                    }
                    memcpy(key, name, len);
                    key[len] = '\0';
                    json_util_write_key(writer, 2, key, false);
                    if (!json_util_copy_value(existing, rjson_next(existing), writer, 2))
                        return false;
                }
            json_util_write_end(writer, 2, false, '}');
        }
        if (rjson_next(existing) != RJSON_DONE)
            return false;
    }
    if (!written) {
        json_util_write_key(writer, 1, "Users", first);
        rjsonwriter_add_start_object(writer);
        user_manager_write_users(writer);
        json_util_write_end(writer, 2, false, '}');
    }
    json_util_write_end(writer, 1, false, '}');
    return true;
}

bool user_manager_save(void) {
    char path[USER_MANAGER_PATH_MAX];
    json_util_writer_t out;
    json_util_reader_t reader;
    bool have_existing;
    bool ok;

    if (!user_manager_path(path, sizeof(path)))
        return false;
    have_existing = json_util_reader_open(&reader, path);
    if (!json_util_writer_open(&out, path)) {
        if (have_existing)
            json_util_reader_close(&reader);
        c_log(C_LOG_CONFIG, C_LOG_LEVEL_ERROR, "user_manager_save",
              "Failed to open user settings for writing: %s", path);
        return false;
    }
    ok = user_manager_write(out.writer, have_existing ? reader.json : NULL);
    if (have_existing)
        json_util_reader_close(&reader);
    if (!ok) {
        /* A bad file is dropped and written afresh */
        json_util_writer_close(&out, path, false);
        if (!json_util_writer_open(&out, path))
            return false;
        ok = user_manager_write(out.writer, NULL);
    }
    if (!json_util_writer_close(&out, path, ok)) {
        c_log(C_LOG_CONFIG, C_LOG_LEVEL_ERROR, "user_manager_save",
              "Failed to write user settings: %s", path);
        return false;
    }
    strlcpy(user_manager.commit_hash, c_get_scm_rev(), sizeof(user_manager.commit_hash));
    return true;
}

/* ---- Users ---- */

size_t user_manager_count(void) {
    return user_manager.count;
}

struct user* user_manager_at(size_t index) {
    return index < user_manager.count ? &user_manager.users[index] : NULL;
}

struct user* user_manager_get_by_id(int32_t user_id) {
    size_t i;
    for (i = 0; i < user_manager.count; i++)
        if (user_manager.users[i].user_id == user_id)
            return &user_manager.users[i];
    return NULL;
}

struct user* user_manager_get_by_player_index(int32_t player_index) {
    size_t i;
    for (i = 0; i < user_manager.count; i++)
        if (user_manager.users[i].player_index == player_index)
            return &user_manager.users[i];
    return NULL;
}

struct user* user_manager_get_default_user(void) {
    return user_manager_get_by_player_index(1);
}

bool user_manager_has_home_dir(const struct user* user) {
    char dir[USER_MANAGER_PATH_MAX];
    return user_manager_home_dir(user, dir, sizeof(dir)) && path_is_directory(dir);
}

struct user* user_manager_get_logged_in(unsigned slot) {
    if (slot >= USER_MANAGER_MAX_LOGGED_IN || !user_manager.logged_in[slot])
        return NULL;
    return &user_manager.users[user_manager.logged_in[slot] - 1];
}

bool user_manager_login(struct user* user, int32_t player_index) {
    unsigned index;
    unsigned slot;
    if (!user || player_index < 1 || player_index > USER_MANAGER_MAX_LOGGED_IN)
        return false;
    index = (unsigned)(user - user_manager.users) + 1;
    /* A controller logging in a user already on its slot (the first user is
     * logged in at boot with no controller connected, then one connects)
     * changes nothing */
    if (user_manager.logged_in[player_index - 1] == index)
        return false;
    for (slot = 0; slot < USER_MANAGER_MAX_LOGGED_IN; slot++) {
        if (user_manager.logged_in[slot] == index) {
            c_log(C_LOG_CONFIG, C_LOG_LEVEL_CRITICAL, "user_manager_login",
                  "User %d is already logged in on player %u", (int)user->user_id, slot + 1);
            return false;
        }
    }
    user->logged_in = true;
    user->player_index = player_index;
    user_manager.logged_in[player_index - 1] = (unsigned char)index;
    return true;
}

bool user_manager_logout(struct user* user) {
    if (!user)
        return false;
    user->logged_in = false;
    if (user->player_index >= 1 && user->player_index <= USER_MANAGER_MAX_LOGGED_IN)
        user_manager.logged_in[user->player_index - 1] = 0;
    return true;
}
