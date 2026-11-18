/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_COMMON_TARGET_FRAMES_H
#define SHADPS4_COMMON_TARGET_FRAMES_H

/* Which of the game's frames an image was drawn into, for the draws that
 * asynchronous shader compilation may leave out.
 *
 * A draw whose pipeline is still being made is left out so that the game
 * does not stop for it. That is harmless when the image already holds a
 * frame of the game's and the game makes it again next frame: the image is
 * a frame old where the draw is missing, and whole again once the pipeline
 * is made. It is not harmless for an image the game has not drawn a whole
 * frame into:
 *
 * - one it draws once (a table it looks colours up in, a shadow map of what
 *   does not move) is never made again;
 * - one it has just started drawing into holds nothing yet: what a left out
 *   clear or fill leaves there is whatever the memory had, and the draws
 *   that read it carry that into what the game keeps from frame to frame
 *   (its exposure, the last frame it blends with), where a value that is
 *   not a number stays for good and the picture with it.
 *
 * So a draw may be left out only when the game drew into every image it
 * draws into the frame before; a draw into any other waits for its
 * pipeline.
 *
 * The two frames are kept in the image itself, noted where the image is
 * bound to be drawn into (a line of the image the binding writes anyway),
 * and looked at only for a draw whose pipeline is not made: a draw whose
 * pipeline is made, with the option on or off, pays one comparison. Frames
 * count from 1; 0 is never. */

#include <stdint.h>

#include <retro_inline.h>

typedef struct target_frames {
    uint32_t drawn;  /* the last frame it was drawn into, 0: never */
    uint32_t before; /* and the one before that, 0: none */
} target_frames_t;

/* The game draws into the image in @frame. */
static INLINE void target_frames_draw(target_frames_t* t, uint32_t frame) {
    if (t->drawn != frame) {
        t->before = t->drawn;
        t->drawn = frame;
    }
}

/* Whether the game drew into the image in the frame before @frame: a draw
 * into it in @frame may be left out. */
static INLINE int target_frames_recurring(const target_frames_t* t, uint32_t frame) {
    const uint32_t last = t->drawn == frame ? t->before : t->drawn;
    return last != 0 && last + 1 == frame;
}

#endif
