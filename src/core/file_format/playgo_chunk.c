/* SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "core/file_format/playgo_chunk.h"

static u32 playgo_u16(const u8* p) {
    return (u32)p[0] | ((u32)p[1] << 8);
}

static u32 playgo_u32(const u8* p) {
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

static u64 playgo_u64(const u8* p) {
    return (u64)playgo_u32(p) | ((u64)playgo_u32(p + 4) << 32);
}

/* A table the header places at @field: where it starts, and its length, if it lies in the file */
static bool playgo_table(const u8* data, size_t size, size_t field, const u8** start, u32* length) {
    const u32 offset = playgo_u32(data + field);
    *length = playgo_u32(data + field + 4);
    if ((u64)offset + *length > size) {
        return false;
    }
    *start = data + offset;
    return true;
}

bool playgo_file_open(playgo_file_t* f, const u8* data, size_t size) {
    const u8* chunk_attrs;
    const u8* chunk_mchunks;
    const u8* chunk_labels;
    const u8* mchunk_attrs;
    u32 chunk_attrs_len;
    u32 chunk_mchunks_len;
    u32 chunk_labels_len;
    u32 mchunk_attrs_len;
    u32 count;
    u32 i;

    f->chunk_count = 0;
    if (size < PLAYGO_HEADER_SIZE) {
        return false;
    }
    f->file_size = playgo_u32(data + PLAYGO_HEADER_FILE_SIZE);
    if (playgo_u32(data + PLAYGO_HEADER_MAGIC) != PLAYGO_MAGIC) {
        return false;
    }
    if (!playgo_table(data, size, PLAYGO_HEADER_CHUNK_ATTRS, &chunk_attrs, &chunk_attrs_len) ||
        !playgo_table(data, size, PLAYGO_HEADER_CHUNK_MCHUNKS, &chunk_mchunks,
                      &chunk_mchunks_len) ||
        !playgo_table(data, size, PLAYGO_HEADER_CHUNK_LABELS, &chunk_labels, &chunk_labels_len) ||
        !playgo_table(data, size, PLAYGO_HEADER_MCHUNK_ATTRS, &mchunk_attrs, &mchunk_attrs_len)) {
        return false;
    }
    (void)chunk_labels;
    (void)chunk_labels_len;

    count = playgo_u16(data + PLAYGO_HEADER_CHUNK_COUNT);
    if ((u64)count * PLAYGO_CHUNK_ATTR_SIZE > chunk_attrs_len) {
        return false;
    }
    for (i = 0; i < count; i++) {
        const u8* attr = chunk_attrs + (size_t)i * PLAYGO_CHUNK_ATTR_SIZE;
        const u32 mchunk_count = playgo_u16(attr + PLAYGO_CHUNK_ATTR_MCHUNK_COUNT);
        const u32 mchunks_offset = playgo_u32(attr + PLAYGO_CHUNK_ATTR_MCHUNKS_OFFSET);
        u64 total_size = 0;
        u32 j;
        if (mchunk_count != 0 && (u64)mchunks_offset + (u64)mchunk_count * 2 > chunk_mchunks_len) {
            return false;
        }
        for (j = 0; j < mchunk_count; j++) {
            const u32 mchunk_id = playgo_u16(chunk_mchunks + mchunks_offset + (size_t)j * 2);
            const u64 at = (u64)mchunk_id * PLAYGO_MCHUNK_ATTR_SIZE;
            if (at + PLAYGO_MCHUNK_ATTR_SIZE > mchunk_attrs_len) {
                return false;
            }
            total_size += playgo_u64(mchunk_attrs + (size_t)at + PLAYGO_MCHUNK_ATTR_SIZE_FIELD) &
                          (((u64)1 << 48) - 1);
        }
        f->chunk_sizes[i] = total_size;
    }
    f->chunk_count = count;
    return true;
}
