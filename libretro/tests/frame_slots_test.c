/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The frames handed from the emulator to the core in the shared memory
 * (libretro/frame_slots.c): a slot claimed is neither the newest nor the held
 * one, every slot taken gives none, and with an emulator thread writing up to
 * three frames at once into the slots it claims (as the GPU does) while a core
 * thread holds the newest and reads it, the core only ever sees whole frames,
 * newer each time, that do not change while it holds them, and a frame goes
 * unshown for want of a slot all but never. */

#include <stdio.h>
#include <string.h>

#include <pthread.h>
#include <sched.h>

#include "../frame_slots.h"

static int failures = 0;

#define CHECK(cond, what)                                                      \
  do {                                                                         \
    if (!(cond)) {                                                             \
      printf("FAIL %s (line %d)\n", what, __LINE__);                           \
      ++failures;                                                              \
    }                                                                          \
  } while (0)

#define SLOTS 5
#define WORDS 64
#define FRAMES 200000
#define IN_FLIGHT 3

static retro_atomic_int_t newest;
static retro_atomic_int_t held;
static retro_atomic_int_t done;
/* The frames, written word by word as the GPU would, read as the core would */
static retro_atomic_int_t pixels[SLOTS][WORDS];
static frame_slots_t slots;
static long dropped = 0, torn = 0, changed = 0, older = 0, shown = 0;

static void write_frame(unsigned slot, int frame) {
  unsigned i;
  for (i = 0; i < WORDS; i++) {
    retro_atomic_store_relaxed_int(&pixels[slot][i], frame);
    if ((i & 15) == 15)
      sched_yield();
  }
}

static void *emulator(void *arg) {
  unsigned pending[IN_FLIGHT];
  unsigned n = 0, i;
  int frame;
  (void)arg;
  for (frame = 1; frame <= FRAMES; frame++) {
    unsigned slot;
    /* The oldest frame the GPU has finished, published, when as many are in
     * flight as can be */
    if (n == IN_FLIGHT || (frame % 7) == 0) {
      if (n) {
        frame_slots_publish(&slots, pending[0]);
        for (i = 1; i < n; i++)
          pending[i - 1] = pending[i];
        n--;
      }
    }
    slot = frame_slots_claim(&slots);
    if (slot == FRAME_SLOTS_NONE) {
      dropped++;
      continue;
    }
    write_frame(slot, frame);
    pending[n++] = slot;
  }
  for (i = 0; i < n; i++)
    frame_slots_publish(&slots, pending[i]);
  retro_atomic_store_release_int(&done, 1);
  return NULL;
}

static void *core(void *arg) {
  unsigned shown_latest = 0;
  int last = 0;
  (void)arg;
  while (!retro_atomic_load_acquire_int(&done)) {
    unsigned slot = 0, latest = 0, i;
    int released = 0, value, again;
    const int took = frame_slots_hold(&newest, &held, SLOTS, shown_latest,
                                      &slot, &latest, &released);
    if (took <= 0) {
      sched_yield();
      continue;
    }
    shown_latest = latest;
    shown++;
    value = retro_atomic_load_relaxed_int(&pixels[slot][0]);
    for (i = 1; i < WORDS; i++)
      if (retro_atomic_load_relaxed_int(&pixels[slot][i]) != value) {
        torn++;
        break;
      }
    if (value <= last)
      older++;
    last = value;
    /* Still held: the frame stays as it is while the frontend shows it */
    for (again = 0; again < 4; again++) {
      sched_yield();
      for (i = 0; i < WORDS; i++)
        if (retro_atomic_load_relaxed_int(&pixels[slot][i]) != value) {
          changed++;
          break;
        }
    }
  }
  return NULL;
}

int main(void) {
  pthread_t threads[2];
  unsigned a, b, c, d, e;

  retro_atomic_int_init(&newest, 0);
  retro_atomic_int_init(&held, FRAME_SLOTS_NONE);
  retro_atomic_int_init(&done, 0);
  frame_slots_init(&slots, &newest, &held, SLOTS);

  /* One thread: the newest and the held slot are never claimed, and with all
   * taken none is */
  a = frame_slots_claim(&slots);
  CHECK(a == 0, "the first slot first");
  frame_slots_publish(&slots, a);
  CHECK((unsigned)retro_atomic_load_seq_cst_int(&newest) ==
            ((1u << FRAME_SLOTS_BITS) | 0),
        "published: one frame, in slot 0");
  retro_atomic_store_release_int(&held, 1);
  b = frame_slots_claim(&slots);
  CHECK(b == 2, "not the newest (0), not the held one (1)");
  c = frame_slots_claim(&slots);
  d = frame_slots_claim(&slots);
  CHECK(c == 3 && d == 4, "the others while they are being written");
  e = frame_slots_claim(&slots);
  CHECK(e == FRAME_SLOTS_NONE, "every slot taken: none");
  frame_slots_release(&slots, c);
  CHECK(frame_slots_claim(&slots) == 3, "one let go: taken again");
  frame_slots_release(&slots, b);
  frame_slots_release(&slots, 3);
  frame_slots_release(&slots, d);
  {
    unsigned slot = 9, latest = 0;
    int released = 0;
    CHECK(frame_slots_hold(&newest, &held, SLOTS, 0, &slot, &latest,
                           &released) == 1 &&
              slot == 0 && released == 1,
          "the core holds the newest, letting go of the one it held");
    CHECK(frame_slots_hold(&newest, &held, SLOTS, latest, &slot, &latest,
                           &released) == 0,
          "and nothing newer after that");
  }

  /* Two threads */
  retro_atomic_int_init(&newest, 0);
  retro_atomic_int_init(&held, FRAME_SLOTS_NONE);
  frame_slots_init(&slots, &newest, &held, SLOTS);
  pthread_create(&threads[0], NULL, emulator, NULL);
  pthread_create(&threads[1], NULL, core, NULL);
  pthread_join(threads[0], NULL);
  pthread_join(threads[1], NULL);
  printf("frame_slots: %ld frames shown, %ld not shown with every slot taken\n",
         shown, dropped);
  CHECK(shown > 0, "frames shown");
  CHECK(torn == 0, "no frame shown before it was whole");
  CHECK(changed == 0, "no frame written over while the core held it");
  CHECK(older == 0, "each frame shown newer than the last");
  /* A frame goes unshown only where the core's held slot moved under both looks
   * of a claim */
  CHECK(dropped * 10000 < FRAMES,
        "with three in flight, a slot free all but rarely");
  printf("frame_slots: %s\n", failures ? "FAILED" : "passed");
  return failures != 0;
}
