/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The buffer cache's page snapshots (src/video_core/buffer_cache/write_snapshots.c): a
 * snapshot is found again and holds what was given, the cutoff parts the ones taken before a
 * page's upload from the ones after, only the bytes changed since are reported and the GPU's
 * are left out, a full table drops old pages and nothing else, and threads taking snapshots
 * while another claims them never hand over a torn page. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <retro_atomic.h>
#include <rthreads/rthreads.h>

#include "../../src/video_core/buffer_cache/write_snapshots.h"

static int failures = 0;

#define CHECK(cond, what)                                                                      \
    do {                                                                                       \
        if (!(cond)) {                                                                         \
            printf("FAIL %s (line %d)\n", what, __LINE__);                                     \
            ++failures;                                                                        \
        }                                                                                      \
    } while (0)

#define PAGE(n) ((uint64_t)(n) * WRITE_SNAPSHOT_PAGE)

static unsigned run_count;
static uint32_t run_begin[64], run_size[64];

static void on_run(void* ctx, uint32_t begin, uint32_t size) {
    (void)ctx;
    if (run_count < 64) {
        run_begin[run_count] = begin;
        run_size[run_count] = size;
    }
    ++run_count;
}

static void test_take_claim(void) {
    write_snapshots_t* s = write_snapshots_create();
    unsigned char page[WRITE_SNAPSHOT_PAGE];
    const unsigned char* got;
    uint64_t cutoff;
    uint32_t slot = 0;
    CHECK(s != NULL, "made");
    CHECK(!write_snapshots_claim(s, PAGE(5), write_snapshots_cutoff(s), &slot), "none yet");

    memset(page, 0x11, sizeof(page));
    write_snapshots_take(s, PAGE(5), page);
    cutoff = write_snapshots_cutoff(s); /* the page's upload starts here */
    memset(page, 0x22, sizeof(page));
    write_snapshots_take(s, PAGE(5), page); /* a write after it: for the next upload */

    got = write_snapshots_claim(s, PAGE(5), cutoff, &slot);
    CHECK(got && got[0] == 0x11 && got[WRITE_SNAPSHOT_PAGE - 1] == 0x11,
          "the snapshot from before the upload");
    write_snapshots_release(s, slot);
    CHECK(!write_snapshots_claim(s, PAGE(5), cutoff, &slot), "claimed once");

    cutoff = write_snapshots_cutoff(s);
    got = write_snapshots_claim(s, PAGE(5), cutoff, &slot);
    CHECK(got && got[0] == 0x22, "the later one kept for the next upload");
    write_snapshots_release(s, slot);

    /* Two before one upload (the first lost its upload): the newest is the one */
    memset(page, 0x33, sizeof(page));
    write_snapshots_take(s, PAGE(9), page);
    memset(page, 0x44, sizeof(page));
    write_snapshots_take(s, PAGE(9), page);
    got = write_snapshots_claim(s, PAGE(9), write_snapshots_cutoff(s), &slot);
    CHECK(got && got[0] == 0x44, "newest of two");
    write_snapshots_release(s, slot);
    CHECK(!write_snapshots_claim(s, PAGE(9), write_snapshots_cutoff(s), &slot),
          "the older one dropped with it");
    CHECK(!write_snapshots_claim(s, PAGE(6), write_snapshots_cutoff(s), &slot),
          "another page has none");
    write_snapshots_destroy(s);
}

static void test_changed(void) {
    unsigned char before[WRITE_SNAPSHOT_PAGE], now[WRITE_SNAPSHOT_PAGE];
    unsigned i;
    for (i = 0; i < WRITE_SNAPSHOT_PAGE; ++i) {
        before[i] = (unsigned char)(i * 7);
    }
    memcpy(now, before, sizeof(now));

    run_count = 0;
    write_snapshots_changed(now, before, 0, WRITE_SNAPSHOT_PAGE, on_run, NULL);
    CHECK(run_count == 0, "nothing changed: nothing uploaded over the GPU's bytes");

    /* The guest wrote a float at 100, twelve bytes at 200 and one byte at 4095 */
    now[101] ^= 0xFF;
    for (i = 200; i < 212; ++i) {
        now[i] ^= 0x55;
    }
    now[4095] ^= 1;
    run_count = 0;
    write_snapshots_changed(now, before, 0, WRITE_SNAPSHOT_PAGE, on_run, NULL);
    CHECK(run_count == 3, "three runs");
    CHECK(run_begin[0] == 100 && run_size[0] == 4, "a value partly changed is all new");
    CHECK(run_begin[1] == 200 && run_size[1] == 12, "a run of changed cells is one run");
    CHECK(run_begin[2] == 4092 && run_size[2] == 4, "the last cell");

    /* Only the GPU's range of the page is looked at, and never past it */
    run_count = 0;
    write_snapshots_changed(now, before, 102, 207, on_run, NULL);
    CHECK(run_count == 1 && run_begin[0] == 200 && run_size[0] == 7,
          "clipped to the range: a change outside it does not count");
    run_count = 0;
    write_snapshots_changed(now, before, 104, 200, on_run, NULL);
    CHECK(run_count == 0, "nothing between the writes");
}

static void test_full_table(void) {
    write_snapshots_t* s = write_snapshots_create();
    unsigned char page[WRITE_SNAPSHOT_PAGE];
    const unsigned char* got;
    uint32_t slot = 0, n, found = 0, wrong = 0;
    /* Four times what it holds: old ones go, and what is found is its own page's */
    for (n = 1; n <= WRITE_SNAPSHOT_SLOTS * 4; ++n) {
        memset(page, (int)(n & 0xFF), sizeof(page));
        page[0] = (unsigned char)(n >> 8);
        write_snapshots_take(s, PAGE(n), page);
    }
    for (n = 1; n <= WRITE_SNAPSHOT_SLOTS * 4; ++n) {
        got = write_snapshots_claim(s, PAGE(n), write_snapshots_cutoff(s), &slot);
        if (got) {
            ++found;
            if (got[0] != (unsigned char)(n >> 8) || got[1] != (unsigned char)(n & 0xFF)) {
                ++wrong;
            }
            write_snapshots_release(s, slot);
        }
    }
    CHECK(found > WRITE_SNAPSHOT_SLOTS / 2 && found <= WRITE_SNAPSHOT_SLOTS,
          "a full table keeps what fits");
    CHECK(wrong == 0, "never another page's bytes");
    printf("full table: %u of %u kept\n", found, WRITE_SNAPSHOT_SLOTS * 4);
    write_snapshots_destroy(s);
}

/* ---- threads ---- */

#define TAKERS 3
#define ROUNDS 40000
#define PAGES 64

static write_snapshots_t* shared;
static retro_atomic_int_t takers_left;

static void taker(void* arg) {
    unsigned char page[WRITE_SNAPSHOT_PAGE];
    unsigned seed = (unsigned)(size_t)arg, n;
    for (n = 0; n < ROUNDS; ++n) {
        unsigned which;
        seed = seed * 1103515245u + 12345u;
        which = 1 + (seed >> 16) % PAGES;
        /* Every byte the same, and telling which page: a torn or foreign page shows */
        memset(page, (int)((which * 3 + (seed >> 24)) & 0xFF), sizeof(page));
        page[0] = (unsigned char)which;
        write_snapshots_take(shared, PAGE(which), page);
    }
    retro_atomic_fetch_sub_int(&takers_left, 1);
}

static void test_threads(void) {
    sthread_t* threads[TAKERS];
    long claimed = 0, torn = 0;
    uint32_t slot = 0, i;
    unsigned which;
    int last_pass = 0;
    shared = write_snapshots_create();
    retro_atomic_int_init(&takers_left, TAKERS);
    for (i = 0; i < TAKERS; ++i) {
        threads[i] = sthread_create(taker, (void*)(size_t)(i + 1));
    }
    while (!last_pass) {
        last_pass = retro_atomic_load_acquire_int(&takers_left) == 0;
        for (which = 1; which <= PAGES; ++which) {
            const unsigned char* got =
                write_snapshots_claim(shared, PAGE(which), write_snapshots_cutoff(shared), &slot);
            if (got) {
                ++claimed;
                if (got[0] != which || memcmp(got + 1, got + 2, WRITE_SNAPSHOT_PAGE - 2) != 0) {
                    ++torn;
                }
                write_snapshots_release(shared, slot);
            }
        }
    }
    for (i = 0; i < TAKERS; ++i) {
        sthread_join(threads[i]);
    }
    CHECK(claimed > 0, "snapshots claimed while they are taken");
    CHECK(torn == 0, "never a torn or foreign page");
    printf("threads: %ld claimed of %d taken\n", claimed, TAKERS * ROUNDS);
    write_snapshots_destroy(shared);
}

int main(void) {
    test_take_claim();
    test_changed();
    test_full_table();
    test_threads();
    if (failures) {
        printf("%d failures\n", failures);
        return 1;
    }
    printf("write_snapshots: all passed\n");
    return 0;
}
