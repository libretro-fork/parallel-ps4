/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "common/patch_halve.h"

int patch_halve_decimal(unsigned char* field, size_t width, unsigned long* before,
                        unsigned long* after) {
    unsigned long value = 0, half;
    size_t i = 0, digits = 0;
    if (!field || width == 0 || width > 18)
        return 0;
    while (i < width && field[i] == ' ')
        i++;
    for (; i < width; i++) {
        if (field[i] < '0' || field[i] > '9')
            return 0;
        value = value * 10 + (unsigned long)(field[i] - '0');
        digits++;
    }
    if (digits == 0 || value < 2)
        return 0;
    half = value / 2;
    if (before)
        *before = value;
    if (after)
        *after = half;
    for (i = width; i-- > 0;) {
        if (half != 0 || i == width - 1) {
            field[i] = (unsigned char)('0' + half % 10);
            half /= 10;
        } else {
            field[i] = ' ';
        }
    }
    return 1;
}
