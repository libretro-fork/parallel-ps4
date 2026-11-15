/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "common/vblank_clock.h"

void vblank_clock_init(vblank_clock_t* clock, unsigned ticks, unsigned hz) {
    clock->base_ticks = ticks;
    clock->hz = hz ? hz : VBLANK_CLOCK_TICK_HZ;
    clock->done = 0;
}

/* The vblanks due since base_ticks: @elapsed ticks at clock->hz, rounded down */
static long vblank_clock_due(const vblank_clock_t* clock, unsigned elapsed) {
    /* elapsed * hz in two parts, so that neither overflows a long of 32 bits (elapsed is
     * kept under 2^28 ticks, and hz under 2^10) */
    const long whole = (long)(elapsed / VBLANK_CLOCK_TICK_HZ) * (long)clock->hz;
    const long part = (long)((elapsed % VBLANK_CLOCK_TICK_HZ) * clock->hz / VBLANK_CLOCK_TICK_HZ);
    return whole + part;
}

unsigned vblank_clock_owed(vblank_clock_t* clock, unsigned ticks, unsigned hz) {
    long due, max_owed;
    if (!hz)
        hz = VBLANK_CLOCK_TICK_HZ;
    else if (hz > 1000)
        hz = 1000;
    if (hz != clock->hz) {
        /* A new rate from here: what is owed is kept, a vblank's fraction under way is not */
        const long owed = vblank_clock_due(clock, ticks - clock->base_ticks) - clock->done;
        clock->base_ticks = ticks;
        clock->done = owed > 0 ? -owed : 0;
        clock->hz = hz;
    }
    due = vblank_clock_due(clock, ticks - clock->base_ticks);
    max_owed = (long)(hz / VBLANK_CLOCK_TICK_HZ) + (long)VBLANK_CLOCK_MAX_OWED;
    if (due - clock->done > max_owed)
        clock->done = due - max_owed;
    /* Far on, the count restarts at this tick, what is owed carried over, before the
     * products could overflow */
    if (ticks - clock->base_ticks >= 0x10000000u) {
        const long owed = due - clock->done;
        clock->base_ticks = ticks;
        clock->done = -owed;
        return (unsigned)owed;
    }
    return (unsigned)(due - clock->done);
}

void vblank_clock_take(vblank_clock_t* clock) {
    clock->done++;
}
