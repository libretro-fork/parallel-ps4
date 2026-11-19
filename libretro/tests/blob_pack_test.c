/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The pipeline cache's pack of blobs (blob_pack.c), in memory as a file would hold it:
 * records written as the cache's writer writes them and found again by type and name, the
 * last of a name saved twice found and the first counted dead, every blob's data 8-aligned,
 * many names through the table's growth; a record cut short or with a byte changed ends the
 * pack where it begins, and a header not this one's is no pack. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "video_core/blob_pack.h"

static int failures;

static void expect(const char* what, int ok) {
    if (!ok) {
        printf("blob_pack: %s\n", what);
        failures++;
    }
}

struct pack {
    uint8_t* v;
    size_t n, cap;
};

static void put(struct pack* p, const void* bytes, size_t n) {
    if (p->n + n > p->cap) {
        p->cap = (p->n + n) * 2;
        p->v = (uint8_t*)realloc(p->v, p->cap);
    }
    memcpy(p->v + p->n, bytes, n);
    p->n += n;
}

/* As the cache's writer does: what goes before the data, then the data as it lies */
static void save(struct pack* p, uint32_t type, const char* name, const void* data, size_t n) {
    uint8_t head[BLOB_PACK_HEAD_BYTES(64)];
    const size_t hn =
        blob_pack_record_head(head, p->n, type, name, (uint32_t)strlen(name), data, n);
    put(p, head, hn);
    put(p, data, n);
}

static const blob_pack_entry_t* find(const blob_pack_index_t* idx, uint32_t type,
                                     const char* name) {
    return blob_pack_find(idx, type, name, (uint32_t)strlen(name));
}

int main(void) {
    struct pack p = {NULL, 0, 0};
    blob_pack_index_t idx;
    uint8_t header[BLOB_PACK_HEADER_SIZE];
    const blob_pack_entry_t* e;
    uint32_t words[5] = {0x07230203u, 0x00010600u, 0, 101, 0};
    char name[32], blob[48];
    size_t i, whole, last_at, prev_data;
    int aligned = 1, found_all = 1;

    blob_pack_header(header);
    put(&p, header, sizeof(header));
    save(&p, 1, "0x000000000b0acf50_0", words, sizeof(words));
    save(&p, 4, "build", "7f23a64", 7);
    save(&p, 0, "0x000000000b0acf50_0", "meta of three", 13); /* same name, another type */
    save(&p, 1, "0x000000000b0acf50_0", "again", 5);          /* saved again */
    for (i = 0; i < 1000; i++) {
        sprintf(name, "key_%u", (unsigned)i);
        sprintf(blob, "pipeline %u", (unsigned)i);
        save(&p, 2, name, blob, strlen(blob) + (i % 9));
    }
    whole = p.n;

    expect("index made", blob_pack_index_build(&idx, p.v, p.n));
    expect("every record read", idx.valid_len == whole && idx.count == 1004);
    e = find(&idx, 4, "build");
    expect("a blob found", e && e->size == 7 && memcmp(e->data, "7f23a64", 7) == 0);
    e = find(&idx, 1, "0x000000000b0acf50_0");
    expect("the last of a name saved twice", e && e->size == 5 && memcmp(e->data, "again", 5) == 0);
    e = find(&idx, 0, "0x000000000b0acf50_0");
    expect("the same name of another type apart", e && e->size == 13);
    expect("the first of a name saved twice dead",
           idx.entries[0].live == 0 && idx.dead_bytes == 32 + 20 + 4 + sizeof(words) + 4);
    expect("nothing for a name never saved", find(&idx, 2, "key_1000") == NULL);
    for (i = 0; i < idx.count; i++) {
        if (((size_t)(idx.entries[i].data - p.v) & 7) != 0) {
            aligned = 0;
        }
    }
    expect("every blob's data 8-aligned in the file", aligned);
    for (i = 0; i < 1000; i++) {
        sprintf(name, "key_%u", (unsigned)i);
        sprintf(blob, "pipeline %u", (unsigned)i);
        e = find(&idx, 2, name);
        if (!e || e->size != strlen(blob) + (i % 9) || memcmp(e->data, blob, strlen(blob)) != 0) {
            found_all = 0;
        }
    }
    expect("a thousand names found through the table's growth", found_all);
    last_at = (size_t)idx.entries[idx.count - 1].offset;
    prev_data = (size_t)(idx.entries[idx.count - 2].data - p.v);
    blob_pack_index_free(&idx);

    /* A write cut short: the last record lost, those before it kept */
    expect("index of a pack cut short", blob_pack_index_build(&idx, p.v, whole - 3));
    expect("cut short: ends where the last record began",
           idx.valid_len <= last_at && idx.count == 1003 && find(&idx, 2, "key_999") == NULL &&
               find(&idx, 2, "key_998") != NULL);
    blob_pack_index_free(&idx);

    /* A byte changed in a record's data: the pack ends there */
    p.v[prev_data + 1] ^= 0x55;
    expect("index of a pack with a byte changed", blob_pack_index_build(&idx, p.v, whole));
    expect("a changed record and those after it not read",
           idx.count == 1002 && find(&idx, 2, "key_998") == NULL);
    blob_pack_index_free(&idx);

    /* Another file */
    p.v[0] ^= 1;
    expect("a header not this one's", !blob_pack_index_build(&idx, p.v, whole));
    expect("an empty file", !blob_pack_index_build(&idx, p.v, 0));

    free(p.v);
    if (failures == 0) {
        printf("blob_pack: all passed\n");
    }
    return failures != 0;
}
