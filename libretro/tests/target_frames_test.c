/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* target_frames: which draws asynchronous shader compilation may leave out.
 *   - an image never drawn into, or not drawn into the frame before (drawn
 *     once, or every other frame), is not one a draw may be left out of;
 *     one drawn into the frame before is, for every draw of the frame;
 *   - a game's frames, played through: its first frame's draws all wait, so
 *     the exposure it keeps from frame to frame starts from a scene that was
 *     drawn; a shader that turns up later is left out of the scene, which is
 *     a frame old there, and the exposure never takes in an image that held
 *     nothing; with every unready draw left out, as before, it does, and
 *     stays that way;
 *   - the table stays in the room made at init, however many images come
 *     and go. */

#include <stdio.h>
#include <stdlib.h>

#include "common/target_frames.h"

static int failures = 0;

#define CHECK(cond, what)                                                     \
    do {                                                                      \
        if (!(cond)) {                                                        \
            printf("FAIL %s (line %d)\n", what, __LINE__);                    \
            ++failures;                                                       \
        }                                                                     \
    } while (0)

#define SCENE 0x200000000ull
#define LUMA 0x200800000ull
#define KEPT 0x200900000ull
#define TABLE 0x200a00000ull

static void test_recurring(void) {
    target_frames_t t;
    int f;
    CHECK(target_frames_init(&t, 64), "init");
    CHECK(!target_frames_draw(&t, SCENE), "never drawn into: the draw waits");
    CHECK(!target_frames_draw(&t, SCENE), "nor the frame's other draws into it");
    target_frames_next(&t);
    CHECK(target_frames_draw(&t, SCENE), "drawn into the frame before: may be left out");
    CHECK(target_frames_draw(&t, SCENE), "so may the frame's other draws");
    /* Drawn once, long ago: waits again */
    CHECK(!target_frames_draw(&t, TABLE), "a new image in a later frame waits");
    target_frames_next(&t);
    target_frames_next(&t);
    CHECK(!target_frames_draw(&t, TABLE), "not drawn into the frame before: waits");
    CHECK(!target_frames_draw(&t, SCENE), "a frame missed: waits");
    /* Every other frame: never one a draw is left out of */
    for (f = 0; f < 10; f++) {
        target_frames_next(&t);
        if (f % 2 == 0)
            CHECK(!target_frames_draw(&t, LUMA), "drawn every other frame: waits");
    }
    target_frames_free(&t);
}

/* The game's frame: the scene, then the scene's brightness into a small
 * image, then the exposure it keeps, blended from the last frame's and the
 * brightness. @ready: whether the scene's pipeline is made. @careful: only
 * draws into images drawn into the frame before are left out; otherwise
 * every unready draw is. What an image holds is a number here, -1 for an
 * image nothing was drawn into yet, which spoils whatever reads it. */
static double scene_holds, kept_holds;

static void play_frame(target_frames_t* t, int ready, int careful) {
    double luma;
    const int may_skip = target_frames_draw(t, SCENE) || !careful;
    if (ready || !may_skip)
        scene_holds = 0.5; /* drawn: ready, or waited for */
    /* else left out: the scene holds what it held */
    target_frames_draw(t, LUMA);
    luma = scene_holds;
    target_frames_draw(t, KEPT);
    kept_holds = luma < 0.0 || kept_holds < 0.0 ? -1.0 : (kept_holds + luma) / 2.0;
    target_frames_next(t);
}

static void test_frame(void) {
    target_frames_t t;
    int f;
    CHECK(target_frames_init(&t, 64), "init");
    scene_holds = -1.0;
    kept_holds = 1.0;
    /* The first frame, the scene's pipeline not made: the image is new, so the draw waits */
    play_frame(&t, 0, 1);
    CHECK(kept_holds == 0.75, "the first frame's draws wait: the exposure is made from the scene");
    /* A shader that turns up later, four frames in the making: left out */
    for (f = 0; f < 4; f++)
        play_frame(&t, 0, 1);
    CHECK(kept_holds > 0.0, "left out of a scene that holds a frame: the exposure is whole");
    play_frame(&t, 1, 1);
    CHECK(kept_holds > 0.0 && kept_holds < 0.75, "and moves on once the pipeline is made");
    target_frames_free(&t);
    /* As it was: every unready draw left out, the first frame's too */
    CHECK(target_frames_init(&t, 64), "init");
    scene_holds = -1.0;
    kept_holds = 1.0;
    play_frame(&t, 0, 0);
    for (f = 0; f < 100; f++)
        play_frame(&t, 1, 0);
    CHECK(kept_holds < 0.0, "left out of an image that held nothing: spoiled for good");
    target_frames_free(&t);
}

static void test_bounded(void) {
    target_frames_t t;
    size_t capacity;
    uint32_t f;
    uint64_t i;
    CHECK(target_frames_init(&t, 64), "init");
    capacity = t.map.capacity;
    /* 20 new images a frame for 4000 frames, each drawn into for 2 frames */
    for (f = 0; f < 4000; f++) {
        for (i = 0; i < 20; i++) {
            target_frames_draw(&t, 0x300000000ull + (uint64_t)f * 0x10000 + i * 0x100);
            if (f > 0)
                target_frames_draw(&t, 0x300000000ull + (uint64_t)(f - 1) * 0x10000 + i * 0x100);
        }
        target_frames_draw(&t, SCENE);
        target_frames_next(&t);
    }
    CHECK(t.map.count <= 20 * 2 * TARGET_FRAMES_KEEP + 1, "old images are forgotten");
    CHECK(target_frames_draw(&t, SCENE), "one drawn into every frame is kept");
    (void)capacity;
    target_frames_free(&t);
    /* Within the room made: 8 images at a time never grow a table made for 64 */
    CHECK(target_frames_init(&t, 64), "init");
    capacity = t.map.capacity;
    for (f = 0; f < 4000; f++) {
        for (i = 0; i < 8; i++)
            target_frames_draw(&t, 0x400000000ull + (uint64_t)(f / 300) * 0x100000 + i * 0x1000);
        target_frames_next(&t);
    }
    CHECK(t.map.capacity == capacity, "no growth while the game runs");
    target_frames_free(&t);
}

int main(void) {
    test_recurring();
    test_frame();
    test_bounded();
    if (failures) {
        printf("target_frames_test: %d failure(s)\n", failures);
        return 1;
    }
    printf("target_frames_test: ok\n");
    return 0;
}
