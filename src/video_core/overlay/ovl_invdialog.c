/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The invitation dialog (libSceInvitationDialog), as core/libraries/invitation_dialog's
 * invitation_dialog_ui.cpp drew it with ImGui: a window in the middle of the screen over
 * ImGui's slowly shifting grey. To send: the session, the friends to tick (a field to search
 * them above their list) when the game lets the player choose, the game's message, and Send
 * and Cancel. To receive: the invitations waiting, one chosen, and Accept, Decline and Cancel.
 *
 * The game's thread opens it and may close it; the overlay's thread draws it and closes it.
 * What is shown goes from one to the other through three buffers, the one between swapped by
 * each side, as the message dialog's; it is finished by whichever side claims it first, its
 * result published with it. Sending and accepting talk to the server, which the core does on
 * a thread of its own (ovl_invitation_dialog_send, _accept): the dialog shows it busy until
 * that thread says how it went (ovl_invitation_dialog_request_done), with the number the
 * request was given, so that an answer to an earlier request is not taken for this one's.
 * Nothing is allocated. */

#include <stdio.h>
#include <string.h>

#include <retro_atomic.h>

#include "ovl_layers.h"

#define NPID_TEXT 31
#define INVITE_ID_TEXT 63
#define SESSION_TEXT 47
#define MESSAGE_TEXT 512
#define FILTER_TEXT 63
#define DIRTY 4

/* The status, CommonDialog::Status's values, and the claim between running and finished */
enum { NONE, INITIALIZED, RUNNING, FINISHED, CLOSING };

typedef struct invite {
    char id[INVITE_ID_TEXT + 1];
    char from[NPID_TEXT + 1];
} invite_t;

typedef struct inv_param {
    unsigned gen; /* which opening it is from */
    int mode, user_id, user_editable;
    char session_id[SESSION_TEXT + 1];
    char message[MESSAGE_TEXT + 1];
    int fixed_count;
    char fixed_to[OVL_INVDLG_FIXED][NPID_TEXT + 1];
    int invite_count;
    invite_t invites[OVL_INVDLG_INVITES];
} inv_param_t;

/* The game's thread's: what it opened last, and the buffer it writes */
static inv_param_t staged;
static int back;
/* The buffers; the one between, with DIRTY when the game's thread put it there since */
static inv_param_t bufs[3];
static retro_atomic_int_t middle;
/* The overlay's thread's: the buffer it reads and the opening it last drew */
static int front;
static unsigned drawn_gen;
static int drawn;
/* and what the player has done in this opening */
static invite_t invites[OVL_INVDLG_INVITES]; /* those still waiting */
static int invite_count, recv_selected;
static char selected[OVL_INVDLG_FRIENDS][NPID_TEXT + 1]; /* the friends ticked */
static int selected_count;
static char filter[FILTER_TEXT + 1];
static ovl_invdlg_friend_t friends[OVL_INVDLG_FRIENDS];
static unsigned request_seq; /* the last request's number */
static int busy;             /* a request waits for its answer */
static char busy_invite[INVITE_ID_TEXT + 1];
static char sending[OVL_INVDLG_FRIENDS][NPID_TEXT + 1]; /* the friends a send is for */
static int sending_count;

static retro_atomic_int_t status;
static retro_atomic_int_t result;
/* A request's number << 2, | 2 once answered, | 1 if it went well */
static retro_atomic_int_t request;
/* What a finished send sent to the friends ticked: the overlay's thread's while it runs, read
 * by the game's once it is finished */
static char sent[OVL_INVDLG_SENT][NPID_TEXT + 1];
static int sent_count, sent_ok;

static void copy_text(char* dst, size_t size, const char* src) {
    size_t n = 0;
    while (src && n < size - 1 && src[n] != '\0')
        n++;
    if (n)
        memcpy(dst, src, n);
    dst[n] = '\0';
}

void ovl_invitation_dialog_init(void) {
    memset(&staged, 0, sizeof(staged));
    memset(bufs, 0, sizeof(bufs));
    back = 0;
    retro_atomic_store_relaxed_int(&middle, 1);
    front = 2;
    drawn = 0;
    drawn_gen = 0;
    busy = 0;
}

void ovl_invitation_dialog_shutdown(void) {
    ovl_invitation_dialog_close(OVL_INVDLG_RESULT_OK);
}

int ovl_invitation_dialog_status(void) {
    const int s = retro_atomic_load_acquire_int(&status);
    return s == CLOSING ? RUNNING : s;
}

void ovl_invitation_dialog_set_status(int s) {
    retro_atomic_store_release_int(&status, s);
}

void ovl_invitation_dialog_open(const ovl_invitation_dialog_param_t* p) {
    int i, n;
    staged.gen++;
    staged.mode = p->mode;
    staged.user_id = p->user_id;
    staged.user_editable = p->user_editable;
    copy_text(staged.session_id, sizeof(staged.session_id), p->session_id);
    copy_text(staged.message, sizeof(staged.message), p->message);
    n = p->fixed_to ? p->fixed_count : 0;
    if (n > OVL_INVDLG_FIXED)
        n = OVL_INVDLG_FIXED;
    for (i = 0; i < n; i++)
        copy_text(staged.fixed_to[i], sizeof(staged.fixed_to[i]), p->fixed_to[i]);
    staged.fixed_count = n < 0 ? 0 : n;
    n = p->invites ? p->invite_count : 0;
    if (n > OVL_INVDLG_INVITES)
        n = OVL_INVDLG_INVITES;
    for (i = 0; i < n; i++) {
        copy_text(staged.invites[i].id, sizeof(staged.invites[i].id), p->invites[i].invitation_id);
        copy_text(staged.invites[i].from, sizeof(staged.invites[i].from), p->invites[i].from_npid);
    }
    staged.invite_count = n < 0 ? 0 : n;
    retro_atomic_store_relaxed_int(&result, OVL_INVDLG_RESULT_OK);
    bufs[back] = staged;
    back = retro_atomic_exchange_int(&middle, back | DIRTY) & ~DIRTY;
    retro_atomic_store_release_int(&status, RUNNING);
}

/* Finished with @r by the overlay's thread, what a send sent published with it */
static int finish(int r, int ok) {
    int i;
    if (!retro_atomic_cas_int(&status, RUNNING, CLOSING))
        return 0;
    sent_ok = ok;
    sent_count = 0;
    if (ok)
        for (i = 0; i < sending_count && sent_count < OVL_INVDLG_SENT; i++)
            memcpy(sent[sent_count++], sending[i], sizeof(sent[0]));
    retro_atomic_store_relaxed_int(&result, r);
    retro_atomic_store_release_int(&status, FINISHED);
    return 1;
}

int ovl_invitation_dialog_close(int r) {
    if (!retro_atomic_cas_int(&status, RUNNING, CLOSING))
        return 0;
    /* Nothing sent that the game hears of: an answer still to come is not waited for */
    sent_ok = 0;
    sent_count = 0;
    retro_atomic_store_relaxed_int(&result, r);
    retro_atomic_store_release_int(&status, FINISHED);
    return 1;
}

int ovl_invitation_dialog_result(void) {
    return retro_atomic_load_relaxed_int(&result);
}

int ovl_invitation_dialog_sent(const char** npids, int max) {
    int i;
    for (i = 0; i < sent_count && i < max; i++)
        npids[i] = sent[i];
    return sent_ok ? i : -1;
}

void ovl_invitation_dialog_request_done(unsigned token, int ok) {
    /* Only the request still waited for: one from an earlier opening changes nothing */
    retro_atomic_cas_int(&request, (int)(token << 2), (int)((token << 2) | 2u | (ok ? 1u : 0u)));
}

int ovl_invitation_dialog_active(void) {
    return retro_atomic_load_acquire_int(&status) == RUNNING;
}

/* ------------------------------------------------------------------------------------------ */
/* Drawing, as invitation_dialog_ui.cpp */

#define BUTTON_W 130.0f
#define BUTTON_H 30.0f

static ovl_vec2_t vec2(float x, float y) {
    ovl_vec2_t v;
    v.x = x;
    v.y = y;
    return v;
}

static unsigned new_request(void) {
    request_seq = (request_seq + 1) & 0x0FFFFFFFu;
    retro_atomic_store_release_int(&request, (int)(request_seq << 2));
    busy = 1;
    return request_seq;
}

static void remove_invite(int i) {
    memmove(&invites[i], &invites[i + 1], sizeof(invites[0]) * (size_t)(invite_count - i - 1));
    invite_count--;
    recv_selected = invite_count ? 0 : -1;
}

static int find_selected(const char* npid) {
    int i;
    for (i = 0; i < selected_count; i++)
        if (strcmp(selected[i], npid) == 0)
            return i;
    return -1;
}

static char lower(char c) {
    return c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c;
}

/* @npid has @needle in it, letters' case aside */
static int matches(const char* npid, const char* needle) {
    size_t i, j;
    if (!needle[0])
        return 1;
    for (i = 0; npid[i]; i++) {
        for (j = 0; needle[j] && lower(npid[i + j]) == lower(needle[j]); j++)
            ;
        if (!needle[j])
            return 1;
    }
    return 0;
}

static void draw_send_body(const inv_param_t* p) {
    char line[MESSAGE_TEXT + 32];
    if (p->session_id[0]) {
        sprintf(line, "Session: %s", p->session_id);
        ovl_text_wrapped(line, NULL);
    }
    if (p->user_editable) {
        const int count = ovl_invitation_dialog_friends(p->user_id, friends, OVL_INVDLG_FRIENDS);
        ovl_text_wrapped("Select friends to invite:", NULL);
        if (count <= 0) {
            ovl_text_disabled("No friends available. Add friends from the friends overlay.", NULL);
        } else {
            int i;
            ovl_set_next_item_width(-1.0f);
            ovl_input_text_with_hint("##invitation_friend_filter", "Search friends", filter,
                                     sizeof(filter));
            if (ovl_begin_child("##invitation_friends", vec2(0.0f, 120.0f), OVL_CHILD_BORDERS, 0)) {
                for (i = 0; i < count; i++) {
                    const int at = find_selected(friends[i].npid);
                    int checked = at >= 0;
                    /* Out of view while the search leaves it out; its tick is kept */
                    if (!matches(friends[i].npid, filter))
                        continue;
                    ovl_push_id_str(friends[i].npid);
                    sprintf(line, "%s%s", friends[i].npid,
                            friends[i].online ? " (online)" : " (offline)");
                    if (ovl_checkbox(line, &checked)) {
                        if (checked && at < 0 && selected_count < OVL_INVDLG_FRIENDS) {
                            copy_text(selected[selected_count], sizeof(selected[0]),
                                      friends[i].npid);
                            selected_count++;
                        } else if (!checked && at >= 0) {
                            memmove(selected[at], selected[at + 1],
                                    sizeof(selected[0]) * (size_t)(selected_count - at - 1));
                            selected_count--;
                        }
                    }
                    ovl_pop_id();
                }
            }
            ovl_end_child();
        }
    }
    if (p->message[0]) {
        ovl_spacing();
        sprintf(line, "Message: %s", p->message);
        ovl_text_wrapped(line, NULL);
    }
}

static void draw_recv_body(void) {
    char line[NPID_TEXT + 64];
    int i;
    if (invite_count == 0) {
        ovl_text_wrapped("You have no pending invitations.", NULL);
        return;
    }
    sprintf(line, "You have %d pending invitation(s):", invite_count);
    ovl_text_wrapped(line, NULL);
    ovl_spacing();
    if (ovl_begin_child("##pending_invitations", vec2(0.0f, 120.0f), OVL_CHILD_BORDERS, 0)) {
        for (i = 0; i < invite_count; i++) {
            ovl_push_id_int(i);
            sprintf(line, "%s invited you to a game session.",
                    invites[i].from[0] ? invites[i].from : "(unknown)");
            if (ovl_selectable(line, recv_selected == i, vec2(0.0f, 0.0f)))
                recv_selected = i;
            ovl_pop_id();
        }
    }
    ovl_end_child();
    ovl_spacing();
    ovl_text_disabled("Accept to join the selected session, or decline to dismiss it.", NULL);
}

static void draw_send_buttons(const inv_param_t* p, int first, ovl_vec2_t ws, float y) {
    const int no_recipients = p->user_editable && selected_count == 0;
    const int was_busy = busy;
    ovl_set_cursor_pos(vec2(ws.x / 2.0f - BUTTON_W - 10.0f, y));
    ovl_begin_disabled(no_recipients || was_busy);
    if (ovl_button(was_busy ? "Sending..." : "Send", vec2(BUTTON_W, BUTTON_H))) {
        /* The friends ticked, or the ones the game fixed when it opened the dialog */
        const char* to[OVL_INVDLG_FRIENDS + OVL_INVDLG_FIXED];
        int n = 0, i;
        sending_count = 0;
        if (p->user_editable)
            for (i = 0; i < selected_count; i++) {
                memcpy(sending[sending_count], selected[i], sizeof(sending[0]));
                to[n++] = sending[sending_count++];
            }
        for (i = 0; i < p->fixed_count; i++)
            to[n++] = p->fixed_to[i];
        ovl_invitation_dialog_send(p->user_id, p->session_id, p->message, to, n, new_request());
    }
    ovl_end_disabled();
    if (first)
        ovl_set_item_current_nav_focus(0);
    ovl_same_line(0.0f, -1.0f);
    ovl_set_cursor_pos(vec2(ws.x / 2.0f + 10.0f, y));
    ovl_begin_disabled(was_busy);
    if (ovl_button("Cancel", vec2(BUTTON_W, BUTTON_H)))
        finish(OVL_INVDLG_RESULT_USER_CANCELED, 0);
    ovl_end_disabled();
}

static void draw_recv_buttons(const inv_param_t* p, int first, ovl_vec2_t ws, float y) {
    const int no_selection = recv_selected < 0 || recv_selected >= invite_count;
    const int was_busy = busy;
    float x = ws.x / 2.0f - (BUTTON_W * 3.0f + 20.0f) / 2.0f;
    ovl_set_cursor_pos(vec2(x, y));
    ovl_begin_disabled(no_selection || was_busy);
    if (ovl_button(was_busy ? "Accepting..." : "Accept", vec2(BUTTON_W, BUTTON_H))) {
        memcpy(busy_invite, invites[recv_selected].id, sizeof(busy_invite));
        ovl_invitation_dialog_accept(p->user_id, busy_invite, new_request());
    }
    ovl_end_disabled();
    if (first && !no_selection)
        ovl_set_item_current_nav_focus(0);
    ovl_same_line(0.0f, -1.0f);
    x += BUTTON_W + 10.0f;
    ovl_set_cursor_pos(vec2(x, y));
    ovl_begin_disabled(no_selection || was_busy);
    if (ovl_button("Decline", vec2(BUTTON_W, BUTTON_H))) {
        ovl_invitation_dialog_decline(p->user_id, invites[recv_selected].id);
        remove_invite(recv_selected);
        if (invite_count == 0)
            finish(OVL_INVDLG_RESULT_USER_CANCELED, 0);
    }
    ovl_end_disabled();
    ovl_same_line(0.0f, -1.0f);
    x += BUTTON_W + 10.0f;
    ovl_set_cursor_pos(vec2(x, y));
    ovl_begin_disabled(was_busy);
    if (ovl_button("Cancel", vec2(BUTTON_W, BUTTON_H)))
        finish(OVL_INVDLG_RESULT_USER_CANCELED, 0);
    ovl_end_disabled();
    if (first && no_selection)
        ovl_set_item_current_nav_focus(0);
}

void ovl_invitation_dialog_draw(void) {
    const inv_param_t* p;
    ovl_vec2_t display, pos, size, pivot;
    int first, send_mode;
    if (retro_atomic_load_acquire_int(&status) != RUNNING)
        return;
    if (retro_atomic_load_acquire_int(&middle) & DIRTY)
        front = retro_atomic_exchange_int(&middle, front) & ~DIRTY;
    p = &bufs[front];
    /* Its first frame, as a new InvitationDialogUi's: nothing chosen yet */
    first = !drawn || p->gen != drawn_gen;
    drawn = 1;
    drawn_gen = p->gen;
    if (first) {
        memcpy(invites, p->invites, sizeof(invites[0]) * (size_t)p->invite_count);
        invite_count = p->invite_count;
        recv_selected = invite_count ? 0 : -1;
        selected_count = 0;
        sending_count = 0;
        filter[0] = '\0';
        busy = 0;
    }
    send_mode = p->mode == OVL_INVDLG_MODE_SEND;

    /* A request answered */
    if (busy) {
        const int r = retro_atomic_load_acquire_int(&request);
        if (r == (int)((request_seq << 2) | 2u) || r == (int)((request_seq << 2) | 3u)) {
            const int ok = r & 1;
            busy = 0;
            if (send_mode) {
                /* Sent or not, the dialog is done; the game is told who only if it was */
                finish(OVL_INVDLG_RESULT_OK, ok);
                return;
            }
            if (ok) {
                finish(OVL_INVDLG_RESULT_OK, 0);
                return;
            } else {
                /* Not to be had any more (expired, used): off the list */
                int i;
                for (i = 0; i < invite_count; i++)
                    if (strcmp(invites[i].id, busy_invite) == 0) {
                        remove_invite(i);
                        break;
                    }
                recv_selected = invite_count ? 0 : -1;
                if (invite_count == 0) {
                    finish(OVL_INVDLG_RESULT_USER_CANCELED, 0);
                    return;
                }
            }
        }
    }

    display = ovl_display_size();
    size.x = display.x < 500.0f ? display.x : 500.0f;
    size.y = display.y < 320.0f ? display.y : 320.0f;
    pos.x = display.x / 2.0f;
    pos.y = display.y / 2.0f;
    pivot.x = pivot.y = 0.5f;
    ovl_set_next_window_pos(pos, OVL_COND_ALWAYS, pivot);
    ovl_set_next_window_size(size, OVL_COND_ALWAYS);
    if (first || !ovl_nav_active())
        ovl_set_next_window_focus();
    ovl_keep_nav_highlight();
    if (ovl_begin("Invitation Dialog##InvitationDialog",
                  OVL_WINDOW_NO_DECORATION | OVL_WINDOW_NO_SAVED_SETTINGS)) {
        ovl_vec2_t ws;
        float y;
        ovl_draw_pretty_background();
        ovl_set_window_font_scale(1.3f);
        ovl_text(send_mode ? "Send Invitation" : "Invitations", NULL);
        ovl_set_window_font_scale(1.0f);
        ovl_separator();
        ovl_spacing();
        if (send_mode)
            draw_send_body(p);
        else
            draw_recv_body();
        ws = ovl_window_size();
        y = ws.y - 10.0f - BUTTON_H;
        if (send_mode)
            draw_send_buttons(p, first, ws, y);
        else
            draw_recv_buttons(p, first, ws, y);
    }
    ovl_end();
}
