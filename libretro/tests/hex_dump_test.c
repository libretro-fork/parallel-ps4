/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* hex_dump: the memory a crash report shows around an address.
 *   - the window is whole lines of 16 bytes holding the bytes asked for
 *     before and after the address, and the address itself;
 *   - it never starts below 0 nor wraps past the end of the address space;
 *   - a line is hex in two groups of eight and the bytes as text, dots for
 *     what does not print; a short line keeps its columns;
 *   - a buffer too small for a line gets an empty string, never an overrun. */

#include <stdio.h>
#include <string.h>

#include "common/hex_dump.h"

static int failures = 0;

#define CHECK(cond, what)                                                     \
    do {                                                                      \
        if (!(cond)) {                                                        \
            printf("FAIL %s (line %d)\n", what, __LINE__);                    \
            ++failures;                                                       \
        }                                                                     \
    } while (0)

static void test_window(void) {
    uint64_t start, length, a;
    /* The address a crash had in rdi: a mutex inside the game's memory */
    hex_dump_window(0xeee080, 0x80, 0x100, &start, &length);
    CHECK(start == 0xeee000, "window starts the bytes asked for before");
    CHECK(length == 0x180, "window runs the bytes asked for after");
    hex_dump_window(0xeee087, 0x80, 0x100, &start, &length);
    CHECK(start == 0xeee000, "start rounded down to a line");
    CHECK(start + length == 0xeee190, "end rounded up to a line");
    /* Near 0 and near the top: clamped, never wrapped */
    hex_dump_window(0x20, 0x80, 0x100, &start, &length);
    CHECK(start == 0 && length == 0x120, "window clamped at 0");
    hex_dump_window(~(uint64_t)0 - 5, 0x80, 0x100, &start, &length);
    CHECK(start % 16 == 0 && length % 16 == 0, "window near the top is whole lines");
    CHECK(start + length >= start, "window near the top does not wrap");
    CHECK(start <= ~(uint64_t)0 - 5 - 0x80, "window near the top keeps the bytes before");
    /* Every address: whole lines that hold it */
    for (a = 0; a < 0x400; a++) {
        hex_dump_window(a + 0x10000, 0x40, 0x40, &start, &length);
        CHECK(start % 16 == 0 && length % 16 == 0, "whole lines");
        CHECK(start <= a + 0x10000 - 0x40, "holds the bytes before");
        CHECK(start + length >= a + 0x10000 + 0x40, "holds the bytes after");
        CHECK(length <= 0x40 + 0x40 + 32, "no more than a line either side");
    }
}

static void test_line(void) {
    /* What the crash had there in place of a mutex pointer, and text */
    static const unsigned char bytes[16] = {0x26, 0x6c, 0x11, 0xf0, 0x42, 0x45, 0x15, 0xdc,
                                            0x00, 0x7f, 'A',  'b',  ' ',  '~',  0xff, 0x1f};
    char out[HEX_DUMP_LINE_CHARS + 8];
    char small[HEX_DUMP_LINE_CHARS - 1];
    size_t n;
    memset(out, 'x', sizeof(out));
    n = hex_dump_line(out, HEX_DUMP_LINE_CHARS, bytes, 16);
    CHECK(strcmp(out, "26 6c 11 f0 42 45 15 dc  00 7f 41 62 20 7e ff 1f  |&l..BE....Ab ~..|") == 0,
          "a full line");
    CHECK(n == strlen(out) && n < HEX_DUMP_LINE_CHARS, "its length, within the room named");
    CHECK(out[HEX_DUMP_LINE_CHARS] == 'x', "nothing written past the room given");
    n = hex_dump_line(out, sizeof(out), bytes, 3);
    CHECK(strncmp(out, "26 6c 11 ", 9) == 0 && strcmp(out + 50, "|&l.|") == 0,
          "a short line keeps its columns");
    CHECK(strspn(out + 9, " ") == 41, "blank where it has no bytes");
    CHECK(n == strlen(out), "a short line's length");
    n = hex_dump_line(out, sizeof(out), bytes, 40);
    CHECK(n == 68, "no more than a line's bytes");
    n = hex_dump_line(out, sizeof(out), bytes, 0);
    CHECK(strstr(out, "||") != NULL && n == strlen(out), "no bytes: an empty line");
    memset(small, 'x', sizeof(small));
    n = hex_dump_line(small, sizeof(small), bytes, 16);
    CHECK(n == 0 && small[0] == '\0' && small[1] == 'x', "too small a buffer: an empty string");
    CHECK(hex_dump_line(NULL, 0, bytes, 16) == 0, "no buffer");
}

int main(void) {
    test_window();
    test_line();
    if (failures) {
        printf("hex_dump_test: %d failure(s)\n", failures);
        return 1;
    }
    printf("hex_dump_test: ok\n");
    return 0;
}
