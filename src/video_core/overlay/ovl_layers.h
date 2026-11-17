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

/* The system's message dialog (ovl_msgdialog.c): the game's, and the emulator's own, which
 * waits for the game's; and the error dialog. The status is CommonDialog::Status's (none,
 * initialized, running, finished); the game's thread sets the first two, opens and changes one, and
 * may close it; the overlay's thread draws it and closes it when a button is pressed. Its values
 * are libSceMsgDialog's. */
enum { OVL_MSG_DIALOG_GAME, OVL_MSG_DIALOG_EMULATOR, OVL_MSG_DIALOG_ERROR };
/* The modes; the error dialog (libSceErrorDialog, its own instance) is its message and OK */
enum {
    OVL_MSG_MODE_USER = 1,
    OVL_MSG_MODE_PROGRESS_BAR = 2,
    OVL_MSG_MODE_SYSTEM = 3,
    OVL_MSG_MODE_ERROR = 4
};
enum {
    OVL_MSG_OK = 0,
    OVL_MSG_YESNO = 1,
    OVL_MSG_NONE = 2,
    OVL_MSG_OK_CANCEL = 3,
    OVL_MSG_WAIT = 5,
    OVL_MSG_WAIT_CANCEL = 6,
    OVL_MSG_YESNO_FOCUS_NO = 7,
    OVL_MSG_OK_CANCEL_FOCUS_CANCEL = 8,
    OVL_MSG_TWO_BUTTONS = 9
};
enum { OVL_MSG_BAR_PERCENTAGE = 0, OVL_MSG_BAR_PERCENTAGE_CANCEL = 1 };
enum {
    OVL_MSG_BUTTON_INVALID = 0,
    OVL_MSG_BUTTON_OK = 1,
    OVL_MSG_BUTTON_1 = 1,
    OVL_MSG_BUTTON_2 = 2
};
enum { OVL_MSG_RESULT_OK = 0, OVL_MSG_RESULT_USER_CANCELED = 1 };
typedef struct ovl_msg_dialog_param {
    int mode;
    int button_type; /* a user message's */
    int bar_type;    /* a progress bar's */
    int sys_type;    /* a system message's */
    const char* msg; /* copied; up to 1024 bytes */
    const char* button1;
    const char* button2; /* two buttons' own texts */
} ovl_msg_dialog_param_t;
void ovl_msg_dialog_init(void);
void ovl_msg_dialog_shutdown(void);
int ovl_msg_dialog_status(int which);
void ovl_msg_dialog_set_status(int which, int status);
void ovl_msg_dialog_open(int which, const ovl_msg_dialog_param_t* param);
void ovl_msg_dialog_set_message(int which, const char* msg);
void ovl_msg_dialog_set_progress(int which, unsigned value);
void ovl_msg_dialog_add_progress(int which, unsigned delta);
/* Finishes it with @button and @result if it runs; 0 if it did not */
int ovl_msg_dialog_close(int which, int button, int result);
void ovl_msg_dialog_result(int which, int* button, int* result);
/* From a thread that is not the overlay's: until it is no longer running */
void ovl_msg_dialog_wait(int which);
void ovl_msg_dialog_draw(void);
int ovl_msg_dialog_active(void);
/* The core's DrawCenteredText, shared with the other dialogs */
void ovl_draw_centered_text(const char* text, const char* end, ovl_vec2_t content);

#ifdef __cplusplus
}
#endif

#endif
