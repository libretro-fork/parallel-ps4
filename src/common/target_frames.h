/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_COMMON_TARGET_FRAMES_H
#define SHADPS4_COMMON_TARGET_FRAMES_H

/* Which of the game's frames each image was drawn into, for the draws that
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
 * pipeline. target_frames_draw notes the draw and says which it is.
 *
 * Images are known by their address in the game's memory. Entries not drawn
 * into for TARGET_FRAMES_KEEP frames are dropped, so the table stays within
 * the room made for it at init: nothing is allocated while the game runs
 * unless it draws into more images than that at once. Not thread-safe: the
 * thread that draws. */

#include <stddef.h>
#include <stdint.h>

#include <boolean.h>
#include <retro_common_api.h>

#include "common/u64_map.h"

RETRO_BEGIN_DECLS

#define TARGET_FRAMES_KEEP 256u

typedef struct target_frames {
    u64_map_t map;
    uint32_t frame; /* the frame being drawn, from 1 */
} target_frames_t;

bool target_frames_init(target_frames_t* t, size_t reserve);
void target_frames_free(target_frames_t* t);

/* The frame is done: the next one starts. */
void target_frames_next(target_frames_t* t);

/* The game draws into the image at @address this frame. Returns 1 when it
 * also drew into it the frame before: a draw into it may be left out. */
int target_frames_draw(target_frames_t* t, uint64_t address);

RETRO_END_DECLS

#endif
