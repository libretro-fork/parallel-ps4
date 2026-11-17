/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The overlays drawn with ovl, each built once a frame by vk_overlay.cpp */

#ifndef SHADPS4_OVL_LAYERS_H
#define SHADPS4_OVL_LAYERS_H

#include "ovl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The FPS counter, when the core option shows it; @framerate the game's frames a second */
void ovl_fps_draw(float framerate);

#ifdef __cplusplus
}
#endif

#endif
