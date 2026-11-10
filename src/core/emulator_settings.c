/* SPDX-FileCopyrightText: Copyright 2025-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <compat/strl.h>
#include <file/file_path.h>
#include <formats/rjson.h>
#include <formats/rjson_helpers.h>
#include <retro_atomic.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>

#include "common/c_bridge.h"
#include "common/json_util.h"
#include "core/emulator_settings.h"

enum emu_settings_type {
    EMU_SETTINGS_TYPE_BOOL = 0,
    EMU_SETTINGS_TYPE_INT,
    EMU_SETTINGS_TYPE_UINT,
    EMU_SETTINGS_TYPE_U64,
    EMU_SETTINGS_TYPE_DOUBLE,
    EMU_SETTINGS_TYPE_STRING,
    EMU_SETTINGS_TYPE_PATH
};

#define EMU_SETTINGS_MEMBER_BOOL(name) bool name;
#define EMU_SETTINGS_MEMBER_INT(name) int name;
#define EMU_SETTINGS_MEMBER_UINT(name) unsigned name;
#define EMU_SETTINGS_MEMBER_U64(name) uint64_t name;
#define EMU_SETTINGS_MEMBER_DOUBLE(name) double name;
#define EMU_SETTINGS_MEMBER_STRING(name) char name[EMU_SETTINGS_STRING_MAX];
#define EMU_SETTINGS_MEMBER_PATH(name) char name[EMU_SETTINGS_PATH_MAX];

struct emu_settings_values {
#define EMU_SETTING(name, key, section, type, def, flags, acc) EMU_SETTINGS_MEMBER_##type(name)
    EMU_SETTINGS_LIST
#undef EMU_SETTING
};

struct emu_settings_desc {
    const char* key;
    unsigned char section;
    unsigned char type;
    unsigned char flags;
    size_t offset;
    size_t size;
};

static const struct emu_settings_desc emu_settings_descs[EMU_SETTING_ID_COUNT] = {
#define EMU_SETTING(name, key, section, type, def, flags, acc)                                     \
    {key,                                                                                          \
     EMU_SECTION_##section,                                                                        \
     EMU_SETTINGS_TYPE_##type,                                                                     \
     EMU_SETTINGS_FLAG_##flags,                                                                    \
     offsetof(struct emu_settings_values, name),                                                   \
     sizeof(((struct emu_settings_values*)0)->name)},
    EMU_SETTINGS_LIST
#undef EMU_SETTING
};

static const struct emu_settings_values emu_settings_defaults = {
#define EMU_SETTING(name, key, section, type, def, flags, acc) def,
    EMU_SETTINGS_LIST
#undef EMU_SETTING
};

static const char* const emu_settings_section_names[EMU_SECTION_COUNT] = {
    "General", "Network", "Log", "Debug", "Input", "GPU", "Vulkan"};

#define EMU_SETTINGS_FILE_PATH_MAX (EMU_SETTINGS_PATH_MAX + 64)

static struct {
    struct emu_settings_values value;
    struct emu_settings_values game;
    bool has_game[EMU_SETTING_ID_COUNT];
    struct emu_settings_install_dir install_dirs[EMU_SETTINGS_INSTALL_DIRS_MAX];
    size_t install_dir_count;
    enum emu_config_mode mode;
    bool initialized;
} emu_settings;

/* Turned on for the rest of a run from network threads */
static retro_atomic_int_t emu_settings_shadnet_session_disabled;

static void emu_settings_init(void) {
    if (emu_settings.initialized)
        return;
    emu_settings.value = emu_settings_defaults;
    emu_settings.initialized = true;
}

static const struct emu_settings_values* emu_settings_pick(enum emu_setting_id id) {
    if (!emu_settings.initialized)
        emu_settings_init();
    switch (emu_settings.mode) {
    case EMU_CONFIG_MODE_DEFAULT:
        return emu_settings.has_game[id] ? &emu_settings.game : &emu_settings.value;
    case EMU_CONFIG_MODE_CLEAN:
        return &emu_settings_defaults;
    default:
        break;
    }
    return &emu_settings.value;
}

static struct emu_settings_values* emu_settings_target(enum emu_setting_id id, bool game_specific) {
    if (!emu_settings.initialized)
        emu_settings_init();
    if (game_specific) {
        emu_settings.has_game[id] = true;
        return &emu_settings.game;
    }
    return &emu_settings.value;
}

#define EMU_SETTINGS_ASSIGN_BOOL(dst, v) (dst) = (v)
#define EMU_SETTINGS_ASSIGN_INT(dst, v) (dst) = (v)
#define EMU_SETTINGS_ASSIGN_UINT(dst, v) (dst) = (v)
#define EMU_SETTINGS_ASSIGN_U64(dst, v) (dst) = (v)
#define EMU_SETTINGS_ASSIGN_DOUBLE(dst, v) (dst) = (v)
#define EMU_SETTINGS_ASSIGN_STRING(dst, v) strlcpy((dst), (v) ? (v) : "", sizeof(dst))

#define EMU_SETTINGS_DEF_GEN(name, type)                                                           \
    EMU_SETTINGS_CTYPE_##type emu_settings_get_##name(void) {                                      \
        return emu_settings_pick(EMU_SETTING_ID_##name)->name;                                     \
    }                                                                                              \
    void emu_settings_set_##name(EMU_SETTINGS_CTYPE_##type value, bool game_specific) {            \
        struct emu_settings_values* dst =                                                          \
            emu_settings_target(EMU_SETTING_ID_##name, game_specific);                             \
        EMU_SETTINGS_ASSIGN_##type(dst->name, value);                                              \
    }
#define EMU_SETTINGS_DEF_CUSTOM(name, type)
#define EMU_SETTING(name, key, section, type, def, flags, acc) EMU_SETTINGS_DEF_##acc(name, type)
EMU_SETTINGS_LIST
#undef EMU_SETTING

/* ---- Directories ---- */

static size_t emu_settings_dir(const char* configured, enum c_user_path fallback, const char* sub,
                               char* s, size_t len) {
    char dir[EMU_SETTINGS_PATH_MAX];
    if (configured[0])
        return strlcpy(s, configured, len);
    if (!sub)
        return c_get_user_path(fallback, s, len);
    if (c_get_user_path(fallback, dir, sizeof(dir)) >= sizeof(dir)) {
        if (len)
            s[0] = '\0';
        return len;
    }
    return fill_pathname_join_special(s, dir, sub, len);
}

size_t emu_settings_get_addon_install_dir(char* s, size_t len) {
    emu_settings_init();
    return emu_settings_dir(emu_settings.value.addon_install_dir, C_USER_PATH_USER, "addcont", s,
                            len);
}

size_t emu_settings_get_home_dir(char* s, size_t len) {
    emu_settings_init();
    return emu_settings_dir(emu_settings.value.home_dir, C_USER_PATH_HOME, NULL, s, len);
}

size_t emu_settings_get_sys_modules_dir(char* s, size_t len) {
    emu_settings_init();
    return emu_settings_dir(emu_settings.value.sys_modules_dir, C_USER_PATH_SYS_MODULE, NULL, s,
                            len);
}

size_t emu_settings_get_font_dir(char* s, size_t len) {
    emu_settings_init();
    return emu_settings_dir(emu_settings.value.font_dir, C_USER_PATH_FONTS, NULL, s, len);
}

void emu_settings_set_addon_install_dir(const char* dir) {
    emu_settings_init();
    strlcpy(emu_settings.value.addon_install_dir, dir,
            sizeof(emu_settings.value.addon_install_dir));
}

void emu_settings_set_home_dir(const char* dir) {
    emu_settings_init();
    strlcpy(emu_settings.value.home_dir, dir, sizeof(emu_settings.value.home_dir));
}

void emu_settings_set_sys_modules_dir(const char* dir) {
    emu_settings_init();
    strlcpy(emu_settings.value.sys_modules_dir, dir, sizeof(emu_settings.value.sys_modules_dir));
}

void emu_settings_set_font_dir(const char* dir) {
    emu_settings_init();
    strlcpy(emu_settings.value.font_dir, dir, sizeof(emu_settings.value.font_dir));
}

/* ---- Special settings ---- */

unsigned emu_settings_get_vblank_frequency(void) {
    emu_settings_init();
    if (emu_settings.value.vblank_frequency < 30)
        return 30;
    return emu_settings_pick(EMU_SETTING_ID_vblank_frequency)->vblank_frequency;
}

void emu_settings_set_vblank_frequency(unsigned value, bool game_specific) {
    emu_settings_target(EMU_SETTING_ID_vblank_frequency, game_specific)->vblank_frequency =
        value < 30 ? 30 : value;
}

bool emu_settings_is_shadnet_enabled(void) {
    return emu_settings_get_shad_net_enabled() &&
           !retro_atomic_load_relaxed_int(&emu_settings_shadnet_session_disabled);
}

void emu_settings_set_shadnet_session_disabled(bool disabled) {
    retro_atomic_store_relaxed_int(&emu_settings_shadnet_session_disabled, disabled ? 1 : 0);
}

bool emu_settings_is_shadnet_session_disabled(void) {
    return retro_atomic_load_relaxed_int(&emu_settings_shadnet_session_disabled) != 0;
}

/* ---- Install directories ---- */

size_t emu_settings_install_dir_count(void) {
    return emu_settings.install_dir_count;
}

const struct emu_settings_install_dir* emu_settings_install_dir_at(size_t index) {
    return index < emu_settings.install_dir_count ? &emu_settings.install_dirs[index] : NULL;
}

static struct emu_settings_install_dir* emu_settings_find_install_dir(const char* path) {
    size_t i;
    for (i = 0; i < emu_settings.install_dir_count; i++)
        if (string_is_equal(emu_settings.install_dirs[i].path, path))
            return &emu_settings.install_dirs[i];
    return NULL;
}

bool emu_settings_add_install_dir(const char* path, bool enabled) {
    struct emu_settings_install_dir* dir;
    if (emu_settings_find_install_dir(path) ||
        emu_settings.install_dir_count == EMU_SETTINGS_INSTALL_DIRS_MAX)
        return false;
    dir = &emu_settings.install_dirs[emu_settings.install_dir_count++];
    strlcpy(dir->path, path, sizeof(dir->path));
    dir->enabled = enabled;
    return true;
}

void emu_settings_remove_install_dir(const char* path) {
    struct emu_settings_install_dir* dir = emu_settings_find_install_dir(path);
    struct emu_settings_install_dir* end;
    if (!dir)
        return;
    end = emu_settings.install_dirs + emu_settings.install_dir_count;
    memmove(dir, dir + 1, (size_t)(end - dir - 1) * sizeof(*dir));
    emu_settings.install_dir_count--;
}

void emu_settings_set_install_dir_enabled(const char* path, bool enabled) {
    struct emu_settings_install_dir* dir = emu_settings_find_install_dir(path);
    if (dir)
        dir->enabled = enabled;
}

void emu_settings_clear_install_dirs(void) {
    emu_settings.install_dir_count = 0;
}

/* ---- Modes and resets ---- */

enum emu_config_mode emu_settings_get_config_mode(void) {
    return emu_settings.mode;
}

void emu_settings_set_config_mode(enum emu_config_mode mode) {
    emu_settings.mode = mode;
}

void emu_settings_set_defaults(void) {
    emu_settings.value = emu_settings_defaults;
    emu_settings.install_dir_count = 0;
    emu_settings.initialized = true;
}

void emu_settings_clear_game_overrides(void) {
    memset(emu_settings.has_game, 0, sizeof(emu_settings.has_game));
}

/* ---- Reading ---- */

static int emu_settings_find(unsigned section, const char* key, size_t len) {
    int i;
    for (i = 0; i < EMU_SETTING_ID_COUNT; i++) {
        const struct emu_settings_desc* desc = &emu_settings_descs[i];
        if (desc->section == section && strlen(desc->key) == len &&
            memcmp(desc->key, key, len) == 0)
            return i;
    }
    return -1;
}

static int emu_settings_find_section(const char* name, size_t len) {
    int i;
    for (i = 0; i < EMU_SECTION_COUNT; i++)
        if (strlen(emu_settings_section_names[i]) == len &&
            memcmp(emu_settings_section_names[i], name, len) == 0)
            return i;
    return -1;
}

/* Reads the value rjson_next returned (type) into the setting desc of
 * values. A value of the wrong type is logged and left out. */
static bool emu_settings_read_value(rjson_t* json, enum rjson_type type,
                                    const struct emu_settings_desc* desc,
                                    struct emu_settings_values* values) {
    char* dst = (char*)values + desc->offset;
    bool ok = false;
    double d;

    switch (desc->type) {
    case EMU_SETTINGS_TYPE_BOOL:
        ok = json_util_get_bool(type, (bool*)dst);
        break;
    case EMU_SETTINGS_TYPE_INT:
        ok = json_util_get_int(json, type, (int*)dst);
        if (!ok && json_util_get_double(json, type, &d)) {
            *(int*)dst = (int)d;
            ok = true;
        }
        break;
    case EMU_SETTINGS_TYPE_UINT:
        ok = json_util_get_uint(json, type, (unsigned*)dst);
        if (!ok && json_util_get_double(json, type, &d) && d >= 0) {
            *(unsigned*)dst = (unsigned)d;
            ok = true;
        }
        break;
    case EMU_SETTINGS_TYPE_U64:
        ok = json_util_get_u64(json, type, (uint64_t*)dst);
        if (!ok && json_util_get_double(json, type, &d) && d >= 0) {
            *(uint64_t*)dst = (uint64_t)d;
            ok = true;
        }
        break;
    case EMU_SETTINGS_TYPE_DOUBLE:
        ok = json_util_get_double(json, type, (double*)dst);
        break;
    case EMU_SETTINGS_TYPE_STRING:
    case EMU_SETTINGS_TYPE_PATH:
        if (type == RJSON_STRING) {
            json_util_get_string(json, dst, desc->size);
            ok = true;
        }
        break;
    }
    if (!ok) {
        c_log(C_LOG_CONFIG, C_LOG_LEVEL_ERROR, "emu_settings_read_value",
              "Setting %s.%s has the wrong type; ignored",
              emu_settings_section_names[desc->section], desc->key);
        return json_util_skip(json, type);
    }
    return true;
}

static bool emu_settings_read_install_dir(rjson_t* json) {
    struct emu_settings_install_dir dir;
    dir.path[0] = '\0';
    dir.enabled = true;
    for (;;) {
        size_t len;
        const char* name;
        enum rjson_type type = rjson_next(json);
        if (type == RJSON_OBJECT_END)
            break;
        if (type != RJSON_STRING)
            return false;
        name = rjson_get_string(json, &len);
        if (len == 4 && memcmp(name, "path", 4) == 0) {
            type = rjson_next(json);
            if (type == RJSON_STRING)
                json_util_get_string(json, dir.path, sizeof(dir.path));
            else if (!json_util_skip(json, type))
                return false;
        } else if (len == 7 && memcmp(name, "enabled", 7) == 0) {
            type = rjson_next(json);
            if (!json_util_get_bool(type, &dir.enabled) && !json_util_skip(json, type))
                return false;
        } else if (!json_util_skip(json, rjson_next(json)))
            return false;
    }
    if (dir.path[0] && !emu_settings_find_install_dir(dir.path) &&
        emu_settings.install_dir_count < EMU_SETTINGS_INSTALL_DIRS_MAX)
        emu_settings.install_dirs[emu_settings.install_dir_count++] = dir;
    return true;
}

static bool emu_settings_read_install_dirs(rjson_t* json, enum rjson_type type) {
    if (type != RJSON_ARRAY)
        return json_util_skip(json, type);
    emu_settings.install_dir_count = 0;
    for (;;) {
        type = rjson_next(json);
        if (type == RJSON_ARRAY_END)
            return true;
        if (type == RJSON_OBJECT) {
            if (!emu_settings_read_install_dir(json))
                return false;
        } else if (!json_util_skip(json, type))
            return false;
    }
}

/* Reads one section object. With game set, only overridable settings are
 * taken, as game-specific values. */
static bool emu_settings_read_section(rjson_t* json, unsigned section, bool game) {
    for (;;) {
        size_t len;
        const char* name;
        int id;
        enum rjson_type type = rjson_next(json);

        if (type == RJSON_OBJECT_END)
            return true;
        if (type != RJSON_STRING)
            return false;
        name = rjson_get_string(json, &len);
        id = emu_settings_find(section, name, len);
        if (!game && section == EMU_SECTION_GENERAL && len == 12 &&
            memcmp(name, "install_dirs", 12) == 0) {
            if (!emu_settings_read_install_dirs(json, rjson_next(json)))
                return false;
            continue;
        }
        type = rjson_next(json);
        if (id < 0 || (game && !(emu_settings_descs[id].flags & EMU_SETTINGS_FLAG_OVR))) {
            if (!json_util_skip(json, type))
                return false;
            continue;
        }
        if (game) {
            const struct emu_settings_desc* desc = &emu_settings_descs[id];
            if (!emu_settings_read_value(json, type, desc, &emu_settings.game))
                return false;
            emu_settings.has_game[id] = true;
            c_log(C_LOG_CONFIG, C_LOG_LEVEL_DEBUG, "emu_settings_load_game",
                  "Game-specific override: %s.%s", emu_settings_section_names[desc->section],
                  desc->key);
        } else if (!emu_settings_read_value(json, type, &emu_settings_descs[id],
                                            &emu_settings.value))
            return false;
    }
}

static bool emu_settings_read(rjson_t* json, bool game) {
    if (rjson_next(json) != RJSON_OBJECT)
        return false;
    for (;;) {
        size_t len;
        const char* name;
        int section;
        enum rjson_type type = rjson_next(json);
        if (type == RJSON_OBJECT_END)
            return rjson_next(json) == RJSON_DONE;
        if (type != RJSON_STRING)
            return false;
        name = rjson_get_string(json, &len);
        section = emu_settings_find_section(name, len);
        type = rjson_next(json);
        if (section >= 0 && type == RJSON_OBJECT) {
            if (!emu_settings_read_section(json, (unsigned)section, game))
                return false;
        } else if (!json_util_skip(json, type))
            return false;
    }
}

static bool emu_settings_config_path(char* s, size_t len) {
    char dir[EMU_SETTINGS_PATH_MAX];
    if (c_get_user_path(C_USER_PATH_USER, dir, sizeof(dir)) >= sizeof(dir))
        return false;
    return fill_pathname_join_special(s, dir, "config.json", len) < len;
}

static bool emu_settings_game_path(char* s, size_t len, const char* serial, bool make_dir) {
    char dir[EMU_SETTINGS_PATH_MAX];
    char name[EMU_SETTINGS_STRING_MAX];
    if (c_get_user_path(C_USER_PATH_CUSTOM_CONFIGS, dir, sizeof(dir)) >= sizeof(dir))
        return false;
    if (make_dir && !path_is_directory(dir))
        path_mkdir(dir);
    strlcpy(name, serial, sizeof(name));
    strlcat(name, ".json", sizeof(name));
    return fill_pathname_join_special(s, dir, name, len) < len;
}

bool emu_settings_load(void) {
    char path[EMU_SETTINGS_FILE_PATH_MAX];
    json_util_reader_t reader;

    emu_settings_init();
    /* A newly loaded profile replaces, rather than extends, the last one */
    emu_settings_clear_game_overrides();

    if (!emu_settings_config_path(path, sizeof(path)))
        return false;
    if (!json_util_reader_open(&reader, path)) {
        emu_settings_set_defaults();
        emu_settings_save();
        return true;
    }
    if (!emu_settings_read(reader.json, false))
        c_log(C_LOG_CONFIG, C_LOG_LEVEL_ERROR, "emu_settings_load",
              "Error loading settings from %s: %s", path, rjson_get_error(reader.json));
    json_util_reader_close(&reader);

    if (!string_is_equal(emu_settings.value.config_version, c_get_scm_rev()))
        emu_settings_save();
    return true;
}

bool emu_settings_load_game(const char* serial) {
    char path[EMU_SETTINGS_FILE_PATH_MAX];
    json_util_reader_t reader;
    bool ok;

    emu_settings_init();
    emu_settings_clear_game_overrides();
    if (!emu_settings_game_path(path, sizeof(path), serial, false) ||
        !json_util_reader_open(&reader, path))
        return false;
    ok = emu_settings_read(reader.json, true);
    if (!ok)
        c_log(C_LOG_CONFIG, C_LOG_LEVEL_ERROR, "emu_settings_load_game",
              "Error loading game settings from %s: %s", path, rjson_get_error(reader.json));
    json_util_reader_close(&reader);
    return true;
}

/* ---- Writing ---- */

static void emu_settings_write_double(rjsonwriter_t* writer, double value) {
    char buf[40];
    char* p;
    double back;
    if (value != value || value - value != 0) {
        /* NaN and infinities have no JSON form */
        rjsonwriter_add_null(writer);
        return;
    }
    /* Shortest of 15 or 17 digits that reads back the same */
    sprintf(buf, "%.15g", value);
    for (p = buf; *p; p++)
        if (*p == ',')
            *p = '.';
    back = 0;
    if (sscanf(buf, "%lf", &back) != 1 || back != value)
        sprintf(buf, "%.17g", value);
    for (p = buf; *p; p++)
        if (*p == ',')
            *p = '.';
    rjsonwriter_raw(writer, buf, (int)strlen(buf));
}

static void emu_settings_write_value(rjsonwriter_t* writer, const struct emu_settings_desc* desc,
                                     const struct emu_settings_values* values) {
    const char* src = (const char*)values + desc->offset;
    switch (desc->type) {
    case EMU_SETTINGS_TYPE_BOOL:
        rjsonwriter_add_bool(writer, *(const bool*)src);
        break;
    case EMU_SETTINGS_TYPE_INT:
        rjsonwriter_add_int(writer, *(const int*)src);
        break;
    case EMU_SETTINGS_TYPE_UINT:
        rjsonwriter_add_unsigned(writer, *(const unsigned*)src);
        break;
    case EMU_SETTINGS_TYPE_U64:
        json_util_write_u64(writer, *(const uint64_t*)src);
        break;
    case EMU_SETTINGS_TYPE_DOUBLE:
        emu_settings_write_double(writer, *(const double*)src);
        break;
    case EMU_SETTINGS_TYPE_STRING:
    case EMU_SETTINGS_TYPE_PATH:
        rjsonwriter_add_string(writer, src);
        break;
    }
}

static void emu_settings_write_install_dirs(rjsonwriter_t* writer, int indent) {
    size_t i;
    rjsonwriter_add_start_array(writer);
    for (i = 0; i < emu_settings.install_dir_count; i++) {
        if (i)
            rjsonwriter_add_comma(writer);
        rjsonwriter_add_newline(writer);
        rjsonwriter_add_spaces(writer, (indent + 1) * 2);
        rjsonwriter_add_start_object(writer);
        json_util_write_key(writer, indent + 2, "path", true);
        rjsonwriter_add_string(writer, emu_settings.install_dirs[i].path);
        json_util_write_key(writer, indent + 2, "enabled", false);
        rjsonwriter_add_bool(writer, emu_settings.install_dirs[i].enabled);
        json_util_write_end(writer, indent + 2, false, '}');
    }
    json_util_write_end(writer, indent + 1, emu_settings.install_dir_count == 0, ']');
}

/* Writes the members of section this build knows, at indent. With game
 * set, only the overridable ones, from their game-specific values when
 * there are some. Returns whether anything was written. */
static bool emu_settings_write_members(rjsonwriter_t* writer, unsigned section, bool game,
                                       int indent) {
    bool first = true;
    int i;
    if (!game && section == EMU_SECTION_GENERAL) {
        json_util_write_key(writer, indent, "install_dirs", true);
        emu_settings_write_install_dirs(writer, indent);
        first = false;
    }
    for (i = 0; i < EMU_SETTING_ID_COUNT; i++) {
        const struct emu_settings_desc* desc = &emu_settings_descs[i];
        if (desc->section != section)
            continue;
        if (game && !(desc->flags & EMU_SETTINGS_FLAG_OVR))
            continue;
        json_util_write_key(writer, indent, desc->key, first);
        emu_settings_write_value(writer, desc,
                                 (game && emu_settings.has_game[i]) ? &emu_settings.game
                                                                    : &emu_settings.value);
        first = false;
    }
    return !first;
}

/* A member of an old General section that is now a Network setting */
static bool emu_settings_is_moved_network_key(const char* name, size_t len) {
    int id = emu_settings_find(EMU_SECTION_NETWORK, name, len);
    return id >= 0 && (emu_settings_descs[id].flags & EMU_SETTINGS_FLAG_OVR);
}

/* Copies the rest of an existing section object, leaving out the members
 * this build writes itself. *first tracks the comma. */
static bool emu_settings_merge_section(rjson_t* json, rjsonwriter_t* writer, unsigned section,
                                       bool* first) {
    for (;;) {
        size_t len;
        const char* name;
        char key[EMU_SETTINGS_STRING_MAX];
        enum rjson_type type = rjson_next(json);

        if (type == RJSON_OBJECT_END)
            return true;
        if (type != RJSON_STRING)
            return false;
        name = rjson_get_string(json, &len);
        if (emu_settings_find(section, name, len) >= 0 ||
            (section == EMU_SECTION_GENERAL &&
             ((len == 12 && memcmp(name, "install_dirs", 12) == 0) ||
              emu_settings_is_moved_network_key(name, len)))) {
            if (!json_util_skip(json, rjson_next(json)))
                return false;
            continue;
        }
        if (len >= sizeof(key)) {
            /* not ours, and too long to have been written by us */
            if (!json_util_skip(json, rjson_next(json)))
                return false;
            continue;
        }
        memcpy(key, name, len);
        key[len] = '\0';
        json_util_write_key(writer, 2, key, *first);
        *first = false;
        if (!json_util_copy_value(json, rjson_next(json), writer, 2))
            return false;
    }
}

static void emu_settings_write_section(rjsonwriter_t* writer, unsigned section, bool game,
                                       bool first) {
    bool any;
    json_util_write_key(writer, 1, emu_settings_section_names[section], first);
    rjsonwriter_add_start_object(writer);
    any = emu_settings_write_members(writer, section, game, 2);
    json_util_write_end(writer, 2, !any, '}');
}

/* Writes config.json, merging the members of existing (when not NULL)
 * that this build doesn't know. */
static bool emu_settings_write_global(rjsonwriter_t* writer, rjson_t* existing) {
    bool written[EMU_SECTION_COUNT];
    bool first = true;
    unsigned i;

    memset(written, 0, sizeof(written));
    rjsonwriter_add_start_object(writer);
    if (existing) {
        if (rjson_next(existing) != RJSON_OBJECT)
            return false;
        for (;;) {
            size_t len;
            const char* name;
            char key[EMU_SETTINGS_STRING_MAX];
            int section;
            enum rjson_type type = rjson_next(existing);

            if (type == RJSON_OBJECT_END)
                break;
            if (type != RJSON_STRING)
                return false;
            name = rjson_get_string(existing, &len);
            section = emu_settings_find_section(name, len);
            if (len >= sizeof(key)) {
                if (!json_util_skip(existing, rjson_next(existing)))
                    return false;
                continue;
            }
            memcpy(key, name, len);
            key[len] = '\0';
            type = rjson_next(existing);
            if (section < 0) {
                json_util_write_key(writer, 1, key, first);
                if (!json_util_copy_value(existing, type, writer, 1))
                    return false;
            } else if (written[section]) {
                if (!json_util_skip(existing, type))
                    return false;
                continue;
            } else {
                bool member_first;
                json_util_write_key(writer, 1, key, first);
                rjsonwriter_add_start_object(writer);
                member_first = !emu_settings_write_members(writer, (unsigned)section, false, 2);
                if (type == RJSON_OBJECT) {
                    if (!emu_settings_merge_section(existing, writer, (unsigned)section,
                                                    &member_first))
                        return false;
                } else if (!json_util_skip(existing, type))
                    return false;
                json_util_write_end(writer, 2, member_first, '}');
                written[section] = true;
            }
            first = false;
        }
        if (rjson_next(existing) != RJSON_DONE)
            return false;
    }
    for (i = 0; i < EMU_SECTION_COUNT; i++) {
        if (written[i])
            continue;
        emu_settings_write_section(writer, i, false, first);
        first = false;
    }
    json_util_write_end(writer, 1, false, '}');
    return true;
}

bool emu_settings_save(void) {
    char path[EMU_SETTINGS_FILE_PATH_MAX];
    json_util_writer_t out;
    json_util_reader_t reader;
    bool have_existing;
    bool ok;

    emu_settings_init();
    if (!emu_settings_config_path(path, sizeof(path)))
        return false;
    strlcpy(emu_settings.value.config_version, c_get_scm_rev(),
            sizeof(emu_settings.value.config_version));

    /* Keep the members of the existing file this build doesn't know. A file
     * that turns out to be bad is dropped and written afresh. */
    have_existing = json_util_reader_open(&reader, path);
    if (!json_util_writer_open(&out, path)) {
        if (have_existing)
            json_util_reader_close(&reader);
        c_log(C_LOG_CONFIG, C_LOG_LEVEL_ERROR, "emu_settings_save",
              "Failed to open config for writing: %s", path);
        return false;
    }
    ok = emu_settings_write_global(out.writer, have_existing ? reader.json : NULL);
    if (have_existing)
        json_util_reader_close(&reader);
    if (!ok) {
        json_util_writer_close(&out, path, false);
        if (!json_util_writer_open(&out, path))
            return false;
        ok = emu_settings_write_global(out.writer, NULL);
    }
    if (!json_util_writer_close(&out, path, ok)) {
        c_log(C_LOG_CONFIG, C_LOG_LEVEL_ERROR, "emu_settings_save", "Failed to write config: %s",
              path);
        return false;
    }
    return true;
}

bool emu_settings_save_game(const char* serial) {
    char path[EMU_SETTINGS_FILE_PATH_MAX];
    json_util_writer_t out;
    unsigned i;

    emu_settings_init();
    if (!emu_settings_game_path(path, sizeof(path), serial, true))
        return false;
    if (!json_util_writer_open(&out, path)) {
        c_log(C_LOG_CONFIG, C_LOG_LEVEL_ERROR, "emu_settings_save_game",
              "Failed to open game config for writing: %s", path);
        return false;
    }
    rjsonwriter_add_start_object(out.writer);
    for (i = 0; i < EMU_SECTION_COUNT; i++)
        emu_settings_write_section(out.writer, i, true, i == 0);
    json_util_write_end(out.writer, 1, false, '}');
    return json_util_writer_close(&out, path, true);
}
