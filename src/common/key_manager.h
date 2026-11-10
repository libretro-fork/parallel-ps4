/* SPDX-FileCopyrightText: Copyright 2025-2026 shadLauncher4 Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_COMMON_KEY_MANAGER_H
#define SHADPS4_COMMON_KEY_MANAGER_H

/* User-supplied keys, kept in keys.json in the user directory:
 *
 *   { "TrophyKeySet": { "ReleaseTrophyKey": "<hex>" } } */

#include <stddef.h>
#include <stdint.h>

#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

#define KEY_MANAGER_KEY_MAX 64

struct key_manager_key {
    uint8_t data[KEY_MANAGER_KEY_MAX];
    size_t size;
};

struct key_manager_keys {
    struct key_manager_key release_trophy_key;
};

/* Reads keys.json. A missing file is created with the defaults (no keys).
 * On a bad file the defaults are kept and false is returned. */
bool key_manager_load(void);
bool key_manager_save(void);

const struct key_manager_keys* key_manager_get_keys(void);
void key_manager_set_keys(const struct key_manager_keys* keys);
void key_manager_set_default_keys(void);
bool key_manager_has_keys(void);

RETRO_END_DECLS

#endif
