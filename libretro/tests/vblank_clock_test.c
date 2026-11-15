/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* vblank_clock: the console's vblanks from the frontend's frames.
 *   - none owed before a tick, then one a tick at 60 Hz;
 *   - two a tick at 120, one every other tick at 30, three every two at 90, 144 and 240
 *     exact over a second;
 *   - a present thread held up owes at most a tick's and VBLANK_CLOCK_MAX_OWED more;
 *   - the rate changed: what is owed kept, the new rate from there;
 *   - the frontend's count wrapping, and running past the restart point, change nothing. */

#include <stdio.h>

#include "common/vblank_clock.h"

static int failures = 0;

#define CHECK(cond, what)                                                                      \
    do {                                                                                       \
        if (!(cond)) {                                                                         \
            printf("FAIL %s (line %d)\n", what, __LINE__);                                     \
            ++failures;                                                                        \
        }                                                                                      \
    } while (0)

/* Runs @frames ticks from @start at @hz, taking all owed each tick: the vblanks given */
static unsigned long run(unsigned start, unsigned frames, unsigned hz) {
    vblank_clock_t c;
    unsigned long given = 0;
    unsigned i;
    vblank_clock_init(&c, start, hz);
    for (i = 1; i <= frames; i++) {
        unsigned owed = vblank_clock_owed(&c, start + i, hz);
        while (owed--) {
            vblank_clock_take(&c);
            given++;
        }
        if (vblank_clock_owed(&c, start + i, hz) != 0)
            return (unsigned long)-1;
    }
    return given;
}

int main(void) {
    vblank_clock_t c;
    unsigned i, owed;
    unsigned long given;

    vblank_clock_init(&c, 100, 60);
    CHECK(vblank_clock_owed(&c, 100, 60) == 0, "none before a tick");
    CHECK(vblank_clock_owed(&c, 101, 60) == 1, "one a tick at 60");
    vblank_clock_take(&c);
    CHECK(vblank_clock_owed(&c, 101, 60) == 0, "taken");

    CHECK(run(0, 60, 60) == 60, "60 Hz: 60 a second");
    CHECK(run(0, 60, 120) == 120, "120 Hz: 120 a second");
    CHECK(run(0, 60, 30) == 30, "30 Hz: 30 a second");
    CHECK(run(0, 60, 90) == 90, "90 Hz: 90 a second");
    CHECK(run(0, 60, 144) == 144, "144 Hz: 144 a second");
    CHECK(run(0, 60, 240) == 240, "240 Hz: 240 a second");
    CHECK(run(0, 3600, 144) == 144 * 60, "144 Hz: a minute, no drift");

    vblank_clock_init(&c, 0, 30);
    CHECK(vblank_clock_owed(&c, 1, 30) == 0, "30 Hz: not after one tick");
    CHECK(vblank_clock_owed(&c, 2, 30) == 1, "30 Hz: after two");
    vblank_clock_init(&c, 0, 120);
    CHECK(vblank_clock_owed(&c, 1, 120) == 2, "120 Hz: two a tick");

    /* Held up for a second: no more than the most owed */
    vblank_clock_init(&c, 0, 60);
    owed = vblank_clock_owed(&c, 60, 60);
    CHECK(owed == 1 + VBLANK_CLOCK_MAX_OWED, "a stall owes at most a tick's and the most more");
    while (owed--)
        vblank_clock_take(&c);
    CHECK(vblank_clock_owed(&c, 60, 60) == 0, "then none");
    CHECK(vblank_clock_owed(&c, 61, 60) == 1, "and one a tick again");

    /* The rate changed with one owed: kept, then the new rate */
    vblank_clock_init(&c, 0, 60);
    CHECK(vblank_clock_owed(&c, 1, 60) == 1, "one owed");
    CHECK(vblank_clock_owed(&c, 1, 120) == 1, "still owed at the new rate");
    vblank_clock_take(&c);
    CHECK(vblank_clock_owed(&c, 1, 120) == 0, "taken");
    CHECK(vblank_clock_owed(&c, 2, 120) == 2, "two a tick from there");

    /* The frontend's count wrapping */
    CHECK(run(0xFFFFFFF0u, 60, 60) == 60, "across the wrap at 60");
    CHECK(run(0xFFFFFFF0u, 60, 90) == 90, "across the wrap at 90");

    /* Past the restart point: nothing lost, nothing doubled */
    vblank_clock_init(&c, 0, 90);
    given = 0;
    for (i = 0x0FFFFFF0u; i <= 0x0FFFFFF0u + 60; i++) {
        if (i == 0x0FFFFFF0u) {
            /* Jump there: what a stall owes, taken */
            owed = vblank_clock_owed(&c, i, 90);
            while (owed--)
                vblank_clock_take(&c);
            continue;
        }
        owed = vblank_clock_owed(&c, i, 90);
        while (owed--) {
            vblank_clock_take(&c);
            given++;
        }
    }
    CHECK(given == 90 - 1 || given == 90 || given == 90 + 1, "90 a second past the restart");

    if (failures) {
        printf("vblank_clock: %d failed\n", failures);
        return 1;
    }
    printf("vblank_clock: all passed\n");
    return 0;
}
