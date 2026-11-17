/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The frames read back into the memory the emulator shares with the core
 * (shared_state.h), a slot each, handed over without a lock and with no wait on
 * either side.
 *
 * The emulator claims a slot for a frame as it records the frame's readback,
 * and the GPU writes the frame straight into it; once the GPU is done, the
 * frame is published: the slot becomes the newest. A slot claimed is neither
 * the newest nor the one the core holds, and several can be claimed at once,
 * one for each frame the GPU has not finished yet. With every slot taken there
 * is none, and the frame is not shown; nothing waits.
 *
 * The core holds the newest slot and shows it from where it is, then looks
 * again: if the newest is still that slot, the emulator either saw it held or
 * had published nothing newer, so it is not writing there. The emulator looks
 * at the newest and the held slot after it has claimed one: a claimed slot
 * cannot become the newest, and if the core holds it, the core finds a newer
 * frame when it looks again and does not show it. Both sides use sequentially
 * consistent operations for the two words; the argument needs it. */

#ifndef SHADPS4_LIBRETRO_FRAME_SLOTS_H
#define SHADPS4_LIBRETRO_FRAME_SLOTS_H

#include <retro_atomic.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Slots at most */
#define FRAME_SLOTS_MAX 8
/* No slot: the held word when the core holds none, and what a claim gives when
 * all are taken */
#define FRAME_SLOTS_NONE 7u
/* The newest word: frames published so far, shifted by this, plus the newest
 * one's slot */
#define FRAME_SLOTS_BITS 3

/* The emulator's side, in its own memory: which slots have a frame being
 * written */
typedef struct frame_slots {
  retro_atomic_int_t
      *newest; /* shared: the newest frame, 0 while there is none */
  retro_atomic_int_t
      *held; /* shared: the slot the core holds, or FRAME_SLOTS_NONE */
  unsigned count;
  retro_atomic_int_t in_flight[FRAME_SLOTS_MAX];
} frame_slots_t;

/* @count slots (FRAME_SLOTS_MAX at most) over the shared words @newest and
 * @held */
void frame_slots_init(frame_slots_t *slots, retro_atomic_int_t *newest,
                      retro_atomic_int_t *held, unsigned count);
/* A slot to write a frame into, or FRAME_SLOTS_NONE when every one is taken */
unsigned frame_slots_claim(frame_slots_t *slots);
/* @slot claimed and not written after all */
void frame_slots_release(frame_slots_t *slots, unsigned slot);
/* The frame in @slot is whole: it becomes the newest. What goes with it (its
 * size) is written before. */
void frame_slots_publish(frame_slots_t *slots, unsigned slot);

/* The core's side: holds the newest slot if it is newer than @shown (a newest
 * word, 0 for none). 1 with the slot in *@slot and its newest word in
 * *@newest_out; 0 when there is nothing newer; -1 when a newer frame was
 * published as it was held, to try again. *@released is set when the slot held
 * before was let go, so that the emulator can be told. */
int frame_slots_hold(retro_atomic_int_t *newest, retro_atomic_int_t *held,
                     unsigned count, unsigned shown, unsigned *slot,
                     unsigned *newest_out, int *released);

/* The images shared with a frontend that draws from them on the GPU: the same
 * handing over, with two things more. An image the frontend may still be
 * drawing from (a bit each in @busy, written by the core) is not drawn into.
 * And an image whose frame the GPU is still drawing can be taken for a newer
 * frame: the older one is then not published, so the image only becomes the
 * newest once the newer frame in it is done. With an image free there is no
 * wait, and with none, one is taken over in turn; nothing ever waits. */
typedef struct frame_images {
  retro_atomic_int_t *newest; /* shared, as for the slots */
  retro_atomic_int_t *held;   /* shared, as for the slots */
  retro_atomic_int_t *busy;   /* shared: images the frontend may draw from */
  unsigned count;
  /* 0 when free, else the generation of the frame being drawn into it, or
   * FRAME_IMAGES_PUBLISHING while it is being published */
  retro_atomic_int_t state[FRAME_SLOTS_MAX];
  int generation;
  unsigned taken_over; /* frames not published, their image taken over */
} frame_images_t;

#define FRAME_IMAGES_PUBLISHING (-1)

/* @count images (FRAME_SLOTS_MAX at most) over the shared words */
void frame_images_init(frame_images_t *images, retro_atomic_int_t *newest,
                       retro_atomic_int_t *held, retro_atomic_int_t *busy,
                       unsigned count);
/* The image to draw the next frame into, and in *@generation that frame's
 * generation for frame_images_publish. From one thread. */
unsigned frame_images_acquire(frame_images_t *images, int *generation);
/* The frame of @generation in @image is done: 1 when it is the image's frame
 * still, the image now being published (its size to be written, then
 * frame_images_published); 0 when a newer frame took the image over. */
int frame_images_publish(frame_images_t *images, unsigned image,
                         int generation);
/* The image being published becomes the newest */
void frame_images_published(frame_images_t *images, unsigned image);

#ifdef __cplusplus
}
#endif

#endif
