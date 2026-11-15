/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* range_runs: sorted runs published by one writer, looked up by any thread.
 *   - empty: nothing found, no next run;
 *   - an address in a run, at its base, at its end (outside), between runs (the next one),
 *     above all;
 *   - more runs than the room: unknown, then known again;
 *   - a writer republishing two alternating sets as fast as it can while three readers look
 *     up: every answer is one of the two sets' whole answers, never a mix of the two. */

#include <stdio.h>

#include <retro_atomic.h>
#include <rthreads/rthreads.h>

#include "common/range_runs.h"

static int failures = 0;

#define CHECK(cond, what)                                                                      \
    do {                                                                                       \
        if (!(cond)) {                                                                         \
            printf("FAIL %s (line %d)\n", what, __LINE__);                                     \
            ++failures;                                                                        \
        }                                                                                      \
    } while (0)

#define CAP 64
#define READERS 3
#define ROUNDS 200000

static range_runs_t rr;
static retro_atomic_int_t stop;
static retro_atomic_int_t bad;
static retro_atomic_int_t looks;

/* Set A: [0x1000*k, 0x1000*k + 0x800) for k = 1..40. Set B: the same runs shifted up by
 * 0x400, and only 20 of them. The address probed, 0x10900, is in A's gap and in B's run. */
static void fill(int which) {
    range_run_t* r = range_runs_begin(&rr);
    unsigned k, n = which ? 20 : 40;
    for (k = 0; k < n; k++) {
        r[k].base = 0x1000ull * (k + 1) + (which ? 0x400 : 0);
        r[k].end = r[k].base + 0x800;
    }
    range_runs_publish(&rr, n);
}

static void reader(void* data) {
    (void)data;
    while (!retro_atomic_load_acquire_int(&stop)) {
        uint64_t base, end;
        const int found = range_runs_lookup(&rr, 0x10900, &base, &end);
        /* A: not in a run, the next 0x11000..0x11800. B: in 0x10400..0x10c00. */
        if (!((found == 0 && base == 0x11000 && end == 0x11800) ||
              (found == 1 && base == 0x10400 && end == 0x10c00)))
            retro_atomic_fetch_add_int(&bad, 1);
        retro_atomic_fetch_add_int(&looks, 1);
    }
}

int main(void) {
    sthread_t* threads[READERS];
    range_run_t* r;
    uint64_t base, end;
    unsigned k;
    int i;

    CHECK(range_runs_init(&rr, CAP), "init");
    CHECK(range_runs_lookup(&rr, 0x1234, &base, &end) == 0 && base == 0 && end == 0,
          "empty: nothing, no next");

    r = range_runs_begin(&rr);
    r[0].base = 0x1000; r[0].end = 0x3000;
    r[1].base = 0x5000; r[1].end = 0x6000;
    range_runs_publish(&rr, 2);
    CHECK(range_runs_lookup(&rr, 0x2000, &base, &end) == 1 && base == 0x1000 && end == 0x3000,
          "in a run");
    CHECK(range_runs_lookup(&rr, 0x1000, &base, &end) == 1 && base == 0x1000, "at its base");
    CHECK(range_runs_lookup(&rr, 0x3000, &base, &end) == 0 && base == 0x5000 && end == 0x6000,
          "at its end: outside, the next run");
    CHECK(range_runs_lookup(&rr, 0x0800, &base, &end) == 0 && base == 0x1000,
          "below all: the first run next");
    CHECK(range_runs_lookup(&rr, 0x7000, &base, &end) == 0 && base == 0 && end == 0,
          "above all: none next");

    r = range_runs_begin(&rr);
    for (k = 0; k < CAP; k++) {
        r[k].base = 0x1000ull * k;
        r[k].end = r[k].base + 0x10;
    }
    range_runs_publish(&rr, CAP + 1);
    CHECK(range_runs_lookup(&rr, 0x1000, &base, &end) == -1, "too many: unknown");
    fill(0);
    CHECK(range_runs_lookup(&rr, 0x1000, &base, &end) == 1, "known again");

    retro_atomic_int_init(&stop, 0);
    retro_atomic_int_init(&bad, 0);
    retro_atomic_int_init(&looks, 0);
    for (i = 0; i < READERS; i++)
        threads[i] = sthread_create(reader, NULL);
    for (i = 0; i < ROUNDS; i++)
        fill(i & 1);
    retro_atomic_store_release_int(&stop, 1);
    for (i = 0; i < READERS; i++)
        sthread_join(threads[i]);
    CHECK(retro_atomic_load_acquire_int(&bad) == 0, "never a mix of two sets");
    printf("range_runs: %d lookups against %d publishes\n",
           retro_atomic_load_acquire_int(&looks), ROUNDS);

    range_runs_free(&rr);
    if (failures) {
        printf("range_runs: %d failed\n", failures);
        return 1;
    }
    printf("range_runs: all passed\n");
    return 0;
}
