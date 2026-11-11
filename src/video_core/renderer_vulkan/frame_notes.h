/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef FRAME_NOTES_H
#define FRAME_NOTES_H

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* A diagnostic: every so many of the game's frames, all that is noted for the
 * GPU during one frame (vk_device_fault_mark: each draw and dispatch with its
 * shaders, targets, textures and registers, each copy, clear and upload) is
 * written as text, a line a note, to frame_<number>.txt in @dir. Called when
 * the game shows a frame, on the thread that records; a null @dir ends it. */
void frame_notes_flip(const char* dir);

RETRO_END_DECLS

#endif
