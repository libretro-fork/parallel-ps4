/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stddef.h>
#include <stdint.h>

#include "common/node_pool.h"

void node_pool_init(node_pool_t* pool) {
    retro_u128_t empty;
    empty.lo = 0;
    empty.hi = 0;
    retro_atomic_128_init(&pool->head, empty);
}

void node_pool_push(node_pool_t* pool, node_pool_node_t* node) {
    retro_u128_t seen = retro_atomic_load_acquire_128(&pool->head);
    retro_u128_t top;
    /* The link is written with release and read with acquire: the swap orders everything
     * already, but a sanitizer does not see into the swap, and so sees the node's contents
     * handed over through its link */
    for (;;) {
        retro_atomic_store_release_ptr(&node->next, (void*)(uintptr_t)seen.lo);
        top.lo = (uint64_t)(uintptr_t)node;
        top.hi = seen.hi + 1;
        if (retro_atomic_cas_seen_128(&pool->head, &seen, top))
            return;
    }
}

node_pool_node_t* node_pool_pop(node_pool_t* pool) {
    retro_u128_t seen = retro_atomic_load_acquire_128(&pool->head);
    retro_u128_t next;
    for (;;) {
        node_pool_node_t* node = (node_pool_node_t*)(uintptr_t)seen.lo;
        if (!node)
            return NULL;
        /* May be another thread's by now: then the swap below fails, as the generation moved */
        next.lo = (uint64_t)(uintptr_t)retro_atomic_load_acquire_ptr(&node->next);
        next.hi = seen.hi + 1;
        if (retro_atomic_cas_seen_128(&pool->head, &seen, next))
            return node;
    }
}
