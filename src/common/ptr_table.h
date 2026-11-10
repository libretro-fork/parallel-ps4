/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_COMMON_PTR_TABLE_H
#define SHADPS4_COMMON_PTR_TABLE_H

/* A hash table of pointers filed by a 64-bit hash, in C89.
 *
 * The caller hashes its key and keeps the key in the object the pointer
 * points at; a lookup walks the entries with that hash and asks the caller's
 * match function which one is the key's. Two keys with the same hash are two
 * entries, so the table is exact whatever the hash. With no match function
 * the hash is the key.
 *
 * Open addressing with linear probing; an erase shifts the following entries
 * back, so no tombstones are left. Hashes and pointers sit in two arrays made
 * once for the capacity asked of ptr_table_init; the table doubles only past
 * 7/8 full. It does not own what it points at. Not thread-safe. */

#include <stddef.h>
#include <stdint.h>

#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

typedef struct ptr_table {
    uint64_t* hashes;
    void** values;   /* NULL: an empty slot */
    size_t capacity; /* a power of two */
    size_t count;
    unsigned shift; /* 64 - log2(capacity) */
} ptr_table_t;

/* Nonzero when @value is the object for the key at @key */
typedef int (*ptr_table_match_t)(const void* value, const void* key);

/* Room for @reserve entries without growing; false when out of memory */
bool ptr_table_init(ptr_table_t* t, size_t reserve);
void ptr_table_free(ptr_table_t* t);
void ptr_table_clear(ptr_table_t* t);

/* The entry for @key with hash @hash, NULL when there is none. With @match
 * NULL, the first entry with that hash. */
void* ptr_table_find(const ptr_table_t* t, uint64_t hash, ptr_table_match_t match, const void* key);

/* Adds @value (not NULL) under @hash, beside any entries already there.
 * False when out of memory. */
bool ptr_table_insert(ptr_table_t* t, uint64_t hash, void* value);

/* Takes out the entry holding exactly @value; true when it was there */
bool ptr_table_erase(ptr_table_t* t, uint64_t hash, const void* value);

/* Walks the entries: start with *@pos = 0; returns each value in turn, NULL
 * at the end. The table must not change while walking. */
void* ptr_table_next(const ptr_table_t* t, size_t* pos);

RETRO_END_DECLS

#endif
