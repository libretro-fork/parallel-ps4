/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "frame_slots.h"

void frame_slots_init(frame_slots_t *slots, retro_atomic_int_t *newest,
                      retro_atomic_int_t *held, unsigned count) {
  unsigned i;
  slots->newest = newest;
  slots->held = held;
  slots->count = count < FRAME_SLOTS_MAX ? count : FRAME_SLOTS_MAX;
  for (i = 0; i < FRAME_SLOTS_MAX; i++)
    retro_atomic_int_init(&slots->in_flight[i], 0);
}

unsigned frame_slots_claim(frame_slots_t *slots) {
  unsigned pass, slot;
  /* Twice round: the core's held slot can move from one slot to the next as it
   * is looked at (a newest frame held as a newer one is published), so that
   * every slot was taken at the moment it was looked at though one is free by
   * the end */
  for (pass = 0; pass < 2 * slots->count; pass++) {
    unsigned newest, held;
    slot = pass % slots->count;
    if (retro_atomic_exchange_int(&slots->in_flight[slot], 1) != 0)
      continue;
    newest = (unsigned)retro_atomic_load_seq_cst_int(slots->newest);
    newest = newest == 0 ? FRAME_SLOTS_NONE
                         : newest & ((1u << FRAME_SLOTS_BITS) - 1);
    held = (unsigned)retro_atomic_load_seq_cst_int(slots->held);
    if (slot != newest && slot != held)
      return slot;
    (void)retro_atomic_exchange_int(&slots->in_flight[slot], 0);
  }
  return FRAME_SLOTS_NONE;
}

void frame_slots_release(frame_slots_t *slots, unsigned slot) {
  if (slot < slots->count)
    (void)retro_atomic_exchange_int(&slots->in_flight[slot], 0);
}

void frame_slots_publish(frame_slots_t *slots, unsigned slot) {
  unsigned newest;
  if (slot >= slots->count)
    return;
  /* Only the emulator writes the word, from one thread at a time */
  newest = (unsigned)retro_atomic_load_seq_cst_int(slots->newest);
  (void)retro_atomic_exchange_int(
      slots->newest,
      (int)((((newest >> FRAME_SLOTS_BITS) + 1) << FRAME_SLOTS_BITS) | slot));
  (void)retro_atomic_exchange_int(&slots->in_flight[slot], 0);
}

int frame_slots_hold(retro_atomic_int_t *newest, retro_atomic_int_t *held,
                     unsigned count, unsigned shown, unsigned *slot,
                     unsigned *newest_out, int *released) {
  const unsigned latest = (unsigned)retro_atomic_load_seq_cst_int(newest);
  const unsigned taken = latest & ((1u << FRAME_SLOTS_BITS) - 1);
  *released = 0;
  if (latest == 0 || latest == shown || taken >= count)
    return 0;
  /* Held first, then looked at again: once the slot is held and still the
   * newest, the emulator does not write into it */
  if ((unsigned)retro_atomic_exchange_int(held, (int)taken) != taken)
    *released = 1;
  if ((unsigned)retro_atomic_load_seq_cst_int(newest) != latest)
    return -1;
  *slot = taken;
  *newest_out = latest;
  return 1;
}
