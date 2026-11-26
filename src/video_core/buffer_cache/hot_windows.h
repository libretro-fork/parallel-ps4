/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The stretches of the game's memory it has lately read back from the GPU. A game reads the same
 * few back every frame, one after another, and each read costs a wait for the GPU. With them
 * remembered, the wait one of them makes can bring the others along, so the reads that follow
 * find their data already there.
 *
 * A fixed table, no allocation. One thread uses it: the one that serves the read backs. */

#ifndef SHADPS4_BUFFER_CACHE_HOT_WINDOWS_H
#define SHADPS4_BUFFER_CACHE_HOT_WINDOWS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The windows remembered at once */
#define HOT_WINDOWS_MAX 64
/* A window not read for this many read backs is forgotten */
#define HOT_WINDOWS_AGE 4096u

typedef struct hot_windows {
    uint64_t addr[HOT_WINDOWS_MAX];
    /* The count of read backs when the window was last read */
    uint32_t seen[HOT_WINDOWS_MAX];
    uint32_t seq;
    uint32_t count;
} hot_windows_t;

void hot_windows_init(hot_windows_t* t);

/* The game read @window back. It is remembered; when the table is full the window read longest
 * ago makes room. */
void hot_windows_note(hot_windows_t* t, uint64_t window);

/* The windows read within the last HOT_WINDOWS_AGE read backs, other than @except, into @out,
 * at most @max of them, the most lately read first. Older ones are forgotten. Returns how many. */
unsigned hot_windows_collect(hot_windows_t* t, uint64_t except, uint64_t* out, unsigned max);

/* Forgets a window: its memory is gone */
void hot_windows_drop(hot_windows_t* t, uint64_t window);

#ifdef __cplusplus
}
#endif

#endif
