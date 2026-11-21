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

RETRO_END_DECLS

#endif
