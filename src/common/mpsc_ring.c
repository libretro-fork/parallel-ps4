/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdlib.h>
#include <string.h>

#include "common/mpsc_ring.h"

int mpsc_ring_init(mpsc_ring_t* ring, unsigned slots, size_t item_size) {
    unsigned i;
    memset(ring, 0, sizeof(*ring));
    if (slots == 0 || (slots & (slots - 1)) != 0) {
        return 0;
    }
    ring->items = (unsigned char*)calloc(slots, item_size);
    ring->seq = (retro_atomic_int_t*)calloc(slots, sizeof(*ring->seq));
    if (!ring->items || !ring->seq) {
        mpsc_ring_free(ring);
        return 0;
    }
    ring->item_size = item_size;
    ring->mask = slots - 1;
    for (i = 0; i < slots; i++) {
        retro_atomic_int_init(&ring->seq[i], (int)i);
    }
    retro_atomic_int_init(&ring->tail, 0);
    retro_atomic_int_init(&ring->head, 0);
    return 1;
}

void mpsc_ring_free(mpsc_ring_t* ring) {
    free(ring->items);
    free((void*)ring->seq);
    ring->items = NULL;
    ring->seq = NULL;
}

int mpsc_ring_put(mpsc_ring_t* ring, const void* item) {
    for (;;) {
        const int pos = retro_atomic_load_acquire_int(&ring->tail);
        const unsigned slot = (unsigned)pos & ring->mask;
        const int seq = retro_atomic_load_acquire_int(&ring->seq[slot]);
        const int diff = (int)((unsigned)seq - (unsigned)pos);
        if (diff == 0) {
            /* The slot's turn is this put's: had once the count moves past it */
            if (retro_atomic_cas_int(&ring->tail, pos, (int)((unsigned)pos + 1u))) {
                memcpy(ring->items + (size_t)slot * ring->item_size, item, ring->item_size);
                retro_atomic_store_release_int(&ring->seq[slot], (int)((unsigned)pos + 1u));
                return 1;
            }
        } else if (diff < 0) {
            /* Not yet taken from the time round before: full */
            return 0;
        }
        /* Another put took the turn: the next */
    }
}

int mpsc_ring_take(mpsc_ring_t* ring, void* item) {
    const int pos = retro_atomic_load_relaxed_int(&ring->head);
    const unsigned slot = (unsigned)pos & ring->mask;
    const int seq = retro_atomic_load_acquire_int(&ring->seq[slot]);
    if (seq != (int)((unsigned)pos + 1u)) {
        return 0;
    }
    memcpy(item, ring->items + (size_t)slot * ring->item_size, ring->item_size);
    retro_atomic_store_release_int(&ring->seq[slot], (int)((unsigned)pos + ring->mask + 1u));
    retro_atomic_store_release_int(&ring->head, (int)((unsigned)pos + 1u));
    return 1;
}

int mpsc_ring_empty(mpsc_ring_t* ring) {
    const int pos = retro_atomic_load_acquire_int(&ring->head);
    return retro_atomic_load_acquire_int(&ring->seq[(unsigned)pos & ring->mask]) !=
           (int)((unsigned)pos + 1u);
}
