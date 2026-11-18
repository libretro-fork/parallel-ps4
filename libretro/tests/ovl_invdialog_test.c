/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The invitation dialog across threads (video_core/overlay/ovl_invdialog.c),
 * as the core has it: a game thread opening dialogs to send (the game's own
 * recipients) and to receive (two invitations waiting), closing every fourth
 * itself; the overlay's thread drawing frames and pressing cross, which sends
 * or accepts; a server thread answering each request a little later, well or
 * not, and answering some twice (the second, for a request no longer waited
 * for, changes nothing). Under ThreadSanitizer: nothing read while written.
 * Every dialog finishes once, by one side; a refused acceptance takes its
 * invitation off the list and the next press accepts the other.
 *
 *   build-tests/ovl_invdialog_test [NotoSans-Regular.ttf] */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <retro_atomic.h>
#include <rthreads/rthreads.h>

#include "../../src/core/emulator_settings.h"
#include "../../src/video_core/overlay/ovl.h"
#include "../../src/video_core/overlay/ovl_layers.h"
#include "../audio_mix.h"

bool emu_settings_get_show_fps_counter(void) { return false; }
bool emu_settings_get_trophy_popup_disabled(void) { return false; }
const char *emu_settings_get_trophy_notification_side(void) { return "right"; }
double emu_settings_get_trophy_notification_duration(void) { return 6.0; }
int emu_settings_get_volume_slider(void) { return 100; }
void audio_mix_play_clip(const float *frames, unsigned count, float gain) {
  (void)frames;
  (void)count;
  (void)gain;
}
void ovl_invite_chosen(int user_id, const char *invitation_id, const char *from_npid,
                       int accept) {
  (void)user_id;
  (void)invitation_id;
  (void)from_npid;
  (void)accept;
}

#define DIALOGS 200

static retro_atomic_int_t game_done;
static retro_atomic_int_t failures; /* from every thread */
static int closed_by_game, finished_on_screen;
/* The requests the overlay's thread made, for the server's: a number each */
static retro_atomic_int_t asked;    /* the latest request's token, 0 for none */
static retro_atomic_int_t requests; /* how many were made */

#define CHECK(cond, what)                                                      \
  do {                                                                         \
    if (!(cond)) {                                                             \
      printf("FAIL %s (line %d)\n", what, __LINE__);                           \
      retro_atomic_fetch_add_int(&failures, 1);                                \
    }                                                                          \
  } while (0)

int ovl_invitation_dialog_friends(int user_id, ovl_invdlg_friend_t *out, int max) {
  (void)user_id;
  (void)max;
  strcpy(out[0].npid, "Kestrel");
  out[0].online = 1;
  return 1;
}
void ovl_invitation_dialog_send(int user_id, const char *session_id,
                                const char *message, const char *const *to,
                                int count, unsigned token) {
  (void)user_id;
  CHECK(strncmp(session_id, "session-", 8) == 0 && message[0] == 'm' &&
            count == 2 && strcmp(to[0], "Kestrel") == 0 &&
            strcmp(to[1], "Moth") == 0,
        "sent as it was opened");
  retro_atomic_fetch_add_int(&requests, 1);
  retro_atomic_store_release_int(&asked, (int)token);
}
void ovl_invitation_dialog_accept(int user_id, const char *invitation_id,
                                  unsigned token) {
  (void)user_id;
  CHECK(strncmp(invitation_id, "inv-", 4) == 0, "one of those waiting");
  retro_atomic_fetch_add_int(&requests, 1);
  retro_atomic_store_release_int(&asked, (int)token);
}
void ovl_invitation_dialog_decline(int user_id, const char *invitation_id) {
  (void)user_id;
  (void)invitation_id;
}

/* The server: each request answered, every third refused, every fifth
 * answered a second time */
static void server_thread(void *arg) {
  unsigned answered = 0;
  (void)arg;
  while (!retro_atomic_load_acquire_int(&game_done)) {
    const int token = retro_atomic_exchange_int(&asked, 0);
    if (token) {
      const int ok = answered % 3 != 2;
      ovl_invitation_dialog_request_done((unsigned)token, ok);
      if (answered % 5 == 4)
        ovl_invitation_dialog_request_done((unsigned)token, !ok);
      answered++;
    }
  }
}

static void game_thread(void *arg) {
  static const char *const fixed[] = {"Kestrel", "Moth"};
  ovl_invdlg_invite_t invites[2];
  ovl_invitation_dialog_param_t p;
  char session[32], message[32], id0[16], id1[16];
  int i;
  (void)arg;
  ovl_invitation_dialog_set_status(1);
  for (i = 0; i < DIALOGS; i++) {
    int step = 0, result;
    const char *sent[OVL_INVDLG_SENT];
    memset(&p, 0, sizeof(p));
    p.user_id = 1;
    if (i & 1) {
      p.mode = OVL_INVDLG_MODE_RECV;
      sprintf(id0, "inv-%d-a", i);
      sprintf(id1, "inv-%d-b", i);
      invites[0].invitation_id = id0;
      invites[0].from_npid = "Lantern_99";
      invites[1].invitation_id = id1;
      invites[1].from_npid = "";
      p.invites = invites;
      p.invite_count = 2;
    } else {
      p.mode = OVL_INVDLG_MODE_SEND;
      sprintf(session, "session-%d", i);
      sprintf(message, "message %d", i);
      p.session_id = session;
      p.message = message;
      p.fixed_to = fixed;
      p.fixed_count = 2;
    }
    ovl_invitation_dialog_open(&p);
    /* Its texts are copied: changed here as soon as it is opened */
    memset(id0, 'x', sizeof(id0) - 1);
    memset(id1, 'x', sizeof(id1) - 1);
    while (ovl_invitation_dialog_status() == 2) {
      if (i % 4 == 0 && step == 200)
        ovl_invitation_dialog_close(OVL_INVDLG_RESULT_OK);
      step++;
    }
    CHECK(ovl_invitation_dialog_status() == 3, "finished");
    result = ovl_invitation_dialog_result();
    CHECK(result == OVL_INVDLG_RESULT_OK ||
              result == OVL_INVDLG_RESULT_USER_CANCELED,
          "a result one side gave");
    /* The game's own recipients: none of the player's reported */
    CHECK(ovl_invitation_dialog_sent(sent, OVL_INVDLG_SENT) <= 0,
          "no friends ticked: none reported");
    if (!ovl_invitation_dialog_close(OVL_INVDLG_RESULT_OK))
      finished_on_screen++;
    CHECK(!ovl_invitation_dialog_close(OVL_INVDLG_RESULT_OK), "finished once");
  }
  closed_by_game = DIALOGS - finished_on_screen;
  retro_atomic_store_release_int(&game_done, 1);
}

static unsigned char *read_file(const char *path, size_t *size) {
  FILE *f = fopen(path, "rb");
  unsigned char *data;
  long n;
  if (!f)
    return NULL;
  fseek(f, 0, SEEK_END);
  n = ftell(f);
  fseek(f, 0, SEEK_SET);
  data = (unsigned char *)malloc((size_t)n);
  if (data && fread(data, 1, (size_t)n, f) != (size_t)n) {
    free(data);
    data = NULL;
  }
  fclose(f);
  *size = (size_t)n;
  return data;
}

int main(int argc, char **argv) {
  const char *path =
      argc > 1 ? argv[1] : "src/resources/fonts/NotoSans-Regular.ttf";
  size_t size;
  unsigned char *noto = read_file(path, &size);
  ovl_face_desc_t face;
  ovl_font_t *font;
  sthread_t *game, *server;
  unsigned frame = 0;
  if (!noto) {
    printf("FAIL no font at %s\n", path);
    return 1;
  }
  face.data = noto;
  face.size = size;
  face.index = 0;
  font = ovl_font_add(&face, 1, 32.0f);
  if (!font || !ovl_create(font, 0.5f)) {
    printf("FAIL the overlay\n");
    return 1;
  }
  ovl_invitation_dialog_init();
  ovl_set_gamepad_connected(1);
  game = sthread_create(game_thread, NULL);
  server = sthread_create(server_thread, NULL);
  /* Frames until the game is done, cross pressed every fourth */
  while (!retro_atomic_load_acquire_int(&game_done)) {
    ovl_key_event(OVL_KEY_PAD_FACE_DOWN, (frame & 3) == 2);
    ovl_new_frame(1280.0f, 720.0f, 1.0f / 60.0f);
    ovl_invitation_dialog_draw();
    ovl_render();
    frame++;
  }
  sthread_join(game);
  sthread_join(server);
  CHECK(retro_atomic_load_acquire_int(&requests) > DIALOGS / 2,
        "sent and accepted by cross");
  ovl_invitation_dialog_shutdown();
  ovl_destroy();
  free(noto);
  if (retro_atomic_load_acquire_int(&failures)) {
    printf("%d failures\n", retro_atomic_load_acquire_int(&failures));
    return 1;
  }
  printf("ovl_invdialog_test: %d dialogs, %d requests answered, in %u "
         "frames\n",
         DIALOGS, retro_atomic_load_acquire_int(&requests), frame);
  return 0;
}
