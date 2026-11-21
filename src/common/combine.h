/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef PARALLEL_PS4_COMMON_COMBINE_H
#define PARALLEL_PS4_COMMON_COMBINE_H

/* Runs functions one at a time, in the order they were handed over, without a lock: what a
 * mutex around them would give, with no thread ever parked on a lock another holds.
 *
 * A caller pushes its request (on its own stack: nothing is allocated) onto a lock-free
 * mpsc_stack and then tries to become the combiner. The one that does runs every request
 * queued, its own and the others', oldest first, until none is left; the others park on a
 * retro_eventcount until theirs has run. A function that hands work to the same combiner from
 * inside it has it run there and then, as a recursive mutex would. */

#include <queues/mpsc_stack.h>
#include <retro_atomic.h>
#include <retro_common_api.h>
#include <rthreads/retro_eventcount.h>

RETRO_BEGIN_DECLS

typedef struct combiner {
    mpsc_stack_t pending;
    retro_atomic_int_t busy;
    /* The thread running the functions, while it does (its sthread id) */
    retro_atomic_ptr_t owner;
    retro_eventcount_t done_ec;
} combiner_t;

void combiner_init(combiner_t* c);
void combiner_free(combiner_t* c);

/* Runs @call(@ctx) with no other function of @c running */
void combiner_run(combiner_t* c, void (*call)(void* ctx), void* ctx);

RETRO_END_DECLS

#endif
