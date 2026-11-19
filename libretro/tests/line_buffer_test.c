/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The game's stdout and stderr cut into lines (line_buffer.c): lines whole across pieces,
 * '\r' and empty lines dropped, a line longer than the buffer in buffer-sized pieces, and what
 * is left given out on a flush. */

#include <stdio.h>
#include <string.h>

#include "common/line_buffer.h"

static char lines[8][LINE_BUFFER_SIZE + 1];
static int count;
static int failures;

static void emit(void* ctx, const char* text, size_t len) {
    (void)ctx;
    if (count < 8) {
        memcpy(lines[count], text, len);
        lines[count][len] = '\0';
    }
    count++;
}

static void expect(int n, const char* const* want, const char* what) {
    int i;
    if (count != n) {
        printf("line_buffer: %s: %d lines, not %d\n", what, count, n);
        failures++;
        return;
    }
    for (i = 0; i < n; i++) {
        if (strcmp(lines[i], want[i]) != 0) {
            printf("line_buffer: %s: line %d \"%s\", not \"%s\"\n", what, i, lines[i], want[i]);
            failures++;
        }
    }
}

int main(void) {
    static line_buffer_t lb;
    static char longline[LINE_BUFFER_SIZE + 10];
    const char* pieces[] = {"hello ", "wor", "ld\r\n\n", "second\nthi", "rd"};
    const char* want1[] = {"hello world", "second"};
    const char* want2[] = {"hello world", "second", "third"};
    const char* want3[2];
    char first[LINE_BUFFER_SIZE + 1];
    int i;

    for (i = 0; i < 5; i++) {
        line_buffer_feed(&lb, pieces[i], strlen(pieces[i]), emit, NULL);
    }
    expect(2, want1, "pieces");
    line_buffer_flush(&lb, emit, NULL);
    expect(3, want2, "flush");
    line_buffer_flush(&lb, emit, NULL);
    expect(3, want2, "flush of nothing");

    count = 0;
    memset(longline, 'x', sizeof(longline) - 1);
    longline[sizeof(longline) - 1] = '\n';
    line_buffer_feed(&lb, longline, sizeof(longline), emit, NULL);
    memset(first, 'x', LINE_BUFFER_SIZE);
    first[LINE_BUFFER_SIZE] = '\0';
    want3[0] = first;
    want3[1] = "xxxxxxxxx";
    expect(2, want3, "a line longer than the buffer");

    if (failures) {
        printf("line_buffer: %d FAILED\n", failures);
        return 1;
    }
    printf("line_buffer: all passed\n");
    return 0;
}
