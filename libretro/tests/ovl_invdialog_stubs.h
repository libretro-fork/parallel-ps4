/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* What the invitation dialog (video_core/overlay/ovl_invdialog.c) asks of the
 * core, for a test that links the overlays and does not open it: no friends,
 * nothing sent. Included once, by the test's one file. */

#ifndef OVL_INVDIALOG_STUBS_H
#define OVL_INVDIALOG_STUBS_H

#include "../../src/video_core/overlay/ovl_layers.h"

int ovl_invitation_dialog_friends(int user_id, ovl_invdlg_friend_t *out, int max) {
  (void)user_id;
  (void)out;
  (void)max;
  return 0;
}
void ovl_invitation_dialog_send(int user_id, const char *session_id,
                                const char *message, const char *const *to,
                                int count, unsigned token) {
  (void)user_id;
  (void)session_id;
  (void)message;
  (void)to;
  (void)count;
  (void)token;
}
void ovl_invitation_dialog_accept(int user_id, const char *invitation_id,
                                  unsigned token) {
  (void)user_id;
  (void)invitation_id;
  (void)token;
}
void ovl_invitation_dialog_decline(int user_id, const char *invitation_id) {
  (void)user_id;
  (void)invitation_id;
}

#endif
