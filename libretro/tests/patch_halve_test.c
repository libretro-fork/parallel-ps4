/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* patch_halve: a number a game carries as text, halved where it is.
 *   - 33333 becomes 16666, and 16666 (what Bloodborne's 60 FPS++ leaves)
 *     " 8333": the same width, spaces before a shorter number;
 *   - an odd number rounds down; spaces before the number are kept as width;
 *   - not a number, a space after digits, 0, 1, no width: nothing written;
 *   - the bytes around the field are never touched. */

#include <stdio.h>
#include <string.h>

#include "common/patch_halve.h"

static int failures = 0;

#define CHECK(cond, what)                                                     \
    do {                                                                      \
        if (!(cond)) {                                                        \
            printf("FAIL %s (line %d)\n", what, __LINE__);                    \
            ++failures;                                                       \
        }                                                                     \
    } while (0)

static void halve(const char* in, int ok, const char* want, unsigned long b, unsigned long a) {
    unsigned char buf[40];
    unsigned long before = 0, after = 0;
    const size_t width = strlen(in);
    int got;
    memset(buf, '#', sizeof(buf));
    memcpy(buf + 4, in, width);
    got = patch_halve_decimal(buf + 4, width, &before, &after);
    CHECK(got == ok, in);
    CHECK(memcmp(buf + 4, want, width) == 0, in);
    CHECK(memcmp(buf, "####", 4) == 0 && buf[4 + width] == '#', "the bytes around left");
    if (ok)
        CHECK(before == b && after == a, in);
}

int main(void) {
    unsigned char one = '8';
    halve("33333", 1, "16666", 33333, 16666);
    halve("16666", 1, " 8333", 16666, 8333);
    halve(" 8333", 1, " 4166", 8333, 4166);
    halve("10", 1, " 5", 10, 5);
    halve("3", 1, "1", 3, 1);
    halve("100000", 1, " 50000", 100000, 50000);
    halve("    2", 1, "    1", 2, 1);
    halve("1", 0, "1", 0, 0);
    halve("0000", 0, "0000", 0, 0);
    halve("33a33", 0, "33a33", 0, 0);
    halve("333 3", 0, "333 3", 0, 0);
    halve("     ", 0, "     ", 0, 0);
    halve("-3333", 0, "-3333", 0, 0);
    CHECK(!patch_halve_decimal(&one, 0, NULL, NULL), "no width");
    CHECK(!patch_halve_decimal(NULL, 5, NULL, NULL), "no field");
    CHECK(patch_halve_decimal(&one, 1, NULL, NULL) && one == '4', "no numbers wanted back");
    if (failures) {
        printf("patch_halve: %d failed\n", failures);
        return 1;
    }
    printf("patch_halve: all passed\n");
    return 0;
}
