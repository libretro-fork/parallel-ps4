/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stddef.h>

#include "core/libraries/avplayer/avplayer_fsm.h"

#define FSM_WORD(current, previous) ((current) | ((previous) << 8))
#define FSM_CURRENT(word) ((word) & 0xff)
#define FSM_PREVIOUS(word) (((word) >> 8) & 0xff)

void avplayer_fsm_init(avplayer_fsm_t* fsm, int state) {
    retro_atomic_int_init(&fsm->word, FSM_WORD(state, AVPLAYER_FSM_UNKNOWN));
}

int avplayer_fsm_allowed(int from, int to) {
    /* Never out of these into a state of playing */
    if (from == AVPLAYER_FSM_STOP || from == AVPLAYER_FSM_END_OF_FILE ||
        from == AVPLAYER_FSM_ERROR) {
        switch (to) {
        case AVPLAYER_FSM_PLAY:
        case AVPLAYER_FSM_PAUSE:
        case AVPLAYER_FSM_JUMP:
        case AVPLAYER_FSM_TRICK_MODE:
        case AVPLAYER_FSM_BUFFERING:
            return 0;
        default:
            return 1;
        }
    }
    switch (to) {
    case AVPLAYER_FSM_PAUSE:
        return from != AVPLAYER_FSM_STARTING;
    case AVPLAYER_FSM_JUMP:
        return from != AVPLAYER_FSM_TRICK_MODE && from != AVPLAYER_FSM_STARTING;
    case AVPLAYER_FSM_TRICK_MODE:
        return from != AVPLAYER_FSM_JUMP && from != AVPLAYER_FSM_STARTING;
    case AVPLAYER_FSM_BUFFERING:
        return from != AVPLAYER_FSM_PAUSE && from != AVPLAYER_FSM_STARTING;
    default:
        return 1;
    }
}

static int fsm_move(avplayer_fsm_t* fsm, int to, int revert, int* from, int* gone_to) {
    for (;;) {
        int word = retro_atomic_load_acquire_int(&fsm->word);
        int current = FSM_CURRENT(word);
        int next = revert ? FSM_PREVIOUS(word) : to;
        int done = avplayer_fsm_allowed(current, next);
        if (done && !retro_atomic_cas_int(&fsm->word, word, FSM_WORD(next, current))) {
            continue;
        }
        if (from) {
            *from = current;
        }
        if (gone_to) {
            *gone_to = done ? next : current;
        }
        return done;
    }
}

int avplayer_fsm_set(avplayer_fsm_t* fsm, int to, int* from) {
    return fsm_move(fsm, to, 0, from, NULL);
}

int avplayer_fsm_revert(avplayer_fsm_t* fsm, int* from, int* to) {
    return fsm_move(fsm, 0, 1, from, to);
}

int avplayer_fsm_current(avplayer_fsm_t* fsm) {
    return FSM_CURRENT(retro_atomic_load_acquire_int(&fsm->word));
}

int avplayer_fsm_previous(avplayer_fsm_t* fsm) {
    return FSM_PREVIOUS(retro_atomic_load_acquire_int(&fsm->word));
}

#define TRANSPORT_HELD 1

void avplayer_transport_init(avplayer_transport_t* transport) {
    retro_atomic_int_init(&transport->word, 0);
}

int avplayer_transport_claim(avplayer_transport_t* transport, int op, int* replaced) {
    for (;;) {
        int word = retro_atomic_load_acquire_int(&transport->word);
        if (!(word & TRANSPORT_HELD)) {
            if (retro_atomic_cas_int(&transport->word, word, TRANSPORT_HELD)) {
                return 1;
            }
        } else if (retro_atomic_cas_int(&transport->word, word, TRANSPORT_HELD | (op << 1))) {
            if (replaced) {
                *replaced = word >> 1;
            }
            return 0;
        }
    }
}

int avplayer_transport_next(avplayer_transport_t* transport) {
    for (;;) {
        int word = retro_atomic_load_acquire_int(&transport->word);
        int wish = word >> 1;
        if (retro_atomic_cas_int(&transport->word, word, wish ? TRANSPORT_HELD : 0)) {
            return wish;
        }
    }
}
