/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* libSceNgs2's helpers (ngs2_util.c): waveform files made here read for what
 * a sampler needs, and places among the speakers and in a world turned into
 * levels that are what they have to be. */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "core/libraries/ngs2/ngs2_util.h"

static int failures;

#define CHECK(cond) \
   do { \
      if (!(cond)) { \
         printf("FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond); \
         failures++; \
      } \
   } while (0)

#define NEAR(a, b) (fabs((double)(a) - (double)(b)) < 1e-4)

static uint8_t file[4096];
static size_t file_size;

static void put(const void *bytes, size_t n) {
   memcpy(file + file_size, bytes, n);
   file_size += n;
}

static void put16(unsigned v) {
   uint8_t b[2];
   b[0] = (uint8_t)v;
   b[1] = (uint8_t)(v >> 8);
   put(b, 2);
}

static void put32(unsigned long v) {
   uint8_t b[4];
   b[0] = (uint8_t)v;
   b[1] = (uint8_t)(v >> 8);
   b[2] = (uint8_t)(v >> 16);
   b[3] = (uint8_t)(v >> 24);
   put(b, 4);
}

static void zeros(size_t n) {
   memset(file + file_size, 0, n);
   file_size += n;
}

static void riff_begin(void) {
   file_size = 0;
   put("RIFF", 4);
   put32(0);
   put("WAVE", 4);
}

static void riff_end(void) {
   size_t size = file_size;
   file_size = 4;
   put32((unsigned long)(size - 8));
   file_size = size;
}

static void smpl(unsigned long begin, unsigned long last) {
   put("smpl", 4);
   put32(60);
   zeros(28);
   put32(1); /* loops */
   put32(0);
   put32(0); /* the loop: id, type */
   put32(0);
   put32(begin);
   put32(last);
   put32(0);
   put32(0);
}

static void test_pcm(void) {
   ngs2_waveform_info info;
   ngs2_waveform_block b;
   uint32_t a, c, d, e;
   /* 16-bit stereo, 100 frames, a loop from 20 to 59 */
   riff_begin();
   put("fmt ", 4);
   put32(16);
   put16(1);
   put16(2);
   put32(44100);
   put32(44100 * 4);
   put16(4);
   put16(16);
   smpl(20, 59);
   put("data", 4);
   put32(400);
   zeros(400);
   riff_end();
   CHECK(ngs2_parse_waveform_data(file, file_size, &info) == NGS2_OK);
   CHECK(info.format.type == NGS2_WAVE_PCM_I16L && info.format.channels == 2);
   CHECK(info.format.sample_rate == 44100 && info.num_samples == 100);
   CHECK(info.data_size == 400 && info.data_offset == file_size - 400);
   CHECK(info.audio_frame_size == 4 && info.audio_frame_samples == 1);
   CHECK(info.loop_begin == 20 && info.loop_end == 60 && info.num_blocks == 3);
   CHECK(info.blocks[0].data_offset == info.data_offset && info.blocks[0].num_samples == 20);
   CHECK(info.blocks[0].data_size == 80 && info.blocks[0].num_repeats == 0);
   CHECK(info.blocks[1].data_offset == info.data_offset + 80 && info.blocks[1].num_samples == 40);
   CHECK(info.blocks[1].num_repeats == NGS2_REPEAT_ENDLESS);
   CHECK(info.blocks[2].data_offset == info.data_offset + 240 && info.blocks[2].data_size == 160);
   /* Cut short, not there, nowhere to put it, not a waveform */
   CHECK(ngs2_parse_waveform_data(file, file_size - 1, &info) == NGS2_ERROR_INVALID_WAVEFORM_DATA);
   CHECK(ngs2_parse_waveform_data(file, 8, &info) == NGS2_ERROR_INVALID_WAVEFORM_DATA);
   CHECK(ngs2_parse_waveform_data(NULL, 8, &info) == NGS2_ERROR_INVALID_WAVEFORM_DATA);
   CHECK(ngs2_parse_waveform_data(file, file_size, NULL) == NGS2_ERROR_INVALID_OUT_ADDRESS);
   file[0] = 'X';
   CHECK(ngs2_parse_waveform_data(file, file_size, &info) == NGS2_ERROR_UNKNOWN_WAVEFORM_FORMAT);

   /* Float, mono, no loop */
   riff_begin();
   put("fmt ", 4);
   put32(16);
   put16(3);
   put16(1);
   put32(48000);
   put32(48000 * 4);
   put16(4);
   put16(32);
   put("data", 4);
   put32(40);
   zeros(40);
   riff_end();
   CHECK(ngs2_parse_waveform_data(file, file_size, &info) == NGS2_OK);
   CHECK(info.format.type == NGS2_WAVE_PCM_F32L && info.num_samples == 10);
   CHECK(info.num_blocks == 1 && info.blocks[0].data_size == 40);

   CHECK(ngs2_calc_waveform_block(&info.format, 3, 5, &b) == NGS2_OK);
   CHECK(b.data_offset == 12 && b.data_size == 20 && b.num_samples == 5);
   CHECK(ngs2_waveform_frame_info(&info.format, &a, &c, &d, &e) == NGS2_OK);
   CHECK(a == 4 && c == 1 && d == 1 && e == 0);
   info.format.type = 0x99;
   CHECK(ngs2_calc_waveform_block(&info.format, 0, 5, &b) == NGS2_ERROR_UNKNOWN_WAVEFORM_FORMAT);
}

static void test_atrac9(void) {
   /* 48 kHz (256 samples a frame), stereo, frames of 200 bytes, 4 to a superframe */
   static const uint8_t guid[16] = {0xd2, 0x42, 0xe1, 0x47, 0xba, 0x36, 0x8d, 0x4d,
                                    0x88, 0xfc, 0x61, 0x65, 0x4f, 0x8c, 0x83, 0x6c};
   unsigned long config = 0xfe000000ul | 7ul << 20 | 2ul << 17 | 199ul << 5 | 2ul << 3;
   uint8_t bytes[4];
   ngs2_waveform_info info;
   ngs2_waveform_block b;
   uint32_t a, c, d, e;
   bytes[0] = (uint8_t)(config >> 24);
   bytes[1] = (uint8_t)(config >> 16);
   bytes[2] = (uint8_t)(config >> 8);
   bytes[3] = (uint8_t)config;
   riff_begin();
   put("fmt ", 4);
   put32(52);
   put16(0xfffe);
   put16(2);
   put32(48000);
   put32(0);
   put16(800);
   put16(0);
   put16(34);
   put16(1024);
   put32(3);
   put(guid, 16);
   put32(1);
   put(bytes, 4);
   put32(0);
   put("fact", 4);
   put32(12);
   put32(3000); /* samples */
   put32(256);
   put32(256); /* to skip */
   smpl(256 + 1024, 256 + 2047);
   put("data", 4);
   put32(2400);
   zeros(2400);
   riff_end();
   CHECK(ngs2_parse_waveform_data(file, file_size, &info) == NGS2_OK);
   CHECK(info.format.type == NGS2_WAVE_ATRAC9 && info.format.channels == 2);
   CHECK(info.format.sample_rate == 48000 && info.format.config_data == config);
   CHECK(info.num_samples == 3000 && info.delay_samples == 256);
   CHECK(info.audio_frame_size == 800 && info.audio_frame_samples == 1024);
   CHECK(info.audio_unit_size == 200 && info.audio_units_per_frame == 4);
   CHECK(info.loop_begin == 1024 && info.loop_end == 2048 && info.num_blocks == 3);
   /* The loop is the second superframe, with the decoder's frame of delay to skip */
   CHECK(info.blocks[1].data_offset == info.data_offset + 800);
   CHECK(info.blocks[1].data_size == 1600 && info.blocks[1].num_skip_samples == 256);
   CHECK(info.blocks[1].num_samples == 1024);
   CHECK(info.blocks[1].num_repeats == NGS2_REPEAT_ENDLESS);
   CHECK(ngs2_calc_waveform_block(&info.format, 100, 500, &b) == NGS2_OK);
   CHECK(b.data_offset == 0 && b.data_size == 800 && b.num_skip_samples == 356);
   CHECK(ngs2_waveform_frame_info(&info.format, &a, &c, &d, &e) == NGS2_OK);
   CHECK(a == 800 && c == 1024 && d == 4 && e == 256);
   /* The same four bytes as a word of the game's machine */
   info.format.config_data = (uint32_t)bytes[3] << 24 | (uint32_t)bytes[2] << 16 |
                             (uint32_t)bytes[1] << 8 | bytes[0];
   CHECK(ngs2_waveform_frame_info(&info.format, &a, &c, &d, &e) == NGS2_OK && a == 800);
   info.format.config_data = 0x12345678;
   CHECK(ngs2_calc_waveform_block(&info.format, 0, 5, &b) == NGS2_ERROR_INVALID_WAVEFORM_FORMAT);
}

static void test_vag(void) {
   ngs2_waveform_info info;
   unsigned i;
   file_size = 0;
   put("VAGp", 4);
   zeros(8);
   file[file_size++] = 0;
   file[file_size++] = 0;
   file[file_size++] = 0;
   file[file_size++] = 160; /* data: 10 frames */
   file[file_size++] = 0;
   file[file_size++] = 0;
   file[file_size++] = 0xac;
   file[file_size++] = 0x44; /* 44100 */
   zeros(0x30 - 0x14);
   for (i = 0; i < 10; i++) {
      uint8_t frame[16];
      memset(frame, 0, sizeof(frame));
      frame[1] = i == 2 ? 6 : i == 7 ? 3 : 2;
      put(frame, 16);
   }
   zeros(16);
   CHECK(ngs2_parse_waveform_data(file, file_size, &info) == NGS2_OK);
   CHECK(info.format.type == NGS2_WAVE_VAG && info.format.channels == 1);
   CHECK(info.format.sample_rate == 44100 && info.num_samples == 280);
   CHECK(info.loop_begin == 56 && info.loop_end == 224 && info.num_blocks == 3);
   CHECK(info.blocks[1].data_offset == 0x30 + 32 && info.blocks[1].data_size == 96);
   CHECK(info.blocks[1].num_samples == 168);
   CHECK(info.blocks[2].data_offset == 0x30 + 128 && info.blocks[2].num_samples == 56);
   CHECK(ngs2_parse_waveform_data(file, 0x30, &info) == NGS2_ERROR_INVALID_WAVEFORM_DATA);
}

static void test_pan(void) {
   ngs2_pan_work work;
   ngs2_pan_param p[2];
   float m[16];
   float angles[8];
   CHECK(ngs2_pan_init(NULL, NULL, 360.0f, 2) == NGS2_ERROR_INVALID_OUT_ADDRESS);
   CHECK(ngs2_pan_init(&work, NULL, 360.0f, 3) == NGS2_ERROR_INVALID_PAN_SPEAKER);
   CHECK(ngs2_pan_init(&work, angles, 100.0f, 2) == NGS2_ERROR_INVALID_PAN_UNIT_ANGLE);
   CHECK(ngs2_pan_init(&work, NULL, 360.0f, 2) == NGS2_OK);
   memset(p, 0, sizeof(p));
   p[0].angle = 90.0f; /* at the right */
   p[0].distance = 1.0f;
   p[0].fbw_level = 1.0f;
   p[1].angle = -360.0f; /* in front */
   p[1].distance = 1.0f;
   p[1].fbw_level = 0.5f;
   CHECK(ngs2_pan_matrix(&work, p, 2, 2, m) == NGS2_OK);
   CHECK(NEAR(m[0], 0.0) && NEAR(m[2], 1.0));
   CHECK(NEAR(m[1], 0.35355) && NEAR(m[3], 0.35355));
   CHECK(ngs2_pan_matrix(&work, p, 2, 3, m) == NGS2_ERROR_INVALID_PAN_MATRIX_FORMAT);
   /* At no distance it is everywhere: as much left as right */
   p[0].distance = 0.0f;
   CHECK(ngs2_pan_matrix(&work, p, 1, 2, m) == NGS2_OK);
   CHECK(NEAR(m[0], m[1]) && m[0] > 0.5f);
   /* Seven speakers into 7.1: the front is the centre, and the LFE has its own level */
   CHECK(ngs2_pan_init(&work, NULL, 0.0f, 0) == NGS2_OK);
   p[0].angle = 0.0f;
   p[0].distance = 1.0f;
   p[0].lfe_level = 0.25f;
   CHECK(ngs2_pan_matrix(&work, p, 1, 8, m) == NGS2_OK);
   CHECK(NEAR(m[2], 1.0) && NEAR(m[0], 0.0) && NEAR(m[1], 0.0) && NEAR(m[3], 0.25));
   /* In radians, half way between the centre and the front right */
   CHECK(ngs2_pan_init(&work, NULL, 6.2831853f, 7) == NGS2_OK);
   p[0].angle = 20.0f / 180.0f * 3.14159265f;
   CHECK(ngs2_pan_matrix(&work, p, 1, 8, m) == NGS2_OK);
   CHECK(NEAR(m[2], 0.70711) && NEAR(m[1], 0.70711) && NEAR(m[0], 0.0));
}

static void test_geom(void) {
   ngs2_geom_listener l;
   ngs2_geom_work w;
   ngs2_geom_source s;
   ngs2_geom_attribute a;
   CHECK(ngs2_geom_reset_listener(&l) == NGS2_OK && ngs2_geom_reset_source(&s) == NGS2_OK);
   CHECK(l.front.z == 1.0f && l.up.y == 1.0f && s.num_speakers == 2 && s.matrix_format == 2);
   l.position.x = 10.0f;
   CHECK(ngs2_geom_calc_listener(&l, &w, 2) == NGS2_ERROR_INVALID_GEOM_FLAG);
   CHECK(ngs2_geom_calc_listener(&l, &w, 0) == NGS2_OK);
   CHECK(NEAR(w.matrix[0][0], 1.0) && NEAR(w.matrix[1][1], 1.0) && NEAR(w.matrix[2][2], 1.0));
   CHECK(NEAR(w.matrix[0][3], -10.0) && w.matrix[3][3] == 1.0f);
   /* Five to the listener's right: on the right, a fifth as loud */
   s.position.x = 15.0f;
   CHECK(ngs2_geom_apply(&w, &s, &a, 0) == NGS2_OK);
   CHECK(NEAR(a.levels[0], 0.0) && NEAR(a.levels[1], 0.2) && NEAR(a.pitch_ratio, 1.0));
   CHECK(NEAR(a.a3d_position.x, 5.0) && NEAR(a.a3d_volume, 0.2));
   /* Coming at a tenth of the speed of sound it is a ninth higher */
   s.velocity.x = -34.3f;
   CHECK(ngs2_geom_apply(&w, &s, &a, 0) == NGS2_OK);
   CHECK(NEAR(a.pitch_ratio, 1.0 / 0.9));
   s.velocity.x = 0.0f;
   /* Behind the listener: as much on each side */
   s.position.x = 10.0f;
   s.position.z = -5.0f;
   CHECK(ngs2_geom_apply(&w, &s, &a, 0) == NGS2_OK);
   CHECK(NEAR(a.levels[0], a.levels[1]) && a.levels[0] > 0.1f);
   /* Pointing away, with a narrow cone, it is as loud as outside the cone */
   s.direction.z = -1.0f;
   s.cone_inner_angle = 30.0f;
   s.cone_outer_angle = 60.0f;
   s.cone_outer_level = 0.5f;
   CHECK(ngs2_geom_apply(&w, &s, &a, 0) == NGS2_OK);
   CHECK(NEAR(a.a3d_volume, 0.1));
   /* Where the listener is: no direction, full level, nothing but numbers */
   s.position.z = 0.0f;
   CHECK(ngs2_geom_apply(&w, &s, &a, 0) == NGS2_OK);
   CHECK(a.levels[0] == a.levels[0] && a.pitch_ratio == 1.0f && NEAR(a.a3d_volume, 1.0));
   /* A listener with no way up still has one */
   l.up.y = 0.0f;
   CHECK(ngs2_geom_calc_listener(&l, &w, 1) == NGS2_OK);
   CHECK(NEAR(w.matrix[0][0], -1.0) && NEAR(w.matrix[1][1], 1.0));
   CHECK(ngs2_geom_apply(NULL, &s, &a, 0) == NGS2_ERROR_INVALID_GEOM_LISTENER_ADDRESS);
}

int main(void) {
   test_pcm();
   test_atrac9();
   test_vag();
   test_pan();
   test_geom();
   if (failures) {
      printf("ngs2_util_test: %d failed\n", failures);
      return 1;
   }
   printf("ngs2_util_test: passed\n");
   return 0;
}
