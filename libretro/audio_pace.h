/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRETRO_AUDIO_PACE_H
#define SHADPS4_LIBRETRO_AUDIO_PACE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* How much of the emulator's sound the core hands the frontend each retro_run. The emulator
 * mixes exactly a frame of it a frame, on its own thread, after the core has counted the
 * frame: the frame this run counted may not be in yet. So the core keeps two in hand before
 * it takes any (silence meanwhile: at the start, and after the ring ran dry), takes one a run,
 * and lets the oldest go if more than three pile up (the frontend took none for a while). */
typedef struct audio_pace {
    int primed;
} audio_pace_t;

/* @queued frames are waiting, @frame a run. @return how many to take now (0 to @frame; the
 * rest of the run is silence); *@skip, how many of the oldest to drop first. */
uint32_t audio_pace_take(audio_pace_t* pace, uint32_t queued, uint32_t frame, uint32_t* skip);

#ifdef __cplusplus
}
#endif

#endif
