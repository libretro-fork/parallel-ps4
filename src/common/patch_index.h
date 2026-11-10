/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_COMMON_PATCH_INDEX_H
#define SHADPS4_COMMON_PATCH_INDEX_H

/* A patch repository's files.json maps each patch file to the serials it
 * is for: { "Game.xml": ["CUSA00001", "CUSA00002"], ... } */

#include <stddef.h>

#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* Finds the first patch file in index_path listing serial and writes its
 * name into s. Returns false when there is none or the index is bad (which
 * is logged). */
bool patch_index_find(const char* index_path, const char* serial, char* s, size_t len);

RETRO_END_DECLS

#endif
