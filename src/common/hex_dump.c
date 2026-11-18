/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "common/hex_dump.h"

void hex_dump_window(uint64_t address, uint64_t before, uint64_t after, uint64_t* start,
                     uint64_t* length) {
    const uint64_t line = HEX_DUMP_LINE_BYTES;
    const uint64_t top = ~(uint64_t)0 - (line - 1); /* the last line's start */
    uint64_t first = address > before ? address - before : 0;
    uint64_t end = after > top - address || address > top ? top : address + after;
    first -= first % line;
    /* whole lines: the end rounded up, where there is room above it */
    if (end % line != 0)
        end += line - end % line;
    if (end < first)
        end = first;
    *start = first;
    *length = end - first;
}

size_t hex_dump_line(char* out, size_t size, const unsigned char* bytes, size_t count) {
    static const char digits[] = "0123456789abcdef";
    size_t n = 0, i;
    if (count > HEX_DUMP_LINE_BYTES)
        count = HEX_DUMP_LINE_BYTES;
    if (!out || size == 0)
        return 0;
    if (size < HEX_DUMP_LINE_CHARS) {
        out[0] = '\0';
        return 0;
    }
    for (i = 0; i < HEX_DUMP_LINE_BYTES; i++) {
        if (i < count) {
            out[n++] = digits[bytes[i] >> 4];
            out[n++] = digits[bytes[i] & 15];
        } else {
            out[n++] = ' ';
            out[n++] = ' ';
        }
        out[n++] = ' ';
        if (i == 7)
            out[n++] = ' ';
    }
    out[n++] = ' ';
    out[n++] = '|';
    for (i = 0; i < count; i++)
        out[n++] = bytes[i] >= 0x20 && bytes[i] < 0x7f ? (char)bytes[i] : '.';
    out[n++] = '|';
    out[n] = '\0';
    return n;
}

int hex_dump_packed(const unsigned char* bytes, size_t count) {
    size_t i, j, zeroes = 0;
    if (!bytes || count < 8)
        return 0;
    for (i = 0; i < count; i++) {
        size_t same = 1;
        if (bytes[i] == 0 && ++zeroes > 1)
            return 0;
        for (j = 0; j < i; j++)
            if (bytes[j] == bytes[i] && ++same > 3)
                return 0;
    }
    return 1;
}
