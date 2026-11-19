/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdlib.h>
#include <string.h>

#include <encodings/xxh3.h>

#include "video_core/blob_pack.h"

#define PACK_MAGIC 0x4b505053u /* "SPPK" */
#define PACK_VERSION 1u
#define RECORD_TAG 0x43455242u /* "BREC" */

static void put32(uint8_t* p, uint32_t v) {
    memcpy(p, &v, 4);
}

static void put64(uint8_t* p, uint64_t v) {
    memcpy(p, &v, 8);
}

static uint32_t get32(const uint8_t* p) {
    uint32_t v;
    memcpy(&v, p, 4);
    return v;
}

static uint64_t get64(const uint8_t* p) {
    uint64_t v;
    memcpy(&v, p, 8);
    return v;
}

static uint64_t record_check(uint32_t type, const char* name, uint32_t name_len, const void* data,
                             uint64_t data_len) {
    return xxh3_64_pair(name, name_len, data, (size_t)data_len, type);
}

void blob_pack_header(uint8_t out[BLOB_PACK_HEADER_SIZE]) {
    memset(out, 0, BLOB_PACK_HEADER_SIZE);
    put32(out, PACK_MAGIC);
    put32(out + 4, PACK_VERSION);
}

uint32_t blob_pack_pad(uint64_t at) {
    return (uint32_t)((8 - (at & 7)) & 7);
}

size_t blob_pack_record_head(uint8_t* out, uint64_t at, uint32_t type, const char* name,
                             uint32_t name_len, const void* data, uint64_t data_len) {
    const uint32_t pre = blob_pack_pad(at);
    const uint32_t post = blob_pack_pad(name_len);
    uint8_t* h = out + pre;
    memset(out, 0, pre);
    put32(h, RECORD_TAG);
    put32(h + 4, type);
    put32(h + 8, name_len);
    put32(h + 12, 0);
    put64(h + 16, data_len);
    put64(h + 24, record_check(type, name, name_len, data, data_len));
    memcpy(h + BLOB_PACK_RECORD_HEAD, name, name_len);
    memset(h + BLOB_PACK_RECORD_HEAD + name_len, 0, post);
    return pre + BLOB_PACK_RECORD_HEAD + name_len + post;
}

static uint32_t key_hash(uint32_t type, const char* name, uint32_t name_len) {
    const uint64_t h = xxh3_64(name, name_len, type);
    return (uint32_t)(h ^ (h >> 32));
}

static bool slots_grow(blob_pack_index_t* idx) {
    const uint32_t nslots = idx->mask ? (idx->mask + 1) * 2 : 256;
    uint32_t* slots = (uint32_t*)calloc(nslots, sizeof(*slots));
    uint32_t i;
    if (!slots) {
        return false;
    }
    /* Live entries only: the table finds the last record of a name */
    for (i = 0; i < idx->count; i++) {
        const blob_pack_entry_t* e = &idx->entries[i];
        uint32_t s;
        if (!e->live) {
            continue;
        }
        s = key_hash(e->type, e->name, e->name_len) & (nslots - 1);
        while (slots[s]) {
            s = (s + 1) & (nslots - 1);
        }
        slots[s] = i + 1;
    }
    free(idx->slots);
    idx->slots = slots;
    idx->mask = nslots - 1;
    return true;
}

/* The slot holding @type and @name, or the free slot where it would go */
static uint32_t* find_slot(const blob_pack_index_t* idx, uint32_t type, const char* name,
                           uint32_t name_len) {
    uint32_t s = key_hash(type, name, name_len) & idx->mask;
    for (;;) {
        uint32_t* slot = &idx->slots[s];
        const blob_pack_entry_t* e;
        if (!*slot) {
            return slot;
        }
        e = &idx->entries[*slot - 1];
        if (e->type == type && e->name_len == name_len && memcmp(e->name, name, name_len) == 0) {
            return slot;
        }
        s = (s + 1) & idx->mask;
    }
}

static bool add_entry(blob_pack_index_t* idx, const blob_pack_entry_t* e) {
    uint32_t* slot;
    if (idx->count == idx->cap) {
        const uint32_t ncap = idx->cap ? idx->cap * 2 : 256;
        blob_pack_entry_t* n = (blob_pack_entry_t*)realloc(idx->entries, (size_t)ncap * sizeof(*n));
        if (!n) {
            return false;
        }
        idx->entries = n;
        idx->cap = ncap;
    }
    /* At most half full, counting the dead it never holds */
    if ((idx->count + 1) * 2 > idx->mask + 1 && !slots_grow(idx)) {
        return false;
    }
    slot = find_slot(idx, e->type, e->name, e->name_len);
    if (*slot) {
        blob_pack_entry_t* old = &idx->entries[*slot - 1];
        old->live = 0;
        idx->dead_bytes += BLOB_PACK_RECORD_HEAD + old->name_len + blob_pack_pad(old->name_len) +
                           old->size + blob_pack_pad(old->size);
    }
    idx->entries[idx->count] = *e;
    idx->entries[idx->count].live = 1;
    *slot = ++idx->count;
    return true;
}

bool blob_pack_index_build(blob_pack_index_t* idx, const uint8_t* map, size_t len) {
    uint64_t at = BLOB_PACK_HEADER_SIZE;
    memset(idx, 0, sizeof(*idx));
    if (len < BLOB_PACK_HEADER_SIZE || get32(map) != PACK_MAGIC || get32(map + 4) != PACK_VERSION) {
        return false;
    }
    if (!slots_grow(idx)) {
        return false;
    }
    idx->valid_len = at;
    for (;;) {
        blob_pack_entry_t e;
        uint64_t name_end, data_at, data_end;
        const uint8_t* h;
        at += blob_pack_pad(at);
        if (at + BLOB_PACK_RECORD_HEAD > len) {
            break;
        }
        h = map + at;
        if (get32(h) != RECORD_TAG || get32(h + 12) != 0) {
            break;
        }
        e.type = get32(h + 4);
        e.name_len = get32(h + 8);
        e.size = get64(h + 16);
        name_end = at + BLOB_PACK_RECORD_HEAD + e.name_len;
        data_at = name_end + blob_pack_pad(e.name_len);
        if (name_end > len || data_at > len || e.size > len - data_at) {
            break;
        }
        data_end = data_at + e.size;
        e.name = (const char*)(h + BLOB_PACK_RECORD_HEAD);
        e.data = map + data_at;
        e.offset = at;
        e.live = 1;
        if (record_check(e.type, e.name, e.name_len, e.data, e.size) != get64(h + 24)) {
            break;
        }
        if (!add_entry(idx, &e)) {
            blob_pack_index_free(idx);
            return false;
        }
        at = data_end;
        idx->valid_len = at;
    }
    return true;
}

void blob_pack_index_free(blob_pack_index_t* idx) {
    free(idx->entries);
    free(idx->slots);
    memset(idx, 0, sizeof(*idx));
}

const blob_pack_entry_t* blob_pack_find(const blob_pack_index_t* idx, uint32_t type,
                                        const char* name, uint32_t name_len) {
    const uint32_t* slot;
    if (!idx->slots) {
        return NULL;
    }
    slot = find_slot(idx, type, name, name_len);
    return *slot ? &idx->entries[*slot - 1] : NULL;
}
