/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_CORE_USER_MANAGER_H
#define SHADPS4_CORE_USER_MANAGER_H

/* The console's users, kept in users.json in the user directory:
 *
 *   { "Users": { "user": [ { "user_id": 1000, ... }, ... ],
 *                "commit_hash": "<build>" } }
 *
 * Each user has a home directory, <home_dir>/<user_id>. */

#include <stddef.h>
#include <stdint.h>

#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

#define USER_MANAGER_MAX_USERS 16
#define USER_MANAGER_MAX_LOGGED_IN 4
#define USER_MANAGER_STRING_MAX 128

struct user {
    int32_t user_id;
    char user_name[USER_MANAGER_STRING_MAX];
    uint32_t user_color;
    /* 1-4, or -1 for none */
    int player_index;
    bool logged_in;
    /* shadNet account */
    char shadnet_npid[USER_MANAGER_STRING_MAX];
    char shadnet_password[USER_MANAGER_STRING_MAX];
    /* validation token (future use) */
    char shadnet_token[USER_MANAGER_STRING_MAX];
    /* email address (future use) */
    char shadnet_email[USER_MANAGER_STRING_MAX];
    bool shadnet_enabled;
    /* NP profile: ISO 3166-1 alpha-2 country, ISO 639-1 language,
     * age 0..127 and ISO 8601 date of birth */
    char np_country[8];
    char np_language[8];
    uint8_t np_age;
    char np_date_of_birth[16];
};

/* Reads users.json. A missing file is created with the four default users
 * (and their home directories), and one written by another build is saved
 * again. */
bool user_manager_load(void);
/* Writes users.json, keeping the members this build doesn't know. */
bool user_manager_save(void);

size_t user_manager_count(void);
struct user* user_manager_at(size_t index);
struct user* user_manager_get_by_id(int32_t user_id);
struct user* user_manager_get_by_player_index(int32_t player_index);
/* The user on player 1, or NULL. */
struct user* user_manager_get_default_user(void);
/* Whether the user's home directory exists. */
bool user_manager_has_home_dir(const struct user* user);

/* The user logged in on player slot 0..3, or NULL. */
struct user* user_manager_get_logged_in(unsigned slot);
/* Logs user in on player_index (1-4). Returns true when that changed
 * anything, for the caller to post the login event. */
bool user_manager_login(struct user* user, int32_t player_index);
/* Returns true when user was logged in, for the caller to post the logout
 * event. */
bool user_manager_logout(struct user* user);

RETRO_END_DECLS

#endif
