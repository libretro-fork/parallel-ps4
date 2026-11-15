/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdlib.h>

#include "common/range_runs.h"

int range_runs_init(range_runs_t* rr, unsigned capacity) {
    rr->runs[0] = (range_run_t*)malloc(sizeof(range_run_t) * (capacity ? capacity : 1));
    rr->runs[1] = (range_run_t*)malloc(sizeof(range_run_t) * (capacity ? capacity : 1));
    rr->count[0] = rr->count[1] = 0;
    rr->overflow[0] = rr->overflow[1] = 0;
    retro_seqlock_init(&rr->lock[0]);
    retro_seqlock_init(&rr->lock[1]);
    retro_atomic_int_init(&rr->current, 0);
    rr->capacity = capacity;
    rr->writing = -1;
    if (!rr->runs[0] || !rr->runs[1]) {
        range_runs_free(rr);
        return 0;
    }
    return 1;
}

void range_runs_free(range_runs_t* rr) {
    free(rr->runs[0]);
    free(rr->runs[1]);
    rr->runs[0] = rr->runs[1] = NULL;
    rr->capacity = 0;
}

range_run_t* range_runs_begin(range_runs_t* rr) {
    const int b = 1 - retro_atomic_load_relaxed_int(&rr->current);
    rr->writing = b;
    retro_seqlock_write_begin(&rr->lock[b]);
    return rr->runs[b];
}

void range_runs_publish(range_runs_t* rr, unsigned count) {
    const int b = rr->writing;
    rr->overflow[b] = count > rr->capacity;
    rr->count[b] = count > rr->capacity ? rr->capacity : count;
    retro_seqlock_write_end(&rr->lock[b]);
    retro_atomic_store_release_int(&rr->current, b);
    rr->writing = -1;
}

int range_runs_lookup(range_runs_t* rr, uint64_t addr, uint64_t* base, uint64_t* end) {
    for (;;) {
        const int b = retro_atomic_load_acquire_int(&rr->current);
        const int seq = retro_seqlock_read_begin(&rr->lock[b]);
        const range_run_t* runs = rr->runs[b];
        unsigned n = rr->count[b];
        unsigned lo = 0, hi;
        int result;
        uint64_t got_base = 0, got_end = 0;
        if (n > rr->capacity)
            n = rr->capacity; /* torn: the retry below throws it away */
        if (rr->overflow[b]) {
            result = -1;
        } else {
            /* The first run whose end is above addr */
            hi = n;
            while (lo < hi) {
                const unsigned mid = lo + (hi - lo) / 2;
                if (runs[mid].end <= addr)
                    lo = mid + 1;
                else
                    hi = mid;
            }
            if (lo < n) {
                got_base = runs[lo].base;
                got_end = runs[lo].end;
                result = got_base <= addr ? 1 : 0;
            } else {
                result = 0;
            }
        }
        if (!retro_seqlock_read_retry(&rr->lock[b], seq)) {
            *base = got_base;
            *end = got_end;
            return result;
        }
    }
}
