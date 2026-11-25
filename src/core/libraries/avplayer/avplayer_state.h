/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_AVPLAYER_AVPLAYER_STATE_H
#define SHADPS4_AVPLAYER_AVPLAYER_STATE_H

/* The player's state, its controller thread and the events it tells the game. Defined in
 * avplayer_state.c. */

#include <stddef.h>

#include <boolean.h>
#include <retro_atomic.h>
#include <retro_common_api.h>
#include <rthreads/retro_eventcount.h>

#include "common/mpsc_ring.h"
#include "common/pin_gate.h"
#include "common/types.h"
#include "core/libraries/avplayer/avplayer.h"
#include "core/libraries/avplayer/avplayer_fsm.h"
#include "core/libraries/avplayer/avplayer_source.h"
#include "core/libraries/kernel/kernel_c.h"

RETRO_BEGIN_DECLS

#define AVPLAYER_EVENT_QUEUE_SIZE 64

typedef struct AvPlayerState {
    /* The source (AvPlayerSource*, owned), published by AddSource; readers go through
     * source_gate, which its changes close (finding its streams, its start, the end) */
    retro_atomic_ptr_t source;
    retro_atomic_int_t source_taken;
    pin_gate_t source_gate;
    AvPlayerInitData init_data;
    AvPlayerPostInitData post_init_data;
    AvPlayerEventReplacement event_replacement;
    bool auto_start;
    char default_language[4];
    s32 sync_mode; /* AvPlayerAvSyncMode */
    avplayer_fsm_t fsm;
    avplayer_transport_t transport;
    retro_atomic_int_t eof_stop_event_sent;
    kernel_service_thread_t controller_thread;
    retro_atomic_int_t controller_stop;
    /* AvPlayerEvent, many threads put, the controller takes */
    mpsc_ring_t event_queue;
    /* Notified on every event pushed: the controller thread parks here between its looks at
     * the buffering state. */
    retro_eventcount_t event_ready;
} AvPlayerState;

/* Made in place (@state does not move after), its controller thread started */
void avplayer_state_init(AvPlayerState* state, const AvPlayerInitData* init_data);
/* No thread of the player's is left, then the source goes */
void avplayer_state_deinit(AvPlayerState* state);

void avplayer_state_post_init(AvPlayerState* state, const AvPlayerPostInitData* post_init_data);
/* @path: @length bytes */
bool avplayer_state_add_source(AvPlayerState* state, const char* path, size_t length,
                               s32 source_type);
s32 avplayer_state_get_stream_count(AvPlayerState* state);
bool avplayer_state_get_stream_info(AvPlayerState* state, u32 stream_index,
                                    AvPlayerStreamInfo* info);
bool avplayer_state_enable_stream(AvPlayerState* state, u32 stream_index);
bool avplayer_state_start(AvPlayerState* state);
bool avplayer_state_stop(AvPlayerState* state);
bool avplayer_state_pause(AvPlayerState* state);
bool avplayer_state_resume(AvPlayerState* state);
void avplayer_state_set_av_sync_mode(AvPlayerState* state, s32 sync_mode);
bool avplayer_state_get_audio_data(AvPlayerState* state, AvPlayerFrameInfo* audio_info);
bool avplayer_state_get_video_data(AvPlayerState* state, AvPlayerFrameInfo* video_info);
bool avplayer_state_get_video_data_ex(AvPlayerState* state, AvPlayerFrameInfoEx* video_info);
bool avplayer_state_is_active(AvPlayerState* state);
u64 avplayer_state_current_time(AvPlayerState* state);
bool avplayer_state_set_looping(AvPlayerState* state, bool is_looping);

/* What the source tells its player, from any of its threads */
s32 avplayer_state_get_sync_mode(AvPlayerState* state);
void avplayer_state_on_warning(AvPlayerState* state, u32 id);
void avplayer_state_on_error(AvPlayerState* state);
void avplayer_state_on_eof(AvPlayerState* state);

RETRO_END_DECLS

#endif
