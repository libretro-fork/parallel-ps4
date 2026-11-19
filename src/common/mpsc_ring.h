/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* A bounded queue of fixed-size items that many threads put into and one takes from, in the
 * order they were put (Vyukov's): one atomic count a slot, no lock, nothing allocated after it
 * is made. A put into a full queue fails rather than waits. Waking the taker is the caller's
 * (an eventcount). */

#ifndef SHADPS4_MPSC_RING_H
#define SHADPS4_MPSC_RING_H

#include <stddef.h>

#include <retro_atomic.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

typedef struct mpsc_ring {
    unsigned char* items;
    retro_atomic_int_t* seq; /* each slot's turn */
    size_t item_size;
    unsigned mask;           /* slots - 1 */
    retro_atomic_int_t tail; /* the next put's turn */
    retro_atomic_int_t head; /* the next take's turn: the taker's alone, read by others */
} mpsc_ring_t;

/* Empty, @slots (a power of two) items of @item_size made now. 0 if they could not be. */
int mpsc_ring_init(mpsc_ring_t* ring, unsigned slots, size_t item_size);
void mpsc_ring_free(mpsc_ring_t* ring);

/* From any thread: @item copied in. 0 if the queue is full. */
int mpsc_ring_put(mpsc_ring_t* ring, const void* item);

/* From the one taking thread: the oldest copied into @item and taken. 0 if none. */
int mpsc_ring_take(mpsc_ring_t* ring, void* item);

/* Whether anything is queued (a look: another thread may put one at once). */
int mpsc_ring_empty(mpsc_ring_t* ring);

RETRO_END_DECLS

#endif
