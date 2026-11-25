/* SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_FILE_FORMAT_PSF_H
#define SHADPS4_FILE_FORMAT_PSF_H

/* param.sfo: a list of keys, each with a string, a 32-bit integer or bytes. */

#include <stddef.h>

#include <boolean.h>
#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

#define PSF_MAGIC 0x00505346u /* "\0PSF", read big-endian */
#define PSF_VERSION_1_1 0x00000101u
#define PSF_VERSION_1_0 0x00000100u

/* Sizes in the file */
#define PSF_HEADER_SIZE 0x14
#define PSF_RAW_ENTRY_SIZE 0x10

/* At most this many keys (a game's param.sfo has about forty, a save's about twelve) */
#define PSF_MAX_ENTRIES 128

/* The kind of a value, a little-endian u16 in the file */
typedef enum psf_fmt {
    PSF_FMT_BINARY = 0x0004, /* bytes */
    PSF_FMT_TEXT = 0x0204,   /* UTF-8, terminated */
    PSF_FMT_INTEGER = 0x0404 /* signed 32-bit */
} psf_fmt_t;

typedef struct psf_entry {
    u32 key;     /* offset in the arena, terminated */
    u32 value;   /* offset in the arena; a string is terminated past its length */
    u32 length;  /* of the value (a string's without its terminator) */
    u32 room;    /* bytes the value may take where it is in the arena */
    u32 max_len; /* the room the file gives it */
    u16 fmt;     /* psf_fmt_t */
} psf_entry_t;

typedef struct psf {
    psf_entry_t entries[PSF_MAX_ENTRIES];
    u32 count;
    /* The keys and values, one allocation; grown when a value outgrows its room */
    u8* arena;
    u32 arena_used;
    u32 arena_size;
    /* When the file was last written (seconds since 1970): its time when opened from a path,
     * the time of writing when written to one */
    s64 last_write;
} psf_t;

/* An empty list. A zeroed psf_t is one too. */
void psf_init(psf_t* psf);
/* Frees what @psf holds; it is empty after */
void psf_free(psf_t* psf);
/* @dst (initialized) becomes a copy of @src: false, @dst empty, if it cannot be allocated */
bool psf_copy(psf_t* dst, const psf_t* src);

/* Reads @size bytes at @data: false, the list empty, when they are not a param.sfo. The time
 * of the last write is left as it was. */
bool psf_open_buffer(psf_t* psf, const void* data, size_t size);
/* Reads the file at @path (mapped where it lies) and takes its time */
bool psf_open_path(psf_t* psf, const char* path);

/* The file's size */
size_t psf_encoded_size(const psf_t* psf);
/* Writes the file into @out, @size bytes (at least psf_encoded_size): its size, or 0 */
size_t psf_encode(const psf_t* psf, void* out, size_t size);
/* Writes the file at @path, whole, and takes the time */
bool psf_encode_path(psf_t* psf, const char* path);

/* The value of @key: false if there is none or it is of another kind. A string comes
 * terminated, with its length in @length (if not NULL). It stays valid until the list is
 * changed. */
bool psf_get_string(const psf_t* psf, const char* key, const char** value, size_t* length);
bool psf_get_integer(const psf_t* psf, const char* key, s32* value);
bool psf_get_binary(const psf_t* psf, const char* key, const u8** value, u32* length);

/* Adds @key, or with @update changes it (without @update a key that is there is left alone
 * and an error logged). False if there is no room or the key has another kind. */
bool psf_set_string(psf_t* psf, const char* key, const char* value, size_t length, bool update);
bool psf_set_integer(psf_t* psf, const char* key, s32 value, bool update);
bool psf_set_binary(psf_t* psf, const char* key, const void* value, u32 length, bool update);
/* Eight bytes, little-endian */
bool psf_set_binary_u64(psf_t* psf, const char* key, u64 value, bool update);

/* The game's param.sfo, read at boot */
psf_t* psf_game(void);

/* The game's string @key: false if there is none. Its length is put in @length (if not NULL),
 * and the string, cut to fit and terminated, in @out (if @size is not 0). */
bool psf_game_get_string(const char* key, char* out, size_t size, size_t* length);

/* The game's integer @key: false if there is none */
bool psf_game_get_integer(const char* key, s32* value);

RETRO_END_DECLS

#endif
