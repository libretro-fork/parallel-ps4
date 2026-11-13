/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* touch_track: the frontend's fingers into the pad's two slots, each finger kept in its own
 * slot as fingers go down, move and lift in any order, and libretro's range to 0..0x7fff. */

#include <stdio.h>

#include "../touch_track.h"

static int failures = 0;

#define CHECK(cond, what)                                                                      \
    do {                                                                                       \
        if (!(cond)) {                                                                         \
            printf("FAIL %s (line %d)\n", what, __LINE__);                                     \
            ++failures;                                                                        \
        }                                                                                      \
    } while (0)

int main(void) {
    unsigned slots[2] = {0, 0};
    unsigned f[2];
    const unsigned a = touch_word(1000, 1000), a2 = touch_word(1100, 1050);
    const unsigned b = touch_word(30000, 20000), b2 = touch_word(29000, 21000);
    const unsigned c = touch_word(5000, 30000);

    CHECK(touch_unit(-0x7fff) == 0 && touch_unit(0x7fff) == 0x7fff, "the ends");
    CHECK(touch_unit(0) == 0x4000 && touch_unit(-0x8000) == 0, "the middle, past the end");
    CHECK(TOUCH_X(touch_word(0x7fff, 3)) == 0x7fff && TOUCH_Y(touch_word(0x7fff, 3)) == 3 &&
              (touch_word(0, 0) & TOUCH_DOWN),
          "packed and unpacked");

    /* One finger down: the first slot */
    f[0] = a;
    touch_track(slots, f, 1);
    CHECK(slots[0] == a && slots[1] == 0, "a first finger in slot 0");
    /* A second: slot 1, whichever order the frontend gives them */
    f[0] = b;
    f[1] = a2;
    touch_track(slots, f, 2);
    CHECK(slots[0] == a2 && slots[1] == b, "the first finger stays in slot 0 as a second comes");
    /* The first lifts: libretro's finger 0 is now the second, which stays in slot 1 */
    f[0] = b2;
    touch_track(slots, f, 1);
    CHECK(slots[0] == 0 && slots[1] == b2, "the second stays in slot 1 when the first lifts");
    /* A new finger: the free slot */
    f[0] = b2;
    f[1] = c;
    touch_track(slots, f, 2);
    CHECK(slots[0] == c && slots[1] == b2, "a new finger in the free slot");
    /* Both moving, given swapped */
    f[0] = touch_word(29500, 21500);
    f[1] = touch_word(5200, 29800);
    touch_track(slots, f, 2);
    CHECK(TOUCH_X(slots[0]) == 5200 && TOUCH_X(slots[1]) == 29500, "both kept as they move");
    /* All lift */
    touch_track(slots, f, 0);
    CHECK(slots[0] == 0 && slots[1] == 0, "all lifted");
    /* One finger, then that one only: stays in slot 0 */
    f[0] = c;
    touch_track(slots, f, 1);
    touch_track(slots, f, 1);
    CHECK(slots[0] == c && slots[1] == 0, "a lone finger stays put");

    if (failures) {
        printf("touch_track: %d failed\n", failures);
        return 1;
    }
    printf("touch_track: all passed\n");
    return 0;
}
