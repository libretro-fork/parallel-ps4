/* SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_FILE_FORMAT_PLAYGO_CHUNK_H
#define SHADPS4_FILE_FORMAT_PLAYGO_CHUNK_H

/* A game's PlayGo file (sce_sys/playgo-chunk.dat): its chunks, and how many bytes each is.
 * Read where it lies, field by field, little-endian: nothing is copied out of it but the
 * sizes. Defined in playgo_chunk.c. */

#include <stddef.h>

#include <boolean.h>
#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

#define PLAYGO_MAGIC 0x6F676C70u

/* The header's size, and where in it what is read lies */
#define PLAYGO_HEADER_SIZE 256
#define PLAYGO_HEADER_MAGIC 0
#define PLAYGO_HEADER_CHUNK_COUNT 10
#define PLAYGO_HEADER_FILE_SIZE 16
#define PLAYGO_HEADER_CHUNK_ATTRS 192   /* offset and length, two u32 */
#define PLAYGO_HEADER_CHUNK_MCHUNKS 200 /* the same */
#define PLAYGO_HEADER_CHUNK_LABELS 208  /* the same */
#define PLAYGO_HEADER_MCHUNK_ATTRS 216  /* the same */

/* A chunk's attributes: 32 bytes, its mchunks' count (u16) at 14, where their ids (u16) start
 * in the mchunk list (u32) at 24 */
#define PLAYGO_CHUNK_ATTR_SIZE 32
#define PLAYGO_CHUNK_ATTR_MCHUNK_COUNT 14
#define PLAYGO_CHUNK_ATTR_MCHUNKS_OFFSET 24
/* An mchunk's attributes: 16 bytes, its size in the low 48 bits of the u64 at 8 */
#define PLAYGO_MCHUNK_ATTR_SIZE 16
#define PLAYGO_MCHUNK_ATTR_SIZE_FIELD 8

/* The header's chunk count is a u16 */
#define PLAYGO_MAX_CHUNKS 65536

typedef struct playgo_file {
    /* The header's file size: 0 for a file too short to have a header */
    u32 file_size;
    /* The chunks read, and their sizes in bytes: 0 chunks unless the whole file was read */
    u32 chunk_count;
    u64 chunk_sizes[PLAYGO_MAX_CHUNKS];
} playgo_file_t;

/* Reads @data (@size bytes) into @f: false if it is not a PlayGo file, or any table in it
 * lies outside it. */
bool playgo_file_open(playgo_file_t* f, const u8* data, size_t size);

RETRO_END_DECLS

#endif
