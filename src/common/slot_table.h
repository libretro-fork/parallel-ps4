/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* A table of slots threads take and give back, one atomic word a slot taken by
 * compare-and-swap: the lowest free one in a range is had by exactly one thread, and none
 * waits. The game's libc file table. */

#ifndef SHADPS4_SLOT_TABLE_H
#define SHADPS4_SLOT_TABLE_H

#include <retro_atomic.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* The lowest slot of [@first, @end) that was free, now taken by the caller; -1 if none was. */
int slot_table_take(retro_atomic_int_t* slots, int first, int end);
/* Whether @index is taken. */
int slot_table_taken(retro_atomic_int_t* slots, int index);
/* @index, taken by the caller, is free again. */
void slot_table_give(retro_atomic_int_t* slots, int index);

RETRO_END_DECLS

#endif
