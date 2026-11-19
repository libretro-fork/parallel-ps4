/* The video player's clock (src/core/libraries/avplayer/avplayer_clock.c): the time into a
 * file from times read off the game's time; time spent paused not counted; with sound, the
 * time since sound was last handed to the game. */

#include <stdio.h>

#include "core/libraries/avplayer/avplayer_clock.h"

static int failures = 0;

#define CHECK(cond, what)                                                                      \
    do {                                                                                       \
        if (!(cond)) {                                                                         \
            printf("avplayer_clock: FAILED: %s\n", what);                                      \
            failures++;                                                                        \
        }                                                                                      \
    } while (0)

#define MS(n) ((uint64_t)(n) * 1000000)

int main(void) {
    avplayer_clock_t c;
    /* Any time of the game's will do for a start: this one is 50 s in */
    const uint64_t t0 = MS(50000);

    avplayer_clock_reset(&c);
    CHECK(!c.started && !c.has_data, "not started at first");
    avplayer_clock_start(&c, t0);
    CHECK(c.started && avplayer_clock_since_start_ms(&c, t0, 0) == 0, "started: no time yet");
    CHECK(avplayer_clock_since_start_ms(&c, t0 + MS(1500) + 999999, 0) == 1500,
          "a second and a half on: whole milliseconds");

    /* Paused at 2 s for 10 s: the time stands, and what comes after does not count the pause */
    avplayer_clock_pause(&c, t0 + MS(2000));
    CHECK(avplayer_clock_since_start_ms(&c, t0 + MS(2000), 1) == 2000 &&
              avplayer_clock_since_start_ms(&c, t0 + MS(9000), 1) == 2000,
          "paused: the time stands still however long");
    avplayer_clock_resume(&c, t0 + MS(12000));
    CHECK(avplayer_clock_since_start_ms(&c, t0 + MS(12000), 0) == 2000 &&
              avplayer_clock_since_start_ms(&c, t0 + MS(12500), 0) == 2500,
          "resumed: it goes on from where it stood");
    avplayer_clock_pause(&c, t0 + MS(13000));
    avplayer_clock_resume(&c, t0 + MS(14000));
    CHECK(avplayer_clock_since_start_ms(&c, t0 + MS(15000), 0) == 4000, "two pauses: neither counted");

    /* With sound: the time since the sound last handed over; before any, none */
    avplayer_clock_reset(&c);
    avplayer_clock_start(&c, t0);
    CHECK(avplayer_clock_since_data_ms(&c, t0 + MS(700), 0) == 0, "no sound handed over yet: 0");
    avplayer_clock_data(&c, t0 + MS(1000));
    CHECK(avplayer_clock_since_data_ms(&c, t0 + MS(1000), 0) == 0 &&
              avplayer_clock_since_data_ms(&c, t0 + MS(1021), 0) == 21,
          "since the sound last handed over");
    avplayer_clock_pause(&c, t0 + MS(1030));
    CHECK(avplayer_clock_since_data_ms(&c, t0 + MS(5000), 1) == 30, "paused: stands");
    avplayer_clock_resume(&c, t0 + MS(6000));
    CHECK(avplayer_clock_since_data_ms(&c, t0 + MS(6010), 0) == 40, "resumed: the pause not counted");
    avplayer_clock_data(&c, t0 + MS(6020));
    CHECK(avplayer_clock_since_data_ms(&c, t0 + MS(6025), 0) == 5,
          "the next sound: counted from it, the pause before it forgotten");
    /* Paused before any sound: still none */
    avplayer_clock_reset(&c);
    avplayer_clock_start(&c, t0);
    avplayer_clock_pause(&c, t0 + MS(100));
    CHECK(avplayer_clock_since_data_ms(&c, t0 + MS(900), 1) == 0, "paused with no sound yet: 0");

    if (failures) {
        printf("avplayer_clock: %d failed\n", failures);
        return 1;
    }
    printf("avplayer_clock: all passed\n");
    return 0;
}
