/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_COMMON_NODE_POOL_H
#define SHADPS4_COMMON_NODE_POOL_H

/* A lock-free pool of free nodes, any number of threads taking and giving
 * back: a stack whose head is a node with a generation, swapped as one in
 * retro_atomic's 128-bit word, so that a node taken and given back between
 * one thread's look at the head and its swap is not taken for the same head.
 *
 * The nodes are the caller's, a node_pool_node_t in whatever they belong to,
 * and must outlive the pool: a thread can read the link of a node another
 * thread has just taken (an atomic, so that read is not a race). Nothing is
 * allocated. */

#include <retro_atomic.h>
#include <retro_common_api.h>

#if !defined(RETRO_ATOMIC_HAS_128)
#error "node_pool needs retro_atomic's 128-bit word"
#endif

RETRO_BEGIN_DECLS

typedef struct node_pool_node {
    retro_atomic_ptr_t next;
} node_pool_node_t;

typedef struct node_pool {
    retro_atomic_128_t head;
} node_pool_t;

/* Empty */
void node_pool_init(node_pool_t* pool);
/* Gives a node to the pool */
void node_pool_push(node_pool_t* pool, node_pool_node_t* node);
/* Takes a node, or NULL when there is none */
node_pool_node_t* node_pool_pop(node_pool_t* pool);

RETRO_END_DECLS

#endif
