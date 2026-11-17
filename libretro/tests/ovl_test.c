/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The overlays' library (src/video_core/overlay) against what ImGui drew: the
 * FPS counter's window, frame by frame (hidden the first frame while it is
 * measured, then placed and sized as ImGui placed and sized it), its glyphs
 * where ImGui put them, as many vertices and indices as ImGui made; the atlas's
 * white texel and baked lines, and what it hands the renderer. The expected
 * numbers are ImGui 1.92's, from the same font at the same sizes.
 *
 *   build-tests/ovl_test [NotoSans-Regular.ttf] */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/core/emulator_settings.h"
#include "../../src/video_core/overlay/ovl.h"
#include "../../src/video_core/overlay/ovl_layers.h"
#include "../audio_mix.h"

/* What the overlays ask of the core's settings and mix */
bool emu_settings_get_show_fps_counter(void) { return true; }
bool emu_settings_get_trophy_popup_disabled(void) { return false; }
const char *emu_settings_get_trophy_notification_side(void) { return "right"; }
double emu_settings_get_trophy_notification_duration(void) { return 6.0; }
int emu_settings_get_volume_slider(void) { return 100; }
static int clips;
void audio_mix_play_clip(const float *frames, unsigned count, float gain) {
  (void)frames;
  (void)count;
  (void)gain;
  clips++;
}

static int failures = 0;

#define CHECK(cond, what)                                                      \
  do {                                                                         \
    if (!(cond)) {                                                             \
      printf("FAIL %s (line %d)\n", what, __LINE__);                           \
      ++failures;                                                              \
    }                                                                          \
  } while (0)

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

/* The FPS counter as imgui/fps_layer.cpp drew it, at 17 frames a second */
static ovl_vec2_t window_pos, window_size;

static void fps_scene(void) {
  ovl_vec4_t orange;
  ovl_vec2_t pos;
  orange.x = 1.0f;
  orange.y = 0.5f;
  orange.z = 0.0f;
  orange.w = 1.0f;
  if (ovl_begin("Video Info", OVL_WINDOW_NO_NAV | OVL_WINDOW_NO_DECORATION |
                                  OVL_WINDOW_ALWAYS_AUTO_RESIZE |
                                  OVL_WINDOW_NO_INPUTS)) {
    pos.x = ovl_display_size().x - ovl_window_size().x;
    pos.y = 0.0f;
    ovl_set_window_pos(pos);
    window_pos = ovl_window_pos();
    window_size = ovl_window_size();
    ovl_push_color(OVL_COLOR_TEXT, orange);
    ovl_text("17 FPS (58.8 ms)", NULL);
    ovl_pop_color(1);
  }
  ovl_end();
}

/* ImGui's glyph quads for the line, at frame 2 */
static const float want_quads[13][4] = {
    {1822.750000f, 12.0f, 1826.750000f, 21.0f},
    {1828.969482f, 12.0f, 1835.469482f, 21.0f},
    {1839.243286f, 12.0f, 1844.743286f, 21.0f},
    {1845.340210f, 12.0f, 1851.840210f, 21.0f},
    {1851.947388f, 12.0f, 1857.947388f, 22.0f},
    {1860.951050f, 12.0f, 1864.951050f, 23.0f},
    {1864.975220f, 12.0f, 1871.475220f, 22.0f},
    {1871.694702f, 12.0f, 1878.194702f, 22.0f},
    {1878.414185f, 19.0f, 1880.914185f, 22.0f},
    {1881.562500f, 12.0f, 1888.062500f, 22.0f},
    {1891.336304f, 14.0f, 1901.836304f, 21.0f},
    {1902.320190f, 14.0f, 1907.820190f, 22.0f},
    {1907.447266f, 12.0f, 1911.447266f, 23.0f},
};

static void check_quads(const ovl_draw_data_t *dd) {
  const ovl_cmd_t *text = NULL;
  uint32_t i;
  int q;
  for (i = 0; i < dd->cmd_count; i++)
    if (dd->cmds[i].clip[0] == 1815.0f && dd->cmds[i].clip[1] == 1.0f &&
        dd->cmds[i].clip[2] == 1919.0f && dd->cmds[i].clip[3] == 31.0f)
      text = &dd->cmds[i];
  CHECK(text != NULL, "the text clipped to the window's inner rectangle");
  if (!text)
    return;
  CHECK(text->idx_count == 13 * 6, "a quad a visible glyph");
  if (text->idx_count != 13 * 6)
    return;
  for (q = 0; q < 13; q++) {
    float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
    int k;
    for (k = 0; k < 6; k++) {
      const ovl_vertex_t *v = &dd->vtx[dd->idx[text->idx_offset + q * 6 + k]];
      if (v->x < x0)
        x0 = v->x;
      if (v->y < y0)
        y0 = v->y;
      if (v->x > x1)
        x1 = v->x;
      if (v->y > y1)
        y1 = v->y;
      CHECK(v->col == OVL_COL(255, 128, 0, 255),
            "the text's colour, as ImGui packs it");
    }
    if (fabs(x0 - want_quads[q][0]) > 0.0005 || y0 != want_quads[q][1] ||
        fabs(x1 - want_quads[q][2]) > 0.0005 || y1 != want_quads[q][3]) {
      printf("glyph %d at %f %f %f %f, ImGui's %f %f %f %f\n", q, x0, y0, x1,
             y1, want_quads[q][0], want_quads[q][1], want_quads[q][2],
             want_quads[q][3]);
      CHECK(0, "a glyph where ImGui put it");
    }
  }
}

static void check_atlas(void) {
  const uint8_t *px = ovl_atlas_pixels();
  int rect[4], cleared = 0, n, i;
  CHECK(ovl_atlas_take_dirty(rect, &cleared) && cleared,
        "a new atlas starts cleared");
  CHECK(rect[0] == 0 && rect[1] == 0 && rect[2] > 0 && rect[3] > 0,
        "with its white texel and lines to upload");
  CHECK(!ovl_atlas_take_dirty(rect, &cleared) && !cleared,
        "and then nothing more");
  /* The white texel, placed */
  CHECK(px[(1 * OVL_ATLAS_SIZE + 1) * 2] == 255 &&
            px[(1 * OVL_ATLAS_SIZE + 1) * 2 + 1] == 255,
        "the white texel");
  /* Each width's row: that many solid texels, centred, in a placed row */
  for (n = 0; n <= 32; n++) {
    int solid = 0, placed = 0;
    for (i = 0; i < 64; i++) {
      const uint8_t *t = px + ((size_t)n * OVL_ATLAS_SIZE + 4 + (size_t)i) * 2;
      solid += t[0] == 255;
      placed += t[1] == 255;
    }
    CHECK(solid == n, "a baked line as wide as its row");
    CHECK(placed == 34, "a baked line's row placed whole");
  }
  ovl_atlas_mark_dirty(10, 20, 30, 40);
  CHECK(ovl_atlas_take_dirty(rect, &cleared) && !cleared && rect[0] == 10 &&
            rect[1] == 20 && rect[2] == 30 && rect[3] == 40,
        "a rectangle put back is taken again");
}

/* A notification and a trophy, frame by frame at 60 a second until @frames, as
 * ImGui drew them: the vertices and indices of the frame then, ImGui's */
static void check_layers(void) {
  static uint8_t slots[OVL_TEX_SLOTS][OVL_TEX_DIM * OVL_TEX_DIM * 4];
  static const float sound[4] = {0, 0, 0, 0};
  uint8_t *pixels[OVL_TEX_SLOTS];
  unsigned char *icon, *gold, *plat;
  size_t icon_size, gold_size, plat_size;
  const ovl_draw_data_t *dd = NULL;
  int i, frame;
  for (i = 0; i < OVL_TEX_SLOTS; i++)
    pixels[i] = slots[i];
  ovl_tex_init(pixels, OVL_TEX_SLOTS);
  icon = read_file("src/resources/shadps4.png", &icon_size);
  gold = read_file("src/resources/gold.png", &gold_size);
  plat = read_file("src/resources/platinum.png", &plat_size);
  CHECK(icon && gold && plat, "the icons");
  if (!icon || !gold || !plat)
    return;

  /* The system fonts warning, bottom right, a minute into its thirty seconds */
  CHECK(ovl_notify_queue("WARNING! PS4 system fonts not found.\n"
                         "Using bundled fallback fonts instead.\n"
                         "Text rendering may be inaccurate.",
                         30.0f, OVL_NOTIFY_BOTTOM_RIGHT, icon, icon_size, 0),
        "a notification queued");
  CHECK(ovl_notify_active(), "waiting to show");
  for (frame = 0; frame <= 60; frame++) {
    ovl_new_frame(1920.0f, 1080.0f, 1.0f / 60.0f);
    ovl_notify_draw();
    dd = ovl_render();
    ovl_tex_take_upload();
  }
  CHECK(dd->vtx_count == 440 && dd->idx_count == 798,
        "the notification as ImGui drew it");
  ovl_notify_reset();

  /* A trophy, right, two thirds of a second in */
  ovl_trophy_set_sound(sound, 2);
  CHECK(
      ovl_trophy_queue("The Hunter of Hunters and the Nightmare Slain Forever",
                       plat, plat_size, gold, gold_size),
      "a trophy queued");
  for (frame = 0; frame <= 40; frame++) {
    ovl_new_frame(1920.0f, 1080.0f, 1.0f / 60.0f);
    ovl_trophy_draw();
    dd = ovl_render();
  }
  CHECK(dd->vtx_count == 304 && dd->idx_count == 594,
        "the trophy as ImGui drew it");
  CHECK(clips == 1, "its sound played once");
  /* Over after its six seconds: its images released, their slots free once
   * frames pass */
  for (frame = 0; frame < 400; frame++) {
    ovl_new_frame(1920.0f, 1080.0f, 1.0f / 60.0f);
    ovl_trophy_draw();
    ovl_render();
    ovl_tex_collect((unsigned)frame, 3);
  }
  CHECK(!ovl_trophy_active(), "the trophy over");
  {
    int handles[OVL_TEX_SLOTS + 1], n = 0;
    while (n <= OVL_TEX_SLOTS &&
           (handles[n] = ovl_tex_decode_png(gold, gold_size)) != 0)
      n++;
    CHECK(n == OVL_TEX_SLOTS, "every image's slot free again");
    for (i = 0; i < n; i++)
      ovl_tex_release(handles[i]);
  }
  ovl_tex_shutdown();
  free(icon);
  free(gold);
  free(plat);
}

int main(int argc, char **argv) {
  const char *path =
      argc > 1 ? argv[1] : "src/imgui/renderer/fonts/NotoSans-Regular.ttf";
  unsigned char *noto;
  size_t size;
  ovl_face_desc_t face;
  ovl_font_t *font;
  const ovl_draw_data_t *dd;
  ovl_vec2_t text_size;
  int frame;

  noto = read_file(path, &size);
  if (!noto) {
    printf("FAIL no font at %s\n", path);
    return 1;
  }
  face.data = noto;
  face.size = size;
  face.index = 0;
  font = ovl_font_add(&face, 1, 32.0f);
  CHECK(font != NULL, "the font");
  CHECK(ovl_create(font, 0.5f), "the overlay");
  if (failures)
    return 1;
  check_atlas();

  for (frame = 0; frame < 3; frame++) {
    ovl_new_frame(1920.0f, 1080.0f, 1.0f / 60.0f);
    fps_scene();
    if (frame == 0) {
      text_size = ovl_calc_text_size("17 FPS (58.8 ms)", NULL, 0.0f);
    }
    dd = ovl_render();
    if (frame == 0) {
      CHECK(dd->idx_count == 0,
            "a new window is measured, not drawn, the first frame");
      CHECK(text_size.x == 90.0f && text_size.y == 16.0f,
            "the line's size, as ImGui's");
    } else {
      CHECK(window_pos.x == 1814.0f && window_pos.y == 0.0f,
            "the window in the corner");
      CHECK(window_size.x == 106.0f && window_size.y == 32.0f,
            "fitted to its line");
      CHECK(dd->vtx_count == (frame == 1 ? 72u : 116u), "ImGui's vertices");
      CHECK(dd->idx_count == (frame == 1 ? 246u : 312u), "ImGui's indices");
      if (frame == 2)
        check_quads(dd);
    }
  }
  check_layers();
  ovl_destroy();
  free(noto);
  if (failures) {
    printf("%d failures\n", failures);
    return 1;
  }
  printf("ovl_test: OK\n");
  return 0;
}
