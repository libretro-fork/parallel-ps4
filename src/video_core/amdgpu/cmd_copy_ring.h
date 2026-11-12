/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef AMDGPU_CMD_COPY_RING_H
#define AMDGPU_CMD_COPY_RING_H

#include <stddef.h>
#include <stdint.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

#define CMD_COPY_RING_MAX_SEGMENTS 8

/* Copies of a game's graphics command buffers (Copy GPU Buffers), for the GPU
 * thread to read after the game has moved on and may write over its own.
 *
 * The room is a few segments, used in turn: a frame's copies go into one, and
 * when the game says the frame is submitted the next is taken, but only once
 * the GPU thread has finished every submission whose copies are in it. Before,
 * the room was used again from the start at every frame, while the GPU thread
 * could still be a frame behind: the next frame's copies were written over
 * commands it had not read yet.
 *
 * The game's submitting thread does all but the two *_finished calls, which
 * are the GPU thread's. Nothing blocks: with no free room a submission is not
 * copied, and is read where the game has it, as with the option off. All the
 * memory is taken at init. */
typedef struct cmd_copy_ring cmd_copy_ring_t;

/* Room for @segments (2 to CMD_COPY_RING_MAX_SEGMENTS) of @dcb_words and
 * @ccb_words each. NULL on failure. */
cmd_copy_ring_t* cmd_copy_ring_new(size_t dcb_words, size_t ccb_words, unsigned segments);
void cmd_copy_ring_free(cmd_copy_ring_t* ring);

/* Copies @dcb and @ccb (in words) and points *@dcb_out and *@ccb_out at the
 * copies, returning 1; or, with no room free, points them at the originals and
 * returns 0. */
int cmd_copy_ring_copy(cmd_copy_ring_t* ring, const uint32_t* dcb, size_t dcb_words,
                       const uint32_t* ccb, size_t ccb_words, const uint32_t** dcb_out,
                       const uint32_t** ccb_out);

/* A graphics submission was handed to the GPU thread, with a constant engine
 * one when @has_ce: after cmd_copy_ring_copy, copied or not */
void cmd_copy_ring_submitted(cmd_copy_ring_t* ring, int has_ce);

/* The game has submitted a frame: the next segment, if the GPU thread is done
 * with it */
void cmd_copy_ring_frame_done(cmd_copy_ring_t* ring);

/* The GPU thread finished a graphics or a constant engine submission */
void cmd_copy_ring_gfx_finished(cmd_copy_ring_t* ring);
void cmd_copy_ring_ce_finished(cmd_copy_ring_t* ring);

RETRO_END_DECLS

#endif
