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

/* The splash screen (ovl_splash.c), drawn by vk_overlay.c. Opened with the game's PNG, lent
 * for as long as it is open: its size, 0 when it is none rpng reads. Started with the memory
 * it decodes into, w x h R, G, B, A bytes, on a thread of its own. From any thread after:
 * whether it is decoded, or still decoding; asked to stop. Closed when the overlays go, the
 * thread stopped and ended. */
int ovl_splash_open(const uint8_t* png, size_t size, unsigned* w, unsigned* h);
int ovl_splash_start(uint8_t* pixels);
int ovl_splash_decoded(void);
int ovl_splash_decoding(void);
void ovl_splash_stop(void);
void ovl_splash_close(void);
/* Its quad within the display, as the core's ImGui window drew it: indices 0 1 2, 0 2 3 */
void ovl_splash_quad(float display_w, float display_h, ovl_vertex_t v[4]);

#ifdef __cplusplus
}
#endif

#endif
