/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdlib.h>
#include <string.h>

#include <retro_atomic.h>
#include <rthreads/retro_eventcount.h>

#include "common/c_bridge.h"
#include "common/stop_thread.h"
#include "common/thread_name.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/videodec/vdecsw.h"
#include "core/libraries/videodec/video_utils.h"
#include "core/libraries/videodec/videodec_error.h"

#define ALIGN_UP(v, a) (((v) + (a) - 1) / (a) * (a))

/* Fake minimum memory size for querying */
#define VDECSW_MINIMUM_MEMORY_SIZE ((u64)16 * 1024 * 1024)

/* The game's calls post to the worker and take its results through rings; nothing takes a
 * lock. The Sync calls wait on an eventcount, as the game asked to wait. */
#define VDECSW_COMMAND_SLOTS 64u /* inputs not yet synced, and other commands */
#define VDECSW_FRAME_SLOTS 32u   /* pictures decoded ahead of the game's buffers */

enum { VDECSW_CMD_INPUT, VDECSW_CMD_FLUSH, VDECSW_CMD_RESET };

/* Bounded MPMC-style cell: seq says whose turn it is. The access unit buffer is kept with the
 * slot and reused, so it grows only for a larger access unit than before. */
typedef struct vdecsw_command {
    retro_atomic_int_t seq;
    u32 type;
    u32 epoch;
    OrbisVdecswInputData input;
    u8* au;
    size_t au_size;
    size_t au_cap;
} vdecsw_command;

typedef struct vdecsw_completed {
    void* au_data;
    u32 output_frame_count;
    s32 result;
    u32 epoch;
} vdecsw_completed;

/* A picture decoded ahead of the game's output buffer, in NV12, in its slot's buffer */
typedef struct vdecsw_frame {
    u8* nv12;
    size_t nv12_size;
    size_t nv12_cap;
    u32 width;
    u32 height;
    u32 epoch;
    video_picture_meta_t meta;
} vdecsw_frame;

struct vdecsw_decoder {
    video_sw_decoder_t decoder;
    bool is_avc;

    vdecsw_command commands[VDECSW_COMMAND_SLOTS];
    retro_atomic_int_t command_tail; /* posters */
    u32 command_head;                /* the worker */

    vdecsw_completed completed[VDECSW_COMMAND_SLOTS];
    retro_atomic_int_t completed_tail; /* the worker */
    retro_atomic_int_t completed_head; /* the game */

    vdecsw_frame frames[VDECSW_FRAME_SLOTS];
    retro_atomic_int_t frame_tail; /* the worker */
    retro_atomic_int_t frame_head; /* the game */

    retro_atomic_int_t epoch;           /* bumped by a reset */
    retro_atomic_int_t finalized_epoch; /* the epoch whose sequence the worker finished */
    retro_atomic_int_t unsynced_inputs;
    retro_atomic_int_t output_set;
    OrbisVdecswFrameBuffer pending_output;
    retro_atomic_int_t stop;

    retro_eventcount_t worker_ec; /* commands, frame slots, stop */
    retro_eventcount_t game_ec;   /* results, pictures, the end of a sequence, stop */
    bool ec_ok;
    stop_thread_t worker_thread;
};

typedef struct vdecsw_decoder vdecsw_decoder;

static u32 load_u32(retro_atomic_int_t* a) {
    return (u32)retro_atomic_load_acquire_int(a);
}

static u32 load_relaxed_u32(retro_atomic_int_t* a) {
    return (u32)retro_atomic_load_relaxed_int(a);
}

/* Waits on @ec until @ready holds */
static void wait_for(retro_eventcount_t* ec, bool (*ready)(void* ctx), void* ctx) {
    while (!ready(ctx)) {
        const int key = retro_eventcount_prepare_wait(ec);
        if (ready(ctx)) {
            retro_eventcount_cancel_wait(ec);
            return;
        }
        retro_eventcount_commit_wait(ec, key);
    }
}

/* @buf of @size grown to @want, the bytes past its old size zeroed, as a vector's resize */
static bool resize_buffer(u8** buf, size_t* size, size_t* cap, size_t want) {
    if (want > *cap) {
        u8* grown = (u8*)realloc(*buf, want);
        if (!grown) {
            return false;
        }
        *buf = grown;
        *cap = want;
    }
    if (want > *size) {
        memset(*buf + *size, 0, want - *size);
    }
    *size = want;
    return true;
}

static bool post_command(vdecsw_decoder* d, u32 type, const OrbisVdecswInputData* input) {
    u32 pos = load_relaxed_u32(&d->command_tail);
    vdecsw_command* cell;
    for (;;) {
        u32 seq;
        s32 diff;
        cell = &d->commands[pos % VDECSW_COMMAND_SLOTS];
        seq = load_u32(&cell->seq);
        diff = (s32)(seq - pos);
        if (diff == 0) {
            if (retro_atomic_cas_int(&d->command_tail, (int)pos, (int)(pos + 1))) {
                break;
            }
            pos = load_u32(&d->command_tail);
        } else if (diff < 0) {
            return false; /* full */
        } else {
            pos = load_relaxed_u32(&d->command_tail);
        }
    }
    cell->type = type;
    cell->epoch = load_relaxed_u32(&d->epoch);
    if (input) {
        /* The game may reuse its buffer as soon as this call returns */
        cell->input = *input;
        if (resize_buffer(&cell->au, &cell->au_size, &cell->au_cap, (size_t)input->au_size)) {
            memcpy(cell->au, input->au_data, (size_t)input->au_size);
        } else {
            cell->au_size = 0;
        }
    }
    retro_atomic_store_release_int(&cell->seq, (int)(pos + 1));
    retro_eventcount_notify(&d->worker_ec);
    return true;
}

static s32 set_decode_input(vdecsw_decoder* d, const OrbisVdecswInputData* input_data) {
    s32 unsynced;
    u32 queued;
    if (!input_data->au_data) {
        c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_ERROR, "SetDecodeInput",
              "ORBIS_VDECSW_ERROR_ACCESS_UNIT_POINTER");
        return ORBIS_VDECSW_ERROR_ACCESS_UNIT_POINTER;
    }
    if (input_data->au_size == 0) {
        c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_ERROR, "SetDecodeInput",
              "ORBIS_VDECSW_ERROR_ACCESS_UNIT_SIZE");
        return ORBIS_VDECSW_ERROR_ACCESS_UNIT_SIZE;
    }
    if (!d->ec_ok) {
        return ORBIS_VDECSW_ERROR_API_FAIL;
    }
    /* Each input gives at most one picture: with the pictures not taken yet, the inputs in
     * flight must fit the picture ring, so the worker never waits for the game here */
    unsynced = (s32)retro_atomic_load_relaxed_int(&d->unsynced_inputs);
    queued = load_u32(&d->frame_tail) - load_relaxed_u32(&d->frame_head);
    if (queued + (u32)unsynced + 1 > VDECSW_FRAME_SLOTS) {
#ifndef NDEBUG
        c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_TRACE, "SetDecodeInput",
              "ORBIS_VDECSW_ERROR_OUTPUT_PENDING");
#endif
        return ORBIS_VDECSW_ERROR_OUTPUT_PENDING;
    }
    /* Room in the results ring too: the inputs not synced yet are bounded by it */
    if (unsynced >= (s32)VDECSW_COMMAND_SLOTS - 2 ||
        !post_command(d, VDECSW_CMD_INPUT, input_data)) {
        c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_ERROR, "SetDecodeInput",
              "ORBIS_VDECSW_ERROR_INPUT_QUEUE_FULL");
        return ORBIS_VDECSW_ERROR_INPUT_QUEUE_FULL;
    }
    retro_atomic_fetch_add_int(&d->unsynced_inputs, 1);
    return ORBIS_OK;
}

/* The game's side: the oldest result of the current epoch, the stale ones dropped */
static bool peek_completed(vdecsw_decoder* d, vdecsw_completed* out) {
    const u32 epoch = load_relaxed_u32(&d->epoch);
    u32 head = load_relaxed_u32(&d->completed_head);
    while (head != load_u32(&d->completed_tail)) {
        *out = d->completed[head % VDECSW_COMMAND_SLOTS];
        retro_atomic_store_release_int(&d->completed_head, (int)++head);
        if (out->epoch == epoch) {
            return true;
        }
    }
    return false;
}

typedef struct sync_input_wait {
    vdecsw_decoder* d;
    vdecsw_completed entry;
    bool got;
} sync_input_wait;

static bool sync_input_ready(void* ctx) {
    sync_input_wait* w = (sync_input_wait*)ctx;
    w->got = w->got || peek_completed(w->d, &w->entry);
    return w->got || (s32)retro_atomic_load_relaxed_int(&w->d->unsynced_inputs) == 0 ||
           retro_atomic_load_acquire_int(&w->d->stop) != 0;
}

static s32 give_input_result(vdecsw_decoder* d, const vdecsw_completed* entry,
                             OrbisVdecswInputResult* input_result) {
    retro_atomic_fetch_sub_int(&d->unsynced_inputs, 1);
    input_result->decoded_au = entry->au_data;
    input_result->output_frame_count = entry->output_frame_count;
    input_result->reserved0 = 0;
    return entry->result;
}

static s32 sync_decode_input(vdecsw_decoder* d, OrbisVdecswInputResult* input_result) {
    sync_input_wait w;
    memset(&w, 0, sizeof(w));
    w.d = d;
    if (d->ec_ok) {
        wait_for(&d->game_ec, sync_input_ready, &w);
    }
    if (!w.got) {
        c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_ERROR, "SyncDecodeInput",
              "ORBIS_VDECSW_ERROR_INPUT_QUEUE_EMPTY");
        return ORBIS_VDECSW_ERROR_INPUT_QUEUE_EMPTY;
    }
    return give_input_result(d, &w.entry, input_result);
}

static s32 try_sync_decode_input(vdecsw_decoder* d, OrbisVdecswInputResult* input_result) {
    vdecsw_completed entry;
    memset(&entry, 0, sizeof(entry));
    if (!peek_completed(d, &entry)) {
        if ((s32)retro_atomic_load_relaxed_int(&d->unsynced_inputs) == 0) {
            c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_ERROR, "TrySyncDecodeInput",
                  "ORBIS_VDECSW_ERROR_INPUT_QUEUE_EMPTY");
            return ORBIS_VDECSW_ERROR_INPUT_QUEUE_EMPTY;
        }
#ifndef NDEBUG
        c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_TRACE, "TrySyncDecodeInput",
              "ORBIS_VDECSW_ERROR_DECODE_PENDING");
#endif
        return ORBIS_VDECSW_ERROR_DECODE_PENDING;
    }
    return give_input_result(d, &entry, input_result);
}

static s32 set_decode_output(vdecsw_decoder* d, const OrbisVdecswFrameBuffer* frame_buffer) {
    if (!frame_buffer->frame_buffer) {
        c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_ERROR, "SetDecodeOutput",
              "ORBIS_VDECSW_ERROR_FRAME_BUFFER_POINTER");
        return ORBIS_VDECSW_ERROR_FRAME_BUFFER_POINTER;
    }
    if (frame_buffer->frame_buffer_size == 0) {
        c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_ERROR, "SetDecodeOutput",
              "ORBIS_VDECSW_ERROR_FRAME_BUFFER_SIZE");
        return ORBIS_VDECSW_ERROR_FRAME_BUFFER_SIZE;
    }
    if (retro_atomic_load_acquire_int(&d->output_set) != 0) {
        c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_ERROR, "SetDecodeOutput",
              "ORBIS_VDECSW_ERROR_OUTPUT_BUFFER_FULL");
        return ORBIS_VDECSW_ERROR_OUTPUT_BUFFER_FULL;
    }
    d->pending_output = *frame_buffer;
    retro_atomic_store_release_int(&d->output_set, 1);
    return ORBIS_OK;
}

static vdecsw_frame* peek_frame(vdecsw_decoder* d) {
    const u32 epoch = load_relaxed_u32(&d->epoch);
    u32 head = load_relaxed_u32(&d->frame_head);
    while (head != load_u32(&d->frame_tail)) {
        vdecsw_frame* frame = &d->frames[head % VDECSW_FRAME_SLOTS];
        if (frame->epoch == epoch) {
            return frame;
        }
        retro_atomic_store_release_int(&d->frame_head, (int)++head);
        retro_eventcount_notify(&d->worker_ec);
    }
    return NULL;
}

static void pop_frame(vdecsw_decoder* d) {
    retro_atomic_fetch_add_int(&d->frame_head, 1);
    retro_eventcount_notify(&d->worker_ec);
}

static bool finalized(vdecsw_decoder* d) {
    return load_u32(&d->finalized_epoch) == load_relaxed_u32(&d->epoch);
}

static void reset_output_info(OrbisVdecswOutputInfo* output_info) {
    output_info->is_valid = false;
    output_info->is_last_frame = false;
    output_info->is_error_frame = false;
    output_info->is_discarded_frame = false;
    output_info->picture_count = 0;
}

static s32 write_frame(vdecsw_decoder* d, OrbisVdecswOutputInfo* output_info,
                       const vdecsw_frame* frame, const OrbisVdecswFrameBuffer* frame_buffer,
                       bool is_last_frame) {
    const u64 info_size =
        d->is_avc ? sizeof(OrbisVdecswAvcPictureInfo) : sizeof(OrbisVdecswHevcPictureInfo);
    u32 width;
    u32 pitch;
    u32 height;
    u8* info;
    if (frame_buffer->frame_buffer_size < info_size ||
        frame->nv12_size > frame_buffer->frame_buffer_size - info_size) {
        c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_ERROR, "WriteFrame",
              "The %ux%u picture does not fit the frame buffer", frame->width, frame->height);
        return ORBIS_VDECSW_ERROR_API_FAIL;
    }
    if (frame->nv12_size) {
        memcpy(frame_buffer->frame_buffer, frame->nv12, frame->nv12_size);
    }

    width = ALIGN_UP(frame->width, 16u);
    pitch = ALIGN_UP(frame->width, 64u);
    height = ALIGN_UP(frame->height, 16u);

    output_info->is_valid = true;
    output_info->is_last_frame = is_last_frame;
    output_info->picture_count = 1; /* TODO: 2 pictures for interlaced video */
    output_info->codec_type =
        d->is_avc ? ORBIS_VDECSW_CODEC_TYPE_AVC : ORBIS_VDECSW_CODEC_TYPE_HEVC;
    output_info->frame_width = width;
    output_info->frame_pitch = pitch;
    output_info->frame_height = height;
    output_info->frame_buffer = frame_buffer->frame_buffer;
    output_info->frame_buffer_size = (pitch * height * 3) / 2;

    /* Only set frameFormat and framePitchInBytes if the game uses the newer struct version */
    if (output_info->this_size == sizeof(OrbisVdecswOutputInfo)) {
        output_info->frame_format = 0;
        output_info->frame_pitch_in_bytes = pitch;
    }

    info = (u8*)output_info->frame_buffer + output_info->frame_buffer_size;
    if (d->is_avc) {
        OrbisVdecswAvcPictureInfo* picture_info = (OrbisVdecswAvcPictureInfo*)(void*)info;
        memset(picture_info, 0, sizeof(*picture_info));
        picture_info->is_valid = true;
        picture_info->pts_data = frame->meta.pts;
        picture_info->dts_data = frame->meta.dts;
        picture_info->attached_data = frame->meta.attached;
        picture_info->frame_crop_top_offset = 0;
        picture_info->frame_crop_left_offset = 0;
        picture_info->frame_crop_right_offset = pitch - frame->width;
        picture_info->frame_crop_bottom_offset = height - frame->height;
    } else {
        OrbisVdecswHevcPictureInfo* picture_info = (OrbisVdecswHevcPictureInfo*)(void*)info;
        memset(picture_info, 0, sizeof(*picture_info));
        picture_info->is_valid = true;
        picture_info->pts_data = frame->meta.pts;
        picture_info->dts_data = frame->meta.dts;
        picture_info->attached_data = frame->meta.attached;
        picture_info->pic_width_in_luma_samples = frame->width;
        picture_info->pic_height_in_luma_samples = frame->height;
        picture_info->frame_crop_top_offset = 0;
        picture_info->frame_crop_left_offset = 0;
        picture_info->frame_crop_right_offset = pitch - frame->width;
        picture_info->frame_crop_bottom_offset = height - frame->height;
    }
    return ORBIS_OK;
}

static s32 take_output(vdecsw_decoder* d, OrbisVdecswOutputInfo* output_info,
                       const OrbisVdecswFrameBuffer* frame_buffer) {
    /* Finalized is read before the ring: the worker finishes a sequence after its last
     * picture */
    const bool is_finalized = finalized(d);
    vdecsw_frame* frame = peek_frame(d);
    u32 head;
    bool is_last_frame;
    s32 result;
    if (!frame) {
        output_info->is_last_frame = is_finalized;
        return ORBIS_OK;
    }
    head = load_relaxed_u32(&d->frame_head);
    is_last_frame = is_finalized && head + 1 == load_u32(&d->frame_tail);
    result = write_frame(d, output_info, frame, frame_buffer, is_last_frame);
    pop_frame(d);
    return result;
}

static bool sync_output_ready(void* ctx) {
    vdecsw_decoder* d = (vdecsw_decoder*)ctx;
    return peek_frame(d) != NULL || finalized(d) || retro_atomic_load_acquire_int(&d->stop) != 0;
}

static s32 sync_decode_output(vdecsw_decoder* d, OrbisVdecswOutputInfo* output_info) {
    OrbisVdecswFrameBuffer frame_buffer;
    reset_output_info(output_info);
    if (retro_atomic_load_acquire_int(&d->output_set) == 0) {
        c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_ERROR, "SyncDecodeOutput",
              "ORBIS_VDECSW_ERROR_OUTPUT_BUFFER_EMPTY");
        return ORBIS_VDECSW_ERROR_OUTPUT_BUFFER_EMPTY;
    }
    frame_buffer = d->pending_output;
    retro_atomic_store_release_int(&d->output_set, 0);
    if (d->ec_ok) {
        wait_for(&d->game_ec, sync_output_ready, d);
    }
    return take_output(d, output_info, &frame_buffer);
}

static s32 try_sync_decode_output(vdecsw_decoder* d, OrbisVdecswOutputInfo* output_info) {
    OrbisVdecswFrameBuffer frame_buffer;
    reset_output_info(output_info);
    if (retro_atomic_load_acquire_int(&d->output_set) == 0) {
        c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_ERROR, "TrySyncDecodeOutput",
              "ORBIS_VDECSW_ERROR_OUTPUT_BUFFER_EMPTY");
        return ORBIS_VDECSW_ERROR_OUTPUT_BUFFER_EMPTY;
    }
    if (!peek_frame(d) && !finalized(d)) {
#ifndef NDEBUG
        c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_TRACE, "TrySyncDecodeOutput",
              "ORBIS_VDECSW_ERROR_DECODE_PENDING");
#endif
        return ORBIS_VDECSW_ERROR_DECODE_PENDING;
    }
    frame_buffer = d->pending_output;
    retro_atomic_store_release_int(&d->output_set, 0);
    return take_output(d, output_info, &frame_buffer);
}

static s32 finalize_decode_sequence(vdecsw_decoder* d) {
    if (!d->ec_ok || !post_command(d, VDECSW_CMD_FLUSH, NULL)) {
        return ORBIS_VDECSW_ERROR_INPUT_QUEUE_FULL;
    }
    return ORBIS_OK;
}

static s32 decoder_reset(vdecsw_decoder* d) {
    vdecsw_completed stale;
    /* A new epoch: whatever the worker still hands back from before is dropped as it is
     * read */
    retro_atomic_fetch_add_int(&d->epoch, 1);
    retro_atomic_store_release_int(&d->output_set, 0);
    retro_atomic_store_relaxed_int(&d->unsynced_inputs, 0);
    (void)peek_completed(d, &stale);
    (void)peek_frame(d);
    if (!d->ec_ok || !post_command(d, VDECSW_CMD_RESET, NULL)) {
        return ORBIS_VDECSW_ERROR_INPUT_QUEUE_FULL;
    }
    return ORBIS_OK;
}

/* ---- The worker ---- */

static void complete_input(vdecsw_decoder* d, void* au_data, u32 frame_count, s32 result,
                           u32 epoch) {
    const u32 tail = load_relaxed_u32(&d->completed_tail);
    vdecsw_completed* entry = &d->completed[tail % VDECSW_COMMAND_SLOTS];
    /* SetDecodeInput keeps the inputs not yet synced below the ring's size */
    entry->au_data = au_data;
    entry->output_frame_count = frame_count;
    entry->result = result;
    entry->epoch = epoch;
    retro_atomic_store_release_int(&d->completed_tail, (int)(tail + 1));
    retro_eventcount_notify(&d->game_ec);
}

typedef struct queue_frame_wait {
    vdecsw_decoder* d;
    u32 tail;
    u32 epoch;
} queue_frame_wait;

static bool queue_frame_ready(void* ctx) {
    const queue_frame_wait* w = (const queue_frame_wait*)ctx;
    return w->tail - load_u32(&w->d->frame_head) < VDECSW_FRAME_SLOTS ||
           retro_atomic_load_acquire_int(&w->d->stop) != 0 ||
           w->epoch != load_relaxed_u32(&w->d->epoch);
}

static u32 queue_frame(vdecsw_decoder* d, u32 epoch) {
    queue_frame_wait w;
    vdecsw_frame* frame;
    w.d = d;
    w.tail = load_relaxed_u32(&d->frame_tail);
    w.epoch = epoch;
    /* Only the pictures a flush hands out can find the ring full: the game takes them next */
    wait_for(&d->worker_ec, queue_frame_ready, &w);
    if (w.tail - load_u32(&d->frame_head) >= VDECSW_FRAME_SLOTS ||
        epoch != load_relaxed_u32(&d->epoch)) {
        return 0;
    }
    frame = &d->frames[w.tail % VDECSW_FRAME_SLOTS];
    if (!resize_buffer(&frame->nv12, &frame->nv12_size, &frame->nv12_cap,
                       (size_t)video_sw_decoder_nv12_size(&d->decoder)) ||
        !video_sw_decoder_write_nv12(&d->decoder, frame->nv12, frame->nv12_size)) {
        return 0;
    }
    frame->width = d->decoder.width;
    frame->height = d->decoder.height;
    frame->epoch = epoch;
    frame->meta = d->decoder.meta;
    retro_atomic_store_release_int(&d->frame_tail, (int)(w.tail + 1));
    retro_eventcount_notify(&d->game_ec);
    return 1;
}

static void process_input(vdecsw_decoder* d, vdecsw_command* command) {
    video_picture_meta_t meta;
    int ret;
    if (command->epoch != load_relaxed_u32(&d->epoch)) {
        return; /* posted before a reset */
    }
    if (!video_sw_decoder_is_open(&d->decoder)) {
        complete_input(d, command->input.au_data, 0, ORBIS_VDECSW_ERROR_API_FAIL, command->epoch);
        return;
    }
    meta.pts = command->input.pts_data;
    meta.dts = command->input.dts_data;
    meta.attached = command->input.attached_data;
    ret = video_sw_decoder_decode(&d->decoder, command->au, command->au_size, &meta);
    if (ret < 0) {
        c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_ERROR, "ProcessInput",
              "Error decoding the access unit: %d", ret);
        complete_input(d, command->input.au_data, 0, ORBIS_VDECSW_ERROR_API_FAIL, command->epoch);
        return;
    }
    complete_input(d, command->input.au_data, ret ? queue_frame(d, command->epoch) : 0, ORBIS_OK,
                   command->epoch);
}

static void process_flush(vdecsw_decoder* d, u32 epoch) {
    if (epoch != load_relaxed_u32(&d->epoch)) {
        return;
    }
    /* The pictures still held for display order */
    while (video_sw_decoder_is_open(&d->decoder) && video_sw_decoder_drain(&d->decoder)) {
        queue_frame(d, epoch);
    }
    retro_atomic_store_release_int(&d->finalized_epoch, (int)epoch);
    retro_eventcount_notify(&d->game_ec);
}

typedef struct worker_wait {
    vdecsw_decoder* d;
    vdecsw_command* cell;
} worker_wait;

static bool worker_ready(void* ctx) {
    worker_wait* w = (worker_wait*)ctx;
    vdecsw_command* next = &w->d->commands[w->d->command_head % VDECSW_COMMAND_SLOTS];
    if (load_u32(&next->seq) == w->d->command_head + 1) {
        w->cell = next;
        return true;
    }
    return retro_atomic_load_acquire_int(&w->d->stop) != 0;
}

static void worker_loop(void* arg) {
    vdecsw_decoder* d = (vdecsw_decoder*)arg;
    thread_name_set("shadPS4:VdecswWorker");
    for (;;) {
        worker_wait w;
        w.d = d;
        w.cell = NULL;
        wait_for(&d->worker_ec, worker_ready, &w);
        if (!w.cell) {
            return;
        }
        switch (w.cell->type) {
        case VDECSW_CMD_INPUT:
            process_input(d, w.cell);
            break;
        case VDECSW_CMD_FLUSH:
            process_flush(d, w.cell->epoch);
            break;
        case VDECSW_CMD_RESET:
            video_sw_decoder_reset(&d->decoder);
            break;
        default:
            break;
        }
        retro_atomic_store_release_int(&w.cell->seq, (int)(d->command_head + VDECSW_COMMAND_SLOTS));
        ++d->command_head;
    }
}

/* A decoder, made with its worker: NULL if there is no memory for it */
static vdecsw_decoder* decoder_create(const OrbisVdecswDecoderConfigInfo* config_info) {
    u32 i;
    bool worker_ec;
    bool game_ec;
    vdecsw_decoder* d = (vdecsw_decoder*)calloc(1, sizeof(*d));
    if (!d) {
        return NULL;
    }
    (void)video_sw_decoder_init(&d->decoder,
                                config_info->codec_type != ORBIS_VDECSW_CODEC_TYPE_AVC);
    d->is_avc = config_info->codec_type == ORBIS_VDECSW_CODEC_TYPE_AVC;
    for (i = 0; i < VDECSW_COMMAND_SLOTS; ++i) {
        retro_atomic_store_relaxed_int(&d->commands[i].seq, (int)i);
    }
    retro_atomic_store_relaxed_int(&d->finalized_epoch, (int)~0u);
    worker_ec = retro_eventcount_init(&d->worker_ec) != 0;
    game_ec = retro_eventcount_init(&d->game_ec) != 0;
    d->ec_ok = worker_ec && game_ec;
    if (!d->ec_ok) {
        c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_ERROR, "VdecDecoder",
              "No eventcount: the decoder cannot run");
        if (worker_ec) {
            retro_eventcount_free(&d->worker_ec);
        }
        if (game_ec) {
            retro_eventcount_free(&d->game_ec);
        }
        return d;
    }
    stop_thread_start(&d->worker_thread, worker_loop, d);
    return d;
}

static void decoder_destroy(vdecsw_decoder* d) {
    u32 i;
    if (d->ec_ok) {
        retro_atomic_store_release_int(&d->stop, 1);
        retro_eventcount_notify(&d->worker_ec);
        retro_eventcount_notify(&d->game_ec);
        stop_thread_join(&d->worker_thread);
        retro_eventcount_free(&d->worker_ec);
        retro_eventcount_free(&d->game_ec);
    }
    for (i = 0; i < VDECSW_COMMAND_SLOTS; ++i) {
        free(d->commands[i].au);
    }
    for (i = 0; i < VDECSW_FRAME_SLOTS; ++i) {
        free(d->frames[i].nv12);
    }
    video_sw_decoder_free(&d->decoder);
    free(d);
}

/* ---- The library ---- */

static u64 compute_frame_size_bytes(s32 width, s32 height) {
    u32 aligned_width;
    u32 aligned_height;
    if (width <= 0 || height <= 0) {
        return 0;
    }
    aligned_width = ALIGN_UP((u32)width, 64u);
    aligned_height = ALIGN_UP((u32)height, 16u);
    return ((u64)aligned_width * (u64)aligned_height * 3) / 2;
}

static void compute_worst_case_dimensions(const OrbisVdecswDecoderConfigInfo* cfg, s32* out_width,
                                          s32* out_height) {
    if (cfg->max_frame_width > 0 && cfg->max_frame_height > 0) {
        *out_width = cfg->max_frame_width;
        *out_height = cfg->max_frame_height;
        return;
    }
    *out_width = 1920;
    *out_height = 1080;
    if (cfg->max_level >= 150) {
        *out_width = 3840;
        *out_height = 2160;
    }
}

#ifndef NDEBUG
#define VDECSW_TRACE_CALLED(func) c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_TRACE, func, "called")
#else
#define VDECSW_TRACE_CALLED(func) ((void)0)
#endif

s32 PS4_SYSV_ABI sceVdecswQueryComputeMemoryInfo(OrbisVdecswComputeMemoryInfo* compute_mem_info) {
    const char* const func = "sceVdecswQueryComputeMemoryInfo";
    c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_INFO, func, "called");
    if (!compute_mem_info) {
        c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_ERROR, func, "Invalid arguments");
        return ORBIS_VDECSW_ERROR_ARGUMENT_POINTER;
    }
    if (compute_mem_info->this_size != sizeof(OrbisVdecswComputeMemoryInfo)) {
        c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_ERROR, func, "Invalid struct size");
        return ORBIS_VDECSW_ERROR_STRUCT_SIZE;
    }
    compute_mem_info->cpu_gpu_memory = NULL;
    compute_mem_info->cpu_gpu_memory_size = VDECSW_MINIMUM_MEMORY_SIZE;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI
sceVdecswAllocateComputeQueue(const OrbisVdecswComputeConfigInfo* compute_cfg_info_in,
                              const OrbisVdecswComputeMemoryInfo* compute_mem_info_in,
                              OrbisVdecswComputeQueue* compute_queue_out) {
    const char* const func = "sceVdecswAllocateComputeQueue";
    c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_WARNING, func, "called");
    if (!compute_cfg_info_in || !compute_mem_info_in || !compute_queue_out) {
        c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_ERROR, func, "Invalid arguments");
        return ORBIS_VDECSW_ERROR_ARGUMENT_POINTER;
    }
    if (compute_cfg_info_in->this_size != sizeof(OrbisVdecswComputeConfigInfo) ||
        compute_mem_info_in->this_size != sizeof(OrbisVdecswComputeMemoryInfo)) {
        c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_ERROR, func, "Invalid struct size");
        return ORBIS_VDECSW_ERROR_STRUCT_SIZE;
    }
    if (compute_cfg_info_in->reserved0 != 0 || compute_cfg_info_in->reserved1 != 0) {
        c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_ERROR, func, "Invalid compute config");
        return ORBIS_VDECSW_ERROR_CONFIG_INFO;
    }
    if (compute_cfg_info_in->compute_pipe_id > 4) {
        c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_ERROR, func, "Invalid compute pipe id");
        return ORBIS_VDECSW_ERROR_COMPUTE_PIPE_ID;
    }
    if (compute_cfg_info_in->compute_queue_id > 7) {
        c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_ERROR, func, "Invalid compute queue id");
        return ORBIS_VDECSW_ERROR_COMPUTE_QUEUE_ID;
    }
    if (!compute_mem_info_in->cpu_gpu_memory) {
        c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_ERROR, func, "Invalid memory pointer");
        return ORBIS_VDECSW_ERROR_MEMORY_POINTER;
    }
    /* The real library returns a pointer to memory inside cpuGpuMemory */
    *compute_queue_out = compute_mem_info_in->cpu_gpu_memory;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceVdecswReleaseComputeQueue(OrbisVdecswComputeQueue compute_queue_in) {
    c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_INFO, "sceVdecswReleaseComputeQueue", "called");
    return ORBIS_OK;
}

static bool config_size_ok(const OrbisVdecswDecoderConfigInfo* cfg) {
    return cfg->this_size == sizeof(OrbisVdecswDecoderConfigInfo) ||
           cfg->this_size == sizeof(OrbisVdecswDecoderConfigInfoV1);
}

s32 PS4_SYSV_ABI
sceVdecswQueryDecoderMemoryInfo(const OrbisVdecswDecoderConfigInfo* decoder_config_info_in,
                                OrbisVdecswDecoderMemoryInfo* decoder_memory_info_out) {
    const char* const func = "sceVdecswQueryDecoderMemoryInfo";
    s32 width = 0;
    s32 height = 0;
    u64 frame_size;
    u64 max_frame_buffer;
    c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_INFO, func, "called");
    if (!decoder_config_info_in || !decoder_memory_info_out) {
        c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_ERROR, func, "Invalid arguments");
        return ORBIS_VDECSW_ERROR_ARGUMENT_POINTER;
    }
    if (!config_size_ok(decoder_config_info_in) ||
        decoder_memory_info_out->this_size != sizeof(OrbisVdecswDecoderMemoryInfo)) {
        c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_ERROR, func, "Invalid struct size");
        return ORBIS_VDECSW_ERROR_STRUCT_SIZE;
    }
    compute_worst_case_dimensions(decoder_config_info_in, &width, &height);
    frame_size = compute_frame_size_bytes(width, height);
    if (frame_size == 0) {
        max_frame_buffer = VDECSW_MINIMUM_MEMORY_SIZE;
    } else {
        max_frame_buffer = ALIGN_UP(frame_size, (u64)256) + 0x4000;
    }
    decoder_memory_info_out->cpu_memory = NULL;
    decoder_memory_info_out->gpu_memory = NULL;
    decoder_memory_info_out->cpu_gpu_memory = NULL;
    decoder_memory_info_out->cpu_gpu_memory_size = VDECSW_MINIMUM_MEMORY_SIZE;
    decoder_memory_info_out->cpu_memory_size = VDECSW_MINIMUM_MEMORY_SIZE;
    decoder_memory_info_out->gpu_memory_size = VDECSW_MINIMUM_MEMORY_SIZE;
    decoder_memory_info_out->max_frame_buffer_size = max_frame_buffer;
    decoder_memory_info_out->frame_buffer_alignment = 0x100;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceVdecswCreateDecoder(const OrbisVdecswDecoderConfigInfo* decoder_config_info_in,
                                        const OrbisVdecswDecoderMemoryInfo* decoder_memory_info_in,
                                        OrbisVdecswDecoder* decoder_instance_out) {
    const char* const func = "sceVdecswCreateDecoder";
    vdecsw_decoder* d;
    c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_INFO, func, "called");
    if (!decoder_config_info_in || !decoder_memory_info_in || !decoder_instance_out) {
        c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_ERROR, func, "Invalid arguments");
        return ORBIS_VDECSW_ERROR_ARGUMENT_POINTER;
    }
    if (!config_size_ok(decoder_config_info_in) ||
        decoder_memory_info_in->this_size != sizeof(OrbisVdecswDecoderMemoryInfo)) {
        c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_ERROR, func, "Invalid struct size");
        return ORBIS_VDECSW_ERROR_STRUCT_SIZE;
    }
    d = decoder_create(decoder_config_info_in);
    if (!d) {
        c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_ERROR, func, "No memory for the decoder");
        return ORBIS_VDECSW_ERROR_API_FAIL;
    }
    *decoder_instance_out = d;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceVdecswDeleteDecoder(OrbisVdecswDecoder decoder_instance_in) {
    c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_INFO, "sceVdecswDeleteDecoder", "called");
    if (!decoder_instance_in) {
        c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_ERROR, "sceVdecswDeleteDecoder", "Invalid arguments");
        return ORBIS_VDECSW_ERROR_DECODER_INSTANCE;
    }
    decoder_destroy(decoder_instance_in);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceVdecswResetDecoder(OrbisVdecswDecoder decoder_instance_in) {
    c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_INFO, "sceVdecswResetDecoder", "called");
    if (!decoder_instance_in) {
        c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_ERROR, "sceVdecswResetDecoder",
              "Invalid decoder instance");
        return ORBIS_VDECSW_ERROR_DECODER_INSTANCE;
    }
    return decoder_reset(decoder_instance_in);
}

s32 PS4_SYSV_ABI sceVdecswSetDecodeInput(OrbisVdecswDecoder decoder_instance_in,
                                         const OrbisVdecswInputData* input_data_in) {
    const char* const func = "sceVdecswSetDecodeInput";
    VDECSW_TRACE_CALLED(func);
    if (!decoder_instance_in) {
        c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_ERROR, func, "Invalid decoder instance");
        return ORBIS_VDECSW_ERROR_DECODER_INSTANCE;
    }
    if (!input_data_in) {
        c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_ERROR, func, "Invalid arguments");
        return ORBIS_VDECSW_ERROR_ARGUMENT_POINTER;
    }
    if (input_data_in->this_size != sizeof(OrbisVdecswInputData)) {
        c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_ERROR, func, "Invalid struct size");
        return ORBIS_VDECSW_ERROR_STRUCT_SIZE;
    }
    return set_decode_input(decoder_instance_in, input_data_in);
}

s32 PS4_SYSV_ABI sceVdecswSyncDecodeInput(OrbisVdecswDecoder decoder_instance_in,
                                          OrbisVdecswInputResult* input_result_out) {
    const char* const func = "sceVdecswSyncDecodeInput";
    VDECSW_TRACE_CALLED(func);
    if (!decoder_instance_in) {
        c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_ERROR, func, "Invalid decoder instance");
        return ORBIS_VDECSW_ERROR_DECODER_INSTANCE;
    }
    if (!input_result_out) {
        c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_ERROR, func, "Invalid arguments");
        return ORBIS_VDECSW_ERROR_ARGUMENT_POINTER;
    }
    return sync_decode_input(decoder_instance_in, input_result_out);
}

s32 PS4_SYSV_ABI sceVdecswTrySyncDecodeInput(OrbisVdecswDecoder decoder_instance_in,
                                             OrbisVdecswInputResult* input_result_out) {
    const char* const func = "sceVdecswTrySyncDecodeInput";
    VDECSW_TRACE_CALLED(func);
    if (!decoder_instance_in) {
        c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_ERROR, func, "Invalid decoder instance");
        return ORBIS_VDECSW_ERROR_DECODER_INSTANCE;
    }
    if (!input_result_out) {
        c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_ERROR, func, "Invalid arguments");
        return ORBIS_VDECSW_ERROR_ARGUMENT_POINTER;
    }
    return try_sync_decode_input(decoder_instance_in, input_result_out);
}

s32 PS4_SYSV_ABI sceVdecswSetDecodeOutput(OrbisVdecswDecoder decoder_instance_in,
                                          OrbisVdecswFrameBuffer* frame_buffer_in_out) {
    const char* const func = "sceVdecswSetDecodeOutput";
    VDECSW_TRACE_CALLED(func);
    if (!decoder_instance_in) {
        c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_ERROR, func, "Invalid decoder instance");
        return ORBIS_VDECSW_ERROR_DECODER_INSTANCE;
    }
    if (!frame_buffer_in_out) {
        c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_ERROR, func, "Invalid arguments");
        return ORBIS_VDECSW_ERROR_ARGUMENT_POINTER;
    }
    if (frame_buffer_in_out->this_size != sizeof(OrbisVdecswFrameBuffer)) {
        c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_ERROR, func, "Invalid struct size");
        return ORBIS_VDECSW_ERROR_STRUCT_SIZE;
    }
    return set_decode_output(decoder_instance_in, frame_buffer_in_out);
}

s32 PS4_SYSV_ABI sceVdecswSyncDecodeOutput(OrbisVdecswDecoder decoder_instance_in,
                                           OrbisVdecswOutputInfo* output_info_out) {
    const char* const func = "sceVdecswSyncDecodeOutput";
    VDECSW_TRACE_CALLED(func);
    if (!decoder_instance_in) {
        c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_ERROR, func, "Invalid decoder instance");
        return ORBIS_VDECSW_ERROR_DECODER_INSTANCE;
    }
    if (!output_info_out) {
        c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_ERROR, func, "Invalid arguments");
        return ORBIS_VDECSW_ERROR_ARGUMENT_POINTER;
    }
    return sync_decode_output(decoder_instance_in, output_info_out);
}

s32 PS4_SYSV_ABI sceVdecswTrySyncDecodeOutput(OrbisVdecswDecoder decoder_instance_in,
                                              OrbisVdecswOutputInfo* output_info_out) {
    const char* const func = "sceVdecswTrySyncDecodeOutput";
    VDECSW_TRACE_CALLED(func);
    if (!decoder_instance_in) {
        c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_ERROR, func, "Invalid decoder instance");
        return ORBIS_VDECSW_ERROR_DECODER_INSTANCE;
    }
    if (!output_info_out) {
        c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_ERROR, func, "Invalid arguments");
        return ORBIS_VDECSW_ERROR_ARGUMENT_POINTER;
    }
    return try_sync_decode_output(decoder_instance_in, output_info_out);
}

s32 PS4_SYSV_ABI sceVdecswFinalizeDecodeSequence(OrbisVdecswDecoder decoder_instance_in) {
    c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_INFO, "sceVdecswFinalizeDecodeSequence", "called");
    if (!decoder_instance_in) {
        c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_ERROR, "sceVdecswFinalizeDecodeSequence",
              "Invalid decoder instance");
        return ORBIS_VDECSW_ERROR_DECODER_INSTANCE;
    }
    return finalize_decode_sequence(decoder_instance_in);
}

s32 PS4_SYSV_ABI sceVdecswGetPictureInfo(const OrbisVdecswOutputInfo* output_info_in,
                                         void* p_1st_picture_info_out,
                                         void* p_2nd_picture_info_out) {
    const char* const func = "sceVdecswGetPictureInfo";
    VDECSW_TRACE_CALLED(func);
    if (!output_info_in) {
        c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_ERROR, func, "Invalid arguments");
        return ORBIS_VDECSW_ERROR_ARGUMENT_POINTER;
    }
    if ((output_info_in->this_size | 8) != sizeof(OrbisVdecswOutputInfo)) {
        c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_ERROR, func, "Invalid struct size");
        return ORBIS_VDECSW_ERROR_STRUCT_SIZE;
    }
    if (output_info_in->picture_count == 0) {
        c_log(C_LOG_LIB_VDECSW, C_LOG_LEVEL_ERROR, func, "No picture info available");
        return ORBIS_OK;
    }
    if (p_1st_picture_info_out) {
        u64 size;
        const u8* picture_info =
            (const u8*)output_info_in->frame_buffer + output_info_in->frame_buffer_size;
        memcpy(&size, p_1st_picture_info_out, sizeof(size));
        /* Copy the requested picture data to the output omitting picture size field */
        memcpy((u8*)p_1st_picture_info_out + sizeof(u64), picture_info + sizeof(u64),
               (size_t)(size - sizeof(u64)));
    }
    if (output_info_in->picture_count > 1) {
        c_unreachable(__FILE__, __LINE__, func, "");
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceVdecswGetAvcPictureInfo(const OrbisVdecswOutputInfo* output_info_in,
                                            OrbisVdecswAvcPictureInfo* p_1st_picture_info_out,
                                            OrbisVdecswAvcPictureInfo* p_2nd_picture_info_out) {
    VDECSW_TRACE_CALLED("sceVdecswGetAvcPictureInfo");
    return sceVdecswGetPictureInfo(output_info_in, p_1st_picture_info_out, p_2nd_picture_info_out);
}

s32 PS4_SYSV_ABI sceVdecswGetHevcPictureInfo(const OrbisVdecswOutputInfo* output_info_in,
                                             OrbisVdecswHevcPictureInfo* picture_info_out) {
    VDECSW_TRACE_CALLED("sceVdecswGetHevcPictureInfo");
    return sceVdecswGetPictureInfo(output_info_in, picture_info_out, NULL);
}
