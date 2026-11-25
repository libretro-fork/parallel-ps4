/* SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_AVPLAYER_AVPLAYER_DATA_STREAMER_H
#define SHADPS4_AVPLAYER_AVPLAYER_DATA_STREAMER_H

/* A media file the demuxer reads by offset, for files that are not mapped: through the game's
 * file replacement callbacks, a file system backend's handle that has no host path, or a host
 * file through libretro-common's VFS. Defined in avplayer_data_streamer.c. */

#include <boolean.h>
#include <retro_common_api.h>

#include "common/types.h"
#include "core/libraries/avplayer/avplayer.h"

RETRO_BEGIN_DECLS

struct RFILE;

enum {
    AVPLAYER_STREAMER_NONE = 0,
    AVPLAYER_STREAMER_FILE_REPLACEMENT,
    AVPLAYER_STREAMER_HANDLE,
    AVPLAYER_STREAMER_VFS
};

typedef struct avplayer_streamer {
    int kind;
    /* AVPLAYER_STREAMER_FILE_REPLACEMENT */
    AvPlayerFileReplacement file_replacement;
    int fd;
    /* AVPLAYER_STREAMER_HANDLE: fs_open_guest_read's, owned */
    void* handle;
    /* AVPLAYER_STREAMER_VFS: the source's, not owned */
    struct RFILE* file;
    u64 file_size;
} avplayer_streamer_t;

void avplayer_streamer_init_file_replacement(avplayer_streamer_t* s,
                                             const AvPlayerFileReplacement* file_replacement);
void avplayer_streamer_init_handle(avplayer_streamer_t* s, void* handle);
void avplayer_streamer_init_vfs(avplayer_streamer_t* s, struct RFILE* file);
/* Opens the file at @path (the file replacement's), false if it cannot be read */
bool avplayer_streamer_open(avplayer_streamer_t* s, const char* path);
u64 avplayer_streamer_size(const avplayer_streamer_t* s);
/* Bytes read into @dst; fewer at the end of the file or on an error. */
u64 avplayer_streamer_read_at(avplayer_streamer_t* s, u64 offset, u8* dst, u64 size);
void avplayer_streamer_close(avplayer_streamer_t* s);

RETRO_END_DECLS

#endif
