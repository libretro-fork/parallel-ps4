/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_FILE_SYS_FS_C_H
#define SHADPS4_FILE_SYS_FS_C_H

/* The game's file system, for code written in C. Defined in fs.cpp. */

#include <stddef.h>

#include <boolean.h>
#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

/* The whole file at @guest_path (a path of the game's: /app0/...), lent to @use for as long as
 * it runs: mapped where it lies, or decoded out of an archive (MntPoints::ReadFile). false,
 * with @use not called, if there is no such file or it cannot be read. */
bool fs_lend_guest_file(const char* guest_path, void (*use)(const u8* data, size_t size, void* ctx),
                        void* ctx);

/* The additional content in the host folder @dir (UTF-8): its folders, .zar and .pkg files as
 * the folder lists them, then with @with_beside the PKGs the libretro core found beside the
 * game's PKG (MntPoints::libretro_addcont). Each one's host path is given to @use, until it
 * returns false. */
void fs_for_each_addcont(const char* dir, bool with_beside,
                         bool (*use)(const char* path, void* ctx), void* ctx);

/* fs_read_addcont's results */
#define FS_ADDCONT_OK 0
#define FS_ADDCONT_NO_BACKEND 1   /* a PKG that cannot be read (no keys, cut short) */
#define FS_ADDCONT_NO_PARAM_SFO 2 /* no sce_sys/param.sfo */
#define FS_ADDCONT_BAD_PARAM_SFO 3

#define FS_ADDCONT_CATEGORY_SIZE 8
#define FS_ADDCONT_CONTENT_ID_SIZE 128
#define FS_ADDCONT_KEY_SIZE 16

/* What a piece of additional content's param.sfo says, and its entitlement key */
typedef struct fs_addcont {
    bool has_category;
    char category[FS_ADDCONT_CATEGORY_SIZE]; /* terminated, cut to fit */
    bool has_content_id;
    size_t content_id_length;                    /* the whole of it */
    char content_id[FS_ADDCONT_CONTENT_ID_SIZE]; /* terminated, cut to fit */
    bool has_key;
    u8 key[FS_ADDCONT_KEY_SIZE];
} fs_addcont_t;

/* Reads the additional content at the host path @path (a folder, .zar or .pkg) through a
 * backend of its own, read-only: its param.sfo and, with @want_key, its entitlement key, from
 * sce_sys/license.info or else sce_sys/license.dat. FS_ADDCONT_OK, or why it could not. */
s32 fs_read_addcont(const char* path, bool want_key, fs_addcont_t* out);

/* MntPoints::Mount and Unmount: the host folder or file @host_path (UTF-8) at the game's
 * @guest_folder */
void fs_mount(const char* host_path, const char* guest_folder, bool read_only);
void fs_unmount(const char* guest_folder);

/* The host folder @host_dir made (emptied first with @format) and mounted at @guest_folder */
void fs_mount_made_folder(const char* host_dir, bool format, const char* guest_folder);

RETRO_END_DECLS

#endif
