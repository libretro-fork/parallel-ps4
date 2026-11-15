/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "common/game_clock.h"

void game_clock_init(game_clock_t* clock, unsigned fps_num, unsigned fps_den) {
    const uint64_t total = (uint64_t)1000000000u * (fps_den ? fps_den : 1);
    if (!fps_num)
        fps_num = 60;
    retro_atomic_64_init(&clock->now, 0);
    retro_atomic_64_init(&clock->frame_end, 0);
    retro_atomic_64_init(&clock->frames, 0);
    clock->fps_num = fps_num;
    clock->period = total / fps_num;
    clock->rem = total % fps_num;
    clock->acc = 0;
}

/* The time moves to @to if that is later: never back, whoever moves it */
static uint64_t game_clock_raise(game_clock_t* clock, uint64_t to) {
    int64_t have = retro_atomic_load_acquire_64(&clock->now);
    while ((uint64_t)have < to) {
        if (retro_atomic_cas_64(&clock->now, have, (int64_t)to))
            return to;
        have = retro_atomic_load_acquire_64(&clock->now);
    }
    return (uint64_t)have;
}

void game_clock_frame(game_clock_t* clock) {
    const uint64_t start = (uint64_t)retro_atomic_load_relaxed_64(&clock->frame_end);
    uint64_t end;
    clock->acc += clock->rem;
    end = start + clock->period + clock->acc / clock->fps_num;
    clock->acc %= clock->fps_num;
    /* The end first: a thread that sees the new start may advance within the new frame */
    retro_atomic_store_release_64(&clock->frame_end, (int64_t)end);
    game_clock_raise(clock, start);
    retro_atomic_fetch_add_64(&clock->frames, 1);
}

uint64_t game_clock_now(game_clock_t* clock) {
    return (uint64_t)retro_atomic_load_acquire_64(&clock->now);
}

uint64_t game_clock_frame_end(game_clock_t* clock) {
    return (uint64_t)retro_atomic_load_acquire_64(&clock->frame_end);
}

uint64_t game_clock_frames(game_clock_t* clock) {
    return (uint64_t)retro_atomic_load_acquire_64(&clock->frames);
}

uint64_t game_clock_advance(game_clock_t* clock, uint64_t until) {
    const uint64_t end = (uint64_t)retro_atomic_load_acquire_64(&clock->frame_end);
    return game_clock_raise(clock, until < end ? until : end);
}
