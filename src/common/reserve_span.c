/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "common/reserve_span.h"

int reserve_span(uint64_t start, uint64_t size, uint64_t max, uint64_t granularity,
                 uint64_t* addr, uint64_t* span) {
    const uint64_t mask = granularity - 1;
    uint64_t end = start + size;
    uint64_t from;
    if (end < start) /* wraps */
        end = ~(uint64_t)0;
    if (end > max)
        end = max;
    end &= ~mask;
    if (start > ~(uint64_t)0 - mask)
        return 0;
    from = (start + mask) & ~mask;
    if (end <= from)
        return 0;
    *addr = from;
    *span = end - from;
    return 1;
}
