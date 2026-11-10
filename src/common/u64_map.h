/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_COMMON_U64_MAP_H
#define SHADPS4_COMMON_U64_MAP_H

/* A hash table from 64-bit keys to fixed-size values, in C89.
 *
 * Open addressing with linear probing; an erase shifts the following
 * entries back, so there are no tombstones and a lookup never walks past an
 * empty slot. Keys, values and the occupancy bytes live in one block made
 * once for the capacity asked of u64_map_init; the table grows (doubling)
 * only when it passes 7/8 full, so a caller that knows its bound never
 * allocates after init.
 *
 * A value_size of 0 makes it a set. Values are plain bytes: no
 * constructors, no destructors, and a pointer to one stays valid only until
 * the next insert or erase. Not thread-safe. */

#include <stddef.h>
#include <stdint.h>

#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

typedef struct u64_map {
    uint64_t* keys;
    unsigned char* values;
    unsigned char* used; /* 1 per slot in use */
    size_t value_size;
    size_t capacity; /* slots, a power of two */
    size_t count;
    unsigned shift; /* 64 - log2(capacity) */
} u64_map_t;

/* Room for @reserve entries without growing. Returns false when out of
 * memory. */
bool u64_map_init(u64_map_t* map, size_t value_size, size_t reserve);
void u64_map_free(u64_map_t* map);
void u64_map_clear(u64_map_t* map);

/* The value for @key, NULL when absent. For a set, a non-NULL pointer that
 * must not be read. */
void* u64_map_find(const u64_map_t* map, uint64_t key);

/* The value for @key, adding the key when it is absent; *@inserted says
 * which (it may be NULL). A new value is zeroed. NULL when out of memory. */
void* u64_map_insert(u64_map_t* map, uint64_t key, bool* inserted);

/* Returns true when @key was there. */
bool u64_map_erase(u64_map_t* map, uint64_t key);

/* Walks the entries: start with *@pos = 0, and each call returns the next
 * entry's value (and key), NULL at the end. The table must not change
 * while walking. */
void* u64_map_next(const u64_map_t* map, size_t* pos, uint64_t* key);

RETRO_END_DECLS

#endif
