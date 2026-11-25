/* SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* param.sfo, read and written. The list keeps its keys and values in one arena; the file is
 * read where it lies (mapped) and written whole. */

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <file/file_path.h>
#include <streams/file_stream.h>

#include "common/c_bridge.h"
#include "core/file_format/psf.h"

/* The room the system gives some keys, whatever their value */
static const struct {
    const char* key;
    u32 size;
} psf_known_max_sizes[] = {
    {"ACCOUNT_ID", 8},  {"CATEGORY", 4},  {"DETAIL", 1024},       {"FORMAT", 4},
    {"MAINTITLE", 128}, {"PARAMS", 1024}, {"SAVEDATA_BLOCKS", 8}, {"SAVEDATA_DIRECTORY", 32},
    {"SUBTITLE", 128},  {"TITLE_ID", 12},
};

static u32 psf_max_size(const char* key, u32 default_value) {
    size_t i;
    for (i = 0; i < sizeof(psf_known_max_sizes) / sizeof(psf_known_max_sizes[0]); i++) {
        if (strcmp(psf_known_max_sizes[i].key, key) == 0) {
            return psf_known_max_sizes[i].size;
        }
    }
    return default_value;
}

static u16 psf_rd16le(const u8* p) {
    return (u16)(p[0] | (p[1] << 8));
}
static u32 psf_rd32le(const u8* p) {
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}
static u32 psf_rd32be(const u8* p) {
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | (u32)p[3];
}
static void psf_wr16le(u8* p, u16 v) {
    p[0] = (u8)v;
    p[1] = (u8)(v >> 8);
}
static void psf_wr32le(u8* p, u32 v) {
    p[0] = (u8)v;
    p[1] = (u8)(v >> 8);
    p[2] = (u8)(v >> 16);
    p[3] = (u8)(v >> 24);
}
static void psf_wr32be(u8* p, u32 v) {
    p[0] = (u8)(v >> 24);
    p[1] = (u8)(v >> 16);
    p[2] = (u8)(v >> 8);
    p[3] = (u8)v;
}

static size_t psf_strnlen(const u8* s, size_t max) {
    const u8* end = (const u8*)memchr(s, 0, max);
    return end != NULL ? (size_t)(end - s) : max;
}

void psf_init(psf_t* psf) {
    psf->count = 0;
    psf->arena = NULL;
    psf->arena_used = 0;
    psf->arena_size = 0;
    psf->last_write = 0;
}

void psf_free(psf_t* psf) {
    free(psf->arena);
    psf->arena = NULL;
    psf->arena_used = 0;
    psf->arena_size = 0;
    psf->count = 0;
}

/* Room for @extra more bytes in the arena */
static bool psf_reserve(psf_t* psf, u32 extra) {
    u32 want;
    u8* grown;
    if (psf->arena_size - psf->arena_used >= extra) {
        return true;
    }
    want = psf->arena_size != 0 ? psf->arena_size : 256;
    while (want - psf->arena_used < extra) {
        if (want > 0x40000000u) {
            return false;
        }
        want *= 2;
    }
    grown = (u8*)realloc(psf->arena, want);
    if (grown == NULL) {
        return false;
    }
    psf->arena = grown;
    psf->arena_size = want;
    return true;
}

bool psf_copy(psf_t* dst, const psf_t* src) {
    if (dst == src) {
        return true;
    }
    if (dst->arena_size < src->arena_used) {
        u8* grown = (u8*)realloc(dst->arena, src->arena_used != 0 ? src->arena_used : 1);
        if (grown == NULL) {
            dst->count = 0;
            dst->arena_used = 0;
            return false;
        }
        dst->arena = grown;
        dst->arena_size = src->arena_used;
    }
    if (src->arena_used != 0) {
        memcpy(dst->arena, src->arena, src->arena_used);
    }
    memcpy(dst->entries, src->entries, sizeof(src->entries[0]) * src->count);
    dst->count = src->count;
    dst->arena_used = src->arena_used;
    dst->last_write = src->last_write;
    return true;
}

static void psf_clear(psf_t* psf) {
    psf->count = 0;
    psf->arena_used = 0;
}

static const char* psf_key(const psf_t* psf, const psf_entry_t* e) {
    return (const char*)psf->arena + e->key;
}

static psf_entry_t* psf_find(const psf_t* psf, const char* key) {
    u32 i;
    for (i = 0; i < psf->count; i++) {
        if (strcmp(psf_key(psf, &psf->entries[i]), key) == 0) {
            return (psf_entry_t*)&psf->entries[i];
        }
    }
    return NULL;
}

/* What the file's value takes */
static u32 psf_param_len(const psf_entry_t* e) {
    return e->fmt == PSF_FMT_TEXT ? e->length + 1 : e->length;
}

bool psf_open_buffer(psf_t* psf, const void* data, size_t size) {
    const u8* p = (const u8*)data;
    u32 version, key_table, data_table, count, i;
    size_t need = 0;

    psf_clear(psf);
    if (p == NULL || size < PSF_HEADER_SIZE) {
        c_log(C_LOG_CORE, C_LOG_LEVEL_ERROR, "psf_open_buffer", "PSF too small (%u bytes)",
              (unsigned)size);
        return false;
    }
    if (psf_rd32be(p) != PSF_MAGIC) {
        c_log(C_LOG_CORE, C_LOG_LEVEL_ERROR, "psf_open_buffer", "Invalid PSF magic number");
        return false;
    }
    version = psf_rd32le(p + 4);
    if (version != PSF_VERSION_1_1 && version != PSF_VERSION_1_0) {
        c_log(C_LOG_CORE, C_LOG_LEVEL_ERROR, "psf_open_buffer", "Unsupported PSF version: 0x%08x",
              version);
        return false;
    }
    key_table = psf_rd32le(p + 8);
    data_table = psf_rd32le(p + 12);
    count = psf_rd32le(p + 16);
    if (count > PSF_MAX_ENTRIES || (size - PSF_HEADER_SIZE) / PSF_RAW_ENTRY_SIZE < (size_t)count ||
        key_table > size || data_table > size) {
        c_log(C_LOG_CORE, C_LOG_LEVEL_ERROR, "psf_open_buffer",
              "PSF of %u bytes cannot hold %u entries", (unsigned)size, count);
        return false;
    }

    /* First the room it all takes, then one allocation (none when the arena is big enough) */
    for (i = 0; i < count; i++) {
        const u8* raw = p + PSF_HEADER_SIZE + i * PSF_RAW_ENTRY_SIZE;
        const u32 key_off = key_table + psf_rd16le(raw);
        const u16 fmt = psf_rd16le(raw + 2);
        const u32 param_len = psf_rd32le(raw + 4);
        const u32 max_len = psf_rd32le(raw + 8);
        const u32 data_off = psf_rd32le(raw + 12);
        size_t key_len, length;
        if (key_off >= size || data_off > size - data_table) {
            c_log(C_LOG_CORE, C_LOG_LEVEL_ERROR, "psf_open_buffer",
                  "PSF entry %u is out of the file", i);
            return false;
        }
        key_len = psf_strnlen(p + key_off, size - key_off);
        if (key_off + key_len == size) {
            c_log(C_LOG_CORE, C_LOG_LEVEL_ERROR, "psf_open_buffer", "PSF key %u is not ended", i);
            return false;
        }
        switch (fmt) {
        case PSF_FMT_BINARY:
            if (param_len > size - data_table - data_off) {
                c_log(C_LOG_CORE, C_LOG_LEVEL_ERROR, "psf_open_buffer",
                      "PSF entry %u is out of the file", i);
                return false;
            }
            length = param_len;
            break;
        case PSF_FMT_TEXT:
            /* Up to its terminator, as the system reads it */
            length = psf_strnlen(p + data_table + data_off, size - data_table - data_off) + 1;
            break;
        case PSF_FMT_INTEGER:
            if (param_len != 4 || size - data_table - data_off < 4) {
                c_log(C_LOG_CORE, C_LOG_LEVEL_ERROR, "psf_open_buffer",
                      "PSF integer entry size mismatch");
                return false;
            }
            length = 4;
            break;
        default:
            c_log(C_LOG_CORE, C_LOG_LEVEL_ERROR, "psf_open_buffer",
                  "Unknown PSF entry format 0x%04x", fmt);
            return false;
        }
        need += key_len + 1 + (length > max_len ? length : max_len);
    }
    if (need > 0x40000000u) {
        return false;
    }
    if (psf->arena_size < need) {
        u8* grown = (u8*)realloc(psf->arena, need != 0 ? need : 1);
        if (grown == NULL) {
            return false;
        }
        psf->arena = grown;
        psf->arena_size = (u32)need;
    }

    for (i = 0; i < count; i++) {
        const u8* raw = p + PSF_HEADER_SIZE + i * PSF_RAW_ENTRY_SIZE;
        const u32 key_off = key_table + psf_rd16le(raw);
        const u8* value = p + data_table + psf_rd32le(raw + 12);
        psf_entry_t* e = &psf->entries[i];
        const size_t key_len = strlen((const char*)p + key_off);
        u32 room;

        e->fmt = psf_rd16le(raw + 2);
        e->max_len = psf_rd32le(raw + 8);
        switch (e->fmt) {
        case PSF_FMT_TEXT:
            e->length = (u32)psf_strnlen(value, size - (size_t)(value - p));
            break;
        case PSF_FMT_INTEGER:
            e->length = 4;
            break;
        default:
            e->length = psf_rd32le(raw + 4);
            break;
        }

        e->key = psf->arena_used;
        memcpy(psf->arena + psf->arena_used, p + key_off, key_len + 1);
        psf->arena_used += (u32)key_len + 1;

        room = psf_param_len(e);
        if (room < e->max_len) {
            room = e->max_len;
        }
        e->value = psf->arena_used;
        e->room = room;
        memcpy(psf->arena + e->value, value, e->length);
        if (e->fmt == PSF_FMT_TEXT) {
            psf->arena[e->value + e->length] = 0;
        }
        psf->arena_used += room;
    }
    psf->count = count;
    return true;
}

bool psf_open_path(psf_t* psf, const char* path) {
    int64_t mtime = 0;
    int64_t len = 0;
    const u8* mapped;
    RFILE* file;
    bool ok;

    if (path_get_mtime(path, &mtime)) {
        psf->last_write = mtime;
    }
    file = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
    if (file == NULL) {
        psf_clear(psf);
        return false;
    }
    mapped = filestream_map(file, &len);
    if (mapped != NULL) {
        ok = len > 0 && psf_open_buffer(psf, mapped, (size_t)len);
    } else {
        /* Not mappable here: read */
        void* buf = NULL;
        int64_t size = filestream_get_size(file);
        ok = false;
        if (size > 0 && size < 0x1000000 && (buf = malloc((size_t)size)) != NULL &&
            filestream_read(file, buf, size) == size) {
            len = size;
            ok = psf_open_buffer(psf, buf, (size_t)size);
        }
        free(buf);
    }
    filestream_close(file);
    if (!ok) {
        if (len <= 0) {
            c_log(C_LOG_CORE, C_LOG_LEVEL_ERROR, "psf_open_path", "SFO file at %s is empty", path);
        }
        psf_clear(psf);
    }
    return ok;
}

/* The room the file gives an entry's value */
static u32 psf_file_max_len(const psf_entry_t* e) {
    const u32 param_len = psf_param_len(e);
    return e->max_len > param_len ? e->max_len : param_len;
}

size_t psf_encoded_size(const psf_t* psf) {
    size_t size = PSF_HEADER_SIZE + (size_t)PSF_RAW_ENTRY_SIZE * psf->count;
    u32 i;
    for (i = 0; i < psf->count; i++) {
        size += strlen(psf_key(psf, &psf->entries[i])) + 1;
    }
    for (i = 0; i < psf->count; i++) {
        size = (size + 3) & ~(size_t)3;
        size += psf_file_max_len(&psf->entries[i]);
    }
    return size;
}

size_t psf_encode(const psf_t* psf, void* out, size_t size) {
    u8* buf = (u8*)out;
    const size_t total = psf_encoded_size(psf);
    size_t pos, key_table, data_table;
    u32 i;

    if (size < total) {
        return 0;
    }
    memset(buf, 0, total);
    psf_wr32be(buf, PSF_MAGIC);
    psf_wr32le(buf + 4, PSF_VERSION_1_1);
    psf_wr32le(buf + 16, psf->count);

    pos = PSF_HEADER_SIZE + (size_t)PSF_RAW_ENTRY_SIZE * psf->count;
    key_table = pos;
    psf_wr32le(buf + 8, (u32)key_table);
    for (i = 0; i < psf->count; i++) {
        const psf_entry_t* e = &psf->entries[i];
        u8* raw = buf + PSF_HEADER_SIZE + i * PSF_RAW_ENTRY_SIZE;
        const char* key = psf_key(psf, e);
        const size_t key_len = strlen(key);
        psf_wr16le(raw, (u16)(pos - key_table));
        psf_wr16le(raw + 2, e->fmt);
        psf_wr32le(raw + 8, psf_file_max_len(e));
        memcpy(buf + pos, key, key_len + 1);
        pos += key_len + 1;
    }

    data_table = pos;
    psf_wr32le(buf + 12, (u32)data_table);
    for (i = 0; i < psf->count; i++) {
        const psf_entry_t* e = &psf->entries[i];
        u8* raw = buf + PSF_HEADER_SIZE + i * PSF_RAW_ENTRY_SIZE;
        pos = (pos + 3) & ~(size_t)3;
        psf_wr32le(raw + 12, (u32)(pos - data_table));
        psf_wr32le(raw + 4, psf_param_len(e));
        memcpy(buf + pos, psf->arena + e->value, e->length); /* a string's terminator is a 0 */
        pos += psf_file_max_len(e);
    }
    return total;
}

bool psf_encode_path(psf_t* psf, const char* path) {
    u8 stack[4096];
    const size_t size = psf_encoded_size(psf);
    u8* buf = size <= sizeof(stack) ? stack : (u8*)malloc(size);
    bool ok;
    if (buf == NULL) {
        return false;
    }
    psf->last_write = (s64)time(NULL);
    ok = psf_encode(psf, buf, size) == size &&
         filestream_write_file_atomic(path, buf, (int64_t)size);
    if (!ok) {
        c_log(C_LOG_CORE, C_LOG_LEVEL_ERROR, "psf_encode_path", "Failed to write PSF file %s",
              path);
    }
    if (buf != stack) {
        free(buf);
    }
    return ok;
}

bool psf_get_string(const psf_t* psf, const char* key, const char** value, size_t* length) {
    const psf_entry_t* e = psf_find(psf, key);
    if (e == NULL) {
        return false;
    }
    C_ASSERT(e->fmt == PSF_FMT_TEXT, "psf_get_string");
    *value = (const char*)psf->arena + e->value;
    if (length != NULL) {
        *length = e->length;
    }
    return true;
}

bool psf_get_integer(const psf_t* psf, const char* key, s32* value) {
    const psf_entry_t* e = psf_find(psf, key);
    if (e == NULL) {
        return false;
    }
    C_ASSERT(e->fmt == PSF_FMT_INTEGER, "psf_get_integer");
    memcpy(value, psf->arena + e->value, 4);
    return true;
}

bool psf_get_binary(const psf_t* psf, const char* key, const u8** value, u32* length) {
    const psf_entry_t* e = psf_find(psf, key);
    if (e == NULL) {
        return false;
    }
    C_ASSERT(e->fmt == PSF_FMT_BINARY, "psf_get_binary");
    *value = psf->arena + e->value;
    *length = e->length;
    return true;
}

/* Puts @length bytes (and a terminator for a string) as @key's value */
static bool psf_set(psf_t* psf, const char* key, u16 fmt, const void* value, u32 length,
                    bool update, const char* func) {
    psf_entry_t* e = psf_find(psf, key);
    const u32 bytes = fmt == PSF_FMT_TEXT ? length + 1 : length;
    /* A value from this list's own arena, kept by its offset as the arena may move */
    const bool own = psf->arena != NULL && (const u8*)value >= psf->arena &&
                     (const u8*)value < psf->arena + psf->arena_used;
    const size_t own_offset = own ? (size_t)((const u8*)value - psf->arena) : 0;
    const u32 max_len =
        fmt == PSF_FMT_INTEGER ? 4 : psf_max_size(key, fmt == PSF_FMT_TEXT ? length + 1 : length);

    if (e != NULL && !update) {
        c_log(C_LOG_CORE, C_LOG_LEVEL_ERROR, func,
              "PSF: Tried to add %s key that already exists: %s",
              fmt == PSF_FMT_TEXT      ? "string"
              : fmt == PSF_FMT_INTEGER ? "integer"
                                       : "binary",
              key);
        return false;
    }
    if (e != NULL) {
        C_ASSERT_MSG(e->fmt == fmt, func, "PSF: Change format is not supported");
        if (bytes > e->room) {
            /* Outgrown: a new place at the end */
            if (!psf_reserve(psf, bytes)) {
                return false;
            }
            e->value = psf->arena_used;
            e->room = bytes;
            psf->arena_used += bytes;
        }
    } else {
        const size_t key_len = strlen(key);
        if (psf->count == PSF_MAX_ENTRIES || key_len > 0xffff ||
            !psf_reserve(psf, (u32)key_len + 1 + bytes)) {
            c_log(C_LOG_CORE, C_LOG_LEVEL_ERROR, func, "PSF: no room for key %s", key);
            return false;
        }
        e = &psf->entries[psf->count++];
        e->fmt = fmt;
        e->key = psf->arena_used;
        memcpy(psf->arena + e->key, key, key_len + 1);
        psf->arena_used += (u32)key_len + 1;
        e->value = psf->arena_used;
        e->room = bytes;
        psf->arena_used += bytes;
    }
    e->max_len = max_len;
    e->length = length;
    if (own) {
        value = psf->arena + own_offset;
    }
    if (length != 0) {
        memmove(psf->arena + e->value, value, length);
    }
    if (fmt == PSF_FMT_TEXT) {
        psf->arena[e->value + length] = 0;
    }
    return true;
}

bool psf_set_string(psf_t* psf, const char* key, const char* value, size_t length, bool update) {
    if (length > 0x40000000u) {
        return false;
    }
    return psf_set(psf, key, PSF_FMT_TEXT, value, (u32)length, update, "psf_set_string");
}

bool psf_set_integer(psf_t* psf, const char* key, s32 value, bool update) {
    return psf_set(psf, key, PSF_FMT_INTEGER, &value, 4, update, "psf_set_integer");
}

bool psf_set_binary(psf_t* psf, const char* key, const void* value, u32 length, bool update) {
    return psf_set(psf, key, PSF_FMT_BINARY, value, length, update, "psf_set_binary");
}

bool psf_set_binary_u64(psf_t* psf, const char* key, u64 value, bool update) {
    u8 bytes[8];
    psf_wr32le(bytes, (u32)value);
    psf_wr32le(bytes + 4, (u32)(value >> 32));
    return psf_set(psf, key, PSF_FMT_BINARY, bytes, 8, update, "psf_set_binary_u64");
}

static psf_t g_psf_game;

psf_t* psf_game(void) {
    return &g_psf_game;
}

bool psf_game_get_string(const char* key, char* out, size_t size, size_t* length) {
    const char* value;
    size_t value_length;
    if (!psf_get_string(&g_psf_game, key, &value, &value_length)) {
        return false;
    }
    if (length != NULL) {
        *length = value_length;
    }
    if (size != 0) {
        const size_t n = value_length < size - 1 ? value_length : size - 1;
        memcpy(out, value, n);
        out[n] = '\0';
    }
    return true;
}

bool psf_game_get_integer(const char* key, s32* value) {
    return psf_get_integer(&g_psf_game, key, value);
}
