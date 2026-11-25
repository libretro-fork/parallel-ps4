/* SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <string.h>

#include <streams/file_stream.h>

#include "core/file_sys/fs_c.h"
#include "core/libraries/avplayer/avplayer_data_streamer.h"

void avplayer_streamer_init_file_replacement(avplayer_streamer_t* s,
                                             const AvPlayerFileReplacement* file_replacement) {
    memset(s, 0, sizeof(*s));
    s->kind = AVPLAYER_STREAMER_FILE_REPLACEMENT;
    s->file_replacement = *file_replacement;
    s->fd = -1;
}

void avplayer_streamer_init_handle(avplayer_streamer_t* s, void* handle) {
    memset(s, 0, sizeof(*s));
    s->kind = AVPLAYER_STREAMER_HANDLE;
    s->handle = handle;
}

void avplayer_streamer_init_vfs(avplayer_streamer_t* s, struct RFILE* file) {
    memset(s, 0, sizeof(*s));
    s->kind = AVPLAYER_STREAMER_VFS;
    s->file = file;
}

bool avplayer_streamer_open(avplayer_streamer_t* s, const char* path) {
    switch (s->kind) {
    case AVPLAYER_STREAMER_FILE_REPLACEMENT:
        s->fd = s->file_replacement.open(s->file_replacement.object_ptr, path);
        if (s->fd < 0) {
            return false;
        }
        s->file_size = s->file_replacement.size(s->file_replacement.object_ptr);
        return true;
    case AVPLAYER_STREAMER_HANDLE:
        if (!fs_guest_file_is_open(s->handle)) {
            return false;
        }
        s->file_size = fs_guest_file_size(s->handle);
        return true;
    case AVPLAYER_STREAMER_VFS:
        return s->file != NULL;
    default:
        return false;
    }
}

u64 avplayer_streamer_size(const avplayer_streamer_t* s) {
    if (s->kind == AVPLAYER_STREAMER_VFS) {
        const s64 size = filestream_get_size((RFILE*)s->file);
        return size > 0 ? (u64)size : 0;
    }
    return s->file_size;
}

static u64 avplayer_min_u64(u64 a, u64 b) {
    return a < b ? a : b;
}

u64 avplayer_streamer_read_at(avplayer_streamer_t* s, u64 offset, u8* dst, u64 size) {
    switch (s->kind) {
    case AVPLAYER_STREAMER_FILE_REPLACEMENT: {
        u64 done = 0;
        while (done < size && offset + done < s->file_size) {
            /* The callback takes a 32-bit length */
            const u32 chunk = (u32)avplayer_min_u64(
                avplayer_min_u64(size - done, s->file_size - offset - done), (u64)1 << 30);
            const s32 got = s->file_replacement.read_offset(s->file_replacement.object_ptr,
                                                            dst + done, offset + done, chunk);
            if (got <= 0) {
                break;
            }
            done += (u64)got;
        }
        return done;
    }
    case AVPLAYER_STREAMER_HANDLE: {
        s64 got;
        if (offset >= s->file_size) {
            return 0;
        }
        got = fs_guest_file_read_at(s->handle, offset, dst,
                                    avplayer_min_u64(size, s->file_size - offset));
        return got > 0 ? (u64)got : 0;
    }
    case AVPLAYER_STREAMER_VFS: {
        s64 got;
        if (filestream_seek((RFILE*)s->file, (s64)offset, RETRO_VFS_SEEK_POSITION_START) != 0) {
            return 0;
        }
        got = filestream_read((RFILE*)s->file, dst, size);
        return got > 0 ? (u64)got : 0;
    }
    default:
        return 0;
    }
}

void avplayer_streamer_close(avplayer_streamer_t* s) {
    switch (s->kind) {
    case AVPLAYER_STREAMER_FILE_REPLACEMENT:
        if (s->file_replacement.close != NULL && s->fd >= 0) {
            s->file_replacement.close(s->file_replacement.object_ptr);
        }
        break;
    case AVPLAYER_STREAMER_HANDLE:
        if (s->handle) {
            fs_guest_file_close(s->handle);
        }
        break;
    default:
        break;
    }
    s->kind = AVPLAYER_STREAMER_NONE;
}
