/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_COMMON_RANGE_RUNS_H
#define SHADPS4_COMMON_RANGE_RUNS_H

#include <stdint.h>

#include <retro_atomic.h>
#include <rthreads/retro_seqlock.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Sorted runs of addresses, [base, end), published by one writer at a time and looked up by
 * any thread without a lock and without ever waiting on the writer's progress: two copies,
 * each under its own retro_seqlock; the writer fills the one readers are not on and then
 * points them at it. A reader that was still on the copy the next write takes finds its
 * seqlock moved and looks again in the new one. Both copies are allocated once, at init:
 * nothing is allocated after. */
typedef struct range_run {
    uint64_t base;
    uint64_t end;
} range_run_t;

typedef struct range_runs {
    range_run_t* runs[2];
    unsigned count[2];
    int overflow[2];
    retro_seqlock_t lock[2];
    retro_atomic_int_t current;
    unsigned capacity;
    int writing; /* the copy being written, the writer's own */
} range_runs_t;

/* Room for @capacity runs in each copy. Starts empty. @return 0 if out of memory. */
int range_runs_init(range_runs_t* rr, unsigned capacity);
void range_runs_free(range_runs_t* rr);

/* The writer (one at a time): the copy to fill, room for range_runs_capacity runs. Fill it
 * sorted, not overlapping, then range_runs_publish with how many there are; more than the
 * capacity marks the set unknown (range_runs_lookup says so) */
range_run_t* range_runs_begin(range_runs_t* rr);
void range_runs_publish(range_runs_t* rr, unsigned count);

/* Any thread. @return 1 if @addr lies in a run (*base, *end its run), 0 if it does not
 * (*base, *end the next run above it, or both 0 if none), -1 if the set is unknown */
int range_runs_lookup(range_runs_t* rr, uint64_t addr, uint64_t* base, uint64_t* end);

#ifdef __cplusplus
}
#endif

#endif
