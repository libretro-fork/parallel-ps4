/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <math.h>
#include <string.h>

#include <retro_atomic.h>

#include "audio_mix.h"

/* A frame in an input: a float a channel, two (stereo) or eight (7.1, with surround). A
 * source's sound stays at full precision until the mix is made, and is rounded to 16 bits once,
 * there, if at all: not once by each source and again after the sum, and never cut towards
 * zero.
 *
 * The inputs live in memory both processes share (audio_mix_shared_t, in the core's shared
 * state): the emulator's sources write them, the core mixes them, exactly one frame of every
 * input in retro_run. Indices, not pointers: each process has the memory at its own address.
 * Each input is a single-producer single-consumer ring of frame counts that only grow. */

/* An input's life: free in the pool, open to its source, or closed by its source and not yet
 * emptied by the mix. Only the source moves FREE to OPEN (by CAS, sources open from several
 * threads) and OPEN to CLOSING; only the mix moves CLOSING to FREE, after it has emptied the
 * ring, so the ring has one producer and one consumer at all times. */
enum
{
   INPUT_FREE = 0,
   INPUT_OPEN,
   INPUT_CLOSING
};

#define RING_MASK (AUDIO_MIX_INPUT_FRAMES - 1)

static audio_mix_shared_t* shm;
/* A pool of its own, for one process (audio_mix_init_channels: the tests) */
static audio_mix_shared_t own_pool;
static int own; /* the clip is mixed by the mix itself */

/* What a source waiting for room parks on, in its process: notified when the frames were
 * taken (audio_mix_notify_room, or the mix itself in one process) */
static retro_eventcount_t room_ec;
static int room_ec_made;

/* The clip: published by its caller with a serial the feeder checks before and after it reads
 * the fields, so a clip started again while being read is taken on the next pass. In the
 * emulator it is fed into an input of its own (audio_mix_feed_clip); in one process the mix
 * adds it. */
static retro_atomic_size_t clip_frames; /* the frames' address */
static retro_atomic_int_t clip_count;
static retro_atomic_int_t clip_gain_milli;
static retro_atomic_int_t clip_serial;
/* The feeder's own: the clip it plays and how far it is. */
static int clip_seen;
static const float* clip_play;
static unsigned clip_left;
static float clip_play_gain;
static audio_mix_input_t* clip_in;

/* The resampler's kernel: a sinc windowed with a Blackman window over 16 taps, at cutoff 1,
 * for 256 fractional positions and one more to interpolate to, each row summing to 1. */
#define SINC_TAPS 16
#define SINC_HALF 7
#define SINC_PHASES 256
static float sinc_table[SINC_PHASES + 1][SINC_TAPS];
/* The mixing thread's sums: one chunk of frames, mix_channels floats apart (the first two
 * front left and right, then the rest of 7.1 in the frontend's order). */
#define PUMP_FRAMES 1024
static float pump_sum[PUMP_FRAMES * 8];
/* 2, or 8 with surround: the widest an input may be, set at init */
static unsigned mix_channels = 2;
/* An eight-channel input added frames to the last mix */
static int pump_wide;
/* Each input's write count at the mix before the last (audio_mix_frame_ready), and at the last */
static uint32_t mix_seen_write[AUDIO_MIX_INPUTS];
static uint32_t mix_last_write[AUDIO_MIX_INPUTS];
/* Inputs held back from this mix (audio_mix_mix): left as they are */
static unsigned char pump_hold[AUDIO_MIX_INPUTS];
/* Told of every commit, in the sources' process (audio_mix_set_written) */
static void (*written_fn)(void* ctx);
static void* written_ctx;

static double sinc_kernel(double x, double cutoff)
{
   const double pi = 3.14159265358979323846;
   double t        = x / (SINC_TAPS / 2.0); /* -1..1 across the taps */
   double w, v;
   if (t <= -1.0 || t >= 1.0)
      return 0.0;
   w = 0.42 + 0.5 * cos(pi * t) + 0.08 * cos(2.0 * pi * t);
   v = x == 0.0 ? cutoff : sin(pi * cutoff * x) / (pi * x);
   return v * w;
}

static void sinc_row(float* row, double frac, double cutoff)
{
   unsigned t;
   double sum = 0.0;
   double w[SINC_TAPS];
   for (t = 0; t < SINC_TAPS; t++)
   {
      w[t] = sinc_kernel((double)((int)t - SINC_HALF) - frac, cutoff);
      sum += w[t];
   }
   for (t = 0; t < SINC_TAPS; t++)
      row[t] = (float)(w[t] / sum);
}

static void make_room_ec(void)
{
   if (!room_ec_made)
   {
      retro_eventcount_init(&room_ec);
      room_ec_made = 1;
   }
}

static void make_tables(unsigned channels)
{
   unsigned i;
   mix_channels = channels == 8 ? 8 : 2;
   for (i = 0; i <= SINC_PHASES; i++)
      sinc_row(sinc_table[i], (double)i / SINC_PHASES, 1.0);
   retro_atomic_size_init(&clip_frames, 0);
   retro_atomic_int_init(&clip_count, 0);
   retro_atomic_int_init(&clip_gain_milli, 0);
   retro_atomic_int_init(&clip_serial, 0);
   clip_seen = 0;
   clip_left = 0;
   clip_in   = NULL;
   make_room_ec();
}

int audio_mix_attach(audio_mix_shared_t* pool, unsigned channels, int make)
{
   unsigned i;
   make_tables(channels);
   shm = pool;
   own = 0;
   if (make)
      for (i = 0; i < AUDIO_MIX_INPUTS; i++)
      {
         retro_atomic_int_init(&pool->inputs[i].state, INPUT_FREE);
         retro_atomic_int_init(&pool->inputs[i].channels, 2);
         retro_atomic_int_init(&pool->inputs[i].write, 0);
         retro_atomic_int_init(&pool->inputs[i].read, 0);
      }
   return 1;
}

int audio_mix_init(void)
{
   return audio_mix_init_channels(2);
}

int audio_mix_init_channels(unsigned channels)
{
   audio_mix_attach(&own_pool, channels, 1);
   own = 1;
   return 1;
}

audio_mix_input_t* audio_mix_input_open(void)
{
   return audio_mix_input_open_channels(2);
}

audio_mix_input_t* audio_mix_input_open_channels(unsigned channels)
{
   unsigned i;
   if (channels != 2 && !(channels == 8 && mix_channels == 8))
      return NULL;
   for (i = 0; i < AUDIO_MIX_INPUTS; i++)
   {
      audio_mix_input_t* in = &shm->inputs[i];
      if (retro_atomic_cas_int(&in->state, INPUT_FREE, INPUT_OPEN))
      {
         /* The mix reads it once it sees frames, which the source commits after this, with
          * release */
         retro_atomic_store_relaxed_int(&in->channels, (int)channels);
         retro_atomic_store_release_int(&in->waits_past, 0);
         return in;
      }
   }
   return NULL;
}

unsigned audio_mix_channels(void)
{
   return mix_channels;
}

void audio_mix_input_close(audio_mix_input_t* in)
{
   if (in)
      retro_atomic_store_release_int(&in->state, INPUT_CLOSING);
}

static unsigned queued_frames(audio_mix_input_t* in)
{
   return (unsigned)((uint32_t)retro_atomic_load_acquire_int(&in->write) -
                     (uint32_t)retro_atomic_load_acquire_int(&in->read));
}

int audio_mix_input_ready(audio_mix_input_t* in, unsigned frames, unsigned queue_frames)
{
   const unsigned queued = queued_frames(in);
   return queued <= queue_frames && AUDIO_MIX_INPUT_FRAMES - queued >= frames;
}

retro_eventcount_t* audio_mix_input_room_ec(audio_mix_input_t* in)
{
   (void)in;
   return &room_ec;
}

void audio_mix_notify_room(void)
{
   retro_eventcount_notify(&room_ec);
}

int audio_mix_input_wait(audio_mix_input_t* in, unsigned frames, int64_t timeout_us)
{
   for (;;)
   {
      const int key = retro_eventcount_prepare_wait(&room_ec);
      if (audio_mix_input_ready(in, frames, AUDIO_MIX_QUEUE_FRAMES))
      {
         retro_eventcount_cancel_wait(&room_ec);
         return 1;
      }
      if (!retro_eventcount_commit_wait_timeout(&room_ec, key, timeout_us))
         return audio_mix_input_ready(in, frames, AUDIO_MIX_QUEUE_FRAMES);
   }
}

unsigned audio_mix_input_reserve(audio_mix_input_t* in, float** ptr, unsigned frames)
{
   const uint32_t write    = (uint32_t)retro_atomic_load_relaxed_int(&in->write);
   const unsigned channels = (unsigned)retro_atomic_load_relaxed_int(&in->channels);
   const unsigned room     = AUDIO_MIX_INPUT_FRAMES - queued_frames(in);
   const unsigned to_wrap  = AUDIO_MIX_INPUT_FRAMES - (write & RING_MASK);
   unsigned got            = room < to_wrap ? room : to_wrap;
   *ptr                    = in->frames + (size_t)(write & RING_MASK) * channels;
   return got < frames ? got : frames;
}

void audio_mix_input_commit(audio_mix_input_t* in, unsigned frames)
{
   retro_atomic_store_release_int(&in->write,
                                  (int)((uint32_t)retro_atomic_load_relaxed_int(&in->write) +
                                        frames));
   if (written_fn)
      written_fn(written_ctx);
}

/* Adds what one input has, up to @frames, to the sums. Returns how many frames it had. */
static unsigned pump_add(audio_mix_input_t* in, unsigned frames)
{
   const unsigned channels = (unsigned)retro_atomic_load_relaxed_int(&in->channels);
   uint32_t read           = (uint32_t)retro_atomic_load_relaxed_int(&in->read);
   unsigned have           = queued_frames(in);
   unsigned done           = 0;
   if (have > frames)
      have = frames;
   while (done < have)
   {
      const unsigned at      = read & RING_MASK;
      const unsigned to_wrap = AUDIO_MIX_INPUT_FRAMES - at;
      unsigned got           = have - done < to_wrap ? have - done : to_wrap;
      const float* s         = in->frames + (size_t)at * channels;
      float* sum             = pump_sum + done * mix_channels;
      unsigned i, c;
      if (channels == mix_channels)
         for (i = 0; i < got * mix_channels; i++)
            sum[i] += s[i];
      else /* stereo into a wider mix: the front pair */
         for (i = 0; i < got; i++)
            for (c = 0; c < 2; c++)
               sum[i * mix_channels + c] += s[i * 2 + c];
      if (channels == 8)
         pump_wide = 1;
      read += got;
      done += got;
   }
   if (done)
      retro_atomic_store_release_int(&in->read, (int)read);
   return done;
}

void audio_mix_play_clip(const float* frames, unsigned count, float gain)
{
   retro_atomic_store_relaxed_size(&clip_frames, (size_t)frames);
   retro_atomic_store_relaxed_int(&clip_count, (int)count);
   retro_atomic_store_relaxed_int(&clip_gain_milli, (int)(gain * 1000.0f));
   retro_atomic_fetch_add_int(&clip_serial, 1);
}

/* Takes a newly started clip. */
static void clip_take(void)
{
   int serial = retro_atomic_load_acquire_int(&clip_serial);
   if (serial != clip_seen)
   {
      const float* f = (const float*)retro_atomic_load_relaxed_size(&clip_frames);
      unsigned count = (unsigned)retro_atomic_load_relaxed_int(&clip_count);
      float gain     = retro_atomic_load_relaxed_int(&clip_gain_milli) / 1000.0f;
      if (retro_atomic_load_acquire_int(&clip_serial) == serial)
      {
         clip_seen      = serial;
         clip_play      = f;
         clip_left      = count;
         clip_play_gain = gain;
      }
   }
}

/* One process: adds what is left of the playing clip to the sums. Returns the frames. */
static unsigned pump_clip(unsigned frames)
{
   unsigned i, n;
   clip_take();
   n = clip_left < frames ? clip_left : frames;
   if (!n) /* no clip: there is no pointer to step */
      return 0;
   for (i = 0; i < n; i++)
   {
      pump_sum[i * mix_channels + 0] += clip_play[i * 2 + 0] * clip_play_gain;
      pump_sum[i * mix_channels + 1] += clip_play[i * 2 + 1] * clip_play_gain;
   }
   clip_play += n * 2;
   clip_left -= n;
   return n;
}

void audio_mix_feed_clip(unsigned frames)
{
   unsigned i, n;
   float* dst = NULL;
   clip_take();
   if (!clip_left)
      return;
   if (!clip_in)
   {
      if (!(clip_in = audio_mix_input_open()))
         return;
      /* Fed as each frame starts, a frame at a time: nothing to wait for */
      audio_mix_input_waits_past(clip_in, AUDIO_MIX_NEVER_WAITED);
   }
   while (frames && clip_left && (n = audio_mix_input_reserve(clip_in, &dst, frames)) > 0)
   {
      if (n > clip_left)
         n = clip_left;
      for (i = 0; i < n * 2; i++)
         dst[i] = clip_play[i] * clip_play_gain;
      audio_mix_input_commit(clip_in, n);
      clip_play += n * 2;
      clip_left -= n;
      frames -= n;
   }
}

/* The one rounding to 16 bits: to the nearest step, halves away from zero, and clamped.
 * Not cut towards zero, which makes the step around silence twice as wide as the others and
 * pulls every sample a little towards zero (distortion that is loudest in quiet passages). */
static int16_t round_s16(float v)
{
   if (v >= 32766.5f)
      return 32767;
   if (v <= -32767.5f)
      return -32768;
   return (int16_t)(v < 0.0f ? v - 0.5f : v + 0.5f);
}

/* Every open input's frames, up to @frames, summed into pump_sum (zeroed for all @frames: an
 * input with fewer adds silence for the rest). Returns the most any had. */
static unsigned pump_mix(unsigned frames)
{
   unsigned i;
   unsigned most = 0;
   if (frames > PUMP_FRAMES)
      frames = PUMP_FRAMES;
   memset(pump_sum, 0, frames * mix_channels * sizeof(pump_sum[0]));
   pump_wide = 0;
   if (own)
      most = pump_clip(frames);

   for (i = 0; i < AUDIO_MIX_INPUTS; i++)
   {
      audio_mix_input_t* in = &shm->inputs[i];
      int state             = retro_atomic_load_acquire_int(&in->state);
      if (state == INPUT_OPEN && !pump_hold[i])
      {
         unsigned got = pump_add(in, frames);
         if (got > most)
            most = got;
      }
      else if (state == INPUT_CLOSING)
      {
         /* Its source has gone: what it left is dropped, and the input goes back. */
         retro_atomic_store_release_int(&in->read, retro_atomic_load_acquire_int(&in->write));
         retro_atomic_store_release_int(&in->state, INPUT_FREE);
      }
   }
   /* Room made: in one process, the sources waiting for it are told here */
   if (own)
      retro_eventcount_notify(&room_ec);
   return most;
}

unsigned audio_mix_pump(int16_t* ring, uint32_t ring_mask, uint32_t at, unsigned frames)
{
   unsigned i;
   const unsigned most = pump_mix(frames);
   for (i = 0; i < most; i++)
   {
      uint32_t pos  = ((at + i) & ring_mask) * 2;
      ring[pos + 0] = round_s16(pump_sum[i * mix_channels + 0]);
      ring[pos + 1] = round_s16(pump_sum[i * mix_channels + 1]);
   }
   return most;
}

/* The sum to the frontend's float scale, -1 to 1: divided by the one scale, 32768, and
 * clamped, as the 16-bit path clamps; not rounded */
static float to_unit(float v)
{
   v *= 1.0f / 32768.0f;
   return v > 1.0f ? 1.0f : v < -1.0f ? -1.0f : v;
}

unsigned audio_mix_pump_f32(float* ring, uint32_t ring_mask, uint32_t at, unsigned frames)
{
   unsigned i;
   const unsigned most = pump_mix(frames);
   for (i = 0; i < most; i++)
   {
      uint32_t pos  = ((at + i) & ring_mask) * 2;
      ring[pos + 0] = to_unit(pump_sum[i * mix_channels + 0]);
      ring[pos + 1] = to_unit(pump_sum[i * mix_channels + 1]);
   }
   return most;
}

unsigned audio_mix_mix(unsigned frames, unsigned* channels)
{
   unsigned i, most;
   /* A source still writing (it wrote in the last frame) that has less than the frame is held
    * back whole, not mixed with silence after it (a gap): it is a source starting, whose
    * first sound lands mid-frame, or one the frame ended without (retro_run waited as long as
    * the game ran). It plays from the next frame, unbroken, its sound a frame later at most. */
   for (i = 0; i < AUDIO_MIX_INPUTS; i++)
   {
      audio_mix_input_t* in = &shm->inputs[i];
      const unsigned have   = queued_frames(in);
      pump_hold[i]          = retro_atomic_load_acquire_int(&in->state) == INPUT_OPEN &&
                     (uint32_t)retro_atomic_load_acquire_int(&in->write) != mix_seen_write[i] &&
                     have && have < frames;
   }
   most = pump_mix(frames);
   for (i = 0; i < AUDIO_MIX_INPUTS; i++)
   {
      audio_mix_input_t* in = &shm->inputs[i];
      pump_hold[i]          = 0;
      /* Where each input's source had written to, for audio_mix_frame_ready */
      mix_seen_write[i] = mix_last_write[i];
      mix_last_write[i] = (uint32_t)retro_atomic_load_acquire_int(&in->write);
      /* Surround while an eight-channel input is open, with sound this frame or not: the
       * layout does not flip with each frame an input is silent (a pop as the frontend
       * changes it) */
      if (retro_atomic_load_acquire_int(&in->state) == INPUT_OPEN &&
          retro_atomic_load_relaxed_int(&in->channels) == 8)
         pump_wide = 1;
   }
   *channels = pump_wide ? 8 : 2;
   return most;
}

int audio_mix_frame_ready(unsigned frames, int frame)
{
   unsigned i;
   for (i = 0; i < AUDIO_MIX_INPUTS; i++)
   {
      audio_mix_input_t* in = &shm->inputs[i];
      int waits;
      if (retro_atomic_load_acquire_int(&in->state) != INPUT_OPEN ||
          (uint32_t)retro_atomic_load_acquire_int(&in->write) == mix_seen_write[i] ||
          queued_frames(in) >= frames)
         continue;
      waits = retro_atomic_load_acquire_int(&in->waits_past);
      if (waits != AUDIO_MIX_NEVER_WAITED && waits != frame)
         return 0;
   }
   return 1;
}

void audio_mix_input_waits_past(audio_mix_input_t* in, int frame)
{
   retro_atomic_store_release_int(&in->waits_past, frame);
}

void audio_mix_set_written(void (*written)(void* ctx), void* ctx)
{
   written_ctx = ctx;
   written_fn  = written;
}

void audio_mix_take_f32(float* dst, unsigned first, unsigned count, unsigned channels)
{
   unsigned i, c;
   const float* sum = pump_sum + first * mix_channels;
   for (i = 0; i < count; i++)
      for (c = 0; c < channels; c++)
         dst[i * channels + c] = to_unit(sum[i * mix_channels + c]);
}

void audio_mix_take_s16(int16_t* dst, unsigned first, unsigned count, unsigned channels)
{
   unsigned i, c;
   const float* sum = pump_sum + first * mix_channels;
   for (i = 0; i < count; i++)
      for (c = 0; c < channels; c++)
         dst[i * channels + c] = round_s16(sum[i * mix_channels + c]);
}

void audio_mix_resampler_init(audio_mix_resampler_t* r, unsigned src_hz, unsigned channels)
{
   r->step     = ((uint64_t)src_hz << 32) / 48000u;
   r->cutoff   = src_hz > 48000u ? 48000.0f / (float)src_hz : 1.0f;
   r->channels = channels == 1 ? 1 : 2;
}

/* The source is 16-bit (@s16) or float in -1..1 (@f32, scaled to 16-bit steps by @scale) */
static unsigned resample(const audio_mix_resampler_t* r, const int16_t* s16, const float* f32,
                         float scale, uint64_t src_frames, uint64_t* pos, float* out,
                         unsigned out_frames, float gain)
{
   unsigned n;
   unsigned ch   = r->channels;
   uint64_t p    = *pos;
   float row[SINC_TAPS];
   gain *= scale;
   for (n = 0; n < out_frames; n++)
   {
      uint64_t idx = p >> 32;
      /* The fraction's top 24 bits, which a float holds exactly: below 1 always. All 32
       * rounded to a float could come to 1.0 (from 0xffffff80 up), and the phase past the
       * table's last row. */
      float frac   = (float)((uint32_t)(p & 0xffffffffu) >> 8) / 16777216.0f;
      float l      = 0.0f;
      float rr     = 0.0f;
      unsigned t;
      if (idx >= src_frames)
         break;
      if (r->cutoff >= 1.0f)
      {
         float a           = frac * SINC_PHASES;
         unsigned ph       = (unsigned)a;
         const float* lo   = sinc_table[ph];
         const float* hi   = sinc_table[ph + 1];
         a -= (float)ph;
         for (t = 0; t < SINC_TAPS; t++)
            row[t] = lo[t] + (hi[t] - lo[t]) * a;
      }
      else
      {
         /* Fewer than 48000 frames a second out than in: the cutoff is lowered to the new
          * Nyquist, and the kernel made for this position. */
         sinc_row(row, frac, r->cutoff);
      }
      for (t = 0; t < SINC_TAPS; t++)
      {
         int64_t at = (int64_t)idx + (int)t - SINC_HALF;
         float a, b;
         if (at < 0 || (uint64_t)at >= src_frames)
            continue;
         if (ch == 1)
         {
            a = s16 ? s16[at] : f32[at];
            b = a;
         }
         else
         {
            a = s16 ? s16[at * 2 + 0] : f32[at * 2 + 0];
            b = s16 ? s16[at * 2 + 1] : f32[at * 2 + 1];
         }
         l += a * row[t];
         rr += b * row[t];
      }
      out[n * 2 + 0] = l * gain;
      out[n * 2 + 1] = rr * gain;
      p += r->step;
   }
   *pos = p;
   return n;
}

unsigned audio_mix_resample(const audio_mix_resampler_t* r, const int16_t* src,
                            uint64_t src_frames, uint64_t* pos, float* out,
                            unsigned out_frames, float gain)
{
   return resample(r, src, NULL, 1.0f, src_frames, pos, out, out_frames, gain);
}

unsigned audio_mix_resample_f32(const audio_mix_resampler_t* r, const float* src,
                                uint64_t src_frames, uint64_t* pos, float* out,
                                unsigned out_frames, float gain)
{
   return resample(r, NULL, src, 32768.0f, src_frames, pos, out, out_frames, gain);
}

typedef char audio_mix_behind_is_sinc_half[AUDIO_MIX_RESAMPLE_BEHIND == SINC_HALF ? 1 : -1];

unsigned audio_mix_resample_ready(const audio_mix_resampler_t* r, uint64_t pos,
                                  uint64_t src_frames, unsigned out_frames)
{
   /* The last tap reads SINC_TAPS - 1 - SINC_HALF frames ahead of the position */
   const uint64_t ahead = SINC_TAPS - 1 - SINC_HALF;
   uint64_t limit, n;
   if (src_frames <= ahead)
      return 0;
   limit = (src_frames - ahead) << 32; /* positions below this have every tap inside */
   if (pos >= limit)
      return 0;
   n = (limit - pos + r->step - 1) / r->step;
   return n < out_frames ? (unsigned)n : out_frames;
}

/* The console's fold of 7.1 to stereo: front at full level, centre and both surround pairs at
 * -3 dB. Channels 4 to 7 are summed in pairs, so the standard layout's swapped pairs fold the
 * same. */
#define FOLD_CENTER 0.7071f
#define FOLD_SURROUND 0.7071f

void audio_mix_convert_s16(float* dst, const int16_t* src, unsigned frames, unsigned channels,
                           float gain)
{
   unsigned i;
   if (channels == 1)
   {
      for (i = 0; i < frames; i++)
      {
         float v        = src[i] * gain;
         dst[i * 2 + 0] = v;
         dst[i * 2 + 1] = v;
      }
   }
   else if (channels == 2)
   {
      for (i = 0; i < frames * 2; i++)
         dst[i] = src[i] * gain;
   }
   else
   {
      for (i = 0; i < frames; i++)
      {
         const int16_t* s = src + i * 8;
         float c          = FOLD_CENTER * s[2];
         dst[i * 2 + 0]   = (s[0] + c + FOLD_SURROUND * (s[4] + s[6])) * gain;
         dst[i * 2 + 1]   = (s[1] + c + FOLD_SURROUND * (s[5] + s[7])) * gain;
      }
   }
}

/* Float sound is -1..1; the mix counts in 16-bit steps, so it is scaled by 32768, the inverse
 * of how a 16-bit sample is read as float. */
void audio_mix_convert_f32(float* dst, const float* src, unsigned frames, unsigned channels,
                           float gain)
{
   unsigned i;
   float g = gain * 32768.0f;
   if (channels == 1)
   {
      for (i = 0; i < frames; i++)
      {
         float v        = src[i] * g;
         dst[i * 2 + 0] = v;
         dst[i * 2 + 1] = v;
      }
   }
   else if (channels == 2)
   {
      for (i = 0; i < frames * 2; i++)
         dst[i] = src[i] * g;
   }
   else
   {
      for (i = 0; i < frames; i++)
      {
         const float* s = src + i * 8;
         float c        = FOLD_CENTER * s[2];
         dst[i * 2 + 0] = (s[0] + c + FOLD_SURROUND * (s[4] + s[6])) * g;
         dst[i * 2 + 1] = (s[1] + c + FOLD_SURROUND * (s[5] + s[7])) * g;
      }
   }
}

void audio_mix_convert_s16_wide(float* dst, const int16_t* src, unsigned frames,
                                const int* order, const float* gains)
{
   unsigned i, c;
   for (i = 0; i < frames; i++)
      for (c = 0; c < 8; c++)
         dst[i * 8 + c] = src[i * 8 + order[c]] * gains[c];
}

void audio_mix_ramp(float* dst, unsigned frames, unsigned width, unsigned at, unsigned total,
                    const float* from, const float* to)
{
   unsigned i, c;
   const float step = total ? 1.0f / (float)total : 1.0f;
   for (i = 0; i < frames; i++)
   {
      const float t = (float)(at + i + 1) * step;
      for (c = 0; c < width; c++)
         dst[i * width + c] *= from[c] + (to[c] - from[c]) * t;
   }
}

void audio_mix_convert_f32_wide(float* dst, const float* src, unsigned frames,
                                const int* order, const float* gains)
{
   unsigned i, c;
   float g[8];
   for (c = 0; c < 8; c++)
      g[c] = gains[c] * 32768.0f;
   for (i = 0; i < frames; i++)
      for (c = 0; c < 8; c++)
         dst[i * 8 + c] = src[i * 8 + order[c]] * g[c];
}
