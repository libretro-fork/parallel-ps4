/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The overlays' queues from many threads at once (ovl_notify.c, ovl_tex.c):
 * four threads queue notifications with icons while the overlay's thread draws
 * frames and releases what it is done with. Every notification is shown, once,
 * each thread's in the order it queued them; none waits for another; every
 * image slot is free again at the end. Run under ThreadSanitizer and
 * AddressSanitizer.
 *
 *   build-tests/ovl_queue_test */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <retro_atomic.h>
#include <retro_timers.h>
#include <rthreads/rthreads.h>

#include "../../src/core/emulator_settings.h"
#include "../../src/video_core/overlay/ovl.h"
#include "../../src/video_core/overlay/ovl_layers.h"
#include "../audio_mix.h"
#include "ovl_invdialog_stubs.h"

#define THREADS 4
#define EACH 12

static int failures = 0;

#define CHECK(cond, what)                                                      \
  do {                                                                         \
    if (!(cond)) {                                                             \
      printf("FAIL %s (line %d)\n", what, __LINE__);                           \
      ++failures;                                                              \
    }                                                                          \
  } while (0)

bool emu_settings_get_show_fps_counter(void) { return true; }
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

static unsigned char *icon;
static size_t icon_size;
static retro_atomic_int_t queued;

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

static void producer(void *arg) {
  const int t = (int)(size_t)arg;
  int i = 0;
  while (i < EACH) {
    char message[32];
    sprintf(message, "t%d n%02d", t, i);
    /* With every slot waiting, the next is queued once one has shown */
    if (ovl_notify_queue(message, 0.05f, OVL_NOTIFY_TOP_LEFT, icon, icon_size,
                         i & 1)) {
      retro_atomic_fetch_add_int(&queued, 1);
      i++;
    } else {
      retro_sleep(0);
    }
  }
}

/* What each frame shows: the notification's message, read from the frame's text
 */
static int last_seen[THREADS];
static int shown;

static void record(const char *message) {
  int t, n;
  if (sscanf(message, "t%d n%d", &t, &n) != 2 || t < 0 || t >= THREADS) {
    CHECK(0, "a notification that was queued");
    return;
  }
  if (n == last_seen[t])
    return; /* still showing */
  if (n != last_seen[t] + 1)
    printf("thread %d: %d after %d\n", t, n, last_seen[t]);
  CHECK(n == last_seen[t] + 1,
        "each thread's notifications in the order it queued them");
  last_seen[t] = n;
  shown++;
}

int main(void) {
  static uint8_t slots[OVL_TEX_SLOTS][OVL_TEX_DIM * OVL_TEX_DIM * 4];
  uint8_t *pixels[OVL_TEX_SLOTS];
  unsigned char *font;
  size_t font_size;
  ovl_face_desc_t face;
  sthread_t *threads[THREADS];
  unsigned frame = 0;
  int i;

  font = read_file("src/resources/fonts/NotoSans-Regular.ttf", &font_size);
  icon = read_file("src/resources/big_picture/settings.png", &icon_size);
  if (!font || !icon) {
    printf("FAIL the font and icon\n");
    return 1;
  }
  face.data = font;
  face.size = font_size;
  face.index = 0;
  if (!ovl_create(ovl_font_add(&face, 1, 32.0f), 0.5f)) {
    printf("FAIL the overlay\n");
    return 1;
  }
  for (i = 0; i < OVL_TEX_SLOTS; i++)
    pixels[i] = slots[i];
  ovl_tex_init(pixels, OVL_TEX_SLOTS);
  for (i = 0; i < THREADS; i++)
    last_seen[i] = -1;

  for (i = 0; i < THREADS; i++)
    threads[i] = sthread_create(producer, (void *)(size_t)i);
  /* The overlay's thread: frames until every notification has shown */
  while (shown < THREADS * EACH && frame < 100000) {
    const ovl_draw_data_t *dd;
    ovl_new_frame(1920.0f, 1080.0f, 1.0f / 60.0f);
    ovl_notify_draw();
    dd = ovl_render();
    while (ovl_tex_take_upload()) {
    }
    ovl_tex_collect(frame++, 3);
    (void)dd;
    if (ovl_notify_showing_message())
      record(ovl_notify_showing_message());
  }
  for (i = 0; i < THREADS; i++)
    sthread_join(threads[i]);
  /* The last ones out, and their slots */
  for (i = 0; i < 600; i++) {
    ovl_new_frame(1920.0f, 1080.0f, 1.0f / 60.0f);
    ovl_notify_draw();
    ovl_render();
    ovl_tex_collect(frame++, 3);
    if (ovl_notify_showing_message())
      record(ovl_notify_showing_message());
  }
  CHECK(retro_atomic_load_acquire_int(&queued) == THREADS * EACH,
        "every notification queued");
  CHECK(shown == THREADS * EACH, "every notification shown once");
  CHECK(!ovl_notify_active(), "none left");
  {
    int handles[OVL_TEX_SLOTS + 1], n = 0;
    while (n <= OVL_TEX_SLOTS &&
           (handles[n] = ovl_tex_decode_png(icon, icon_size)) != 0)
      n++;
    CHECK(n == OVL_TEX_SLOTS, "every image's slot free again");
    for (i = 0; i < n; i++)
      ovl_tex_release(handles[i]);
  }
  ovl_tex_shutdown();
  ovl_destroy();
  free(font);
  free(icon);
  if (failures) {
    printf("ovl_queue_test: %d failures\n", failures);
    return 1;
  }
  printf("ovl_queue_test: %d notifications from %d threads, each once and in "
         "order\n",
         shown, THREADS);
  return 0;
}
