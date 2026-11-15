/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_COMMON_GAME_CLOCK_H
#define SHADPS4_COMMON_GAME_CLOCK_H

#include <stdint.h>

#include <retro_atomic.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The game's time, in nanoseconds, from the frontend's frames rather than a wall clock.
 * Each retro_run starts a frame (game_clock_frame): the time moves to the frame's start,
 * and the frame's end is the most it may reach before the next one. Within a frame it moves
 * only when the game's threads are all idle (game_clock_advance), to the earliest deadline
 * they wait for, never past the frame's end: so a game gets each frame's worth of time, and
 * no more, however fast or slow the host runs it. The frame's length is exact over time
 * (1e9 / 60 ns, its fraction carried). One thread starts frames; any thread reads and
 * advances. */
typedef struct game_clock {
    retro_atomic_64_t now;       /* the time */
    retro_atomic_64_t frame_end; /* the most the time may reach before the next frame */
    retro_atomic_64_t frames;    /* frames started */
    uint64_t period;             /* a frame's whole nanoseconds */
    uint64_t rem;                /* and the fraction's numerator, over fps_num */
    uint64_t acc;
    uint64_t fps_num;
} game_clock_t;

/* At time 0, no frame started (the time stands until the first), frames of
 * @fps_den / @fps_num seconds (60/1: 1/60 s) */
void game_clock_init(game_clock_t* clock, unsigned fps_num, unsigned fps_den);

/* A frame starts (retro_run): the time moves to its start (the last frame's end), if the
 * game had not used all of the last one. The one thread that starts frames. */
void game_clock_frame(game_clock_t* clock);

/* The time now */
uint64_t game_clock_now(game_clock_t* clock);

/* The end of the frame under way */
uint64_t game_clock_frame_end(game_clock_t* clock);

/* Frames started */
uint64_t game_clock_frames(game_clock_t* clock);

/* The game's threads are all idle until @until: the time moves there, or to the frame's end
 * if that is sooner, never back. @return the time now. */
uint64_t game_clock_advance(game_clock_t* clock, uint64_t until);

#ifdef __cplusplus
}
#endif

#endif
