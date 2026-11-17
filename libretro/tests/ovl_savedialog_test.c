/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The save data dialog across threads (video_core/overlay/ovl_savedialog.c), as
 * the core has it: a game thread building lists of saves where the dialog says
 * to, their icons decoded into the icon atlas, opening them, a progress bar's
 * value changed while it shows, some closed by the game itself, the last
 * dialog's icons let go at the next; the overlay's thread drawing frames,
 * pressing down and cross, taking the icons to upload (reading their pixels, as
 * the renderer copies them) and retiring those let go. Under ThreadSanitizer:
 * nothing read while written, the saves' texts and icons included. Every dialog
 * finishes once, by one side, with that side's result, a save chosen being one
 * of its list; every icon cell free after.
 *
 *   build-tests/ovl_savedialog_test [NotoSans-Regular.ttf] [icon.png] */

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
#define MAX_ITEMS 6

/* What the game's thread builds for a dialog, one for each of the dialog's
 * buffers */
typedef struct storage {
  char text[MAX_ITEMS][4][48];
  ovl_save_item_t items[MAX_ITEMS]; /* what the overlay reads */
  int icons[MAX_ITEMS];             /* the game's own, let go from here */
  int count;
} storage_t;

static storage_t storages[3];
static unsigned char *png;
static size_t png_size;
static retro_atomic_int_t game_done;
static retro_atomic_int_t failures; /* from both threads */
static int closed_by_game, chosen, cancelled;

#define CHECK(cond, what)                                                      \
  do {                                                                         \
    if (!(cond)) {                                                             \
      printf("FAIL %s (line %d)\n", what, __LINE__);                           \
      retro_atomic_fetch_add_int(&failures, 1);                                \
    }                                                                          \
  } while (0)

/* The overlay may still be drawing a dialog just finished: its items are not
 * written, only the game's own record of the icons */
static void release_icons(storage_t *s) {
  int i;
  for (i = 0; i < s->count; i++)
    if (s->icons[i]) {
      ovl_tex_release(s->icons[i]);
      s->icons[i] = 0;
    }
}

static void game_thread(void *arg) {
  ovl_save_dialog_param_t p;
  int opened = -1, i;
  (void)arg;
  ovl_save_dialog_set_status(1);
  for (i = 0; i < DIALOGS; i++) {
    const int back = ovl_save_dialog_back();
    storage_t *s = &storages[back];
    int j, button, result, item, spins = 0;
    CHECK(back != opened, "never the buffer just opened");
    /* The last dialog's icons go, then what was built here before */
    if (opened >= 0)
      release_icons(&storages[opened]);
    release_icons(s);
    s->count = 1 + i % MAX_ITEMS;
    for (j = 0; j < s->count; j++) {
      ovl_save_item_t *it = &s->items[j];
      sprintf(s->text[j][0], "Save %d of dialog %d", j, i);
      sprintf(s->text[j][1], "Subtitle %d", i * 7 + j);
      sprintf(s->text[j][2], "%d Nov, 2026", 1 + (i + j) % 28);
      sprintf(s->text[j][3], "%d KB", 100 + j);
      it->title = s->text[j][0];
      it->subtitle = s->text[j][1];
      it->date = s->text[j][2];
      it->size = s->text[j][3];
      it->icon = s->icons[j] = ovl_tex_decode_icon(png, png_size);
      it->corrupted = j == 2;
    }
    memset(&p, 0, sizeof(p));
    p.mode = i % 4 == 3 ? OVL_SAVE_MODE_PROGRESS_BAR : OVL_SAVE_MODE_LIST;
    p.type = OVL_SAVE_TYPE_LOAD;
    p.enable_back = -1;
    p.items = s->items;
    p.item_count = s->count;
    p.focus = i % s->count;
    p.msg = "Loading...";
    opened = ovl_save_dialog_open(&p);
    CHECK(opened == back, "opened from the buffer built");
    while (ovl_save_dialog_status() == 2) {
      if (p.mode == OVL_SAVE_MODE_PROGRESS_BAR) {
        ovl_save_dialog_set_progress((unsigned)(spins % 101));
        ovl_save_dialog_add_progress(1);
      }
      /* Every fifth closed here, and every progress bar (it has no button) */
      if ((i % 5 == 0 || p.mode == OVL_SAVE_MODE_PROGRESS_BAR) && spins == 200)
        ovl_save_dialog_close(OVL_SAVE_BUTTON_INVALID, OVL_SAVE_RESULT_OK);
      spins++;
    }
    CHECK(ovl_save_dialog_status() == 3, "finished");
    ovl_save_dialog_result(&button, &result, &item);
    CHECK(button == OVL_SAVE_BUTTON_INVALID, "no button for a list");
    if (item >= 0) {
      CHECK(item < s->count && result == OVL_SAVE_RESULT_OK,
            "a save of its list chosen");
      chosen++;
    } else if (result == OVL_SAVE_RESULT_USER_CANCELED) {
      cancelled++;
    } else {
      CHECK(result == OVL_SAVE_RESULT_OK, "a result one side gave");
      closed_by_game++;
    }
  }
  release_icons(&storages[opened]);
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

/* The renderer's part: icons taken to upload, their pixels read as it copies
 * them; those let go retired */
static unsigned upload(const unsigned char *atlas, unsigned frame) {
  unsigned sum = 0;
  int handle, r[4], x, y, c;
  while ((handle = ovl_tex_take_upload()) != 0)
    if (ovl_tex_icon_rect(handle, r))
      for (y = r[1]; y < r[1] + r[3]; y++)
        for (x = r[0]; x < r[0] + r[2]; x++)
          for (c = 0; c < 4; c++)
            sum += atlas[((size_t)y * OVL_ICON_ATLAS_W + (size_t)x) * 4 +
                         (size_t)c];
  ovl_tex_collect(frame, 3);
  return sum;
}

int main(int argc, char **argv) {
  const char *path =
      argc > 1 ? argv[1] : "src/imgui/renderer/fonts/NotoSans-Regular.ttf";
  const char *icon_path =
      argc > 2 ? argv[2] : "src/resources/big_picture/settings.png";
  size_t size;
  unsigned char *noto = read_file(path, &size);
  unsigned char *atlas;
  ovl_face_desc_t face;
  ovl_font_t *font;
  sthread_t *game;
  unsigned frame = 0, sum = 0;
  int handles[OVL_ICON_CELLS], i;
  png = read_file(icon_path, &png_size);
  if (!noto || !png) {
    printf("FAIL no font at %s or icon at %s\n", path, icon_path);
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
  atlas =
      (unsigned char *)calloc((size_t)OVL_ICON_ATLAS_W * OVL_ICON_ATLAS_H, 4);
  ovl_tex_init_icons(atlas);
  ovl_save_dialog_init();
  ovl_set_gamepad_connected(1);
  game = sthread_create(game_thread, NULL);
  /* Frames until it is done: down pressed now and then, cross every eighth */
  while (!retro_atomic_load_acquire_int(&game_done)) {
    ovl_key_event(OVL_KEY_PAD_DPAD_DOWN, (frame % 16) == 5);
    ovl_key_event(OVL_KEY_PAD_FACE_DOWN, (frame & 7) == 2);
    ovl_key_event(OVL_KEY_PAD_FACE_RIGHT, (frame % 64) == 30);
    ovl_new_frame(1280.0f, 720.0f, 1.0f / 60.0f);
    ovl_save_dialog_draw();
    ovl_render();
    sum += upload(atlas, frame);
    frame++;
  }
  sthread_join(game);
  CHECK(closed_by_game + chosen + cancelled == DIALOGS,
        "every dialog finished once");
  CHECK(chosen > 0 && closed_by_game > 0, "by both sides");
  /* The icons let go retire; then every cell is free again */
  for (i = 0; i < 4; i++)
    upload(atlas, frame++);
  for (i = 0; i < OVL_ICON_CELLS; i++) {
    handles[i] = ovl_tex_decode_icon(png, png_size);
    CHECK(handles[i] > OVL_TEX_SLOTS, "a cell free for each icon");
  }
  for (i = 0; i < OVL_ICON_CELLS; i++)
    ovl_tex_release(handles[i]);
  ovl_save_dialog_shutdown();
  ovl_destroy();
  free(atlas);
  free(png);
  free(noto);
  if (retro_atomic_load_acquire_int(&failures)) {
    printf("%d failures\n", retro_atomic_load_acquire_int(&failures));
    return 1;
  }
  printf("ovl_savedialog_test: %d dialogs (%d saves chosen, %d cancelled, %d "
         "closed by "
         "the game), in %u frames, icon bytes read %u\n",
         DIALOGS, chosen, cancelled, closed_by_game, frame, sum);
  return 0;
}
