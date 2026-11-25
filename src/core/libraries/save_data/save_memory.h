/* SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_SAVE_DATA_SAVE_MEMORY_H
#define SHADPS4_LIBRARIES_SAVE_DATA_SAVE_MEMORY_H

/* Save data memory (save_memory.c): a save the game reads and writes as bytes, kept in
 * memory.dat of the save sce_sdmemory (sce_sdmemoryN for slot N), with a param.sfo and an
 * icon. Up to SAVE_MEMORY_SLOTS slots. */

#include <stddef.h>

#include <boolean.h>
#include <retro_common_api.h>

#include "common/types.h"
#include "core/file_format/psf.h"

RETRO_BEGIN_DECLS

#define SAVE_MEMORY_SLOTS 16

/* Writes the slot's bytes to memory.dat */
void save_memory_persist(u32 slot_id);

/* The slot's save folder name */
void save_memory_dir_name(u32 slot_id, char* out, size_t size);
/* The slot's save folder */
void save_memory_save_path(s32 user_id, u32 slot_id, const char* game_serial, char* out,
                           size_t size);

/* Attaches the slot, @memory_size bytes, with a new param.sfo or the one there (restoring the
 * backup of a broken save): false if there is no room for another slot. @existed_size is the
 * size of the memory.dat there, 0 if none. */
bool save_memory_setup(s32 user_id, u32 slot_id, const char* game_serial, size_t memory_size,
                       size_t* existed_size);

/* Writes the slot's icon from @buf, or with @buf NULL the game's own */
void save_memory_set_icon(u32 slot_id, const void* buf, size_t buf_size);

bool save_memory_is_initialized(u32 slot_id);

/* The slot's param.sfo (NULL if the slot is not attached) */
psf_t* save_memory_param_sfo(u32 slot_id);

/* Copies the slot's icon0.png into @buf, up to @size bytes: how many */
size_t save_memory_copy_icon(u32 slot_id, void* buf, size_t size);

/* Writes the slot's param.sfo: false if it could not */
bool save_memory_save_sfo(u32 slot_id);

void save_memory_read(u32 slot_id, void* buf, size_t buf_size, s64 offset);
void save_memory_write(u32 slot_id, const void* buf, size_t buf_size, s64 offset);

RETRO_END_DECLS

#endif
