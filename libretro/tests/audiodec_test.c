/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* libSceAudiodec as a game calls it: an ATRAC9 superframe at a time through a
 * decoder gives what the decoder under it gives, 16-bit and float; what a game
 * may not pass is refused; the slots run out and come back. */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <formats/ratrac9.h>

#include "at9_vector.h"
#include "common/c_bridge.h"
#include "core/libraries/audiodec/audiodec.h"

static int failures;

#define CHECK(cond) \
   do { \
      if (!(cond)) { \
         printf("FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond); \
         failures++; \
      } \
   } while (0)

/* The core's log, which the library writes to */
void c_log(enum c_log_class log_class, enum c_log_level level, const char *func, const char *fmt,
      ...) {
   (void)log_class;
   (void)level;
   (void)func;
   (void)fmt;
}

#define SUPERFRAME (4 * 164)
#define SAMPLES (4 * 64 * 2)

static void test_at9(int word) {
   audiodec_param_at9 param;
   audiodec_at9_info info;
   audiodec_au_info au;
   audiodec_pcm_item item;
   audiodec_ctrl ctrl;
   union {
      int16_t s16[SAMPLES];
      float f32[SAMPLES];
   } pcm, want;
   ratrac9_t *d = ratrac9_open(at9_config);
   int32_t handle;
   unsigned sf, f;
   const size_t bytes = SAMPLES * (word == AUDIODEC_WORD_FLOAT ? sizeof(float) : sizeof(int16_t));
   memset(&param, 0, sizeof(param));
   memset(&info, 0, sizeof(info));
   param.size = sizeof(param);
   param.word = word;
   memcpy(param.config, at9_config, 4);
   info.size = sizeof(info);
   ctrl.param = &param;
   ctrl.bsi_info = &info;
   ctrl.au_info = &au;
   ctrl.pcm_item = &item;
   handle = sceAudiodecCreateDecoder(&ctrl, AUDIODEC_TYPE_AT9);
   CHECK(handle > 0);
   CHECK(info.channels == 2 && info.sampling_rate == 48000 && info.super_frame_size == SUPERFRAME);
   CHECK(info.frames_in_super_frame == 4 && info.frame_samples == 64);
   for (sf = 0; sf < 2; sf++) {
      for (f = 0; f < 4; f++) {
         size_t used;
         const uint8_t *in = at9_data + sf * SUPERFRAME + f * 164;
         if (word == AUDIODEC_WORD_FLOAT)
            CHECK(ratrac9_decode_f32(d, in, 164, want.f32 + f * 128, false, &used) == 0);
         else
            CHECK(ratrac9_decode_s16(d, in, 164, want.s16 + f * 128, false, &used) == 0);
      }
      au.size = sizeof(au);
      au.au = (void *)(at9_data + sf * SUPERFRAME);
      /* More than a superframe on offer: one is taken */
      au.au_size = sf == 0 ? 2 * SUPERFRAME : SUPERFRAME;
      item.size = sizeof(item);
      item.pcm = &pcm;
      item.pcm_size = sizeof(pcm);
      memset(&pcm, 0, sizeof(pcm));
      CHECK(sceAudiodecDecode(handle, &ctrl) == 0);
      CHECK(au.au_size == SUPERFRAME);
      CHECK(item.pcm_size == bytes);
      CHECK(memcmp(&pcm, &want, bytes) == 0);
   }
   /* Half a superframe; a buffer too small for one */
   au.au_size = SUPERFRAME / 2;
   item.pcm_size = sizeof(pcm);
   CHECK(sceAudiodecDecode(handle, &ctrl) == AUDIODEC_ERROR_API_FAIL);
   CHECK(au.au_size == 0 && item.pcm_size == 0);
   au.au_size = SUPERFRAME;
   item.pcm_size = 16;
   CHECK(sceAudiodecDecode(handle, &ctrl) == AUDIODEC_ERROR_INVALID_PCM_SIZE);
   CHECK(sceAudiodecClearContext(handle) == 0);
   CHECK(sceAudiodecDeleteDecoder(handle) == 0);
   CHECK(sceAudiodecDeleteDecoder(handle) == AUDIODEC_ERROR_INVALID_HANDLE);
   CHECK(sceAudiodecDecode(handle, &ctrl) == AUDIODEC_ERROR_INVALID_HANDLE);
   ratrac9_close(d);
}

static void test_refusals(void) {
   audiodec_param_at9 at9;
   audiodec_at9_info at9_info;
   audiodec_param_mp3 mp3;
   audiodec_mp3_info mp3_info;
   audiodec_param_m4aac aac;
   audiodec_m4aac_info aac_info;
   audiodec_ctrl ctrl;
   int32_t handles[65];
   int i, made = 0;
   memset(&ctrl, 0, sizeof(ctrl));
   memset(&at9, 0, sizeof(at9));
   memset(&at9_info, 0, sizeof(at9_info));
   CHECK(sceAudiodecInitLibrary(AUDIODEC_TYPE_AT9) == 0);
   CHECK(sceAudiodecInitLibrary(9) == AUDIODEC_ERROR_INVALID_TYPE);
   CHECK(sceAudiodecCreateDecoder(NULL, AUDIODEC_TYPE_AT9) == AUDIODEC_ERROR_INVALID_CTRL_POINTER);
   CHECK(sceAudiodecCreateDecoder(&ctrl, 0) == AUDIODEC_ERROR_INVALID_TYPE);
   CHECK(sceAudiodecCreateDecoder(&ctrl, AUDIODEC_TYPE_AT9) ==
         AUDIODEC_ERROR_INVALID_PARAM_POINTER);
   ctrl.param = &at9;
   ctrl.bsi_info = &at9_info;
   CHECK(sceAudiodecCreateDecoder(&ctrl, AUDIODEC_TYPE_AT9) == AUDIODEC_ERROR_INVALID_PARAM_SIZE);
   at9.size = sizeof(at9);
   at9_info.size = sizeof(at9_info);
   CHECK(sceAudiodecCreateDecoder(&ctrl, AUDIODEC_TYPE_AT9) ==
         AUDIODEC_ERROR_INVALID_WORD_LENGTH);
   at9.word = AUDIODEC_WORD_16BIT;
   /* A configuration that is none */
   CHECK(sceAudiodecCreateDecoder(&ctrl, AUDIODEC_TYPE_AT9) ==
         AUDIODEC_ERROR_AT9_INVALID_CONFIG_DATA);
   /* MP3 gives 16 bits only */
   memset(&mp3, 0, sizeof(mp3));
   memset(&mp3_info, 0, sizeof(mp3_info));
   mp3.size = sizeof(mp3);
   mp3.word = AUDIODEC_WORD_FLOAT;
   mp3_info.size = sizeof(mp3_info);
   ctrl.param = &mp3;
   ctrl.bsi_info = &mp3_info;
   CHECK(sceAudiodecCreateDecoder(&ctrl, AUDIODEC_TYPE_MP3) ==
         AUDIODEC_ERROR_INVALID_WORD_LENGTH);
   /* AAC: raw needs a rate, and a layout there is */
   memset(&aac, 0, sizeof(aac));
   memset(&aac_info, 0, sizeof(aac_info));
   aac.size = sizeof(aac);
   aac.word = AUDIODEC_WORD_16BIT;
   aac.config_number = AUDIODEC_M4AAC_RAW;
   aac.sampling_freq_index = 15;
   aac.max_channels = 2;
   aac_info.size = sizeof(aac_info);
   ctrl.param = &aac;
   ctrl.bsi_info = &aac_info;
   CHECK(sceAudiodecCreateDecoder(&ctrl, AUDIODEC_TYPE_M4AAC) ==
         AUDIODEC_ERROR_M4AAC_INVALID_SAMPLING_FREQ);
   aac.sampling_freq_index = 3;
   aac.max_channels = 3;
   CHECK(sceAudiodecCreateDecoder(&ctrl, AUDIODEC_TYPE_M4AAC) ==
         AUDIODEC_ERROR_M4AAC_INVALID_MAX_CHANNELS);
   aac.max_channels = 2;
   /* 64 decoders and no more; one given back is there to take */
   mp3.word = AUDIODEC_WORD_16BIT;
   ctrl.param = &mp3;
   ctrl.bsi_info = &mp3_info;
   for (i = 0; i < 65; i++) {
      handles[i] = sceAudiodecCreateDecoder(&ctrl, AUDIODEC_TYPE_MP3);
      if (handles[i] > 0)
         made++;
   }
   CHECK(made == 64);
   CHECK(handles[64] == AUDIODEC_ERROR_API_FAIL);
   CHECK(sceAudiodecDeleteDecoder(handles[10]) == 0);
   ctrl.param = &aac;
   ctrl.bsi_info = &aac_info;
   handles[10] = sceAudiodecCreateDecoder(&ctrl, AUDIODEC_TYPE_M4AAC);
   CHECK(handles[10] > 0);
   CHECK(aac_info.sampling_freq == 48000 && aac_info.channels == 2);
   for (i = 0; i < 64; i++)
      CHECK(sceAudiodecDeleteDecoder(handles[i]) == 0);
   CHECK(sceAudiodecTermLibrary(AUDIODEC_TYPE_AT9) == 0);
}

int main(void) {
   test_at9(AUDIODEC_WORD_16BIT);
   test_at9(AUDIODEC_WORD_FLOAT);
   test_refusals();
   if (failures) {
      printf("audiodec_test: %d failed\n", failures);
      return 1;
   }
   printf("audiodec_test: passed\n");
   return 0;
}
