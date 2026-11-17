/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The overlays drawn with ovl, each built once a frame by vk_overlay.c */

#ifndef SHADPS4_OVL_LAYERS_H
#define SHADPS4_OVL_LAYERS_H

#include "ovl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The FPS counter, when the core option shows it; @framerate the game's frames a second */
void ovl_fps_draw(float framerate);

/* Notifications (ovl_notify.c). Queued from any thread: the message, how many seconds it
 * shows, where, and an icon's PNG (decoded before it returns; null for none), on a light grey
 * square when @icon_background. 0 when too many are waiting. */
enum { OVL_NOTIFY_TOP_LEFT, OVL_NOTIFY_TOP_RIGHT, OVL_NOTIFY_BOTTOM_LEFT, OVL_NOTIFY_BOTTOM_RIGHT };
int ovl_notify_queue(const char* message, float timer, int pos, const uint8_t* png, size_t png_size,
                     int icon_background);
void ovl_notify_draw(void);
/* From any thread: one shows or waits, so frames are to be drawn even with no new game frame */
int ovl_notify_active(void);
/* The overlay thread's: the message of the one showing, null for none */
const char* ovl_notify_showing_message(void);
void ovl_notify_reset(void);

/* The trophy popup (ovl_trophy.c). Queued from the game's thread: the trophy's name, its icon's
 * PNG and its type's (decoded before it returns; null for none). The sound it plays is lent,
 * stereo frames for the core's mix. */
int ovl_trophy_queue(const char* name, const uint8_t* icon_png, size_t icon_size,
                     const uint8_t* type_png, size_t type_size);
void ovl_trophy_set_sound(const float* frames, unsigned count);
void ovl_trophy_draw(void);
int ovl_trophy_active(void);
void ovl_trophy_reset(void);

#ifdef __cplusplus
}
#endif

#endif
