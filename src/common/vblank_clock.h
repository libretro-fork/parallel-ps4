/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_COMMON_VBLANK_CLOCK_H
#define SHADPS4_COMMON_VBLANK_CLOCK_H

#ifdef __cplusplus
extern "C" {
#endif

/* The console's vblanks, counted from the frontend's frames instead of a clock: each
 * retro_run is one tick of VBLANK_CLOCK_TICK_HZ, and a console running at @hz has
 * hz / VBLANK_CLOCK_TICK_HZ vblanks a tick (two at 120 Hz, one every other tick at 30,
 * three every two at 90). Nothing sleeps and nothing reads the time: the frontend's
 * pacing (the display, its audio) is the console's. */
#define VBLANK_CLOCK_TICK_HZ 60u

/* The most vblanks owed at once, past one tick's: a present thread held up for longer (a
 * stall) catches up this many, the rest are let go, as the console's would be missed */
#define VBLANK_CLOCK_MAX_OWED 2u

typedef struct vblank_clock {
    unsigned base_ticks; /* the ticks when the count (or the rate) began */
    unsigned hz;         /* the rate counted at since */
    long done;           /* the vblanks given since base_ticks (less any carried over) */
} vblank_clock_t;

/* Starts counting at @ticks, @hz a second */
void vblank_clock_init(vblank_clock_t* clock, unsigned ticks, unsigned hz);

/* How many vblanks are owed now that the frontend's tick count reads @ticks, the console
 * at @hz (it can change: the count goes on from where it is). The ticks may wrap. Taking
 * them is vblank_clock_take. */
unsigned vblank_clock_owed(vblank_clock_t* clock, unsigned ticks, unsigned hz);

/* One owed vblank given */
void vblank_clock_take(vblank_clock_t* clock);

#ifdef __cplusplus
}
#endif

#endif
