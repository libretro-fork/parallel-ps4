/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "common/pin_gate.h"

#define PIN_GATE_CLOSED 1
#define PIN_GATE_READER 2

void pin_gate_init(pin_gate_t* gate) {
    retro_atomic_int_init(&gate->word, 0);
    retro_eventcount_init(&gate->drained);
}

void pin_gate_free(pin_gate_t* gate) {
    retro_eventcount_free(&gate->drained);
}

int pin_gate_enter(pin_gate_t* gate) {
    /* In first, then look: a closer that set its bit before this either sees this reader in
     * or is seen by it */
    if (retro_atomic_fetch_add_seq_cst_int(&gate->word, PIN_GATE_READER) & PIN_GATE_CLOSED) {
        pin_gate_leave(gate);
        return 0;
    }
    return 1;
}

void pin_gate_leave(pin_gate_t* gate) {
    /* The last reader out of a closed gate wakes its closer */
    if (retro_atomic_fetch_add_seq_cst_int(&gate->word, -PIN_GATE_READER) ==
        PIN_GATE_CLOSED + PIN_GATE_READER) {
        retro_eventcount_notify(&gate->drained);
    }
}

void pin_gate_shut(pin_gate_t* gate) {
    (void)retro_atomic_fetch_or_int(&gate->word, PIN_GATE_CLOSED);
}

int pin_gate_drained(pin_gate_t* gate) {
    return retro_atomic_load_acquire_int(&gate->word) == PIN_GATE_CLOSED;
}

void pin_gate_close(pin_gate_t* gate) {
    pin_gate_shut(gate);
    for (;;) {
        int key;
        if (pin_gate_drained(gate)) {
            return;
        }
        key = retro_eventcount_prepare_wait(&gate->drained);
        if (pin_gate_drained(gate)) {
            retro_eventcount_cancel_wait(&gate->drained);
            return;
        }
        retro_eventcount_commit_wait(&gate->drained, key);
    }
}

void pin_gate_open(pin_gate_t* gate) {
    (void)retro_atomic_fetch_and_int(&gate->word, ~PIN_GATE_CLOSED);
}
