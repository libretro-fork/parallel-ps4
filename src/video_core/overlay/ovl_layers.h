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
/* The core's DrawPrettyBackground: the window's inside in greys that shift with time */
void ovl_draw_pretty_background(void);

/* The save data dialog (ovl_savedialog.c), libSceSaveDataDialog's: the game's saves to choose
 * one of, or one save over a message, or a progress bar. Its values are libSceSaveDataDialog's,
 * the status CommonDialog::Status's. The game's thread opens it and may close it; the overlay's
 * thread draws it and closes it when a save or a button is chosen.
 *
 * What it shows is lent: the game's thread builds it where ovl_save_dialog_back says, a buffer
 * of three that the overlay's thread reads only once it is opened, and keeps it until it builds
 * in that buffer again; opening returns the buffer opened. */
enum {
    OVL_SAVE_MODE_LIST = 1,
    OVL_SAVE_MODE_USER = 2,
    OVL_SAVE_MODE_SYSTEM = 3,
    OVL_SAVE_MODE_ERROR = 4,
    OVL_SAVE_MODE_PROGRESS_BAR = 5
};
enum { OVL_SAVE_TYPE_SAVE = 1, OVL_SAVE_TYPE_LOAD = 2, OVL_SAVE_TYPE_DELETE = 3 };
enum {
    OVL_SAVE_STYLE_TITLE_DATESIZE_SUBTITLE = 0,
    OVL_SAVE_STYLE_TITLE_SUBTITLE_DATESIZE = 1,
    OVL_SAVE_STYLE_TITLE_DATESIZE = 2
};
enum {
    OVL_SAVE_BUTTONS_OK = 0,
    OVL_SAVE_BUTTONS_YESNO = 1,
    OVL_SAVE_BUTTONS_NONE = 2,
    OVL_SAVE_BUTTONS_OKCANCEL = 3
};
enum {
    OVL_SAVE_BUTTON_INVALID = 0,
    OVL_SAVE_BUTTON_OK = 1,
    OVL_SAVE_BUTTON_YES = 1,
    OVL_SAVE_BUTTON_NO = 2
};
enum { OVL_SAVE_RESULT_OK = 0, OVL_SAVE_RESULT_USER_CANCELED = 1 };
typedef struct ovl_save_item {
    const char* title;
    const char* subtitle;
    const char* date;
    const char* size;
    int icon; /* an image's handle (ovl_tex_decode_icon), 0 for none */
    int corrupted;
} ovl_save_item_t;
typedef struct ovl_save_dialog_param {
    int mode, type, style;
    int enable_back;              /* the game's option: 1, 0, or -1 when it gave none */
    const ovl_save_item_t* items; /* the new save first, when there is one */
    int item_count;
    int has_new_item;
    int focus;                    /* the save focused when the list appears, -1 for none */
    const char* msg;              /* a message's */
    int user_buttons, user_error; /* a user message's */
    int hide_ok, show_no, show_cancel, return_cancel; /* a system message's */
    int old_firmware; /* the game's before 4.50: OK alone and No answer as cancelled */
} ovl_save_dialog_param_t;
void ovl_save_dialog_init(void);
void ovl_save_dialog_shutdown(void);
int ovl_save_dialog_status(void);
void ovl_save_dialog_set_status(int status);
int ovl_save_dialog_back(void);
int ovl_save_dialog_open(const ovl_save_dialog_param_t* param);
void ovl_save_dialog_set_progress(unsigned value);
void ovl_save_dialog_add_progress(unsigned delta);
/* Finishes it with @button and @result if it runs, no save chosen; 0 if it did not */
int ovl_save_dialog_close(int button, int result);
/* The button, result and the save chosen (an index into the items, -1 for none) */
void ovl_save_dialog_result(int* button, int* result, int* item);
void ovl_save_dialog_draw(void);
int ovl_save_dialog_active(void);

/* The profile dialog (ovl_profile.c), libSceNpProfileDialog's: a card naming a player and what
 * was asked about them, and Close. Its modes are libSceNpProfileDialog's, the status and the
 * result CommonDialog's. The game's thread opens it (its text copied) and may close it; the
 * overlay's thread draws it and closes it when Close is pressed. */
enum {
    OVL_PROFILE_MODE_NORMAL = 1,
    OVL_PROFILE_MODE_FRIEND_REQUEST = 2,
    OVL_PROFILE_MODE_ADD_TO_BLOCK_LIST = 3,
    OVL_PROFILE_MODE_GRIEF_REPORT = 4
};
enum { OVL_PROFILE_RESULT_OK = 0, OVL_PROFILE_RESULT_USER_CANCELED = 1 };
typedef struct ovl_profile_dialog_param {
    int mode;
    const char* online_id; /* shown unless there is an account id; up to 32 bytes */
    int has_account_id;
    uint64_t account_id; /* shown as its number */
} ovl_profile_dialog_param_t;
void ovl_profile_dialog_init(void);
void ovl_profile_dialog_shutdown(void);
int ovl_profile_dialog_status(void);
void ovl_profile_dialog_set_status(int status);
void ovl_profile_dialog_open(const ovl_profile_dialog_param_t* param);
/* Finishes it with @user_action if it runs; 0 if it did not */
int ovl_profile_dialog_close(int user_action);
int ovl_profile_dialog_result(void);
void ovl_profile_dialog_draw(void);
int ovl_profile_dialog_active(void);

/* The store's dialog (ovl_commerce.c), libSceNpCommerce's: what the game asked the store for,
 * for whom, and one or two buttons. Its modes and results are libSceNpCommerce's, the status
 * CommonDialog's. The game's thread opens it (its texts copied) and may close it; the overlay's
 * thread draws it and closes it when a button is pressed. */
enum {
    OVL_COMMERCE_MODE_CATEGORY = 0,
    OVL_COMMERCE_MODE_PRODUCT = 1,
    OVL_COMMERCE_MODE_PRODUCT_CODE = 2,
    OVL_COMMERCE_MODE_CHECKOUT = 3,
    OVL_COMMERCE_MODE_DOWNLOADLIST = 4,
    OVL_COMMERCE_MODE_PLUS = 5
};
enum {
    OVL_COMMERCE_RESULT_OK = 0,
    OVL_COMMERCE_RESULT_USER_CANCELED = 1,
    OVL_COMMERCE_RESULT_PURCHASED = 2
};
#define OVL_COMMERCE_TARGETS 10 /* ORBIS_NP_COMMERCE_DIALOG_NUM_TARGETS_MAX */
typedef struct ovl_commerce_dialog_param {
    int mode;
    int user_id;
    const char* username; /* null or empty when the user has none; up to 64 bytes */
    unsigned service_label;
    uint64_t features;          /* the subscription's (ORBIS_NP_PLUS_FEATURE_*), 0 otherwise */
    const char* const* targets; /* products or categories, up to 128 bytes each */
    int target_count;
} ovl_commerce_dialog_param_t;
void ovl_commerce_dialog_init(void);
void ovl_commerce_dialog_shutdown(void);
int ovl_commerce_dialog_status(void);
void ovl_commerce_dialog_set_status(int status);
/* Running, its result cancelled until a button or the game says otherwise */
void ovl_commerce_dialog_open(const ovl_commerce_dialog_param_t* param);
/* Finishes it with @result if it runs; 0 if it did not */
int ovl_commerce_dialog_close(int result);
int ovl_commerce_dialog_result(void);
void ovl_commerce_dialog_draw(void);
int ovl_commerce_dialog_active(void);
/* The store's icon at the bottom of the screen, from any thread: shown at a place
 * (OrbisNpCommercePsStoreIconPos), hidden, and how far in from the edges
 * (OrbisNpCommercePsStoreIconLayout) */
enum { OVL_STORE_ICON_CENTER = 0, OVL_STORE_ICON_LEFT = 1, OVL_STORE_ICON_RIGHT = 2 };
enum {
    OVL_STORE_ICON_LAYOUT_DEFAULT = 0,
    OVL_STORE_ICON_LAYOUT_FOLLOW_SAFE_AREA = 1,
    OVL_STORE_ICON_LAYOUT_FIXED_90 = 2
};
void ovl_store_icon_show(int pos);
void ovl_store_icon_hide(void);
void ovl_store_icon_set_layout(int layout);
void ovl_store_icon_draw(void);
int ovl_store_icon_active(void);

/* The invitation dialog (ovl_invdialog.c), libSceInvitationDialog's: to send a session's
 * invitation to friends the player ticks or the game fixed, or to answer the invitations
 * waiting. Its modes are libSceInvitationDialog's, the status and the result CommonDialog's.
 * The game's thread opens it (its texts copied) and may close it; the overlay's thread draws
 * it and closes it. */
enum { OVL_INVDLG_MODE_SEND = 1, OVL_INVDLG_MODE_RECV = 2 };
enum { OVL_INVDLG_RESULT_OK = 0, OVL_INVDLG_RESULT_USER_CANCELED = 1 };
#define OVL_INVDLG_FRIENDS 128 /* the friends listed at most */
#define OVL_INVDLG_INVITES 32  /* the invitations listed at most */
#define OVL_INVDLG_FIXED 16    /* ORBIS_INVITATION_DIALOG_ADDRESS_USER_LIST_MAX_SIZE */
#define OVL_INVDLG_SENT 16     /* and as many reported sent to */
typedef struct ovl_invdlg_invite {
    const char* invitation_id; /* up to 63 bytes */
    const char* from_npid;     /* up to 31; empty when unknown */
} ovl_invdlg_invite_t;
typedef struct ovl_invdlg_friend {
    char npid[32];
    int online;
} ovl_invdlg_friend_t;
typedef struct ovl_invitation_dialog_param {
    int mode;
    int user_id;
    /* To send */
    int user_editable;           /* the player ticks friends; else the game's list below */
    const char* session_id;      /* up to 47 bytes */
    const char* message;         /* up to 512 */
    const char* const* fixed_to; /* the game's recipients: online ids, or account numbers */
    int fixed_count;
    /* To receive: the invitations waiting */
    const ovl_invdlg_invite_t* invites;
    int invite_count;
} ovl_invitation_dialog_param_t;
void ovl_invitation_dialog_init(void);
void ovl_invitation_dialog_shutdown(void);
int ovl_invitation_dialog_status(void);
void ovl_invitation_dialog_set_status(int status);
void ovl_invitation_dialog_open(const ovl_invitation_dialog_param_t* param);
/* The game's: finishes it with @result if it runs; 0 if it did not */
int ovl_invitation_dialog_close(int result);
int ovl_invitation_dialog_result(void);
/* Once finished: -1 if nothing was sent, else the friends ticked that it was sent to (@npids
 * filled, up to @max; 0 when the recipients were the game's own) */
int ovl_invitation_dialog_sent(const char** npids, int max);
void ovl_invitation_dialog_draw(void);
int ovl_invitation_dialog_active(void);
/* The core's, called on the overlay's thread, none of them waiting on the server: the user's
 * friends, now (@out filled, up to @max; how many); an invitation sent to @to, or accepted,
 * the outcome told later with @token; one declined */
int ovl_invitation_dialog_friends(int user_id, ovl_invdlg_friend_t* out, int max);
void ovl_invitation_dialog_send(int user_id, const char* session_id, const char* message,
                                const char* const* to, int count, unsigned token);
void ovl_invitation_dialog_accept(int user_id, const char* invitation_id, unsigned token);
void ovl_invitation_dialog_decline(int user_id, const char* invitation_id);
/* From any thread: how the request numbered @token went */
void ovl_invitation_dialog_request_done(unsigned token, int ok);

/* shadNet's notifications (ovl_shadnet.c): queued from any thread, a kind and its text (copied,
 * up to 511 bytes); 0 when too many are waiting. Up to six show, newest first. */
enum {
    OVL_SHADNET_FRIEND_REQUEST,
    OVL_SHADNET_FRIEND_NEW,
    OVL_SHADNET_FRIEND_LOST,
    OVL_SHADNET_ONLINE,
    OVL_SHADNET_INFO,
    OVL_SHADNET_WARNING /* the server refused something */
};
int ovl_shadnet_push(int kind, const char* text);
void ovl_shadnet_draw(void);
int ovl_shadnet_active(void);
void ovl_shadnet_reset(void);

/* The session invitation prompt (ovl_invite.c), the system software's stand-in. From any thread:
 * an invitation received (its texts copied; one with the same id replaced), or one gone (taken
 * through the game's dialog, or answered). 0 when too many changes are waiting. */
int ovl_invite_push(int user_id, const char* invitation_id, const char* session_id,
                    const char* from_npid);
int ovl_invite_remove(const char* invitation_id);
void ovl_invite_draw(void);
int ovl_invite_active(void);
void ovl_invite_reset(void);
/* The core's: called on the overlay's thread when one is accepted or declined on screen; it
 * answers the server without waiting there, and removes the prompt when done */
void ovl_invite_chosen(int user_id, const char* invitation_id, const char* from_npid, int accept);

/* The on-screen keyboard (libSceIme and libSceImeDialog). The core's, in C++ with the keyboard
 * (core/libraries/ime/ime_ui_shared.cpp): the keyboards a game has open are drawn, on the
 * overlay's thread; and whether one is open. */
void ovl_ime_draw(void);
int ovl_ime_must_keep_drawing(void);

#ifdef __cplusplus
}
#endif

#endif
