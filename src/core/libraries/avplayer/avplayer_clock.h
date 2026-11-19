/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The video player's clock: how far into a file its playing is, from times its caller reads
 * off the game's time (nanoseconds), which the frontend's frames move. It reads no clock
 * itself. Time spent paused does not count. */

#ifndef SHADPS4_AVPLAYER_CLOCK_H
#define SHADPS4_AVPLAYER_CLOCK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct avplayer_clock {
    uint64_t start;          /* when the playing started */
    uint64_t last_data;      /* when sound was last handed to the game */
    uint64_t pause_time;     /* when it was last paused */
    uint64_t pause_duration; /* how long it has been paused since the later of the two above */
    int started;
    int has_data;
} avplayer_clock_t;

/* Not started. */
void avplayer_clock_reset(avplayer_clock_t* clock);
/* The playing starts at @now. */
void avplayer_clock_start(avplayer_clock_t* clock, uint64_t now);
/* Paused at @now; resumed at @now. */
void avplayer_clock_pause(avplayer_clock_t* clock, uint64_t now);
void avplayer_clock_resume(avplayer_clock_t* clock, uint64_t now);
/* Sound was handed to the game at @now: the time since is counted from here. */
void avplayer_clock_data(avplayer_clock_t* clock, uint64_t now);

/* Milliseconds of playing since it started, or since sound was last handed over (since @now
 * if none was yet: 0), at @now. @paused: it is paused, and the time stands at the pause's. */
int64_t avplayer_clock_since_start_ms(const avplayer_clock_t* clock, uint64_t now, int paused);
int64_t avplayer_clock_since_data_ms(const avplayer_clock_t* clock, uint64_t now, int paused);

#ifdef __cplusplus
}
#endif

#endif
