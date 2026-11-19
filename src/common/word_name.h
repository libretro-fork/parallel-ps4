/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* A short name (a game thread's) that any thread may read while another renames it: its bytes
 * packed four to an atomic word, written and read a word at a time. A read during a rename may
 * see some of each name, never a byte that was neither's, and never past its end. No lock,
 * nothing allocated. */

#ifndef SHADPS4_WORD_NAME_H
#define SHADPS4_WORD_NAME_H

#include <stddef.h>

#include <retro_atomic.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

#define WORD_NAME_MAX 32 /* bytes kept, the terminator not counted */
#define WORD_NAME_WORDS ((WORD_NAME_MAX + 4) / 4)

typedef struct word_name {
    retro_atomic_int_t words[WORD_NAME_WORDS];
} word_name_t;

/* Empty. */
void word_name_init(word_name_t* name);
/* @text (NULL: empty), cut to WORD_NAME_MAX bytes. */
void word_name_set(word_name_t* name, const char* text);
/* Into @out, terminated: WORD_NAME_MAX + 1 bytes. Returns its length. */
size_t word_name_get(const word_name_t* name, char* out);

RETRO_END_DECLS

#endif
