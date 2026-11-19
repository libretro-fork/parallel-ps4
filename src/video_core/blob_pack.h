/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The pipeline cache's blobs (shader binaries, shader descriptions, pipeline keys, the build
 * stamp) in one file instead of a file each: what is kept is a log of records, appended as
 * blobs are saved, and read back by mapping the file once and indexing it.
 *
 * The file: a 16-byte header (magic, version), then records, each starting 8-aligned:
 *
 *   u32 tag, u32 type, u32 name length, u32 0, u64 data length, u64 check,
 *   the name, zeros to 8-align, the data, zeros to 8-align
 *
 * The check is xxh3 of the name then the data, seeded with the type. A record whose tag,
 * lengths or check are wrong ends the file there: a write cut short (the emulator's process
 * is ended when the content closes) costs that record and nothing before it. A blob saved
 * again under its name is found by its last record; the earlier ones are dead bytes, dropped
 * when the file is written again whole (blob_pack_index_t.dead_bytes says how many).
 *
 * The data of a record starts 8-aligned in the file, so in a mapping of it: words can be read
 * where they lie. Host order (little-endian, as every host the core builds for).
 *
 * This file knows the layout only; reading and writing files is the caller's. C89, no global
 * state, no lock: an index is built once and then only read, by any number of threads. */

#ifndef SHADPS4_BLOB_PACK_H
#define SHADPS4_BLOB_PACK_H

#include <stddef.h>
#include <stdint.h>

#include <boolean.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BLOB_PACK_HEADER_SIZE 16
#define BLOB_PACK_RECORD_HEAD 32
/* The most bytes blob_pack_record_head writes: padding before, the head, a name of @n bytes */
#define BLOB_PACK_HEAD_BYTES(n) (7 + BLOB_PACK_RECORD_HEAD + (n) + 7)

/* The file's header at @out */
void blob_pack_header(uint8_t out[BLOB_PACK_HEADER_SIZE]);

/* Zeros from @at (the file's length so far) to the next 8-aligned offset */
uint32_t blob_pack_pad(uint64_t at);

/* The record of @data under @type and @name, written at file offset @at (the file's length):
 * what goes before its data is put at @out (BLOB_PACK_HEAD_BYTES(name_len) bytes at most):
 * the zeros that 8-align it, its head, the name and the zeros after the name. Its byte count.
 * The data is then written as it lies, and the next record's zeros pad it. */
size_t blob_pack_record_head(uint8_t* out, uint64_t at, uint32_t type, const char* name,
                             uint32_t name_len, const void* data, uint64_t data_len);

typedef struct blob_pack_entry {
    const char* name; /* in the mapping, not terminated */
    const uint8_t* data;
    uint64_t size;
    uint64_t offset; /* of its head in the file */
    uint32_t type;
    uint32_t name_len;
    uint32_t live; /* 0: saved again later under its name */
} blob_pack_entry_t;

typedef struct blob_pack_index {
    blob_pack_entry_t* entries; /* in the order of the file */
    uint32_t count;
    uint32_t cap;
    uint32_t* slots; /* entry index plus one by (type, name); 0: free */
    uint32_t mask;
    uint64_t valid_len;  /* bytes of whole records, the header with them: where the next goes */
    uint64_t dead_bytes; /* in records saved again later */
} blob_pack_index_t;

/* The index of the file mapped at @map, @len bytes: false when its header is not this one's
 * or memory ran out. Records past a bad one are not read; valid_len says where it began. */
bool blob_pack_index_build(blob_pack_index_t* idx, const uint8_t* map, size_t len);
void blob_pack_index_free(blob_pack_index_t* idx);

/* The last record of @type under @name; NULL when there is none */
const blob_pack_entry_t* blob_pack_find(const blob_pack_index_t* idx, uint32_t type,
                                        const char* name, uint32_t name_len);

#ifdef __cplusplus
}
#endif

#endif
