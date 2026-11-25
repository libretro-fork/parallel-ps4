/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <string.h>

#include <retro_atomic.h>

#include "common/c_bridge.h"
#include "common/enum_names.h"
#include "common/thread_name.h"
#include "core/libraries/avplayer/avplayer_common.h"
#include "core/libraries/avplayer/avplayer_error.h"
#include "core/libraries/avplayer/avplayer_state.h"
#include "core/libraries/kernel/kernel_c.h"

static void avplayer_state_emit_event(AvPlayerState* state, s32 event_id, void* event_data);
static bool avplayer_state_set_state(AvPlayerState* state, int to);

/* A string of at most @max bytes, as long as it is up to its NUL */
static size_t bounded_length(const char* s, size_t max) {
    size_t n = 0;
    while (n < max && s[n] != '\0') {
        n++;
    }
    return n;
}

static bool language_is(const char* language, size_t language_length, const char* code) {
    const size_t code_length = bounded_length(code, 4);
    return language_length == code_length && memcmp(language, code, code_length) == 0;
}

static void PS4_SYSV_ABI DefaultEventCallback(void* opaque, s32 event_id, s32 source_id,
                                              void* event_data) {
    AvPlayerState* const self = (AvPlayerState*)opaque;
    const AvPlayerEventCallback callback = self->event_replacement.event_callback;
    void* const ptr = self->event_replacement.object_ptr;
    (void)source_id;
    if (callback != NULL) {
        callback(ptr, event_id, 0, event_data);
    }
}

static void PS4_SYSV_ABI AutoPlayEventCallback(void* opaque, s32 event_id, s32 source_id,
                                               void* event_data) {
    AvPlayerState* const self = (AvPlayerState*)opaque;
    s32 video_stream_index = -1;
    s32 audio_stream_index = -1;
    s32 timedtext_stream_index = -1;
    s32 stream_count;
    u32 stream_index;
    size_t language_length;
    (void)source_id;
    if (event_id != AV_PLAYER_EVENTS_STATE_READY) {
        DefaultEventCallback(opaque, event_id, 0, event_data);
        return;
    }

    stream_count = avplayer_state_get_stream_count(self);
    if (AVPLAYER_IS_ERROR(stream_count)) {
        avplayer_state_stop(self);
        return;
    }
    if (stream_count == 0) {
        avplayer_state_stop(self);
        return;
    }

    language_length = bounded_length(self->default_language, sizeof(self->default_language));
    for (stream_index = 0; stream_index < (u32)stream_count; ++stream_index) {
        AvPlayerStreamInfo info;
        memset(&info, 0, sizeof(info));
        if (!avplayer_state_get_stream_info(self, stream_index, &info)) {
            avplayer_state_stop(self);
            return;
        }
        switch (info.type) {
        case AV_PLAYER_STREAM_TYPE_VIDEO:
            if (video_stream_index == -1) {
                video_stream_index = (s32)stream_index;
            }
            if (language_length != 0 && language_is(self->default_language, language_length,
                                                    info.details.video.language_code)) {
                video_stream_index = (s32)stream_index;
            }
            break;
        case AV_PLAYER_STREAM_TYPE_AUDIO:
            if (audio_stream_index == -1) {
                audio_stream_index = (s32)stream_index;
            }
            if (language_length != 0 && language_is(self->default_language, language_length,
                                                    info.details.audio.language_code)) {
                audio_stream_index = (s32)stream_index;
            }
            break;
        case AV_PLAYER_STREAM_TYPE_TIMED_TEXT:
            if (timedtext_stream_index == -1) {
                timedtext_stream_index = (s32)stream_index;
            }
            if (language_length != 0 && language_is(self->default_language, language_length,
                                                    info.details.subs.language_code)) {
                timedtext_stream_index = (s32)stream_index;
            }
            break;
        default:
            break;
        }
    }

    if (video_stream_index != -1) {
        avplayer_state_enable_stream(self, (u32)video_stream_index);
    }
    if (audio_stream_index != -1) {
        avplayer_state_enable_stream(self, (u32)audio_stream_index);
    }
    if (timedtext_stream_index != -1) {
        avplayer_state_enable_stream(self, (u32)timedtext_stream_index);
    }
    avplayer_state_start(self);
}

/* The source, pinned until avplayer_state_unpin_source, if there is one and it is not being
 * changed (the gate is open); else NULL, and nothing to unpin. No one waits. */
static AvPlayerSource* avplayer_state_pin_source(AvPlayerState* state) {
    AvPlayerSource* source;
    if (!pin_gate_enter(&state->source_gate)) {
        return NULL;
    }
    source = (AvPlayerSource*)retro_atomic_load_acquire_ptr(&state->source);
    if (source == NULL) {
        pin_gate_leave(&state->source_gate);
    }
    return source;
}

static void avplayer_state_unpin_source(AvPlayerState* state) {
    pin_gate_leave(&state->source_gate);
}

static int avplayer_state_current(AvPlayerState* state) {
    return avplayer_fsm_current(&state->fsm);
}

static int avplayer_state_previous(AvPlayerState* state) {
    return avplayer_fsm_previous(&state->fsm);
}

/* Called inside CONTROLLER thread */
static void avplayer_state_on_playback_state_changed(AvPlayerState* state, int to) {
    switch (to) {
    case AV_STATE_READY:
        avplayer_state_emit_event(state, AV_PLAYER_EVENTS_STATE_READY, NULL);
        break;
    case AV_STATE_PLAY:
        avplayer_state_emit_event(state, AV_PLAYER_EVENTS_STATE_PLAY, NULL);
        break;
    case AV_STATE_STOP:
        avplayer_state_emit_event(state, AV_PLAYER_EVENTS_STATE_STOP, NULL);
        break;
    case AV_STATE_PAUSE:
        avplayer_state_emit_event(state, AV_PLAYER_EVENTS_STATE_PAUSE, NULL);
        break;
    case AV_STATE_BUFFERING:
        avplayer_state_emit_event(state, AV_PLAYER_EVENTS_STATE_BUFFERING, NULL);
        break;
    default:
        break;
    }
}

static void avplayer_state_push_event(AvPlayerState* state, const AvPlayerEvent* event,
                                      const char* func) {
    if (!mpsc_ring_put(&state->event_queue, event)) {
        c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_ERROR, func,
              "The event queue is full: an event is dropped");
    }
    retro_eventcount_notify(&state->event_ready);
}

/* Called inside GAME thread */
static void avplayer_state_add_source_event(AvPlayerState* state) {
    AvPlayerEvent event;
    avplayer_state_set_state(state, AV_STATE_ADDING_SOURCE);
    memset(&event, 0, sizeof(event));
    event.event = AV_EVENT_TYPE_ADD_SOURCE;
    avplayer_state_push_event(state, &event, "AddSourceEvent");
}

static void avplayer_state_warning_event(AvPlayerState* state, s32 id) {
    AvPlayerEvent event;
    memset(&event, 0, sizeof(event));
    event.event = AV_EVENT_TYPE_WARNING_ID;
    event.payload.error = id;
    avplayer_state_push_event(state, &event, "WarningEvent");
}

/* Called inside CONTROLLER and GAME threads */
static bool avplayer_state_set_state(AvPlayerState* state, int to) {
    int from = 0;
    if (!avplayer_fsm_set(&state->fsm, to, &from)) {
        c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_ERROR, "SetState",
              "Invalid state transition: %s -> %s", enum_name(&enum_names_AvState, (int64_t)from),
              enum_name(&enum_names_AvState, (int64_t)to));
        return false;
    }
    return true;
}

/* Called inside CONTROLLER thread */
static void avplayer_state_emit_event(AvPlayerState* state, s32 event_id, void* event_data) {
    AvPlayerEventCallback callback;
    c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_INFO, "EmitEvent", "Sending event to the game: id = %s",
          enum_name(&enum_names_AvPlayerEvents, (int64_t)event_id));
    callback = state->init_data.event_replacement.event_callback;
    if (callback) {
        callback(state->init_data.event_replacement.object_ptr, event_id, 0, event_data);
    }
}

/* Called inside CONTROLLER thread: whether the source has frames, -1 when it cannot be asked */
static int avplayer_state_on_buffering_check_event(AvPlayerState* state, u32 num_frames) {
    AvPlayerSource* const source = avplayer_state_pin_source(state);
    bool has;
    if (source == NULL) {
        return -1;
    }
    has = avplayer_source_has_frames(source, num_frames);
    avplayer_state_unpin_source(state);
    return has ? 1 : 0;
}

/* Called inside CONTROLLER thread */
static void avplayer_state_process_event(AvPlayerState* state) {
    AvPlayerEvent event;
    if (avplayer_state_current(state) == AV_STATE_JUMP) {
        return;
    }
    if (!mpsc_ring_take(&state->event_queue, &event)) {
        return;
    }
    switch (event.event) {
    case AV_EVENT_TYPE_WARNING_ID:
        avplayer_state_emit_event(state, AV_PLAYER_EVENTS_WARNING_ID, &event.payload.error);
        break;
    case AV_EVENT_TYPE_REVERT_STATE:
        (void)avplayer_fsm_revert(&state->fsm, NULL, NULL);
        break;
    case AV_EVENT_TYPE_ADD_SOURCE: {
        /* AddSource closed the gate on it: its streams are found with no reader in, then
         * readers are let in */
        AvPlayerSource* const source =
            (AvPlayerSource*)retro_atomic_load_acquire_ptr(&state->source);
        const bool found = source != NULL && avplayer_source_find_streams(source);
        pin_gate_open(&state->source_gate);
        if (found) {
            avplayer_state_set_state(state, AV_STATE_READY);
            avplayer_state_on_playback_state_changed(state, AV_STATE_READY);
        } else {
            avplayer_state_on_warning(state, (u32)ORBIS_AVPLAYER_ERROR_NOT_SUPPORTED);
            avplayer_state_set_state(state, AV_STATE_ERROR);
        }
        break;
    }
    case AV_EVENT_TYPE_ERROR:
        avplayer_state_on_warning(state, (u32)event.payload.error);
        avplayer_state_set_state(state, AV_STATE_ERROR);
        break;
    default:
        break;
    }
}

static void avplayer_state_update_end_of_file_state(AvPlayerState* state) {
    /* Kept out (being changed) counts as still active */
    AvPlayerSource* const source = avplayer_state_pin_source(state);
    const bool active = source == NULL || avplayer_source_is_active(source);
    if (source != NULL) {
        avplayer_state_unpin_source(state);
    }
    if (active || (retro_atomic_exchange_int(&state->eof_stop_event_sent, 1) != 0)) {
        return;
    }
    avplayer_state_emit_event(state, AV_PLAYER_EVENTS_STATE_STOP, NULL);
}

/* Called inside CONTROLLER thread */
static void avplayer_state_update_buffering_state(AvPlayerState* state) {
    if (avplayer_state_current(state) == AV_STATE_END_OF_FILE) {
        avplayer_state_update_end_of_file_state(state);
    } else if (avplayer_state_current(state) == AV_STATE_BUFFERING) {
        const int has_frames = avplayer_state_on_buffering_check_event(state, 10);
        if (has_frames < 0) {
            return;
        }
        if (has_frames) {
            const int to = avplayer_state_previous(state) >= AV_STATE_C0X0B
                               ? avplayer_state_previous(state)
                               : AV_STATE_PLAY;
            avplayer_state_set_state(state, to);
            avplayer_state_on_playback_state_changed(state, to);
        }
    } else if (avplayer_state_current(state) == AV_STATE_PLAY) {
        const int has_frames = avplayer_state_on_buffering_check_event(state, 0);
        if (has_frames < 0) {
            return;
        }
        if (!has_frames) {
            avplayer_state_set_state(state, AV_STATE_BUFFERING);
            avplayer_state_on_playback_state_changed(state, AV_STATE_BUFFERING);
        }
    }
}

static int avplayer_state_controller_ready(void* ctx) {
    AvPlayerState* const state = (AvPlayerState*)ctx;
    return !mpsc_ring_empty(&state->event_queue) ||
           retro_atomic_load_acquire_int(&state->controller_stop) != 0;
}

static void avplayer_state_controller_thread(void* arg) {
    AvPlayerState* const state = (AvPlayerState*)arg;
    /* An event wakes the thread at once; the buffering state is looked at every 5 ms of the
     * game's time, as it was every 5 ms of the host's */
    const u64 buffering_check_ns = 5000000;
    thread_name_set("shadPS4:AvController");
    while (retro_atomic_load_acquire_int(&state->controller_stop) == 0) {
        if (!mpsc_ring_empty(&state->event_queue)) {
            avplayer_state_process_event(state);
            continue;
        }
        kernel_game_wait(&state->event_ready, kernel_game_deadline_in(buffering_check_ns),
                         avplayer_state_controller_ready, state);
        avplayer_state_update_buffering_state(state);
    }
}

/* Called inside GAME thread */
void avplayer_state_init(AvPlayerState* state, const AvPlayerInitData* init_data) {
    memset(state, 0, sizeof(*state));
    state->init_data = *init_data;
    state->event_replacement = init_data->event_replacement;
    state->sync_mode = AV_PLAYER_AV_SYNC_MODE_DEFAULT;
    if (state->event_replacement.event_callback == NULL || init_data->auto_start) {
        state->auto_start = true;
        state->init_data.event_replacement.event_callback = AutoPlayEventCallback;
    } else {
        state->init_data.event_replacement.event_callback = DefaultEventCallback;
    }
    state->init_data.event_replacement.object_ptr = state;

    if (init_data->default_language != NULL) {
        /* Up to its NUL: a shorter string is not read past */
        memcpy(state->default_language, init_data->default_language,
               bounded_length(init_data->default_language, sizeof(state->default_language)));
    }

    avplayer_fsm_init(&state->fsm, AV_STATE_INITIAL);
    avplayer_transport_init(&state->transport);
    pin_gate_init(&state->source_gate);
    mpsc_ring_init(&state->event_queue, AVPLAYER_EVENT_QUEUE_SIZE, sizeof(AvPlayerEvent));
    retro_eventcount_init(&state->event_ready);
    retro_atomic_store_release_int(&state->controller_stop, 0);
    kernel_service_thread_start(&state->controller_thread, avplayer_state_controller_thread, state);
}

void avplayer_state_deinit(AvPlayerState* state) {
    AvPlayerEvent event;
    /* No thread of the player's is left, and no reader is in: then the source goes */
    if (state->controller_thread.thread != NULL) {
        /* With a full fence, as an eventcount's notifier's state change */
        (void)retro_atomic_exchange_int(&state->controller_stop, 1);
        retro_eventcount_notify(&state->event_ready);
        kernel_service_thread_join(&state->controller_thread);
    }
    while (mpsc_ring_take(&state->event_queue, &event)) {
    }
    pin_gate_close(&state->source_gate);
    avplayer_source_delete((AvPlayerSource*)retro_atomic_load_acquire_ptr(&state->source));
    pin_gate_free(&state->source_gate);
    mpsc_ring_free(&state->event_queue);
    retro_eventcount_free(&state->event_ready);
}

void avplayer_state_post_init(AvPlayerState* state, const AvPlayerPostInitData* post_init_data) {
    state->post_init_data = *post_init_data;
}

/* Called inside GAME thread */
bool avplayer_state_add_source(AvPlayerState* state, const char* path, size_t length,
                               s32 source_type) {
    s32 sdk_ver = 0;
    AvPlayerSource* source;
    (void)source_type;
    if (length == 0) {
        c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_ERROR, "AddSource", "File path is empty.");
        return false;
    }

    if (!retro_atomic_cas_int(&state->source_taken, 0, 1)) {
        c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_ERROR, "AddSource", "Only one source is supported.");
        return false;
    }

    kernel_get_compiled_sdk_version(&sdk_ver);

    source = avplayer_source_new(state);
    if (source == NULL || !avplayer_source_init(source, &state->init_data, path, length)) {
        avplayer_state_set_state(state, AV_STATE_ERROR);
        avplayer_source_delete(source);
        retro_atomic_store_release_int(&state->source_taken, 0);
        return false;
    }

    /* Readers are kept out until its streams are found (the controller's AddSource event) */
    pin_gate_close(&state->source_gate);
    retro_atomic_store_release_ptr(&state->source, source);
    avplayer_state_add_source_event(state);
    return true;
}

/* Called inside GAME thread */
s32 avplayer_state_get_stream_count(AvPlayerState* state) {
    AvPlayerSource* const source = avplayer_state_pin_source(state);
    s32 count;
    if (source == NULL) {
        c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_ERROR, "GetStreamCount",
              "Could not get stream count. No source.");
        return -1;
    }
    count = avplayer_source_get_stream_count(source);
    avplayer_state_unpin_source(state);
    return count;
}

/* Called inside GAME thread */
bool avplayer_state_get_stream_info(AvPlayerState* state, u32 stream_index,
                                    AvPlayerStreamInfo* info) {
    AvPlayerSource* const source = avplayer_state_pin_source(state);
    bool got;
    if (source == NULL) {
        return false;
    }
    got = avplayer_source_get_stream_info(source, stream_index, info);
    avplayer_state_unpin_source(state);
    return got;
}

static bool avplayer_state_do_stop(AvPlayerState* state);

/* Has the transport */
static bool avplayer_state_do_start(AvPlayerState* state) {
    AvPlayerSource* const source = (AvPlayerSource*)retro_atomic_load_acquire_ptr(&state->source);
    bool started;
    if (source == NULL ||
        !(avplayer_state_current(state) == AV_STATE_READY ||
          avplayer_state_current(state) == AV_STATE_STOP || avplayer_state_do_stop(state))) {
        c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_ERROR, "DoStart", "Could not start playback.");
        return false;
    }
    (void)retro_atomic_exchange_int(&state->eof_stop_event_sent, 0);
    avplayer_state_set_state(state, AV_STATE_STARTING);
    /* Its queues and buffers are made anew: readers are kept out meanwhile */
    pin_gate_close(&state->source_gate);
    started = avplayer_source_start(source);
    pin_gate_open(&state->source_gate);
    if (!started) {
        c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_ERROR, "DoStart", "Could not start playback.");
        avplayer_state_set_state(state, AV_STATE_ERROR);
        return false;
    }
    avplayer_state_set_state(state, AV_STATE_PLAY);
    avplayer_state_on_playback_state_changed(state, AV_STATE_PLAY);
    return true;
}

/* Has the transport */
static bool avplayer_state_do_stop(AvPlayerState* state) {
    AvPlayerSource* const source = (AvPlayerSource*)retro_atomic_load_acquire_ptr(&state->source);
    if (source == NULL || avplayer_state_current(state) == AV_STATE_STOP) {
        return false;
    }
    if (!avplayer_source_stop(source)) {
        return false;
    }
    if (!avplayer_state_set_state(state, AV_STATE_STOP)) {
        return false;
    }
    (void)retro_atomic_exchange_int(&state->eof_stop_event_sent, 1);
    avplayer_state_on_playback_state_changed(state, AV_STATE_STOP);
    return true;
}

/* Start and Stop: the thread that gets the transport does it, and then whatever was asked of
 * it meanwhile (avplayer_fsm.h) */
static bool avplayer_state_transport(AvPlayerState* state, int op) {
    bool result;
    /* Another thread is starting or stopping it: that one does this after, and the last of
     * what was asked meanwhile is what stands */
    if (!avplayer_transport_claim(&state->transport, op, NULL)) {
        return true;
    }
    result = op == AVPLAYER_TRANSPORT_START ? avplayer_state_do_start(state)
                                            : avplayer_state_do_stop(state);
    while ((op = avplayer_transport_next(&state->transport)) != AVPLAYER_TRANSPORT_NONE) {
        (void)(op == AVPLAYER_TRANSPORT_START ? avplayer_state_do_start(state)
                                              : avplayer_state_do_stop(state));
    }
    return result;
}

/* Called inside GAME and CONTROLLER threads (auto-play) */
bool avplayer_state_start(AvPlayerState* state) {
    return avplayer_state_transport(state, AVPLAYER_TRANSPORT_START);
}

bool avplayer_state_stop(AvPlayerState* state) {
    return avplayer_state_transport(state, AVPLAYER_TRANSPORT_STOP);
}

/* Called inside GAME thread */
bool avplayer_state_pause(AvPlayerState* state) {
    int current;
    AvPlayerSource* source;
    if (avplayer_state_current(state) == AV_STATE_END_OF_FILE) {
        return true;
    }
    current = avplayer_state_current(state);
    source = current == AV_STATE_PAUSE || current == AV_STATE_READY ||
                     current == AV_STATE_INITIAL || current == AV_STATE_UNKNOWN ||
                     current == AV_STATE_ADDING_SOURCE
                 ? NULL
                 : avplayer_state_pin_source(state);
    if (source == NULL) {
        c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_ERROR, "Pause", "Could not pause playback.");
        return false;
    }
    avplayer_source_pause(source);
    avplayer_state_unpin_source(state);
    if (!avplayer_state_set_state(state, AV_STATE_PAUSE)) {
        return false;
    }
    avplayer_state_on_playback_state_changed(state, AV_STATE_PAUSE);
    return true;
}

/* Called inside GAME thread */
bool avplayer_state_resume(AvPlayerState* state) {
    AvPlayerSource* const source =
        avplayer_state_current(state) == AV_STATE_PAUSE ? avplayer_state_pin_source(state) : NULL;
    int resumed = 0;
    if (source == NULL) {
        c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_ERROR, "Resume", "Could not resume playback.");
        return false;
    }
    avplayer_source_resume(source);
    avplayer_state_unpin_source(state);
    /* Back to the state before the pause */
    if (!avplayer_fsm_revert(&state->fsm, NULL, &resumed)) {
        return true;
    }
    avplayer_state_on_playback_state_changed(state, resumed);
    return true;
}

void avplayer_state_set_av_sync_mode(AvPlayerState* state, s32 sync_mode) {
    state->sync_mode = sync_mode;
}

/* Called inside GAME thread */
bool avplayer_state_enable_stream(AvPlayerState* state, u32 stream_index) {
    AvPlayerSource* const source = avplayer_state_pin_source(state);
    bool enabled;
    if (source == NULL) {
        return false;
    }
    enabled = avplayer_source_enable_stream(source, stream_index);
    avplayer_state_unpin_source(state);
    return enabled;
}

/* Called inside GAME thread */
bool avplayer_state_get_video_data(AvPlayerState* state, AvPlayerFrameInfo* video_info) {
    AvPlayerSource* const source = avplayer_state_pin_source(state);
    bool got;
    if (source == NULL) {
        return false;
    }
    got = avplayer_source_get_video_data(source, video_info);
    avplayer_state_unpin_source(state);
    return got;
}

bool avplayer_state_get_video_data_ex(AvPlayerState* state, AvPlayerFrameInfoEx* video_info) {
    AvPlayerSource* const source = avplayer_state_pin_source(state);
    bool got;
    if (source == NULL) {
        return false;
    }
    got = avplayer_source_get_video_data_ex(source, video_info);
    avplayer_state_unpin_source(state);
    return got;
}

bool avplayer_state_get_audio_data(AvPlayerState* state, AvPlayerFrameInfo* audio_info) {
    AvPlayerSource* const source = avplayer_state_pin_source(state);
    bool got;
    if (source == NULL) {
        return false;
    }
    got = avplayer_source_get_audio_data(source, audio_info);
    avplayer_state_unpin_source(state);
    return got;
}

bool avplayer_state_is_active(AvPlayerState* state) {
    AvPlayerSource* source;
    int current;
    bool active;
    /* Kept out while it starts: about to be active */
    if (!pin_gate_enter(&state->source_gate)) {
        return avplayer_state_current(state) == AV_STATE_STARTING;
    }
    source = (AvPlayerSource*)retro_atomic_load_acquire_ptr(&state->source);
    current = avplayer_state_current(state);
    active = source != NULL && current != AV_STATE_STOP && current != AV_STATE_ERROR &&
             avplayer_source_is_active(source);
    pin_gate_leave(&state->source_gate);
    return active;
}

u64 avplayer_state_current_time(AvPlayerState* state) {
    AvPlayerSource* const source = avplayer_state_pin_source(state);
    u64 time;
    if (source == NULL) {
        return 0;
    }
    time = avplayer_source_current_time(source);
    avplayer_state_unpin_source(state);
    return time;
}

bool avplayer_state_set_looping(AvPlayerState* state, bool is_looping) {
    AvPlayerSource* const source = avplayer_state_pin_source(state);
    if (source == NULL) {
        return false;
    }
    avplayer_source_set_looping(source, is_looping);
    avplayer_state_unpin_source(state);
    return true;
}

/* May be called from different threads */
void avplayer_state_on_warning(AvPlayerState* state, u32 id) {
    /* Forward to CONTROLLER thread */
    avplayer_state_warning_event(state, (s32)id);
}

s32 avplayer_state_get_sync_mode(AvPlayerState* state) {
    return state->sync_mode;
}

void avplayer_state_on_error(AvPlayerState* state) {
    avplayer_state_set_state(state, AV_STATE_ERROR);
    avplayer_state_on_playback_state_changed(state, AV_STATE_ERROR);
}

void avplayer_state_on_eof(AvPlayerState* state) {
    avplayer_state_set_state(state, AV_STATE_END_OF_FILE);
}
