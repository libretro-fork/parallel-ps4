/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "core/libraries/avplayer/avplayer_clock.h"

#include <string.h>

void avplayer_clock_reset(avplayer_clock_t* clock) {
    memset(clock, 0, sizeof(*clock));
}

void avplayer_clock_start(avplayer_clock_t* clock, uint64_t now) {
    clock->start = now;
    clock->started = 1;
}

void avplayer_clock_pause(avplayer_clock_t* clock, uint64_t now) {
    clock->pause_time = now;
}

void avplayer_clock_resume(avplayer_clock_t* clock, uint64_t now) {
    clock->pause_duration += now - clock->pause_time;
}

void avplayer_clock_data(avplayer_clock_t* clock, uint64_t now) {
    clock->last_data = now;
    clock->has_data = 1;
    clock->pause_duration = 0;
}

static int64_t elapsed_ms(const avplayer_clock_t* clock, uint64_t since, uint64_t now, int paused) {
    if (paused)
        now = clock->pause_time;
    return ((int64_t)now - (int64_t)since - (int64_t)clock->pause_duration) / 1000000;
}

int64_t avplayer_clock_since_start_ms(const avplayer_clock_t* clock, uint64_t now, int paused) {
    return elapsed_ms(clock, clock->start, now, paused);
}

int64_t avplayer_clock_since_data_ms(const avplayer_clock_t* clock, uint64_t now, int paused) {
    if (paused)
        now = clock->pause_time;
    return elapsed_ms(clock, clock->has_data ? clock->last_data : now, now, 0);
}
