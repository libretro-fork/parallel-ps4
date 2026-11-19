/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* A gate on something one owner changes and many threads read: readers pin it as they come
 * in and unpin it as they go, and never wait. While the owner has it closed, a reader is
 * turned away at once and does without (as if it had come before the change). Closing waits,
 * on a retro_eventcount, only for the readers already in to go. One atomic word: the readers
 * in, times two, and whether it is closed. No lock, no spin, no timeout.
 *
 * One closer at a time: what is changed has one owner, which closes and opens. */

#ifndef SHADPS4_PIN_GATE_H
#define SHADPS4_PIN_GATE_H

#include <retro_atomic.h>
#include <retro_common_api.h>
#include <rthreads/retro_eventcount.h>

RETRO_BEGIN_DECLS

typedef struct pin_gate {
    retro_atomic_int_t word; /* readers in times 2, plus 1 while closed */
    retro_eventcount_t drained;
} pin_gate_t;

/* Open, no reader in. Before any other call; pin_gate_free after the last. */
void pin_gate_init(pin_gate_t* gate);
void pin_gate_free(pin_gate_t* gate);

/* A reader comes in: 1, and what is behind the gate stays as it is until its pin_gate_leave;
 * 0 if the gate is closed, and it must not touch it (and does not leave). Never waits. */
int pin_gate_enter(pin_gate_t* gate);
void pin_gate_leave(pin_gate_t* gate);

/* The owner closes it, and returns once every reader that was in has left: from here until
 * pin_gate_open it may change what is behind it. */
void pin_gate_close(pin_gate_t* gate);
void pin_gate_open(pin_gate_t* gate);

RETRO_END_DECLS

#endif
