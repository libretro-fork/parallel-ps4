/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <string.h>

#include <retro_atomic.h>
#include <retro_waitable_spsc.h>

#include "audio_mix.h"

#define FRAME_BYTES 4

/* An input's life: free in the pool, open to its source, or closed by its source and not yet
 * emptied by the mixing thread. Only the source moves FREE to OPEN (by CAS, sources open from
 * several threads) and OPEN to CLOSING; only the mixing thread moves CLOSING to FREE, after it
 * has emptied the ring, so the ring has one producer and one consumer at all times. */
enum
{
   INPUT_FREE = 0,
   INPUT_OPEN,
   INPUT_CLOSING
};

struct audio_mix_input
{
   retro_waitable_spsc_t ring;
   retro_atomic_int_t state;
};

static struct audio_mix_input inputs[AUDIO_MIX_INPUTS];
/* The mixing thread's sums: one chunk of stereo frames. */
#define PUMP_FRAMES 1024
static int32_t pump_sum[PUMP_FRAMES * 2];

int audio_mix_init(void)
{
   unsigned i;
   for (i = 0; i < AUDIO_MIX_INPUTS; i++)
   {
      retro_atomic_int_init(&inputs[i].state, INPUT_FREE);
      if (!retro_waitable_spsc_init(&inputs[i].ring, AUDIO_MIX_INPUT_FRAMES * FRAME_BYTES))
         return 0;
   }
   return 1;
}

audio_mix_input_t* audio_mix_input_open(void)
{
   unsigned i;
   for (i = 0; i < AUDIO_MIX_INPUTS; i++)
      if (retro_atomic_cas_int(&inputs[i].state, INPUT_FREE, INPUT_OPEN))
         return &inputs[i];
   return NULL;
}

void audio_mix_input_close(audio_mix_input_t* in)
{
   if (in)
      retro_atomic_store_release_int(&in->state, INPUT_CLOSING);
}

int audio_mix_input_wait(audio_mix_input_t* in, unsigned frames, int64_t timeout_us)
{
   /* Room for @frames on top of the most that may stay queued, out of what the ring holds. */
   size_t capacity = retro_spsc_write_avail(&in->ring.queue) +
                     retro_spsc_read_avail(&in->ring.queue);
   size_t want     = capacity - AUDIO_MIX_QUEUE_FRAMES * FRAME_BYTES;
   if (frames * FRAME_BYTES > want)
      want = frames * FRAME_BYTES;
   return retro_waitable_spsc_wait_writable(&in->ring, want, timeout_us) ? 1 : 0;
}

unsigned audio_mix_input_reserve(audio_mix_input_t* in, int16_t** ptr, unsigned frames)
{
   void* span   = NULL;
   size_t bytes = retro_spsc_write_begin(&in->ring.queue, &span);
   unsigned got = (unsigned)(bytes / FRAME_BYTES);
   *ptr         = (int16_t*)span;
   return got < frames ? got : frames;
}

void audio_mix_input_commit(audio_mix_input_t* in, unsigned frames)
{
   retro_spsc_write_end(&in->ring.queue, (size_t)frames * FRAME_BYTES);
   retro_waitable_spsc_notify_readable(&in->ring);
}

/* Adds what one input has, up to @frames, to the sums. Returns how many frames it had. */
static unsigned pump_add(struct audio_mix_input* in, unsigned frames)
{
   unsigned done = 0;
   while (done < frames)
   {
      const void* span = NULL;
      size_t bytes     = retro_spsc_read_begin(&in->ring.queue, &span);
      unsigned got     = (unsigned)(bytes / FRAME_BYTES);
      const int16_t* s = (const int16_t*)span;
      int32_t* sum     = pump_sum + done * 2;
      unsigned i;
      if (got == 0)
         break;
      if (got > frames - done)
         got = frames - done;
      for (i = 0; i < got * 2; i++)
         sum[i] += s[i];
      retro_spsc_read_end(&in->ring.queue, (size_t)got * FRAME_BYTES);
      done += got;
   }
   if (done)
      retro_waitable_spsc_notify_writable(&in->ring);
   return done;
}

unsigned audio_mix_pump(int16_t* ring, uint32_t ring_mask, uint32_t at, unsigned frames)
{
   unsigned i;
   unsigned most = 0;
   if (frames > PUMP_FRAMES)
      frames = PUMP_FRAMES;
   memset(pump_sum, 0, frames * 2 * sizeof(pump_sum[0]));

   for (i = 0; i < AUDIO_MIX_INPUTS; i++)
   {
      struct audio_mix_input* in = &inputs[i];
      int state                  = retro_atomic_load_acquire_int(&in->state);
      if (state == INPUT_OPEN)
      {
         unsigned got = pump_add(in, frames);
         if (got > most)
            most = got;
      }
      else if (state == INPUT_CLOSING)
      {
         /* Its source has gone: what it left is dropped, and the input goes back. */
         retro_spsc_skip(&in->ring.queue, retro_spsc_read_avail(&in->ring.queue));
         retro_atomic_store_release_int(&in->state, INPUT_FREE);
      }
   }

   for (i = 0; i < most; i++)
   {
      uint32_t pos = ((at + i) & ring_mask) * 2;
      int32_t l    = pump_sum[i * 2 + 0];
      int32_t r    = pump_sum[i * 2 + 1];
      ring[pos + 0] = (int16_t)(l < -32768 ? -32768 : (l > 32767 ? 32767 : l));
      ring[pos + 1] = (int16_t)(r < -32768 ? -32768 : (r > 32767 ? 32767 : r));
   }
   return most;
}

static int16_t clamp_s16(float v)
{
   if (v > 32767.0f)
      return 32767;
   if (v < -32768.0f)
      return -32768;
   return (int16_t)v;
}

/* The console's fold of 7.1 to stereo: front at full level, centre and both surround pairs at
 * -3 dB. Channels 4 to 7 are summed in pairs, so the standard layout's swapped pairs fold the
 * same. */
#define FOLD_CENTER 0.7071f
#define FOLD_SURROUND 0.7071f

void audio_mix_convert_s16(int16_t* dst, const int16_t* src, unsigned frames, unsigned channels,
                           float gain)
{
   unsigned i;
   if (channels == 1)
   {
      for (i = 0; i < frames; i++)
      {
         int16_t v      = clamp_s16(src[i] * gain);
         dst[i * 2 + 0] = v;
         dst[i * 2 + 1] = v;
      }
   }
   else if (channels == 2)
   {
      if (gain == 1.0f)
         memcpy(dst, src, (size_t)frames * FRAME_BYTES);
      else
         for (i = 0; i < frames * 2; i++)
            dst[i] = clamp_s16(src[i] * gain);
   }
   else
   {
      for (i = 0; i < frames; i++)
      {
         const int16_t* s = src + i * 8;
         float c          = FOLD_CENTER * s[2];
         dst[i * 2 + 0] = clamp_s16((s[0] + c + FOLD_SURROUND * (s[4] + s[6])) * gain);
         dst[i * 2 + 1] = clamp_s16((s[1] + c + FOLD_SURROUND * (s[5] + s[7])) * gain);
      }
   }
}

void audio_mix_convert_f32(int16_t* dst, const float* src, unsigned frames, unsigned channels,
                           float gain)
{
   unsigned i;
   float g = gain * 32768.0f;
   if (channels == 1)
   {
      for (i = 0; i < frames; i++)
      {
         int16_t v      = clamp_s16(src[i] * g);
         dst[i * 2 + 0] = v;
         dst[i * 2 + 1] = v;
      }
   }
   else if (channels == 2)
   {
      for (i = 0; i < frames * 2; i++)
         dst[i] = clamp_s16(src[i] * g);
   }
   else
   {
      for (i = 0; i < frames; i++)
      {
         const float* s = src + i * 8;
         float c        = FOLD_CENTER * s[2];
         dst[i * 2 + 0] = clamp_s16((s[0] + c + FOLD_SURROUND * (s[4] + s[6])) * g);
         dst[i * 2 + 1] = clamp_s16((s[1] + c + FOLD_SURROUND * (s[5] + s[7])) * g);
      }
   }
}
