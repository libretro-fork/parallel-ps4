/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The video player's state machine: its state and the one before, in one atomic word, moved
 * by compare-and-swap. A move is checked against the state it leaves and made only from that
 * state, so two threads moving it at once never both leave the same state, and the state
 * before is always the one the last move left. No lock. */

#ifndef SHADPS4_AVPLAYER_FSM_H
#define SHADPS4_AVPLAYER_FSM_H

#include <retro_atomic.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The states, as AvState (avplayer_common.h) numbers them */
enum {
    AVPLAYER_FSM_UNKNOWN = 0,
    AVPLAYER_FSM_INITIAL,
    AVPLAYER_FSM_ADDING_SOURCE,
    AVPLAYER_FSM_READY,
    AVPLAYER_FSM_PLAY,
    AVPLAYER_FSM_STOP,
    AVPLAYER_FSM_END_OF_FILE,
    AVPLAYER_FSM_PAUSE,
    AVPLAYER_FSM_C0X08,
    AVPLAYER_FSM_JUMP,
    AVPLAYER_FSM_TRICK_MODE,
    AVPLAYER_FSM_C0X0B,
    AVPLAYER_FSM_BUFFERING,
    AVPLAYER_FSM_STARTING,
    AVPLAYER_FSM_ERROR,
    AVPLAYER_FSM_COUNT
};

typedef struct avplayer_fsm {
    retro_atomic_int_t word; /* the state, and the one before it times 256 */
} avplayer_fsm_t;

/* In @state, with none before it (UNKNOWN). Before any other thread sees @fsm. */
void avplayer_fsm_init(avplayer_fsm_t* fsm, int state);

/* Whether the player may go from @from to @to. */
int avplayer_fsm_allowed(int from, int to);

/* Goes to @to if it may from where it is: 1, and *@from (if not NULL) the state it left.
 * 0 if it may not, and *@from the state it stays in. */
int avplayer_fsm_set(avplayer_fsm_t* fsm, int to, int* from);

/* Goes back to the state before, if it may: as avplayer_fsm_set, and *@to (if not NULL) the
 * state gone back to. */
int avplayer_fsm_revert(avplayer_fsm_t* fsm, int* from, int* to);

int avplayer_fsm_current(avplayer_fsm_t* fsm);
int avplayer_fsm_previous(avplayer_fsm_t* fsm);

/* Who starts and stops the player: one thread at a time, without waiting. A thread that asks
 * while another is starting or stopping it leaves its wish with that one, which carries it
 * out after its own; of wishes left meanwhile, the last one stands (a stop after a start
 * leaves it stopped). */
enum { AVPLAYER_TRANSPORT_NONE = 0, AVPLAYER_TRANSPORT_START, AVPLAYER_TRANSPORT_STOP };

typedef struct avplayer_transport {
    retro_atomic_int_t word; /* 1 while a thread has it, plus the wish left for it times 2 */
} avplayer_transport_t;

void avplayer_transport_init(avplayer_transport_t* transport);

/* Asks for @op. 1: the caller has it, does @op, then takes avplayer_transport_next until it
 * returns NONE. 0: the thread that has it will do @op; *@replaced (if not NULL) the wish @op
 * took the place of, which will not be done (NONE: none). */
int avplayer_transport_claim(avplayer_transport_t* transport, int op, int* replaced);

/* The thread that has it is done with its op: the wish left meanwhile, which it does next, or
 * NONE, and it no longer has it. */
int avplayer_transport_next(avplayer_transport_t* transport);

#ifdef __cplusplus
}
#endif

#endif
