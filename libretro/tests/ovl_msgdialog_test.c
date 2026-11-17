/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The message dialog across threads (video_core/overlay/ovl_msgdialog.c), as
 * the core has it: a game thread opening progress dialogs, changing their value
 * and message while they show and closing some itself; a second thread opening
 * the emulator's own dialog and waiting for it, as ShowMsgDialog does; the
 * overlay's thread drawing frames and pressing cross. Under ThreadSanitizer:
 * nothing read while written. Every dialog finishes once, by one side, with
 * that side's result; every wait returns, with the dialog pressed.
 *
 *   build-tests/ovl_msgdialog_test [NotoSans-Regular.ttf] */

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

#define GAME_DIALOGS 300
#define EMULATOR_DIALOGS 40

static retro_atomic_int_t threads_done;
static retro_atomic_int_t failures; /* from every thread */
static int closed_by_game, closed_by_overlay, waits_pressed;

#define CHECK(cond, what)                                                      \
  do {                                                                         \
    if (!(cond)) {                                                             \
      printf("FAIL %s (line %d)\n", what, __LINE__);                           \
      retro_atomic_fetch_add_int(&failures, 1);                                \
    }                                                                          \
  } while (0)

static void game_thread(void *arg) {
  ovl_msg_dialog_param_t p;
  char msg[64];
  int i;
  (void)arg;
  memset(&p, 0, sizeof(p));
  p.mode = OVL_MSG_MODE_PROGRESS_BAR;
  p.bar_type = OVL_MSG_BAR_PERCENTAGE_CANCEL;
  ovl_msg_dialog_set_status(OVL_MSG_DIALOG_GAME, 1);
  for (i = 0; i < GAME_DIALOGS; i++) {
    int step = 0, button, result;
    sprintf(msg, "Dialog %d", i);
    p.msg = msg;
    ovl_msg_dialog_open(OVL_MSG_DIALOG_GAME, &p);
    /* Its value and message changed while it shows; every third closed here */
    while (ovl_msg_dialog_status(OVL_MSG_DIALOG_GAME) == 2) {
      sprintf(msg, "Dialog %d, step %d", i, step);
      ovl_msg_dialog_set_message(OVL_MSG_DIALOG_GAME, msg);
      ovl_msg_dialog_set_progress(OVL_MSG_DIALOG_GAME, (unsigned)(step % 101));
      ovl_msg_dialog_add_progress(OVL_MSG_DIALOG_GAME, 1);
      if (i % 3 == 0 && step == 50)
        ovl_msg_dialog_close(OVL_MSG_DIALOG_GAME, OVL_MSG_BUTTON_INVALID,
                             OVL_MSG_RESULT_OK);
      step++;
    }
    CHECK(ovl_msg_dialog_status(OVL_MSG_DIALOG_GAME) == 3, "finished");
    ovl_msg_dialog_result(OVL_MSG_DIALOG_GAME, &button, &result);
    CHECK(button == OVL_MSG_BUTTON_INVALID, "no button for a progress bar");
    if (result == OVL_MSG_RESULT_USER_CANCELED)
      closed_by_overlay++;
    else if (result == OVL_MSG_RESULT_OK)
      closed_by_game++;
    else
      CHECK(0, "a result one side gave");
  }
  retro_atomic_fetch_add_int(&threads_done, 1);
}

static void emulator_thread(void *arg) {
  ovl_msg_dialog_param_t p;
  int i;
  (void)arg;
  memset(&p, 0, sizeof(p));
  p.mode = OVL_MSG_MODE_USER;
  p.button_type = OVL_MSG_OK;
  p.msg = "The emulator's own";
  for (i = 0; i < EMULATOR_DIALOGS; i++) {
    int button, result;
    ovl_msg_dialog_open(OVL_MSG_DIALOG_EMULATOR, &p);
    ovl_msg_dialog_wait(OVL_MSG_DIALOG_EMULATOR);
    CHECK(ovl_msg_dialog_status(OVL_MSG_DIALOG_EMULATOR) == 3,
          "finished when the wait returns");
    ovl_msg_dialog_result(OVL_MSG_DIALOG_EMULATOR, &button, &result);
    CHECK(button == OVL_MSG_BUTTON_OK && result == OVL_MSG_RESULT_OK,
          "pressed OK");
    waits_pressed++;
  }
  retro_atomic_fetch_add_int(&threads_done, 1);
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
      argc > 1 ? argv[1] : "src/imgui/renderer/fonts/NotoSans-Regular.ttf";
  size_t size;
  unsigned char *noto = read_file(path, &size);
  ovl_face_desc_t face;
  ovl_font_t *font;
  sthread_t *game, *emulator;
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
  ovl_msg_dialog_init();
  ovl_set_gamepad_connected(1);
  game = sthread_create(game_thread, NULL);
  emulator = sthread_create(emulator_thread, NULL);
  /* Frames until both are done, cross pressed every fourth */
  while (retro_atomic_load_acquire_int(&threads_done) < 2) {
    ovl_key_event(OVL_KEY_PAD_FACE_DOWN, (frame & 3) == 2);
    ovl_new_frame(1280.0f, 720.0f, 1.0f / 60.0f);
    ovl_msg_dialog_draw();
    ovl_render();
    frame++;
  }
  sthread_join(game);
  sthread_join(emulator);
  CHECK(closed_by_game + closed_by_overlay == GAME_DIALOGS,
        "every game dialog finished once");
  CHECK(closed_by_overlay > 0 && closed_by_game > 0, "by both sides");
  CHECK(waits_pressed == EMULATOR_DIALOGS, "every wait returned");
  ovl_msg_dialog_shutdown();
  ovl_destroy();
  free(noto);
  if (retro_atomic_load_acquire_int(&failures)) {
    printf("%d failures\n", retro_atomic_load_acquire_int(&failures));
    return 1;
  }
  printf("ovl_msgdialog_test: %d game dialogs (%d closed by the game, %d "
         "cancelled on screen), %d waited for, in %u frames\n",
         GAME_DIALOGS, closed_by_game, closed_by_overlay, EMULATOR_DIALOGS,
         frame);
  return 0;
}
