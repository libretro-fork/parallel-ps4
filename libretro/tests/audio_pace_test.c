/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* audio_pace: what the core hands the frontend each run of the emulator's frame-a-frame
 * sound.
 *   - silence until two frames are in hand, then one a run;
 *   - the emulator's mix of each frame landing before or after the run that counted it, at
 *     random, never more than a frame late: once primed, never a frame short, never more than
 *     three behind;
 *   - a mix late by more than a frame: one short run, then primed again;
 *   - the frontend taking none for a while: the oldest go, at most three behind after. */

#include <stdio.h>

#include "../audio_pace.h"

static int failures = 0;

#define CHECK(cond, what)                                                                      \
    do {                                                                                       \
        if (!(cond)) {                                                                         \
            printf("FAIL %s (line %d)\n", what, __LINE__);                                     \
            ++failures;                                                                        \
        }                                                                                      \
    } while (0)

#define FRAME 800u

static unsigned long seed = 12345;
static unsigned rnd(void) {
    seed = seed * 1103515245ul + 12345ul;
    return (unsigned)(seed >> 16) & 0x7fff;
}

int main(void) {
    audio_pace_t pace = {0};
    uint32_t written = 0, read = 0, skip, take;
    unsigned run, short_runs = 0, most = 0, pending = 0;

    CHECK(audio_pace_take(&pace, 0, FRAME, &skip) == 0, "empty: silence");
    CHECK(audio_pace_take(&pace, FRAME, FRAME, &skip) == 0, "one in hand: silence still");
    CHECK(audio_pace_take(&pace, 2 * FRAME, FRAME, &skip) == FRAME && skip == 0,
          "two in hand: one a run");

    /* Each run counts a frame; its mix lands now or by the next run (pending) */
    pace.primed = 0;
    for (run = 0; run < 100000; run++) {
        if (pending) {
            written += FRAME;
            pending = 0;
        }
        if (rnd() & 1)
            written += FRAME; /* this run's frame in before the core looks */
        else
            pending = 1;
        take = audio_pace_take(&pace, written - read, FRAME, &skip);
        read += skip + take;
        if (run > 4 && take < FRAME)
            short_runs++;
        if (written - read > most)
            most = written - read;
    }
    CHECK(short_runs == 0, "never more than a frame late: never short once primed");
    CHECK(most <= 3 * FRAME, "never more than three behind");

    /* A frame two late: one short run, then primed again */
    pace.primed = 1;
    take = audio_pace_take(&pace, FRAME / 2, FRAME, &skip);
    CHECK(take == FRAME / 2 && !pace.primed, "late: what there is, then not primed");
    CHECK(audio_pace_take(&pace, FRAME, FRAME, &skip) == 0, "silence until two in hand");
    CHECK(audio_pace_take(&pace, 2 * FRAME, FRAME, &skip) == FRAME, "and then one a run");

    /* Ten frames piled up: the oldest go */
    take = audio_pace_take(&pace, 10 * FRAME, FRAME, &skip);
    CHECK(skip == 8 * FRAME && take == FRAME, "piled up: down to two, one taken");

    if (failures) {
        printf("audio_pace: %d failed\n", failures);
        return 1;
    }
    printf("audio_pace: all passed\n");
    return 0;
}
