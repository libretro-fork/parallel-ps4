/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* A reader-writer word: the guest's rwlocks, as one atomic int taken by compare-and-swap.
 * 0 free, n > 0 held by n readers, -1 held by a writer. Readers come first: a reader gets in
 * whenever no writer holds it, as the lock it stands for let them. Taking never waits here:
 * a thread turned away parks where its caller parks it (on the game's time) and tries again
 * when an unlock says the word may now be had. */

#ifndef SHADPS4_RW_WORD_H
#define SHADPS4_RW_WORD_H

#include <retro_atomic.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* Free. */
void rw_word_init(retro_atomic_int_t* word);

/* Taken for reading (shared) or writing (alone): 1, or 0 and nothing changed. */
int rw_word_try_read(retro_atomic_int_t* word);
int rw_word_try_write(retro_atomic_int_t* word);

/* Given back. 1 if the word is free now, and threads turned away should try again; a reader
 * that leaves others reading returns 0. -1, and nothing changed, if it was not held that way. */
int rw_word_unlock_read(retro_atomic_int_t* word);
int rw_word_unlock_write(retro_atomic_int_t* word);

RETRO_END_DECLS

#endif
