/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef FRAME_NOTES_H
#define FRAME_NOTES_H

#include <stddef.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* A diagnostic: all that is noted for the GPU during one frame
 * (vk_device_fault_mark: each draw and dispatch with its shaders, targets,
 * textures and registers, each copy, clear and upload) is written as text, a
 * line a note, to frame_<number>.txt in @dir. Three frames are written: the
 * first that follows a frame with many draws (so not a logo or a loading
 * screen), and two more, five seconds apart. Called when the game shows a
 * frame, on the thread that records; a null @dir ends it. */
void frame_notes_flip(const char* dir);

/* A draw was asked for: counted, to tell a frame of the game proper */
void frame_notes_draw(void);

/* The file for an image's pixels read back during the frame being written:
 * frame_<number>_img<seq>_<address>_f<format>_<w>x<h>_s<step>.raw, where
 * every @step-th pixel of every @step-th row is kept. Returns 0 when no
 * frame is being written or @path is too short. */
int frame_notes_image_path(char* path, size_t size, unsigned seq, unsigned long address_high,
                           unsigned long address_low, unsigned format, unsigned width,
                           unsigned height, unsigned step);

/* The number of the frame being written, 0 when none is */
unsigned long frame_notes_frame(void);

RETRO_END_DECLS

#endif
