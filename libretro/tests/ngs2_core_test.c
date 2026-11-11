/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* libSceNgs2's engine on its own: voices set the way a game sets them, and
 * the grains that come out compared with what they have to be. */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <formats/ratrac9.h>

#include "at9_vector.h"
#include "core/libraries/ngs2/ngs2_core.h"

#define GRAIN 256u
#define RATE 48000u
#define PI 3.14159265358979

static int failures;

#define CHECK(cond) \
   do { \
      if (!(cond)) { \
         printf("FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond); \
         failures++; \
      } \
   } while (0)

static unsigned fx_calls;
static unsigned fx2_setups, fx2_cleanups, fx2_processes;

static int host_user_fx(void *handler, ngs2_user_fx_context *context) {
   unsigned c, i;
   (void)handler;
   fx_calls++;
   /* Halves every channel */
   for (c = 0; c < context->channels; c++)
      for (i = 0; i < context->grain_samples; i++)
         context->channel_data[c][i] *= 0.5f;
   return 0;
}

static int host_fx2_setup(void *handler, ngs2_user_fx2_setup_context *context) {
   (void)handler;
   (void)context;
   fx2_setups++;
   return 0;
}

static int host_fx2_cleanup(void *handler, ngs2_user_fx2_setup_context *context) {
   (void)handler;
   (void)context;
   fx2_cleanups++;
   return 0;
}

static int host_fx2_control(void *handler, ngs2_user_fx2_control_context *context) {
   (void)handler;
   (void)context;
   return 0;
}

static int host_fx2_process(void *handler, ngs2_user_fx2_process_context *context) {
   (void)handler;
   (void)context;
   fx2_processes++;
   return 0;
}

static const ngs2_host host = {host_user_fx, host_fx2_setup, host_fx2_cleanup, host_fx2_control,
                               host_fx2_process};

/* Parameters as a game builds them */

typedef struct setup_param {
   ngs2_param_header header;
   ngs2_waveform_format format;
   uint32_t flags;
   uint32_t reserved;
} setup_param;

typedef struct blocks_param {
   ngs2_param_header header;
   const void *data;
   uint32_t flags;
   uint32_t num_blocks;
   const ngs2_waveform_block *blocks;
} blocks_param;

typedef struct two_u32_param {
   ngs2_param_header header;
   uint32_t a;
   uint32_t b;
} two_u32_param;

typedef struct patch_param {
   ngs2_param_header header;
   uint32_t port;
   uint32_t dest_input;
   uintptr_t dest;
} patch_param;

typedef struct levels_param {
   ngs2_param_header header;
   uint32_t matrix;
   uint32_t num_levels;
   const float *levels;
} levels_param;

typedef struct volume_param {
   ngs2_param_header header;
   uint32_t port;
   float level;
} volume_param;

typedef struct callback_param {
   ngs2_param_header header;
   void *handler;
   uintptr_t data;
   uint32_t flags;
   uint32_t reserved;
} callback_param;

typedef struct handler_param {
   ngs2_param_header header;
   void *handler;
   uintptr_t data[3];
} handler_param;

typedef struct pitch_param {
   ngs2_param_header header;
   float ratio;
   uint32_t reserved;
} pitch_param;

typedef struct i3dl2_param {
   ngs2_param_header header;
   ngs2_i3dl2 i3dl2;
} i3dl2_param;

typedef struct gain_param {
   ngs2_param_header header;
   float levels[NGS2_MAX_CHANNELS];
} gain_param;

static void head(ngs2_param_header *h, size_t size, uint32_t id) {
   h->size = (uint16_t)size;
   h->next = 0;
   h->id = id;
}

static int event(ngs2_voice *v, uint32_t id) {
   two_u32_param p;
   memset(&p, 0, sizeof(p));
   head(&p.header, 12, 6);
   p.a = id;
   return ngs2_voice_control(v, &p.header);
}

static int patch(ngs2_voice *v, uint32_t port, ngs2_voice *dest) {
   patch_param p;
   memset(&p, 0, sizeof(p));
   head(&p.header, sizeof(p), 5);
   p.port = port;
   p.dest = (uintptr_t)dest;
   return ngs2_voice_control(v, &p.header);
}

static int mixer_setup(ngs2_voice *v, uint32_t id, uint32_t in, uint32_t out) {
   two_u32_param p;
   memset(&p, 0, sizeof(p));
   head(&p.header, sizeof(p), id);
   p.a = in;
   p.b = out;
   return ngs2_voice_control(v, &p.header);
}

static int sampler_setup(ngs2_voice *v, uint32_t type, uint32_t channels, uint32_t rate) {
   setup_param p;
   memset(&p, 0, sizeof(p));
   head(&p.header, sizeof(p), 0x10000000u);
   p.format.type = type;
   p.format.channels = channels;
   p.format.sample_rate = rate;
   return ngs2_voice_control(v, &p.header);
}

static int add_block(ngs2_voice *v, const void *data, uint32_t bytes, uint32_t samples,
      uint32_t repeats, uint32_t flags, uintptr_t user) {
   blocks_param p;
   ngs2_waveform_block b;
   memset(&p, 0, sizeof(p));
   memset(&b, 0, sizeof(b));
   b.data_size = bytes;
   b.num_samples = samples;
   b.num_repeats = repeats;
   b.user_data = user;
   head(&p.header, sizeof(p), 0x10000001u);
   p.data = data;
   p.flags = flags;
   p.num_blocks = 1;
   p.blocks = &b;
   return ngs2_voice_control(v, &p.header);
}

/* A system with a sampler rack and a mastering rack, each in memory of its own */

typedef struct rig {
   void *system_mem, *sampler_mem, *master_mem, *extra_mem;
   ngs2_system *system;
   ngs2_rack *samplers, *masters, *extra;
   ngs2_voice *master;
} rig;

static void *aligned(size_t size, void **raw) {
   *raw = malloc(size + 16);
   return (void *)(((uintptr_t)*raw + 15u) & ~(uintptr_t)15u);
}

static ngs2_rack *make_rack(ngs2_system *system, const ngs2_rack_desc *desc, void **raw) {
   size_t size = ngs2_rack_size(desc);
   CHECK(size != 0);
   return ngs2_rack_init(system, aligned(size, raw), size, desc);
}

static void base_desc(ngs2_rack_desc *d, uint32_t id) {
   memset(d, 0, sizeof(*d));
   d->rack_id = id;
   d->max_grain_samples = 512;
   d->max_voices = 4;
   d->max_matrices = 2;
   d->max_ports = 4;
   d->max_channels = 8;
   d->max_inputs = 1;
   d->num_buffers = 1;
   d->max_waveform_blocks = 4;
   d->max_filters = 2;
   d->max_atrac9_decoders = 4;
}

static void rig_open(rig *r, uint32_t rate) {
   ngs2_rack_desc d;
   size_t size = ngs2_system_size(512);
   memset(r, 0, sizeof(*r));
   CHECK(size != 0);
   r->system = ngs2_system_init(aligned(size, &r->system_mem), size, "test", 512, GRAIN, rate,
         &host);
   CHECK(r->system != NULL);
   CHECK(ngs2_system_from((uintptr_t)r->system) == r->system);
   base_desc(&d, NGS2_RACK_SAMPLER);
   r->samplers = make_rack(r->system, &d, &r->sampler_mem);
   base_desc(&d, NGS2_RACK_MASTERING);
   d.max_voices = 1;
   d.max_ports = 0;
   d.max_matrices = 0;
   r->masters = make_rack(r->system, &d, &r->master_mem);
   CHECK(r->samplers && r->masters);
   r->master = ngs2_rack_voice(r->masters, 0);
   CHECK(mixer_setup(r->master, 0x30000000u, 2, 0) == NGS2_OK);
   CHECK(event(r->master, NGS2_EVENT_PLAY) == NGS2_OK);
}

static void rig_close(rig *r) {
   ngs2_system_destroy(r->system);
   CHECK(ngs2_system_from((uintptr_t)r->system) == NULL);
   free(r->system_mem);
   free(r->sampler_mem);
   free(r->master_mem);
   free(r->extra_mem);
}

static void render(rig *r, float *out, uint32_t channels) {
   ngs2_render_buffer b;
   b.buffer = out;
   b.buffer_size = GRAIN * channels * sizeof(float);
   b.waveform_type = NGS2_WAVE_PCM_F32L;
   b.channels = channels;
   CHECK(ngs2_system_render(r->system, &b, 1) == NGS2_OK);
}

static int finite_all(const float *x, size_t n) {
   size_t i;
   for (i = 0; i < n; i++)
      if (x[i] != x[i] || x[i] > 100.0f || x[i] < -100.0f)
         return 0;
   return 1;
}

/* A 16-bit ramp through a sampler into a stereo mastering voice comes out as
 * the ramp, three samples late, at the level a centre has in stereo. */
static void test_passthrough(void) {
   rig r;
   int16_t pcm[1024];
   float out[GRAIN * 2];
   ngs2_voice *v;
   unsigned i, g;
   ngs2_sampler_state state;
   rig_open(&r, RATE);
   for (i = 0; i < 1024; i++)
      pcm[i] = (int16_t)(i * 16);
   v = ngs2_rack_voice(r.samplers, 0);
   CHECK(ngs2_voice_from((uintptr_t)v) == v);
   CHECK(ngs2_voice_state_flags(v) == 0);
   CHECK(event(v, NGS2_EVENT_PLAY) == NGS2_OK); /* not set up: nothing happens */
   CHECK(ngs2_voice_state_flags(v) == 0);
   CHECK(sampler_setup(v, NGS2_WAVE_PCM_I16L, 1, RATE) == NGS2_OK);
   CHECK(ngs2_voice_state_flags(v) == NGS2_STATE_INUSE);
   CHECK(add_block(v, pcm, sizeof(pcm), 1024, 0, 0, 77) == NGS2_OK);
   CHECK(patch(v, 0, r.master) == NGS2_OK);
   CHECK(patch(v, 1, v) != NGS2_OK);
   CHECK(event(v, NGS2_EVENT_PLAY) == NGS2_OK);
   for (g = 0; g < 4; g++) {
      render(&r, out, 2);
      for (i = 0; i < GRAIN; i++) {
         unsigned at = g * GRAIN + i;
         float want = at < 3 ? 0.0f : (float)pcm[at - 3] / 32768.0f * 0.70710678f;
         if (fabs(out[i * 2] - want) > 1e-6 || fabs(out[i * 2 + 1] - want) > 1e-6) {
            CHECK(!"sample differs");
            break;
         }
      }
      CHECK(ngs2_voice_state_flags(v) == (NGS2_STATE_INUSE | NGS2_STATE_PLAYING));
   }
   ngs2_voice_sampler_state(v, &state);
   CHECK(state.decoded_samples == 1024);
   CHECK(state.decoded_bytes == sizeof(pcm));
   CHECK(state.user_data == 77);
   /* The waveform is at its end: one more grain and the voice has stopped */
   render(&r, out, 2);
   render(&r, out, 2);
   CHECK(ngs2_voice_state_flags(v) == NGS2_STATE_INUSE);
   for (i = 0; i < GRAIN * 2; i++)
      CHECK(out[i] == 0.0f);
   /* Played again it starts from the top */
   CHECK(event(v, NGS2_EVENT_PLAY) == NGS2_OK);
   render(&r, out, 2);
   CHECK(fabs(out[2 * 100] - (float)pcm[97] / 32768.0f * 0.70710678f) < 1e-6);
   CHECK(event(v, NGS2_EVENT_KILL) == NGS2_OK);
   CHECK(ngs2_voice_state_flags(v) == 0);
   render(&r, out, 2);
   for (i = 0; i < GRAIN * 2; i++)
      CHECK(out[i] == 0.0f);
   rig_close(&r);
}

/* A sine at another rate and pitch stays that sine */
static void test_resample(void) {
   rig r;
   static float wave[24000];
   static float out[GRAIN * 2];
   ngs2_voice *v;
   unsigned i, g;
   double worst = 0.0;
   pitch_param p;
   rig_open(&r, RATE);
   /* 220 Hz at 32 kHz, played 1.25 times as fast: 275 Hz */
   for (i = 0; i < 24000; i++)
      wave[i] = (float)(0.5 * sin(2.0 * PI * 220.0 * i / 32000.0));
   v = ngs2_rack_voice(r.samplers, 1);
   CHECK(sampler_setup(v, NGS2_WAVE_PCM_F32L, 1, 32000) == NGS2_OK);
   CHECK(add_block(v, wave, sizeof(wave), 24000, 0, 0, 0) == NGS2_OK);
   CHECK(patch(v, 0, r.master) == NGS2_OK);
   memset(&p, 0, sizeof(p));
   head(&p.header, sizeof(p), 0x10000005u);
   p.ratio = 1.25f;
   CHECK(ngs2_voice_control(v, &p.header) == NGS2_OK);
   CHECK(event(v, NGS2_EVENT_PLAY) == NGS2_OK);
   for (g = 0; g < 40; g++) {
      render(&r, out, 2);
      for (i = 0; i < GRAIN; i++) {
         /* Three frames of the waveform late */
         double t = (double)(g * GRAIN + i) / RATE - 3.0 / (32000.0 * 1.25);
         double want = t < 0 ? 0.0 : 0.5 * sin(2.0 * PI * 275.0 * t) * 0.70710678;
         double diff = fabs(out[i * 2] - want);
         if (g > 0 && diff > worst)
            worst = diff;
      }
   }
   CHECK(worst < 2e-3);
   rig_close(&r);
}

static unsigned ends, repeats;
static uintptr_t last_user;

/* A block that goes round three times and one that plays once, with the game
 * told of each end */
static void test_blocks(void) {
   rig r;
   int16_t a[300], b[200];
   float out[GRAIN * 2];
   ngs2_voice *v;
   unsigned i, g, total = 0;
   callback_param cb;
   rig_open(&r, RATE);
   for (i = 0; i < 300; i++)
      a[i] = 8192;
   for (i = 0; i < 200; i++)
      b[i] = -8192;
   v = ngs2_rack_voice(r.samplers, 0);
   CHECK(sampler_setup(v, NGS2_WAVE_PCM_I16L, 1, RATE) == NGS2_OK);
   memset(&cb, 0, sizeof(cb));
   head(&cb.header, sizeof(cb), 7);
   cb.handler = (void *)(uintptr_t)0x1234;
   cb.data = 99;
   cb.flags = NGS2_CALLBACK_BLOCK_END | NGS2_CALLBACK_BLOCK_REPEAT;
   CHECK(ngs2_voice_control(v, &cb.header) == NGS2_OK);
   CHECK(add_block(v, a, sizeof(a), 300, 3, 0, 1) == NGS2_OK);
   CHECK(add_block(v, b, sizeof(b), 200, 0, 0, 2) == NGS2_OK);
   CHECK(patch(v, 0, r.master) == NGS2_OK);
   CHECK(event(v, NGS2_EVENT_PLAY) == NGS2_OK);
   ends = repeats = 0;
   for (g = 0; g < 8; g++) {
      void *handler;
      ngs2_callback_info info;
      render(&r, out, 2);
      while (ngs2_system_next_callback(r.system, &handler, &info)) {
         CHECK(handler == (void *)(uintptr_t)0x1234);
         CHECK(info.callback_data == 99);
         CHECK(info.voice_handle == (uintptr_t)v);
         if (info.flag == NGS2_CALLBACK_BLOCK_REPEAT)
            repeats++;
         else
            ends++;
         last_user = info.user_data;
      }
      for (i = 0; i < GRAIN; i++) {
         if (out[i * 2] > 0.1f)
            total++;
         if (out[i * 2] < -0.1f)
            total += 10000;
      }
   }
   CHECK(repeats == 2);
   CHECK(ends == 2);
   CHECK(last_user == 2);
   /* 900 samples of the first, 200 of the second */
   CHECK(total == 900 + 200 * 10000);
   CHECK(ngs2_voice_state_flags(v) == NGS2_STATE_INUSE);

   /* A stream: with more to come the voice waits, flagged empty, and goes on */
   CHECK(sampler_setup(v, NGS2_WAVE_PCM_I16L, 1, RATE) == NGS2_OK);
   CHECK(add_block(v, a, sizeof(a), 300, 0, NGS2_BLOCKS_CONTINUE, 0) == NGS2_OK);
   CHECK(patch(v, 0, r.master) == NGS2_OK);
   CHECK(event(v, NGS2_EVENT_PLAY) == NGS2_OK);
   render(&r, out, 2);
   render(&r, out, 2);
   render(&r, out, 2);
   CHECK(ngs2_voice_state_flags(v) == (NGS2_STATE_INUSE | NGS2_STATE_PLAYING | NGS2_STATE_EMPTY));
   CHECK(add_block(v, b, sizeof(b), 200, 0, 0, 0) == NGS2_OK);
   render(&r, out, 2);
   CHECK(out[2 * 100] < -0.1f);
   CHECK(ngs2_voice_state_flags(v) == (NGS2_STATE_INUSE | NGS2_STATE_PLAYING));
   render(&r, out, 2);
   render(&r, out, 2);
   CHECK(ngs2_voice_state_flags(v) == NGS2_STATE_INUSE);
   /* More blocks than the rack has room for */
   for (i = 0; i < 4; i++)
      CHECK(add_block(v, a, sizeof(a), 300, 0, 0, 0) == NGS2_OK);
   CHECK(add_block(v, a, sizeof(a), 300, 0, 0, 0) != NGS2_OK);
   rig_close(&r);
}

/* A matrix and a port volume place and scale a voice; a level that changes
 * slides; stop and pause fade a grain and go quiet */
static void test_levels(void) {
   rig r;
   static int16_t pcm[48000];
   float out[GRAIN * 2];
   float levels[2];
   ngs2_voice *v;
   unsigned i;
   levels_param lp;
   volume_param vp;
   two_u32_param pm;
   ngs2_port_info info;
   uint32_t count;
   float got[NGS2_MAX_LEVELS];
   rig_open(&r, RATE);
   for (i = 0; i < 48000; i++)
      pcm[i] = 16384;
   v = ngs2_rack_voice(r.samplers, 2);
   CHECK(sampler_setup(v, NGS2_WAVE_PCM_I16L, 1, RATE) == NGS2_OK);
   CHECK(add_block(v, pcm, sizeof(pcm), 48000, 0, 0, 0) == NGS2_OK);
   CHECK(patch(v, 0, r.master) == NGS2_OK);
   levels[0] = 0.25f; /* left */
   levels[1] = 1.0f;  /* right */
   memset(&lp, 0, sizeof(lp));
   head(&lp.header, sizeof(lp), 1);
   lp.matrix = 1;
   lp.num_levels = 2;
   lp.levels = levels;
   CHECK(ngs2_voice_control(v, &lp.header) == NGS2_OK);
   memset(&pm, 0, sizeof(pm));
   head(&pm.header, sizeof(pm), 3);
   pm.a = 0;
   pm.b = 1;
   CHECK(ngs2_voice_control(v, &pm.header) == NGS2_OK);
   pm.b = 7;
   CHECK(ngs2_voice_control(v, &pm.header) != NGS2_OK);
   memset(&vp, 0, sizeof(vp));
   head(&vp.header, sizeof(vp), 2);
   vp.port = 0;
   vp.level = 0.5f;
   CHECK(ngs2_voice_control(v, &vp.header) == NGS2_OK);
   CHECK(ngs2_voice_port_info(v, 0, &info) == NGS2_OK);
   CHECK(info.matrix_id == 1 && info.volume == 0.5f && info.dest_handle == (uintptr_t)r.master);
   CHECK(ngs2_voice_port_info(v, 9, &info) != NGS2_OK);
   CHECK(ngs2_voice_matrix_info(v, 1, &count, got) == NGS2_OK);
   CHECK(count == 2 && got[0] == 0.25f && got[1] == 1.0f);
   CHECK(event(v, NGS2_EVENT_PLAY) == NGS2_OK);
   render(&r, out, 2);
   render(&r, out, 2);
   CHECK(fabs(out[100 * 2] - 0.5f * 0.25f * 0.5f) < 1e-6);
   CHECK(fabs(out[100 * 2 + 1] - 0.5f * 1.0f * 0.5f) < 1e-6);
   /* Twice as loud: there by the end of the grain, half way in the middle */
   vp.level = 1.0f;
   CHECK(ngs2_voice_control(v, &vp.header) == NGS2_OK);
   render(&r, out, 2);
   CHECK(fabs(out[1] - 0.25f) < 1e-3);
   CHECK(fabs(out[128 * 2 + 1] - 0.375f) < 1e-3);
   render(&r, out, 2);
   CHECK(fabs(out[1] - 0.5f) < 1e-6);
   /* Pause: a grain that fades, then nothing; resume: a grain that rises */
   CHECK(event(v, NGS2_EVENT_PAUSE) == NGS2_OK);
   render(&r, out, 2);
   CHECK(fabs(out[1] - 0.5f) < 1e-6 && out[255 * 2 + 1] < 0.01f && out[255 * 2 + 1] > 0.0f);
   CHECK(ngs2_voice_state_flags(v) == (NGS2_STATE_INUSE | NGS2_STATE_PAUSED));
   render(&r, out, 2);
   CHECK(out[1] == 0.0f && out[255 * 2 + 1] == 0.0f);
   CHECK(event(v, NGS2_EVENT_RESUME) == NGS2_OK);
   render(&r, out, 2);
   CHECK(out[1] == 0.0f && out[255 * 2 + 1] > 0.49f);
   CHECK(ngs2_voice_state_flags(v) == (NGS2_STATE_INUSE | NGS2_STATE_PLAYING));
   CHECK(event(v, NGS2_EVENT_STOP) == NGS2_OK);
   render(&r, out, 2);
   CHECK(out[1] > 0.49f && out[255 * 2 + 1] < 0.01f);
   CHECK(ngs2_voice_state_flags(v) == NGS2_STATE_INUSE);
   render(&r, out, 2);
   CHECK(out[1] == 0.0f);
   rig_close(&r);
}

/* Sampler to submixer to reverb to mastering, the submixer with the game's
 * effect on it; then two submixers in a ring, which still render */
static void test_chain(void) {
   rig r;
   ngs2_rack_desc d;
   static int16_t pcm[4800];
   static float out[GRAIN * 2];
   ngs2_voice *v, *sub, *sub2;
   void *reverb_mem;
   ngs2_rack *reverbs;
   ngs2_voice *rev;
   handler_param hp;
   i3dl2_param ip;
   unsigned i, g, tail = 0;
   float peak = 0.0f;
   rig_open(&r, RATE);
   base_desc(&d, NGS2_RACK_SUBMIXER);
   r.extra = make_rack(r.system, &d, &r.extra_mem);
   base_desc(&d, NGS2_RACK_REVERB);
   d.max_voices = 1;
   reverbs = make_rack(r.system, &d, &reverb_mem);
   CHECK(r.extra && reverbs);
   CHECK(ngs2_system_rack_count(r.system) == 4);
   for (i = 0; i < 4800; i++)
      pcm[i] = (int16_t)(16384.0 * sin(2.0 * PI * 440.0 * i / RATE));
   v = ngs2_rack_voice(r.samplers, 0);
   sub = ngs2_rack_voice(r.extra, 0);
   sub2 = ngs2_rack_voice(r.extra, 1);
   rev = ngs2_rack_voice(reverbs, 0);
   CHECK(sampler_setup(v, NGS2_WAVE_PCM_I16L, 1, RATE) == NGS2_OK);
   CHECK(add_block(v, pcm, sizeof(pcm), 4800, 0, 0, 0) == NGS2_OK);
   CHECK(mixer_setup(sub, 0x20000000u, 2, 0) == NGS2_OK);
   CHECK(mixer_setup(rev, 0x20010000u, 2, 2) == NGS2_OK);
   CHECK(patch(v, 0, sub) == NGS2_OK);
   CHECK(patch(sub, 0, rev) == NGS2_OK);
   CHECK(patch(rev, 0, r.master) == NGS2_OK);
   memset(&hp, 0, sizeof(hp));
   head(&hp.header, sizeof(hp), 0x20000004u);
   hp.handler = (void *)(uintptr_t)1;
   CHECK(ngs2_voice_control(sub, &hp.header) == NGS2_OK);
   memset(&ip, 0, sizeof(ip));
   head(&ip.header, sizeof(ip), 0x20010001u);
   ip.i3dl2.wet = 1.0f;
   ip.i3dl2.dry = 1.0f;
   ip.i3dl2.room = 0;
   ip.i3dl2.decay_time = 0.4f;
   ip.i3dl2.decay_hf_ratio = 0.5f;
   ip.i3dl2.reflections = -600;
   ip.i3dl2.reflections_delay = 0.01f;
   ip.i3dl2.reverb = 0;
   ip.i3dl2.reverb_delay = 0.02f;
   CHECK(ngs2_voice_control(rev, &ip.header) == NGS2_OK);
   CHECK(event(sub, NGS2_EVENT_PLAY) == NGS2_OK);
   CHECK(event(rev, NGS2_EVENT_PLAY) == NGS2_OK);
   CHECK(event(v, NGS2_EVENT_PLAY) == NGS2_OK);
   fx_calls = 0;
   for (g = 0; g < 400; g++) {
      float loud = 0.0f;
      render(&r, out, 2);
      CHECK(finite_all(out, GRAIN * 2));
      for (i = 0; i < GRAIN * 2; i++)
         if (fabs(out[i]) > loud)
            loud = (float)fabs(out[i]);
      if (g == 5) {
         /* The dry sound, halved by the effect, with the reverb on it */
         CHECK(loud > 0.1f && loud < 1.0f);
         peak = loud;
      }
      /* The waveform is 19 grains long: what sounds after is the reverb's */
      if (g > 25 && loud > 1e-5f)
         tail = g;
   }
   CHECK(peak > 0.0f);
   CHECK(fx_calls == 400);
   CHECK(tail > 30);  /* it rang on */
   CHECK(tail < 399); /* and rang out */
   /* A ring */
   CHECK(mixer_setup(sub2, 0x20000000u, 2, 0) == NGS2_OK);
   CHECK(patch(sub, 1, sub2) == NGS2_OK);
   CHECK(patch(sub2, 0, sub) == NGS2_OK);
   CHECK(event(sub2, NGS2_EVENT_PLAY) == NGS2_OK);
   CHECK(event(v, NGS2_EVENT_PLAY) == NGS2_OK);
   for (g = 0; g < 4; g++) {
      render(&r, out, 2);
      CHECK(finite_all(out, GRAIN * 2));
   }
   /* The rack a voice sends into goes away: the voice sends nowhere */
   ngs2_rack_destroy(reverbs);
   CHECK(ngs2_rack_from((uintptr_t)reverbs) == NULL);
   CHECK(ngs2_voice_from((uintptr_t)rev) == NULL);
   render(&r, out, 2);
   for (i = 0; i < GRAIN * 2; i++)
      CHECK(out[i] == 0.0f);
   free(reverb_mem);
   rig_close(&r);
}

/* A custom submixer: a gain, the game's two kinds of effect and a module the
 * engine has not, which passes the sound on */
static void test_custom(void) {
   rig r;
   ngs2_rack_desc d;
   static int16_t pcm[9600];
   float out[GRAIN * 2];
   ngs2_voice *v, *sub;
   gain_param gp;
   handler_param hp;
   unsigned i;
   rig_open(&r, RATE);
   base_desc(&d, NGS2_RACK_CUSTOM_SUBMIXER);
   d.max_voices = 2;
   d.num_modules = 4;
   d.modules[0].id = 0x15;
   d.modules[1].id = 0x19;
   d.modules[2].id = 0x1f;
   d.modules[2].setup_handler = (void *)(uintptr_t)1;
   d.modules[2].cleanup_handler = (void *)(uintptr_t)1;
   d.modules[2].process_handler = (void *)(uintptr_t)1;
   d.modules[2].common_size = 32;
   d.modules[2].param_size = 16;
   d.modules[2].work_size = 64;
   d.modules[2].state_size = 8;
   d.modules[3].id = 0x1d;
   fx2_setups = fx2_cleanups = fx2_processes = 0;
   r.extra = make_rack(r.system, &d, &r.extra_mem);
   CHECK(r.extra != NULL);
   CHECK(fx2_setups == 2);
   for (i = 0; i < 9600; i++)
      pcm[i] = 16384;
   v = ngs2_rack_voice(r.samplers, 0);
   sub = ngs2_rack_voice(r.extra, 1);
   CHECK(sampler_setup(v, NGS2_WAVE_PCM_I16L, 2, RATE) == NGS2_OK);
   CHECK(add_block(v, pcm, sizeof(pcm), 4800, 0, 0, 0) == NGS2_OK);
   CHECK(mixer_setup(sub, 0x40020000u, 2, 2) == NGS2_OK);
   CHECK(patch(v, 0, sub) == NGS2_OK);
   CHECK(patch(sub, 0, r.master) == NGS2_OK);
   memset(&gp, 0, sizeof(gp));
   head(&gp.header, sizeof(gp), 0x40001500u);
   gp.levels[0] = 0.5f;
   gp.levels[1] = 2.0f;
   CHECK(ngs2_voice_control(sub, &gp.header) == NGS2_OK);
   memset(&hp, 0, sizeof(hp));
   head(&hp.header, sizeof(hp), 0x40001901u);
   hp.handler = (void *)(uintptr_t)1;
   CHECK(ngs2_voice_control(sub, &hp.header) == NGS2_OK);
   {
      handler_param fx2; /* data, size */
      memset(&fx2, 0, sizeof(fx2));
      head(&fx2.header, 24, 0x40001f02u);
      CHECK(ngs2_voice_control(sub, &fx2.header) == NGS2_OK);
   }
   /* A parameter for a module that is not there is let by */
   head(&gp.header, sizeof(gp), 0x40001509u);
   CHECK(ngs2_voice_control(sub, &gp.header) == NGS2_OK);
   CHECK(event(sub, NGS2_EVENT_PLAY) == NGS2_OK);
   CHECK(event(v, NGS2_EVENT_PLAY) == NGS2_OK);
   fx_calls = 0;
   render(&r, out, 2);
   render(&r, out, 2);
   /* 0.5 in, times the gain, halved by the effect */
   CHECK(fabs(out[100 * 2] - 0.5f * 0.5f * 0.5f) < 1e-6);
   CHECK(fabs(out[100 * 2 + 1] - 0.5f * 2.0f * 0.5f) < 1e-6);
   CHECK(fx_calls == 2);
   CHECK(fx2_processes == 2);
   rig_close(&r);
   CHECK(fx2_cleanups == 2);
}

/* A VAG waveform, sixteen bits out, and a filter: none of it by number,
 * all of it finite and where it should be */
static void test_formats(void) {
   rig r;
   static uint8_t vag[16 * 64];
   static int16_t pcm[4800];
   int16_t out16[GRAIN * 2];
   float out[GRAIN * 6];
   ngs2_voice *v;
   ngs2_render_buffer b;
   unsigned i;
   struct {
      ngs2_param_header header;
      uint32_t index, location, type, mask;
      float values[5];
      uint32_t reserved;
   } fp;
   rig_open(&r, RATE);
   /* Frames with no prediction and a shift of 4: each nibble 7 is 0x700 */
   for (i = 0; i < 64; i++) {
      vag[i * 16] = 0x04;
      memset(&vag[i * 16 + 2], 0x77, 14);
   }
   v = ngs2_rack_voice(r.samplers, 0);
   CHECK(sampler_setup(v, NGS2_WAVE_VAG, 1, RATE) == NGS2_OK);
   CHECK(add_block(v, vag, sizeof(vag), 28 * 64, 0, 0, 0) == NGS2_OK);
   CHECK(patch(v, 0, r.master) == NGS2_OK);
   CHECK(event(v, NGS2_EVENT_PLAY) == NGS2_OK);
   b.buffer = out16;
   b.buffer_size = sizeof(out16);
   b.waveform_type = NGS2_WAVE_PCM_I16L;
   b.channels = 2;
   CHECK(ngs2_system_render(r.system, &b, 1) == NGS2_OK);
   CHECK(abs(out16[200] - (int)(0x700 * 0.70710678 * 32767.0 / 32768.0)) <= 1);
   b.buffer_size = 16;
   CHECK(ngs2_system_render(r.system, &b, 1) != NGS2_OK);
   /* A low pass far under a tone leaves little of it */
   for (i = 0; i < 4800; i++)
      pcm[i] = (int16_t)(16384.0 * sin(2.0 * PI * 8000.0 * i / RATE));
   CHECK(sampler_setup(v, NGS2_WAVE_PCM_I16L, 1, RATE) == NGS2_OK);
   CHECK(add_block(v, pcm, sizeof(pcm), 4800, 0, 0, 0) == NGS2_OK);
   CHECK(patch(v, 0, r.master) == NGS2_OK);
   memset(&fp, 0, sizeof(fp));
   head(&fp.header, sizeof(fp), 0x1000000au);
   fp.type = 1;
   fp.values[0] = 200.0f;
   fp.values[1] = 0.707f;
   fp.values[2] = 1.0f;
   CHECK(ngs2_voice_control(v, &fp.header) == NGS2_OK);
   CHECK(event(v, NGS2_EVENT_PLAY) == NGS2_OK);
   render(&r, out, 2);
   render(&r, out, 2);
   CHECK(finite_all(out, GRAIN * 2));
   for (i = 0; i < GRAIN * 2; i++)
      CHECK(fabs(out[i]) < 0.01f);
   /* Stereo into six channels: left and right where they belong, the rest silent */
   CHECK(mixer_setup(r.master, 0x30000000u, 2, 0) == NGS2_OK);
   CHECK(event(r.master, NGS2_EVENT_PLAY) == NGS2_OK);
   CHECK(sampler_setup(v, NGS2_WAVE_PCM_I16L, 1, RATE) == NGS2_OK);
   for (i = 0; i < 4800; i++)
      pcm[i] = 16384;
   CHECK(add_block(v, pcm, sizeof(pcm), 4800, 0, 0, 0) == NGS2_OK);
   CHECK(patch(v, 0, r.master) == NGS2_OK);
   CHECK(event(v, NGS2_EVENT_PLAY) == NGS2_OK);
   render(&r, out, 6);
   CHECK(out[100 * 6] > 0.3f && out[100 * 6 + 1] > 0.3f);
   CHECK(out[100 * 6 + 2] == 0.0f && out[100 * 6 + 3] == 0.0f && out[100 * 6 + 5] == 0.0f);
   /* An ATRAC9 configuration that is none */
   {
      setup_param p;
      memset(&p, 0, sizeof(p));
      head(&p.header, sizeof(p), 0x10000000u);
      p.format.type = NGS2_WAVE_ATRAC9;
      p.format.channels = 2;
      p.format.sample_rate = RATE;
      p.format.config_data = 0x12345678u;
      CHECK(ngs2_voice_control(v, &p.header) != NGS2_OK);
   }
   /* A list whose parameters point at one another for ever */
   {
      two_u32_param ring[2];
      memset(ring, 0, sizeof(ring));
      head(&ring[0].header, 16, 2);
      head(&ring[1].header, 16, 2);
      ring[0].header.next = (int16_t)sizeof(ring[0]);
      ring[1].header.next = -(int16_t)sizeof(ring[0]);
      CHECK(ngs2_voice_control(v, &ring[0].header) == NGS2_ERROR_DETECTED_CIRCULAR_VOICE_CONTROL);
   }
   CHECK(ngs2_default_level(1, 0, 2, 0) > 0.7f && ngs2_default_level(1, 0, 2, 0) < 0.71f);
   CHECK(ngs2_default_level(2, 0, 8, 0) == 1.0f && ngs2_default_level(2, 0, 8, 1) == 0.0f);
   CHECK(ngs2_default_level(8, 6, 6, 4) == 1.0f && ngs2_default_level(6, 3, 2, 0) == 0.0f);
   rig_close(&r);
}


/* ATRAC9 through a sampler is what the decoder gives for the block's frames,
 * less the samples the block skips, and no more than it says */
static void test_atrac9(void) {
   rig r;
   static float want[2][8 * 64];
   float frame[2 * 64];
   float out[GRAIN * 2];
   ratrac9_t *d = ratrac9_open(at9_config);
   ngs2_voice *v;
   unsigned i, g, order;
   setup_param p;
   blocks_param bp;
   ngs2_waveform_block b;
   CHECK(d != NULL);
   CHECK(ratrac9_frame_bytes(d) == 164 && ratrac9_frame_samples(d) == 64);
   for (i = 0; i < 8; i++) {
      size_t used;
      CHECK(ratrac9_decode_f32(d, at9_data + i * 164, 164, frame, true, &used) == 0);
      memcpy(&want[0][i * 64], frame, 64 * sizeof(float));
      memcpy(&want[1][i * 64], frame + 64, 64 * sizeof(float));
   }
   ratrac9_close(d);
   /* The configuration word in either byte order */
   for (order = 0; order < 2; order++) {
      rig_open(&r, RATE);
      v = ngs2_rack_voice(r.samplers, 3);
      memset(&p, 0, sizeof(p));
      head(&p.header, sizeof(p), 0x10000000u);
      p.format.type = NGS2_WAVE_ATRAC9;
      p.format.channels = 2;
      p.format.sample_rate = RATE;
      p.format.config_data = order ? 0x701494feu : 0xfe941470u;
      CHECK(ngs2_voice_control(v, &p.header) == NGS2_OK);
      memset(&b, 0, sizeof(b));
      b.data_size = sizeof(at9_data);
      b.num_skip_samples = 10;
      b.num_samples = 400;
      memset(&bp, 0, sizeof(bp));
      head(&bp.header, sizeof(bp), 0x10000001u);
      bp.data = at9_data;
      bp.num_blocks = 1;
      bp.blocks = &b;
      CHECK(ngs2_voice_control(v, &bp.header) == NGS2_OK);
      CHECK(patch(v, 0, r.master) == NGS2_OK);
      CHECK(event(v, NGS2_EVENT_PLAY) == NGS2_OK);
      for (g = 0; g < 2; g++) {
         render(&r, out, 2);
         for (i = 0; i < GRAIN; i++) {
            unsigned at = g * GRAIN + i;
            float l = at < 3 || at - 3 >= 400 ? 0.0f : want[0][at - 3 + 10];
            float rr = at < 3 || at - 3 >= 400 ? 0.0f : want[1][at - 3 + 10];
            if (fabs(out[i * 2] - l) > 1e-6 || fabs(out[i * 2 + 1] - rr) > 1e-6) {
               CHECK(!"ATRAC9 sample differs");
               break;
            }
         }
      }
      /* A second voice with the configuration, and the first again after it was killed:
       * each has a decoder */
      CHECK(ngs2_voice_control(ngs2_rack_voice(r.samplers, 0), &p.header) == NGS2_OK);
      CHECK(event(v, NGS2_EVENT_KILL) == NGS2_OK);
      CHECK(ngs2_voice_control(v, &p.header) == NGS2_OK);
      rig_close(&r);
   }
}

int main(void) {
   test_passthrough();
   test_resample();
   test_blocks();
   test_levels();
   test_chain();
   test_custom();
   test_formats();
   test_atrac9();
   if (failures) {
      printf("ngs2_core_test: %d failed\n", failures);
      return 1;
   }
   printf("ngs2_core_test: passed\n");
   return 0;
}
