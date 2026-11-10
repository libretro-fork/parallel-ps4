/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdlib.h>
#include <string.h>

#include "common/u64_map.h"

#define U64_MAP_MIN_CAPACITY 16

/* Fibonacci hashing: the top bits of key * 2^64/phi */
static size_t u64_map_slot(const u64_map_t* map, uint64_t key) {
    const uint64_t golden = ((uint64_t)0x9E3779B9u << 32) | 0x7F4A7C15u;
    return (size_t)((key * golden) >> map->shift);
}

static unsigned char* u64_map_value(const u64_map_t* map, size_t slot) {
    return map->values + slot * map->value_size;
}

/* Fresh storage for @capacity slots; the old block is the caller's */
static bool u64_map_alloc(u64_map_t* map, size_t capacity) {
    size_t keys_bytes = capacity * sizeof(uint64_t);
    size_t values_bytes = capacity * map->value_size;
    unsigned char* block = (unsigned char*)malloc(keys_bytes + values_bytes + capacity);
    unsigned shift = 64;
    size_t c = capacity;
    if (!block)
        return false;
    while (c > 1) {
        c >>= 1;
        shift--;
    }
    map->keys = (uint64_t*)block;
    map->values = block + keys_bytes;
    map->used = block + keys_bytes + values_bytes;
    memset(map->used, 0, capacity);
    map->capacity = capacity;
    map->shift = shift;
    map->count = 0;
    return true;
}

/* Slots for @n entries at no more than 7/8 full */
static size_t u64_map_capacity_for(size_t n) {
    size_t capacity = U64_MAP_MIN_CAPACITY;
    while (capacity - capacity / 8 < n)
        capacity <<= 1;
    return capacity;
}

bool u64_map_init(u64_map_t* map, size_t value_size, size_t reserve) {
    memset(map, 0, sizeof(*map));
    map->value_size = value_size;
    return u64_map_alloc(map, u64_map_capacity_for(reserve));
}

void u64_map_free(u64_map_t* map) {
    free(map->keys); /* the start of the block */
    map->keys = NULL;
    map->values = NULL;
    map->used = NULL;
    map->capacity = 0;
    map->count = 0;
}

void u64_map_clear(u64_map_t* map) {
    if (map->used)
        memset(map->used, 0, map->capacity);
    map->count = 0;
}

void* u64_map_find(const u64_map_t* map, uint64_t key) {
    size_t mask = map->capacity - 1;
    size_t slot;
    if (!map->capacity)
        return NULL;
    for (slot = u64_map_slot(map, key); map->used[slot]; slot = (slot + 1) & mask)
        if (map->keys[slot] == key)
            return u64_map_value(map, slot);
    return NULL;
}

static bool u64_map_grow(u64_map_t* map) {
    u64_map_t old = *map;
    size_t i;
    if (!u64_map_alloc(map, old.capacity ? old.capacity * 2 : U64_MAP_MIN_CAPACITY)) {
        *map = old;
        return false;
    }
    for (i = 0; i < old.capacity; i++) {
        size_t mask = map->capacity - 1;
        size_t slot;
        if (!old.used[i])
            continue;
        for (slot = u64_map_slot(map, old.keys[i]); map->used[slot]; slot = (slot + 1) & mask) {
        }
        map->used[slot] = 1;
        map->keys[slot] = old.keys[i];
        if (map->value_size)
            memcpy(u64_map_value(map, slot), u64_map_value(&old, i), map->value_size);
        map->count++;
    }
    free(old.keys);
    return true;
}

void* u64_map_insert(u64_map_t* map, uint64_t key, bool* inserted) {
    size_t mask, slot;
    void* found = u64_map_find(map, key);
    if (found) {
        if (inserted)
            *inserted = false;
        return found;
    }
    if (map->count + 1 > map->capacity - map->capacity / 8 && !u64_map_grow(map))
        return NULL;
    mask = map->capacity - 1;
    for (slot = u64_map_slot(map, key); map->used[slot]; slot = (slot + 1) & mask) {
    }
    map->used[slot] = 1;
    map->keys[slot] = key;
    if (map->value_size)
        memset(u64_map_value(map, slot), 0, map->value_size);
    map->count++;
    if (inserted)
        *inserted = true;
    return u64_map_value(map, slot);
}

bool u64_map_erase(u64_map_t* map, uint64_t key) {
    size_t mask = map->capacity - 1;
    size_t hole, next;
    unsigned char* value = (unsigned char*)u64_map_find(map, key);
    if (!value)
        return false;
    hole = map->value_size ? (size_t)(value - map->values) / map->value_size : 0;
    if (!map->value_size) {
        /* every value pointer is the same; find the slot by key */
        for (hole = u64_map_slot(map, key); map->keys[hole] != key; hole = (hole + 1) & mask) {
        }
    }
    /* Shift the run after the hole back over it: an entry moves when its
     * home slot is not between the hole and where it sits. */
    for (next = (hole + 1) & mask; map->used[next]; next = (next + 1) & mask) {
        size_t home = u64_map_slot(map, map->keys[next]);
        bool stays = (next > hole) ? (home > hole && home <= next) : (home > hole || home <= next);
        if (stays)
            continue;
        map->keys[hole] = map->keys[next];
        if (map->value_size)
            memcpy(u64_map_value(map, hole), u64_map_value(map, next), map->value_size);
        hole = next;
    }
    map->used[hole] = 0;
    map->count--;
    return true;
}

void* u64_map_next(const u64_map_t* map, size_t* pos, uint64_t* key) {
    size_t i;
    for (i = *pos; i < map->capacity; i++) {
        if (map->used[i]) {
            *pos = i + 1;
            if (key)
                *key = map->keys[i];
            return u64_map_value(map, i);
        }
    }
    *pos = map->capacity;
    return NULL;
}
