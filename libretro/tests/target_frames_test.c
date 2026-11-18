/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* target_frames: which draws asynchronous shader compilation may leave out.
 *   - an image never drawn into, or not drawn into the frame before (drawn
 *     once, or every other frame), is not one a draw may be left out of;
 *     one drawn into the frame before is, before and after this frame's
 *     draws are noted;
 *   - a game's frames, played through: its first frame's draws all wait, so
 *     the exposure it keeps from frame to frame starts from a scene that was
 *     drawn; a shader that turns up later is left out of the scene, which is
 *     a frame old there, and the exposure never takes in an image that held
 *     nothing; with every unready draw left out, as before, it does, and
 *     stays that way;
 *   - a draw left out, noted, keeps the image one the next frame's draws
 *     may be left out of. */

#include <stdio.h>

#include "common/target_frames.h"

static int failures = 0;

#define CHECK(cond, what)                                                     \
    do {                                                                      \
        if (!(cond)) {                                                        \
            printf("FAIL %s (line %d)\n", what, __LINE__);                    \
            ++failures;                                                       \
        }                                                                     \
    } while (0)

static void test_recurring(void) {
    target_frames_t scene = {0, 0}, table = {0, 0}, half = {0, 0};
    uint32_t frame = 1;
    int f;
    CHECK(!target_frames_recurring(&scene, frame), "never drawn into: the draw waits");
    target_frames_draw(&scene, frame);
    CHECK(!target_frames_recurring(&scene, frame), "nor the frame's other draws into it");
    target_frames_draw(&table, frame);
    frame++;
    CHECK(target_frames_recurring(&scene, frame), "drawn into the frame before: may be left out");
    target_frames_draw(&scene, frame);
    CHECK(target_frames_recurring(&scene, frame), "so may the frame's later draws");
    target_frames_draw(&scene, frame);
    CHECK(target_frames_recurring(&scene, frame), "however many are noted");
    frame++;
    CHECK(!target_frames_recurring(&table, frame), "drawn once, two frames ago: waits");
    frame++;
    CHECK(!target_frames_recurring(&scene, frame), "a frame missed: waits");
    target_frames_draw(&scene, frame);
    CHECK(!target_frames_recurring(&scene, frame), "for the whole of the frame");
    for (f = 0; f < 10; f++) {
        frame++;
        if (f % 2 == 0) {
            CHECK(!target_frames_recurring(&half, frame), "drawn every other frame: waits");
            target_frames_draw(&half, frame);
        }
    }
}

/* The game's frame: the scene, then the scene's brightness into a small
 * image, then the exposure it keeps, blended from the last frame's and the
 * brightness. @ready: whether the scene's pipeline is made. @careful: only
 * a draw into an image drawn into the frame before is left out; otherwise
 * every unready draw is. What an image holds is a number here, -1 for an
 * image nothing was drawn into yet, which spoils whatever reads it. */
static double scene_holds, kept_holds;
static target_frames_t scene_frames;
static uint32_t game_frame;
static int left_out;

static void play_frame(int ready, int careful) {
    if (ready) {
        scene_holds = 0.5;
    } else if (careful && !target_frames_recurring(&scene_frames, game_frame)) {
        scene_holds = 0.5; /* waited for */
    } else {
        left_out++; /* the scene holds what it held */
    }
    target_frames_draw(&scene_frames, game_frame); /* drawn, or left out: the game drew */
    kept_holds = scene_holds < 0.0 || kept_holds < 0.0 ? -1.0 : (kept_holds + scene_holds) / 2.0;
    game_frame++;
}

static void test_frame(void) {
    int f;
    scene_holds = -1.0;
    kept_holds = 1.0;
    scene_frames.drawn = scene_frames.before = 0;
    game_frame = 1;
    left_out = 0;
    /* The first frame, the scene's pipeline not made: the image is new, so the draw waits */
    play_frame(0, 1);
    CHECK(kept_holds == 0.75, "the first frame's draws wait: the exposure is made from the scene");
    CHECK(left_out == 0, "nothing left out of a new image");
    /* A shader that turns up later, four frames in the making: left out each frame */
    for (f = 0; f < 4; f++)
        play_frame(0, 1);
    CHECK(left_out == 4, "left out of an image that holds a frame, frame after frame");
    CHECK(kept_holds > 0.0, "and the exposure is whole");
    play_frame(1, 1);
    CHECK(kept_holds > 0.0 && kept_holds < 0.75, "it moves on once the pipeline is made");
    /* As it was: every unready draw left out, the first frame's too */
    scene_holds = -1.0;
    kept_holds = 1.0;
    scene_frames.drawn = scene_frames.before = 0;
    game_frame = 1;
    play_frame(0, 0);
    for (f = 0; f < 100; f++)
        play_frame(1, 0);
    CHECK(kept_holds < 0.0, "left out of an image that held nothing: spoiled for good");
}

int main(void) {
    test_recurring();
    test_frame();
    if (failures) {
        printf("target_frames_test: %d failure(s)\n", failures);
        return 1;
    }
    printf("target_frames_test: ok\n");
    return 0;
}
