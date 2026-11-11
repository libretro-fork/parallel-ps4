/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdlib.h>
#include <string.h>

#include <retro_atomic.h>

#include "video_core/buffer_cache/write_snapshots.h"

#if !defined(RETRO_ATOMIC_LOCK_FREE) || !defined(RETRO_ATOMIC_HAS_64)
#error "write_snapshots needs libretro-common's lock-free 64-bit atomics"
#endif

/* A slot's tag: free, held by the one thread writing or reading it, or its page's address
 * (never 0 or 1: a multiple of the page size, and page 0 is never the guest's) */
#define TAG_FREE 0
#define TAG_BUSY 1
/* Slots a page may be in, from its hash on */
#define PROBES 4u

struct write_snapshots {
    retro_atomic_64_t tags[WRITE_SNAPSHOT_SLOTS];
    uint64_t seqs[WRITE_SNAPSHOT_SLOTS]; /* a slot's, its holder's to read and write */
    retro_atomic_64_t next_seq;
    unsigned char* data;
};

static uint32_t slot_of(uint64_t page, uint32_t probe) {
    const uint64_t hash = (page / WRITE_SNAPSHOT_PAGE) * 0x9E3779B97F4A7C15ull;
    return ((uint32_t)(hash >> 40) + probe) & (WRITE_SNAPSHOT_SLOTS - 1);
}

write_snapshots_t* write_snapshots_create(void) {
    write_snapshots_t* snapshots = (write_snapshots_t*)calloc(1, sizeof(*snapshots));
    uint32_t i;
    if (!snapshots) {
        return NULL;
    }
    /* Not touched here: a slot's page is resident once a snapshot has been put in it */
    snapshots->data = (unsigned char*)malloc((size_t)WRITE_SNAPSHOT_SLOTS * WRITE_SNAPSHOT_PAGE);
    if (!snapshots->data) {
        free(snapshots);
        return NULL;
    }
    for (i = 0; i < WRITE_SNAPSHOT_SLOTS; ++i) {
        retro_atomic_64_init(&snapshots->tags[i], TAG_FREE);
    }
    retro_atomic_64_init(&snapshots->next_seq, 1);
    return snapshots;
}

void write_snapshots_destroy(write_snapshots_t* snapshots) {
    if (snapshots) {
        free(snapshots->data);
        free(snapshots);
    }
}

void write_snapshots_take(write_snapshots_t* snapshots, uint64_t page, const void* content) {
    uint32_t probe, slot = 0;
    int64_t tag;
    int held = 0;
    /* A free slot, else whichever can be had. Not the page's own for choice: an upload under
     * way may be about to claim the one there. */
    for (probe = 0; probe < PROBES && !held; ++probe) {
        slot = slot_of(page, probe);
        held = retro_atomic_cas_64(&snapshots->tags[slot], TAG_FREE, TAG_BUSY);
    }
    for (probe = 0; probe < PROBES && !held; ++probe) {
        slot = slot_of(page, probe);
        tag = retro_atomic_load_acquire_64(&snapshots->tags[slot]);
        if (tag != TAG_BUSY) {
            held = retro_atomic_cas_64(&snapshots->tags[slot], tag, TAG_BUSY);
        }
    }
    if (!held) {
        return;
    }
    snapshots->seqs[slot] = (uint64_t)retro_atomic_fetch_add_64(&snapshots->next_seq, 1);
    memcpy(snapshots->data + (size_t)slot * WRITE_SNAPSHOT_PAGE, content, WRITE_SNAPSHOT_PAGE);
    retro_atomic_store_release_64(&snapshots->tags[slot], (int64_t)page);
}

uint64_t write_snapshots_cutoff(const write_snapshots_t* snapshots) {
    /* retro_atomic's loads take a pointer to non-const */
    return (uint64_t)retro_atomic_load_acquire_64(&((write_snapshots_t*)snapshots)->next_seq);
}

const unsigned char* write_snapshots_claim(write_snapshots_t* snapshots, uint64_t page,
                                           uint64_t cutoff, uint32_t* slot) {
    uint32_t probe, at, best = WRITE_SNAPSHOT_SLOTS;
    for (probe = 0; probe < PROBES; ++probe) {
        at = slot_of(page, probe);
        if (retro_atomic_load_acquire_64(&snapshots->tags[at]) != (int64_t)page ||
            !retro_atomic_cas_64(&snapshots->tags[at], (int64_t)page, TAG_BUSY)) {
            continue;
        }
        if (snapshots->seqs[at] >= cutoff) {
            /* Taken since: for the page's next upload */
            retro_atomic_store_release_64(&snapshots->tags[at], (int64_t)page);
        } else if (best == WRITE_SNAPSHOT_SLOTS) {
            best = at;
        } else if (snapshots->seqs[at] > snapshots->seqs[best]) {
            retro_atomic_store_release_64(&snapshots->tags[best], TAG_FREE);
            best = at;
        } else {
            retro_atomic_store_release_64(&snapshots->tags[at], TAG_FREE);
        }
    }
    if (best == WRITE_SNAPSHOT_SLOTS) {
        return NULL;
    }
    *slot = best;
    return snapshots->data + (size_t)best * WRITE_SNAPSHOT_PAGE;
}

void write_snapshots_release(write_snapshots_t* snapshots, uint32_t slot) {
    retro_atomic_store_release_64(&snapshots->tags[slot], TAG_FREE);
}

void write_snapshots_changed(const unsigned char* now, const unsigned char* base, uint32_t begin,
                             uint32_t end, write_snapshots_run_t run, void* ctx) {
    uint32_t at = begin, run_begin = 0, cell_end;
    int in_run = 0;
    while (at < end) {
        cell_end = (at | 3u) + 1u;
        if (cell_end > end) {
            cell_end = end;
        }
        if (memcmp(now + at, base + at, cell_end - at) != 0) {
            if (!in_run) {
                run_begin = at;
                in_run = 1;
            }
        } else if (in_run) {
            run(ctx, run_begin, at - run_begin);
            in_run = 0;
        }
        at = cell_end;
    }
    if (in_run) {
        run(ctx, run_begin, end - run_begin);
    }
}
