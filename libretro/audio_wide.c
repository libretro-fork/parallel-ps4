/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <string.h>

#include <retro_atomic.h>

#include "audio_wide.h"

typedef struct audio_wide_segment
{
   uint32_t start; /* counts samples, only grows: masked by the ring's size to address it */
   uint32_t frames;
   uint32_t channels;
} audio_wide_segment_t;

typedef struct audio_wide_ctl
{
   retro_atomic_int_t seg_written; /* segments published, written by the writer */
   retro_atomic_int_t seg_read;    /* segments done with, written by the reader */
   retro_atomic_int_t sample_read; /* the end of the last segment done with */
   audio_wide_segment_t seg[AUDIO_WIDE_SEGMENTS];
} audio_wide_ctl_t;

typedef char audio_wide_ctl_fits[sizeof(audio_wide_ctl_t) <= AUDIO_WIDE_BYTES ? 1 : -1];
typedef char audio_wide_ring_is_pow2[(AUDIO_WIDE_RING_SAMPLES & (AUDIO_WIDE_RING_SAMPLES - 1)) == 0 ? 1 : -1];

#define RING_MASK (AUDIO_WIDE_RING_SAMPLES - 1u)

void audio_wide_init(void* ctl)
{
   audio_wide_ctl_t* c = (audio_wide_ctl_t*)ctl;
   memset(c, 0, sizeof(*c));
   retro_atomic_int_init(&c->seg_written, 0);
   retro_atomic_int_init(&c->seg_read, 0);
   retro_atomic_int_init(&c->sample_read, 0);
}

uint32_t audio_wide_room(const void* ctl, const audio_wide_writer_t* w, uint32_t frames)
{
   audio_wide_ctl_t* c = (audio_wide_ctl_t*)ctl;
   const uint32_t read = (uint32_t)retro_atomic_load_acquire_int(&c->sample_read);
   const uint32_t segs = (uint32_t)retro_atomic_load_relaxed_int(&c->seg_written) -
                         (uint32_t)retro_atomic_load_acquire_int(&c->seg_read);
   const uint32_t used = w->sample - read;
   uint32_t room;
   /* A chunk may take two segments, where the ring wraps */
   if (segs + 2 > AUDIO_WIDE_SEGMENTS || used + 16 > AUDIO_WIDE_RING_SAMPLES)
      return 0;
   /* Frames of 8 channels, and the padding to the next multiple of 8 after them */
   room = (AUDIO_WIDE_RING_SAMPLES - used - 8) / 8;
   return room < frames ? room : frames;
}

void audio_wide_plan(const audio_wide_writer_t* w, uint32_t frames, uint32_t channels,
                     audio_wide_piece_t pieces[2], unsigned* n)
{
   const uint32_t pos = w->sample & RING_MASK;
   /* pos is a multiple of 8, and so is the ring's size: what is left to its end is whole
    * frames of either width */
   const uint32_t to_end = (AUDIO_WIDE_RING_SAMPLES - pos) / channels;
   const uint32_t first  = frames < to_end ? frames : to_end;
   pieces[0].pos         = pos;
   pieces[0].frames      = first;
   pieces[0].channels    = channels;
   *n                    = 1;
   if (first < frames)
   {
      pieces[1].pos      = 0;
      pieces[1].frames   = frames - first;
      pieces[1].channels = channels;
      *n                 = 2;
   }
}

void audio_wide_publish(void* ctl, audio_wide_writer_t* w, const audio_wide_piece_t pieces[2],
                        unsigned n)
{
   audio_wide_ctl_t* c = (audio_wide_ctl_t*)ctl;
   uint32_t written    = (uint32_t)retro_atomic_load_relaxed_int(&c->seg_written);
   unsigned i;
   for (i = 0; i < n; i++)
   {
      audio_wide_segment_t* s = &c->seg[written % AUDIO_WIDE_SEGMENTS];
      if (pieces[i].frames == 0)
         continue;
      s->start    = w->sample;
      s->frames   = pieces[i].frames;
      s->channels = pieces[i].channels;
      w->sample += pieces[i].frames * pieces[i].channels;
      written++;
   }
   /* The next segment starts on a multiple of 8 */
   w->sample = (w->sample + 7u) & ~7u;
   retro_atomic_store_release_int(&c->seg_written, (int)written);
}

int audio_wide_peek(const void* ctl, uint32_t done, uint32_t frames, audio_wide_piece_t* piece)
{
   audio_wide_ctl_t* c = (audio_wide_ctl_t*)ctl;
   const uint32_t read = (uint32_t)retro_atomic_load_relaxed_int(&c->seg_read);
   const audio_wide_segment_t* s;
   uint32_t left;
   if (read == (uint32_t)retro_atomic_load_acquire_int(&c->seg_written))
      return 0;
   s               = &c->seg[read % AUDIO_WIDE_SEGMENTS];
   left            = s->frames - done;
   piece->pos      = (s->start & RING_MASK) + done * s->channels;
   piece->frames   = left < frames ? left : frames;
   piece->channels = s->channels;
   return 1;
}

void audio_wide_consume(void* ctl, uint32_t* done, uint32_t frames)
{
   audio_wide_ctl_t* c = (audio_wide_ctl_t*)ctl;
   const uint32_t read = (uint32_t)retro_atomic_load_relaxed_int(&c->seg_read);
   const audio_wide_segment_t* s = &c->seg[read % AUDIO_WIDE_SEGMENTS];
   *done += frames;
   if (*done < s->frames)
      return;
   *done = 0;
   retro_atomic_store_release_int(&c->sample_read, (int)(s->start + s->frames * s->channels));
   retro_atomic_store_release_int(&c->seg_read, (int)(read + 1));
}
