/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdlib.h>
#include <string.h>

#include "common/ptr_table.h"

#define PTR_TABLE_MIN_CAPACITY 16

/* Fibonacci hashing spreads hashes that differ only in high or low bits */
static size_t ptr_table_slot(const ptr_table_t* t, uint64_t hash) {
    const uint64_t golden = ((uint64_t)0x9E3779B9u << 32) | 0x7F4A7C15u;
    return (size_t)((hash * golden) >> t->shift);
}

static size_t ptr_table_capacity_for(size_t n) {
    size_t capacity = PTR_TABLE_MIN_CAPACITY;
    while (capacity - capacity / 8 < n)
        capacity <<= 1;
    return capacity;
}

/* Fresh, empty arrays for @capacity slots; the old ones are the caller's */
static bool ptr_table_alloc(ptr_table_t* t, size_t capacity) {
    unsigned char* block = (unsigned char*)malloc(capacity * (sizeof(uint64_t) + sizeof(void*)));
    unsigned shift = 64;
    size_t c = capacity;
    if (!block)
        return false;
    while (c > 1) {
        c >>= 1;
        shift--;
    }
    t->hashes = (uint64_t*)block;
    t->values = (void**)(block + capacity * sizeof(uint64_t));
    memset(t->values, 0, capacity * sizeof(void*));
    t->capacity = capacity;
    t->shift = shift;
    t->count = 0;
    return true;
}

bool ptr_table_init(ptr_table_t* t, size_t reserve) {
    memset(t, 0, sizeof(*t));
    return ptr_table_alloc(t, ptr_table_capacity_for(reserve));
}

void ptr_table_free(ptr_table_t* t) {
    free(t->hashes); /* the start of the block */
    memset(t, 0, sizeof(*t));
}

void ptr_table_clear(ptr_table_t* t) {
    if (t->values)
        memset(t->values, 0, t->capacity * sizeof(void*));
    t->count = 0;
}

void* ptr_table_find(const ptr_table_t* t, uint64_t hash, ptr_table_match_t match,
                     const void* key) {
    size_t mask = t->capacity - 1;
    size_t slot;
    if (!t->capacity)
        return NULL;
    for (slot = ptr_table_slot(t, hash); t->values[slot]; slot = (slot + 1) & mask)
        if (t->hashes[slot] == hash && (!match || match(t->values[slot], key)))
            return t->values[slot];
    return NULL;
}

static void ptr_table_place(ptr_table_t* t, uint64_t hash, void* value) {
    size_t mask = t->capacity - 1;
    size_t slot;
    for (slot = ptr_table_slot(t, hash); t->values[slot]; slot = (slot + 1) & mask) {
    }
    t->hashes[slot] = hash;
    t->values[slot] = value;
    t->count++;
}

bool ptr_table_insert(ptr_table_t* t, uint64_t hash, void* value) {
    if (t->count + 1 > t->capacity - t->capacity / 8) {
        ptr_table_t old = *t;
        size_t i;
        if (!ptr_table_alloc(t, old.capacity ? old.capacity * 2 : PTR_TABLE_MIN_CAPACITY)) {
            *t = old;
            return false;
        }
        for (i = 0; i < old.capacity; i++)
            if (old.values[i])
                ptr_table_place(t, old.hashes[i], old.values[i]);
        free(old.hashes);
    }
    ptr_table_place(t, hash, value);
    return true;
}

bool ptr_table_erase(ptr_table_t* t, uint64_t hash, const void* value) {
    size_t mask = t->capacity - 1;
    size_t hole, next;
    if (!t->capacity)
        return false;
    for (hole = ptr_table_slot(t, hash); t->values[hole]; hole = (hole + 1) & mask)
        if (t->values[hole] == value)
            break;
    if (!t->values[hole])
        return false;
    /* Shift the run after the hole back over it: an entry moves when its
     * home slot is not between the hole and where it sits. */
    for (next = (hole + 1) & mask; t->values[next]; next = (next + 1) & mask) {
        size_t home = ptr_table_slot(t, t->hashes[next]);
        bool stays = (next > hole) ? (home > hole && home <= next) : (home > hole || home <= next);
        if (stays)
            continue;
        t->hashes[hole] = t->hashes[next];
        t->values[hole] = t->values[next];
        hole = next;
    }
    t->values[hole] = NULL;
    t->count--;
    return true;
}

void* ptr_table_next(const ptr_table_t* t, size_t* pos) {
    size_t i;
    for (i = *pos; i < t->capacity; i++) {
        if (t->values[i]) {
            *pos = i + 1;
            return t->values[i];
        }
    }
    *pos = t->capacity;
    return NULL;
}
