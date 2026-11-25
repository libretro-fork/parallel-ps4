/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_AVPLAYER_AVPLAYER_COMMON_H
#define SHADPS4_AVPLAYER_AVPLAYER_COMMON_H

#include <stddef.h>

#include <retro_common_api.h>

#include "core/libraries/avplayer/avplayer.h"
#include "core/libraries/avplayer/avplayer_fsm.h"

RETRO_BEGIN_DECLS

#define AVPLAYER_IS_ERROR(x) ((x) < 0)

/* The player's states, as avplayer_fsm.h numbers them */
enum AvState {
    AV_STATE_UNKNOWN = AVPLAYER_FSM_UNKNOWN,
    AV_STATE_INITIAL = AVPLAYER_FSM_INITIAL,
    AV_STATE_ADDING_SOURCE = AVPLAYER_FSM_ADDING_SOURCE,
    AV_STATE_READY = AVPLAYER_FSM_READY,
    AV_STATE_PLAY = AVPLAYER_FSM_PLAY,
    AV_STATE_STOP = AVPLAYER_FSM_STOP,
    AV_STATE_END_OF_FILE = AVPLAYER_FSM_END_OF_FILE,
    AV_STATE_PAUSE = AVPLAYER_FSM_PAUSE,
    AV_STATE_C0X08 = AVPLAYER_FSM_C0X08,
    AV_STATE_JUMP = AVPLAYER_FSM_JUMP,
    AV_STATE_TRICK_MODE = AVPLAYER_FSM_TRICK_MODE,
    AV_STATE_C0X0B = AVPLAYER_FSM_C0X0B,
    AV_STATE_BUFFERING = AVPLAYER_FSM_BUFFERING,
    AV_STATE_STARTING = AVPLAYER_FSM_STARTING,
    AV_STATE_ERROR = AVPLAYER_FSM_ERROR
};

enum AvEventType {
    AV_EVENT_TYPE_CHANGE_FLOW_STATE = 21,
    AV_EVENT_TYPE_WARNING_ID = 22,
    AV_EVENT_TYPE_REVERT_STATE = 30,
    AV_EVENT_TYPE_ADD_SOURCE = 40,
    AV_EVENT_TYPE_ERROR = 255
};

typedef union AvPlayerEventData {
    u32 num_frames; /* 20 */
    s32 state;      /* AV_EVENT_TYPE_CHANGE_FLOW_STATE: an AvState */
    s32 error;      /* AV_EVENT_TYPE_WARNING_ID */
    u32 attempt;    /* AV_EVENT_TYPE_ADD_SOURCE */
} AvPlayerEventData;

typedef struct AvPlayerEvent {
    s32 event; /* AvEventType */
    AvPlayerEventData payload;
} AvPlayerEvent;

/* What a path names, by its extension (a URI's, without its query or fragment) */
s32 avplayer_get_source_type(const char* path, size_t length);

RETRO_END_DECLS

#endif
