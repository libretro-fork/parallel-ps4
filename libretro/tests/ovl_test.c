/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The overlays' library (src/video_core/overlay) against what ImGui drew: the
 * FPS counter's window, frame by frame (hidden the first frame while it is
 * measured, then placed and sized as ImGui placed and sized it), its glyphs
 * where ImGui put them, as many vertices and indices as ImGui made; the atlas's
 * white texel and baked lines, and what it hands the renderer. The expected
 * numbers are ImGui 1.92's, from the same font at the same sizes. The splash:
 * its size from its header, decoded on its thread as rpng decodes it, fitted
 * where the presenter's ImGui window put it; a decode stopped part way. The
 * message and error dialogs: as many vertices and indices as ImGui made, the
 * focus moved and the button pressed by the pad as ImGui's navigation does. A
 * list in a child flattened into its window, scrolled to the focused item as
 * the d-pad goes down it, with its scrollbar, as ImGui drew it.
 *
 *   build-tests/ovl_test [NotoSans-Regular.ttf] */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <encodings/crc32.h>

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
void ovl_invite_chosen(int user_id, const char *invitation_id, const char *from_npid,
                       int accept) {
  (void)user_id;
  (void)invitation_id;
  (void)from_npid;
  (void)accept;
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

/* The splash's quad at @dw x @dh: from (x0, y0) to (x1, y1), white, ImGui's
 * texture coordinates */
static int quad_is(float dw, float dh, float x0, float y0, float x1, float y1) {
  ovl_vertex_t v[4];
  int i, ok = 1;
  ovl_splash_quad(dw, dh, v);
  for (i = 0; i < 4; i++)
    ok &= v[i].col == 0xFFFFFFFFu;
  return ok && v[0].x == x0 && v[0].y == y0 && v[2].x == x1 && v[2].y == y1 &&
         v[1].x == x1 && v[1].y == y0 && v[3].x == x0 && v[3].y == y1 &&
         v[0].u == 0.0f && v[0].v == 0.0f && v[2].u == 1.0f && v[2].v == 1.0f;
}

/* A PNG of @w x @h RGB pixels, its image data stored uncompressed: a chunk at
 * a time into @out, which is large enough; its size */
static size_t chunk(uint8_t *out, const char *type, const uint8_t *data,
                    uint32_t n) {
  uint32_t crc;
  out[0] = (uint8_t)(n >> 24);
  out[1] = (uint8_t)(n >> 16);
  out[2] = (uint8_t)(n >> 8);
  out[3] = (uint8_t)n;
  memcpy(out + 4, type, 4);
  if (n)
    memcpy(out + 8, data, n);
  crc = encoding_crc32(0, out + 4, n + 4);
  out[8 + n] = (uint8_t)(crc >> 24);
  out[9 + n] = (uint8_t)(crc >> 16);
  out[10 + n] = (uint8_t)(crc >> 8);
  out[11 + n] = (uint8_t)crc;
  return 12 + n;
}

static size_t make_png(uint8_t *out, uint8_t *scratch, const uint8_t *rgb,
                       unsigned w, unsigned h) {
  static const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
  const size_t raw_size = (size_t)(w * 3 + 1) * h;
  uint8_t ihdr[13];
  uint8_t *z = scratch, *raw;
  uint32_t a = 1, b = 0;
  size_t n = 0, done = 0, i;
  memcpy(out, sig, 8);
  memset(ihdr, 0, sizeof(ihdr));
  ihdr[2] = (uint8_t)(w >> 8);
  ihdr[3] = (uint8_t)w;
  ihdr[6] = (uint8_t)(h >> 8);
  ihdr[7] = (uint8_t)h;
  ihdr[8] = 8; /* bits */
  ihdr[9] = 2; /* RGB */
  n = 8 + chunk(out + 8, "IHDR", ihdr, 13);
  /* zlib: its header, stored blocks, Adler-32 */
  raw = scratch + raw_size + raw_size / 8 + 64;
  for (i = 0; i < h; i++) {
    raw[i * (w * 3 + 1)] = 0;
    memcpy(raw + i * (w * 3 + 1) + 1, rgb + i * w * 3, w * 3);
  }
  for (i = 0; i < raw_size; i++) {
    a = (a + raw[i]) % 65521;
    b = (b + a) % 65521;
  }
  *z++ = 0x78;
  *z++ = 0x01;
  while (done < raw_size) {
    const size_t len = raw_size - done > 65535 ? 65535 : raw_size - done;
    *z++ = done + len == raw_size;
    *z++ = (uint8_t)len;
    *z++ = (uint8_t)(len >> 8);
    *z++ = (uint8_t)~len;
    *z++ = (uint8_t)(~len >> 8);
    memcpy(z, raw + done, len);
    z += len;
    done += len;
  }
  *z++ = (uint8_t)(b >> 8);
  *z++ = (uint8_t)b;
  *z++ = (uint8_t)(a >> 8);
  *z++ = (uint8_t)a;
  n += chunk(out + n, "IDAT", scratch, (uint32_t)(z - scratch));
  n += chunk(out + n, "IEND", NULL, 0);
  return n;
}

static void check_splash(void) {
  /* 300 x 100 of a pattern */
  enum { SW = 300, SH = 100 };
  static uint8_t rgb[SW * SH * 3];
  static uint8_t pixels[SW * SH * 4];
  static uint8_t scratch[SW * SH * 8];
  static uint8_t png[SW * SH * 4];
  const uint8_t junk[16] = {1, 2, 3};
  size_t png_size;
  unsigned w = 0, h = 0;
  int i, same = 1;
  for (i = 0; i < SW * SH; i++) {
    rgb[i * 3 + 0] = (uint8_t)(i % SW);
    rgb[i * 3 + 1] = (uint8_t)(i / SW);
    rgb[i * 3 + 2] = (uint8_t)(i * 7);
  }
  png_size = make_png(png, scratch, rgb, SW, SH);
  CHECK(!ovl_splash_open(NULL, 0, &w, &h), "no splash");
  CHECK(!ovl_splash_open(junk, sizeof(junk), &w, &h), "not a PNG");
  CHECK(ovl_splash_open(png, png_size, &w, &h) && w == SW && h == SH,
        "its size from its header");
  memset(pixels, 0, sizeof(pixels));
  CHECK(ovl_splash_start(pixels), "decoding");
  while (ovl_splash_decoding()) {
  }
  CHECK(ovl_splash_decoded(), "decoded");
  for (i = 0; i < SW * SH; i++)
    same &= pixels[i * 4 + 0] == rgb[i * 3 + 0] &&
            pixels[i * 4 + 1] == rgb[i * 3 + 1] &&
            pixels[i * 4 + 2] == rgb[i * 3 + 2] && pixels[i * 4 + 3] == 255;
  CHECK(same, "as R, G, B, A bytes, where it was lent");
  /* Wider than the screen: its width, centred, as FitImage */
  CHECK(quad_is(1920.0f, 1080.0f, 0.0f, 220.0f, 1920.0f, 860.0f),
        "fitted at 1080p");
  CHECK(quad_is(1280.0f, 720.0f, 0.0f, 147.0f, 1280.0f, 573.0f),
        "fitted at 720p, its height truncated");
  CHECK(quad_is(300.0f, 1000.0f, 0.0f, 450.0f, 300.0f, 550.0f),
        "fitted to a tall screen");
  /* Narrower: its height */
  CHECK(quad_is(1000.0f, 50.0f, 425.0f, 0.0f, 575.0f, 50.0f),
        "fitted to a wide screen");
  ovl_splash_close();
  CHECK(!ovl_splash_decoded() && !ovl_splash_decoding(), "closed");

  /* Closed while it decodes: its thread stops and ends first */
  CHECK(ovl_splash_open(png, png_size, &w, &h) && ovl_splash_start(pixels),
        "decoding again");
  ovl_splash_close();
  CHECK(!ovl_splash_decoding(), "stopped");
}

/* A frame of the dialogs with the pad's d-pad left and cross as given */
static const ovl_draw_data_t *dialog_frame(int left, int cross) {
  ovl_key_event(OVL_KEY_PAD_DPAD_LEFT, left);
  ovl_key_event(OVL_KEY_PAD_FACE_DOWN, cross);
  ovl_new_frame(1920.0f, 1080.0f, 1.0f / 60.0f);
  ovl_msg_dialog_draw();
  return ovl_render();
}

static void check_dialogs(void) {
  static const char long_msg[] =
      "The saved data could not be loaded because it was made by a newer "
      "version of the game. Update the game to the latest version and try "
      "again. Do you want to continue without loading it?";
  ovl_msg_dialog_param_t p;
  const ovl_draw_data_t *dd;
  int button = -1, result = -1, frame;
  ovl_msg_dialog_init();
  ovl_set_gamepad_connected(1);
  memset(&p, 0, sizeof(p));
  p.mode = OVL_MSG_MODE_USER;
  p.button_type = OVL_MSG_YESNO;
  p.msg = long_msg;
  ovl_msg_dialog_set_status(OVL_MSG_DIALOG_GAME, 1);
  ovl_msg_dialog_open(OVL_MSG_DIALOG_GAME, &p);
  CHECK(ovl_msg_dialog_status(OVL_MSG_DIALOG_GAME) == 2, "running");
  CHECK(ovl_msg_dialog_active(), "frames drawn while it shows");
  /* ImGui's: the focus put on Yes after it is drawn, its cursor from the
   * next frame */
  dd = dialog_frame(0, 0);
  CHECK(dd->vtx_count == 684 && dd->idx_count == 1164,
        "the dialog's first frame as ImGui's");
  dd = dialog_frame(0, 0);
  CHECK(dd->vtx_count == 692 && dd->idx_count == 1188,
        "with the focus cursor, as ImGui's");
  /* Left to No (found the next frame), then cross */
  dialog_frame(1, 0);
  dialog_frame(0, 0);
  dialog_frame(0, 1);
  CHECK(ovl_msg_dialog_status(OVL_MSG_DIALOG_GAME) == 3, "finished by cross");
  ovl_msg_dialog_result(OVL_MSG_DIALOG_GAME, &button, &result);
  CHECK(button == OVL_MSG_BUTTON_2 && result == OVL_MSG_RESULT_OK, "on No");
  dd = dialog_frame(0, 0);
  CHECK(dd->vtx_count == 0 && !ovl_msg_dialog_active(), "gone");
  /* The game closing it */
  ovl_msg_dialog_open(OVL_MSG_DIALOG_GAME, &p);
  CHECK(ovl_msg_dialog_close(OVL_MSG_DIALOG_GAME, 0, 0) &&
            !ovl_msg_dialog_close(OVL_MSG_DIALOG_GAME, 0, 0),
        "closed once");

  /* The error dialog */
  memset(&p, 0, sizeof(p));
  p.mode = OVL_MSG_MODE_ERROR;
  p.msg = "An error has occurred. \nCode: 0X80020016";
  ovl_msg_dialog_set_status(OVL_MSG_DIALOG_ERROR, 1);
  ovl_msg_dialog_open(OVL_MSG_DIALOG_ERROR, &p);
  dd = dialog_frame(0, 0);
  CHECK(dd->vtx_count == 216 && dd->idx_count == 462,
        "the error dialog's first frame as ImGui's");
  dd = dialog_frame(0, 0);
  CHECK(dd->vtx_count == 224 && dd->idx_count == 486,
        "with the focus cursor, as ImGui's");
  for (frame = 0; frame < 3; frame++)
    dialog_frame(0, frame == 1);
  CHECK(ovl_msg_dialog_status(OVL_MSG_DIALOG_ERROR) == 3, "finished by OK");
  dialog_frame(0, 0);
  ovl_msg_dialog_shutdown();
}

/* The profile dialog: an account's number on its card, as ImGui drew it, closed
 * by cross on Close, and by the game */
static const ovl_draw_data_t *profile_frame(int cross) {
  ovl_key_event(OVL_KEY_PAD_FACE_DOWN, cross);
  ovl_new_frame(1920.0f, 1080.0f, 1.0f / 60.0f);
  ovl_profile_dialog_draw();
  return ovl_render();
}

static void check_profile(void) {
  ovl_profile_dialog_param_t p;
  const ovl_draw_data_t *dd;
  int frame;
  ovl_profile_dialog_init();
  ovl_set_gamepad_connected(1);
  memset(&p, 0, sizeof(p));
  p.mode = OVL_PROFILE_MODE_NORMAL;
  p.has_account_id = 1;
  p.account_id = ((uint64_t)0xFFFFFFFFu << 32) | 0xFFFFFFFFu;
  ovl_profile_dialog_set_status(1);
  ovl_profile_dialog_open(&p);
  CHECK(ovl_profile_dialog_status() == 2 && ovl_profile_dialog_active(),
        "running, frames drawn while it shows");
  dd = profile_frame(0);
  CHECK(dd->vtx_count == 1236 && dd->idx_count == 4758,
        "the profile dialog's first frame as ImGui's");
  dd = profile_frame(0);
  CHECK(dd->vtx_count == 1268 && dd->idx_count == 4854,
        "with the focus cursor, as ImGui's");
  for (frame = 0; frame < 3; frame++)
    profile_frame(frame == 1);
  CHECK(ovl_profile_dialog_status() == 3, "finished by cross on Close");
  CHECK(ovl_profile_dialog_result() == OVL_PROFILE_RESULT_USER_CANCELED,
        "as cancelled by the player");
  dd = profile_frame(0);
  CHECK(dd->vtx_count == 0 && !ovl_profile_dialog_active(), "gone");
  /* The game closing it */
  p.has_account_id = 0;
  p.online_id = "Kestrel";
  ovl_profile_dialog_open(&p);
  profile_frame(0);
  CHECK(ovl_profile_dialog_close(OVL_PROFILE_RESULT_OK) &&
            !ovl_profile_dialog_close(OVL_PROFILE_RESULT_USER_CANCELED),
        "closed once");
  CHECK(ovl_profile_dialog_result() == OVL_PROFILE_RESULT_OK,
        "with the game's result");
  ovl_profile_dialog_shutdown();
}

/* The store's dialog: a product for a user, as ImGui drew it, the focus on
 * Close so that cross buys nothing; and the store's icon beside it */
static const ovl_draw_data_t *commerce_frame(int cross) {
  ovl_key_event(OVL_KEY_PAD_FACE_DOWN, cross);
  ovl_new_frame(1920.0f, 1080.0f, 1.0f / 60.0f);
  ovl_commerce_dialog_draw();
  ovl_store_icon_draw();
  return ovl_render();
}

static void check_commerce(void) {
  static const char *const targets[] = {"A"};
  ovl_commerce_dialog_param_t p;
  const ovl_draw_data_t *dd;
  int frame;
  ovl_commerce_dialog_init();
  ovl_set_gamepad_connected(1);
  memset(&p, 0, sizeof(p));
  p.mode = OVL_COMMERCE_MODE_PRODUCT;
  p.user_id = 1;
  p.username = "Lib";
  p.targets = targets;
  p.target_count = 1;
  ovl_commerce_dialog_set_status(1);
  ovl_commerce_dialog_open(&p);
  CHECK(ovl_commerce_dialog_status() == 2 && ovl_commerce_dialog_active(),
        "running, frames drawn while it shows");
  dd = commerce_frame(0);
  CHECK(dd->vtx_count == 416 && dd->idx_count == 804,
        "the store dialog's first frame as ImGui's");
  dd = commerce_frame(0);
  CHECK(dd->vtx_count == 424 && dd->idx_count == 828,
        "with the focus cursor, as ImGui's");
  ovl_store_icon_show(OVL_STORE_ICON_CENTER);
  CHECK(ovl_store_icon_active(), "frames drawn while the icon shows");
  dd = commerce_frame(0);
  CHECK(dd->vtx_count == 512 && dd->idx_count == 1134,
        "and the store's icon, as ImGui's: no border on its window");
  for (frame = 0; frame < 3; frame++)
    commerce_frame(frame == 1);
  CHECK(ovl_commerce_dialog_status() == 3, "finished by cross");
  CHECK(ovl_commerce_dialog_result() == OVL_COMMERCE_RESULT_USER_CANCELED,
        "on Close: nothing bought");
  dd = commerce_frame(0);
  CHECK(dd->vtx_count == 88 && dd->idx_count == 306 &&
            !ovl_commerce_dialog_active(),
        "the icon left alone");
  ovl_store_icon_hide();
  dd = commerce_frame(0);
  CHECK(dd->vtx_count == 0 && !ovl_store_icon_active(), "gone");
  /* The game closing it */
  ovl_commerce_dialog_open(&p);
  CHECK(ovl_commerce_dialog_result() == OVL_COMMERCE_RESULT_USER_CANCELED,
        "cancelled until something says otherwise");
  commerce_frame(0);
  CHECK(ovl_commerce_dialog_close(OVL_COMMERCE_RESULT_OK) &&
            !ovl_commerce_dialog_close(OVL_COMMERCE_RESULT_PURCHASED),
        "closed once");
  CHECK(ovl_commerce_dialog_result() == OVL_COMMERCE_RESULT_OK,
        "with the game's result");
  ovl_commerce_dialog_shutdown();
}

/* A frame of the save data dialog's shape: a title between separators, a
 * scrolling child of twelve buttons flattened into the window, back below */
static const ovl_draw_data_t *list_frame(int first, int down, int up) {
  ovl_vec2_t pos, size, pivot, avail, button;
  char label[16];
  int i;
  ovl_key_event(OVL_KEY_PAD_DPAD_DOWN, down);
  ovl_key_event(OVL_KEY_PAD_DPAD_UP, up);
  ovl_new_frame(1920.0f, 1080.0f, 1.0f / 60.0f);
  pos.x = 960.0f;
  pos.y = 540.0f;
  pivot.x = pivot.y = 0.5f;
  size.x = 1100.0f;
  size.y = 700.0f;
  ovl_set_next_window_pos(pos, OVL_COND_ALWAYS, pivot);
  ovl_set_next_window_size(size, OVL_COND_ALWAYS);
  if (first || !ovl_nav_active())
    ovl_set_next_window_focus();
  if (ovl_begin("List##List",
                OVL_WINDOW_NO_DECORATION | OVL_WINDOW_NO_SAVED_SETTINGS)) {
    ovl_separator();
    ovl_set_window_font_scale(1.7f);
    ovl_text("Save", NULL);
    ovl_set_window_font_scale(1.0f);
    ovl_separator();
    avail = ovl_content_region_avail();
    avail.y -= 31.0f;
    ovl_begin_child("##ScrollingRegion", avail, OVL_CHILD_NAV_FLATTENED, 0);
    for (i = 0; i < 12; i++) {
      sprintf(label, "Item %d", i);
      button.x = ovl_content_region_avail().x;
      button.y = 85.0f;
      ovl_button(label, button);
      if (first && i == 0)
        ovl_set_item_current_nav_focus(0);
    }
    ovl_end_child();
    ovl_separator();
    ovl_set_cursor_pos_x(ovl_content_region_avail().x - 114.0f);
    button.x = 114.0f;
    button.y = 23.0f;
    ovl_button("Back", button);
  }
  ovl_end();
  return ovl_render();
}

static void check_list(void) {
  const ovl_draw_data_t *dd = NULL;
  int frame;
  for (frame = 0; frame < 100; frame++) {
    /* As the comparison with ImGui: six presses, then held down to back */
    const int down = (frame >= 5 && frame <= 30 && frame % 5 == 0) ||
                     (frame >= 40 && frame < 100);
    dd = list_frame(frame == 0, down, 0);
    if (frame == 0)
      CHECK(dd->vtx_count == 280 && dd->idx_count == 558,
            "the list's first frame as ImGui's");
    if (frame == 1)
      CHECK(dd->vtx_count == 324 && dd->idx_count == 726,
            "its second, the scrollbar and focus cursor shown, as ImGui's");
  }
  CHECK(dd->vtx_count == 332 && dd->idx_count == 738,
        "scrolled to its end, back focused, as ImGui's");
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
  check_splash();
  check_dialogs();
  check_profile();
  check_commerce();
  check_list();
  ovl_destroy();
  free(noto);
  if (failures) {
    printf("%d failures\n", failures);
    return 1;
  }
  printf("ovl_test: OK\n");
  return 0;
}
