/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The store's dialog and icon across threads (video_core/overlay/
 * ovl_commerce.c), as the core has them: a game thread opening dialogs in
 * every mode with up to ten targets, closing every third itself, and showing,
 * moving and hiding the icon; the overlay's thread drawing frames and pressing
 * cross. Under ThreadSanitizer: nothing read while written. Every dialog
 * finishes once, by one side, with that side's result.
 *
 *   build-tests/ovl_commerce_test [NotoSans-Regular.ttf] */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <retro_atomic.h>
#include <rthreads/rthreads.h>

#include "../../src/core/emulator_settings.h"
#include "../../src/video_core/overlay/ovl.h"
#include "../../src/video_core/overlay/ovl_layers.h"
#include "../audio_mix.h"
#include "ovl_invdialog_stubs.h"

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

#define DIALOGS 300

static retro_atomic_int_t game_done;
static retro_atomic_int_t failures; /* from both threads */
static int closed_by_game, closed_by_overlay;

#define CHECK(cond, what)                                                      \
  do {                                                                         \
    if (!(cond)) {                                                             \
      printf("FAIL %s (line %d)\n", what, __LINE__);                           \
      retro_atomic_fetch_add_int(&failures, 1);                                \
    }                                                                          \
  } while (0)

static void game_thread(void *arg) {
  ovl_commerce_dialog_param_t p;
  char labels[OVL_COMMERCE_TARGETS + 2][160];
  const char *targets[OVL_COMMERCE_TARGETS + 2];
  char name[32];
  int i, t;
  (void)arg;
  ovl_commerce_dialog_set_status(1);
  for (i = 0; i < DIALOGS; i++) {
    int step = 0, result;
    memset(&p, 0, sizeof(p));
    p.mode = i % 6;
    p.user_id = i;
    sprintf(name, "User%d", i);
    p.username = i & 1 ? name : NULL;
    p.service_label = (unsigned)i;
    p.features = (uint64_t)(i % 3);
    /* More than it takes, longer than it takes */
    p.target_count = i % (OVL_COMMERCE_TARGETS + 3);
    for (t = 0; t < OVL_COMMERCE_TARGETS + 2; t++) {
      memset(labels[t], 'A' + t, sizeof(labels[t]) - 1);
      labels[t][sizeof(labels[t]) - 1] = '\0';
      if (t & 1)
        sprintf(labels[t], "%d:PRODUCT%d-SKU%d", t, i, t);
      targets[t] = labels[t];
    }
    p.targets = targets;
    ovl_commerce_dialog_open(&p);
    /* Its texts are copied: changed here as soon as it is opened */
    memset(labels, 'x', sizeof(labels));
    memset(name, 'x', sizeof(name) - 1);
    ovl_store_icon_set_layout(i % 3);
    if (i % 5 == 4)
      ovl_store_icon_hide();
    else
      ovl_store_icon_show(i % 3);
    while (ovl_commerce_dialog_status() == 2) {
      if (i % 3 == 0 && step == 50)
        ovl_commerce_dialog_close(OVL_COMMERCE_RESULT_OK);
      step++;
    }
    CHECK(ovl_commerce_dialog_status() == 3, "finished");
    result = ovl_commerce_dialog_result();
    /* Close has the focus: a press never buys */
    if (result == OVL_COMMERCE_RESULT_USER_CANCELED)
      closed_by_overlay++;
    else if (result == OVL_COMMERCE_RESULT_OK)
      closed_by_game++;
    else
      CHECK(0, "a result one side gave");
    CHECK(!ovl_commerce_dialog_close(OVL_COMMERCE_RESULT_OK), "finished once");
  }
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
      argc > 1 ? argv[1] : "src/imgui/renderer/fonts/NotoSans-Regular.ttf";
  size_t size;
  unsigned char *noto = read_file(path, &size);
  ovl_face_desc_t face;
  ovl_font_t *font;
  sthread_t *game;
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
  ovl_commerce_dialog_init();
  ovl_set_gamepad_connected(1);
  game = sthread_create(game_thread, NULL);
  /* Frames until the game is done, cross pressed every fourth */
  while (!retro_atomic_load_acquire_int(&game_done)) {
    ovl_key_event(OVL_KEY_PAD_FACE_DOWN, (frame & 3) == 2);
    ovl_new_frame(1280.0f, 720.0f, 1.0f / 60.0f);
    ovl_commerce_dialog_draw();
    ovl_store_icon_draw();
    ovl_render();
    frame++;
  }
  sthread_join(game);
  CHECK(closed_by_game + closed_by_overlay == DIALOGS,
        "every dialog finished once");
  CHECK(closed_by_overlay > 0 && closed_by_game > 0, "by both sides");
  ovl_commerce_dialog_shutdown();
  CHECK(!ovl_store_icon_active(), "the icon hidden with it");
  ovl_destroy();
  free(noto);
  if (retro_atomic_load_acquire_int(&failures)) {
    printf("%d failures\n", retro_atomic_load_acquire_int(&failures));
    return 1;
  }
  printf("ovl_commerce_test: %d dialogs (%d closed by the game, %d closed on "
         "screen), in %u frames\n",
         DIALOGS, closed_by_game, closed_by_overlay, frame);
  return 0;
}
