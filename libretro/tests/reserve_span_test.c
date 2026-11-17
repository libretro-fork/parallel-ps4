/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* reserve_span: what of a free region can be reserved on a 64 KiB granularity.
 *   - a region starting on it: all of it;
 *   - one starting a page after it, a whole number of granules long (what made
 *     the emulator stop at start with "Unable to reserve virtual address space:
 *     Attempt to access invalid address"): from the next granule to the last
 *     whole one, never past the region's end;
 *   - cut at the maximum; nothing left; a region smaller than a granule;
 *   - every start and size around the granularity: inside the region, aligned,
 *     and no aligned granule of the region left out. */

#include <stdio.h>

#include "common/reserve_span.h"

static int failures = 0;

#define CHECK(cond, what)                                                     \
    do {                                                                      \
        if (!(cond)) {                                                        \
            printf("FAIL %s (line %d)\n", what, __LINE__);                    \
            ++failures;                                                       \
        }                                                                     \
    } while (0)

#define G 0x10000u

int main(void) {
    uint64_t addr = 0, span = 0, start, size;
    const uint64_t max = (uint64_t)0x5fffffffffff;

    CHECK(reserve_span(0x400000, 0x2000000, max, G, &addr, &span) && addr == 0x400000 &&
              span == 0x2000000,
          "aligned: all of it");
    CHECK(reserve_span(0x401000, 0x2000000, max, G, &addr, &span) && addr == 0x410000 &&
              span == 0x1ff0000,
          "a page in: from the next granule, not past its end");
    CHECK(addr + span <= 0x401000 + 0x2000000, "inside the region");
    CHECK(reserve_span(0x5ffffff00000, 0x200000000000, max, G, &addr, &span) &&
              addr == 0x5ffffff00000 && span == 0xf0000,
          "cut at the maximum");
    CHECK(!reserve_span(0x600000000000, 0x1000000, max, G, &addr, &span), "above the maximum");
    CHECK(!reserve_span(0x401000, 0xe000, max, G, &addr, &span), "less than a granule");
    CHECK(!reserve_span(0x401000, 0x1e000, max, G, &addr, &span), "no whole granule in it");
    CHECK(reserve_span(~(uint64_t)0 - 0x2ffff, 0x30000, ~(uint64_t)0, G, &addr, &span) == 1 &&
              addr + span > addr,
          "at the top of the address space");

    for (start = 0x400000 - 0x3000; start <= 0x400000 + 0x23000; start += 0x1000)
        for (size = 0x1000; size <= 0x45000; size += 0x1000) {
            const uint64_t end = start + size;
            if (reserve_span(start, size, max, G, &addr, &span)) {
                CHECK(addr >= start && addr + span <= end, "inside");
                CHECK(!(addr & (G - 1)) && !(span & (G - 1)) && span, "aligned");
                CHECK(addr - start < G && end - (addr + span) < G, "nothing whole left out");
            } else {
                CHECK(((start + G - 1) & ~(uint64_t)(G - 1)) + G > end, "no granule fits");
            }
        }

    if (failures) {
        printf("reserve_span: %d failed\n", failures);
        return 1;
    }
    printf("reserve_span: all passed\n");
    return 0;
}
