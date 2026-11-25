/* SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdlib.h>
#include <string.h>

#include <formats/raac.h>
#include <formats/rmp4.h>
#include <retro_atomic.h>
#include <retro_miscellaneous.h>
#include <streams/file_stream.h>

#include "common/c_bridge.h"
#include "common/thread_name.h"
#include "core/file_sys/fs_c.h"
#include "core/libraries/avplayer/avplayer_error.h"
#include "core/libraries/avplayer/avplayer_source.h"
#include "core/libraries/avplayer/avplayer_state.h"
#include "core/libraries/kernel/kernel_c.h"

#define FIRST_READ ((size_t)64 * 1024)
#define READ_STEP ((size_t)1024 * 1024)
#define MAX_AUDIO_CHANNELS 8u
#define MAX_AUDIO_FRAME 2048u /* HE-AAC */
/* Samples a decoder may fall behind the demuxer; well past what it reads ahead. */
#define MAX_QUEUED_PACKETS ((size_t)4096)
/* A wake comes first; this only re-checks */
#define WAIT_BOUND_US ((int64_t)1000000)

static size_t min_size(size_t a, size_t b) {
    return a < b ? a : b;
}

static size_t max_size(size_t a, size_t b) {
    return a > b ? a : b;
}

static u32 align_up_u32(u32 value, u32 align) {
    return (value + align - 1) & ~(align - 1);
}

static u64 nanos_to_millis(s64 ns) {
    return ns > 0 ? (u64)(ns / 1000000) : 0;
}

static bool is_track_supported(const rmp4_track* track) {
    return track->codec == RMP4_CODEC_H264 || track->codec == RMP4_CODEC_H265 ||
           track->codec == RMP4_CODEC_AAC;
}

/*
 * The queues
 **/
/* Not while another thread uses the ring. */
static bool spsc_init(avplayer_spsc_t* r, size_t count, size_t item) {
    if (r->live) {
        retro_waitable_spsc_free(&r->queue);
        r->live = false;
    }
    r->item = item;
    r->live = retro_waitable_spsc_init(&r->queue, (count > 1 ? count : 1) * item);
    r->capacity = r->live ? retro_spsc_write_avail(&r->queue.queue) : 0;
    return r->live;
}

static void spsc_free(avplayer_spsc_t* r) {
    if (r->live) {
        retro_waitable_spsc_free(&r->queue);
        r->live = false;
    }
}

/* Exact on the consumer's side, a hint elsewhere. */
static size_t spsc_size(const avplayer_spsc_t* r) {
    if (!r->live) {
        return 0;
    }
    return retro_spsc_read_avail(&r->queue.queue) / r->item;
}

static bool spsc_cancelled(const avplayer_spsc_t* r) {
    return retro_waitable_spsc_cancelled(&r->queue);
}

/* The producer's: parks until at most @count records are left, the ring is closed or
 * @timeout_us has gone by; a record taken wakes it. */
static void spsc_wait_down_to(avplayer_spsc_t* r, size_t count, int64_t timeout_us) {
    size_t room;
    if (!r->live || spsc_size(r) <= count || spsc_cancelled(r)) {
        return;
    }
    room = r->capacity > count * r->item ? r->capacity - count * r->item : 0;
    retro_waitable_spsc_wait_writable(&r->queue, room, timeout_us);
}

/* The producer's: waits for room, false once closed. */
static bool spsc_push(avplayer_spsc_t* r, const void* value) {
    if (!r->live) {
        return false;
    }
    while (retro_spsc_write_avail(&r->queue.queue) < r->item) {
        if (spsc_cancelled(r)) {
            return false;
        }
        retro_waitable_spsc_wait_writable(&r->queue, r->item, WAIT_BOUND_US);
    }
    retro_waitable_spsc_write(&r->queue, value, r->item);
    return true;
}

/* The consumer's. */
static bool spsc_try_pop(avplayer_spsc_t* r, void* out) {
    if (spsc_size(r) == 0) {
        return false;
    }
    retro_waitable_spsc_read(&r->queue, out, r->item);
    return true;
}

/* The consumer's: waits for a record, false once closed and empty. */
static bool spsc_pop(avplayer_spsc_t* r, void* out) {
    for (;;) {
        if (spsc_try_pop(r, out)) {
            return true;
        }
        if (!r->live || spsc_cancelled(r)) {
            return false;
        }
        retro_waitable_spsc_wait_readable(&r->queue, r->item, WAIT_BOUND_US);
    }
}

static bool spsc_peek(const avplayer_spsc_t* r, void* out) {
    return spsc_size(r) != 0 && retro_spsc_peek(&r->queue.queue, out, r->item) == r->item;
}

static void spsc_close(avplayer_spsc_t* r) {
    if (r->live) {
        retro_waitable_spsc_cancel(&r->queue);
    }
}

/* Once both ends have stopped. */
static void spsc_drain(avplayer_spsc_t* r) {
    if (r->live) {
        retro_spsc_skip(&r->queue.queue, retro_spsc_read_avail(&r->queue.queue));
    }
}

/*
 * The threads
 **/
static void thread_main(void* arg) {
    avplayer_thread_t* t = (avplayer_thread_t*)arg;
    t->run(t->arg);
    retro_atomic_store_release_int(&t->exited, 1);
    retro_eventcount_notify(&t->exited_ec);
}

static void thread_init(avplayer_thread_t* t) {
    memset(&t->thread, 0, sizeof(t->thread));
    retro_atomic_store_relaxed_int(&t->stop, 0);
    retro_atomic_store_relaxed_int(&t->done, 0);
    retro_atomic_store_relaxed_int(&t->exited, 0);
    retro_atomic_store_relaxed_int(&t->armed, 0);
    retro_eventcount_init(&t->exited_ec);
}

/* The thread's end waited for, if one was started; it can then be started again */
static void thread_collect(avplayer_thread_t* t) {
    kernel_service_thread_join(&t->thread);
}

/* About to be started: from here a thread of the player's waiting for its end waits */
static void thread_arm(avplayer_thread_t* t) {
    thread_collect(t);
    retro_atomic_store_release_int(&t->stop, 0);
    retro_atomic_store_release_int(&t->done, 0);
    retro_atomic_store_release_int(&t->exited, 0);
    retro_atomic_store_release_int(&t->armed, 1);
}

static void thread_run(avplayer_thread_t* t, void (*run)(void* arg), void* arg) {
    t->run = run;
    t->arg = arg;
    if (!kernel_service_thread_start(&t->thread, thread_main, t)) {
        /* None to wait for */
        retro_atomic_store_release_int(&t->exited, 1);
        retro_eventcount_notify(&t->exited_ec);
    }
}

static bool thread_joinable(avplayer_thread_t* t) {
    return t->thread.thread != NULL && retro_atomic_load_acquire_int(&t->done) == 0;
}

/* On the thread, and anywhere: whether it has been asked to stop. */
static bool thread_stopping(avplayer_thread_t* t) {
    return retro_atomic_load_acquire_int(&t->stop) != 0;
}

/* Asks it to stop and waits for its end, if it has not ended of itself */
static void thread_stop(avplayer_thread_t* t) {
    if (thread_joinable(t)) {
        /* With a full fence, as an eventcount's notifier's state change */
        (void)retro_atomic_exchange_int(&t->stop, 1);
        thread_collect(t);
    }
    retro_atomic_store_release_int(&t->stop, 0);
}

/* On the thread: it ends of itself */
static void thread_done(avplayer_thread_t* t) {
    retro_atomic_store_release_int(&t->done, 1);
}

/* From another thread of the player's: until @t has ended, if it was armed; it is then done */
static void thread_wait_end(avplayer_thread_t* t) {
    if (retro_atomic_load_acquire_int(&t->armed) == 0) {
        return;
    }
    for (;;) {
        const int key = retro_eventcount_prepare_wait(&t->exited_ec);
        if (retro_atomic_load_acquire_int(&t->exited) != 0) {
            retro_eventcount_cancel_wait(&t->exited_ec);
            break;
        }
        retro_eventcount_commit_wait(&t->exited_ec, key);
    }
    retro_atomic_store_release_int(&t->done, 1);
}

static void thread_free(avplayer_thread_t* t) {
    thread_collect(t);
    retro_eventcount_free(&t->exited_ec);
}

/*
 * The guest buffers
 **/
static void guest_buffer_make(const AvPlayerMemAllocator* memory, avplayer_guest_buffer_t* buffer,
                              u32 align, u32 size, bool is_texture) {
    buffer->data = (u8*)(is_texture ? memory->allocate_texture(memory->object_ptr, align, size)
                                    : memory->allocate(memory->object_ptr, align, size));
    buffer->size = size;
    buffer->is_texture = is_texture;
    C_ASSERT_MSG(buffer->data, "GuestBuffer", "Could not allocate frame buffer.");
}

static void guest_pool_clear(const AvPlayerMemAllocator* memory, avplayer_guest_buffer_t* pool,
                             u32* count) {
    u32 i;
    for (i = 0; i < *count; i++) {
        if (pool[i].data != NULL) {
            if (pool[i].is_texture) {
                memory->deallocate_texture(memory->object_ptr, pool[i].data);
            } else {
                memory->deallocate(memory->object_ptr, pool[i].data);
            }
            pool[i].data = NULL;
        }
    }
    *count = 0;
}

/*
 * The source
 **/
AvPlayerSource* avplayer_source_new(struct AvPlayerState* state) {
    AvPlayerSource* source = (AvPlayerSource*)calloc(1, sizeof(*source));
    if (!source) {
        return NULL;
    }
    source->state = state;
    thread_init(&source->demuxer_thread);
    thread_init(&source->video_decoder_thread);
    thread_init(&source->audio_decoder_thread);
    avplayer_clock_reset(&source->clock);
    return source;
}

void avplayer_source_delete(AvPlayerSource* source) {
    if (!source) {
        return;
    }
    avplayer_source_stop(source);
    thread_free(&source->video_decoder_thread);
    thread_free(&source->audio_decoder_thread);
    thread_free(&source->demuxer_thread);
    if (source->audio_decoder) {
        raac_close((raac_t*)source->audio_decoder);
    }
    if (source->demuxer) {
        rmp4_close((rmp4_t*)source->demuxer);
    }
    free(source->owned);
    if (source->file) {
        filestream_close((RFILE*)source->file);
    }
    if (source->has_video_decoder) {
        video_sw_decoder_free(&source->video_decoder);
    }
    spsc_free(&source->video_frames);
    spsc_free(&source->audio_frames);
    spsc_free(&source->video_packets);
    spsc_free(&source->audio_packets);
    spsc_free(&source->video_buffers);
    spsc_free(&source->audio_buffers);
    guest_pool_clear(&source->memory_replacement, source->audio_pool, &source->audio_pool_size);
    guest_pool_clear(&source->memory_replacement, source->video_pool, &source->video_pool_size);
    if (source->has_streamer) {
        avplayer_streamer_close(&source->streamer);
    }
    free(source->streams);
    free(source);
}

/* Makes the bytes below @hi of a file that is not mapped readable. */
static bool source_load(AvPlayerSource* source, size_t hi) {
    size_t lo;
    /* The demuxer goes in file order: everything up to hi is read, from where the last read
     * stopped, so no byte is read twice */
    hi = min_size(hi, source->size);
    if (source->fully_loaded || hi <= source->loaded_hi) {
        return true;
    }
    lo = source->loaded_hi;
    if (avplayer_streamer_read_at(&source->streamer, lo, source->owned + lo, hi - lo) != hi - lo) {
        c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_ERROR, "Load",
              "Could not read bytes %zu..%zu of the source", lo, hi);
        return false;
    }
    source->loaded_hi = hi;
    return true;
}

static bool source_open_demuxer(AvPlayerSource* source) {
    if (source->fully_loaded) {
        source->demuxer = rmp4_open_memory(source->data, source->size);
    } else {
        /* The header first: the moov is read where it lies, the media only as it is reached */
        size_t avail = min_size(source->size, FIRST_READ);
        if (!source_load(source, avail)) {
            return false;
        }
        for (;;) {
            int need_more = 0;
            size_t lo = 0;
            size_t hi = 0;
            source->demuxer =
                rmp4_open_memory_avail(source->data, source->size, avail, &need_more, &lo, &hi);
            if (source->demuxer || !need_more || avail >= source->size) {
                break;
            }
            if (hi > lo && hi <= source->size) {
                const size_t keep = source->loaded_hi;
                if (avplayer_streamer_read_at(&source->streamer, lo, source->owned + lo, hi - lo) !=
                    hi - lo) {
                    return false;
                }
                source->loaded_hi = lo <= keep ? max_size(keep, hi) : keep;
                avail = max_size(avail, hi);
            } else {
                const size_t next = min_size(source->size, avail + READ_STEP);
                if (!source_load(source, next)) {
                    return false;
                }
                avail = next;
            }
        }
        if (source->demuxer) {
            /* Every sample is loaded before it is read: see the demuxer thread */
            rmp4_set_avail((rmp4_t*)source->demuxer, source->size);
        }
    }
    if (!source->demuxer) {
        c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_ERROR, "OpenDemuxer",
              "Not an MP4 file the demuxer can read");
        return false;
    }
    return true;
}

bool avplayer_source_init(AvPlayerSource* source, const AvPlayerInitData* init_data,
                          const char* path, size_t length) {
    const int framebuffers = init_data->num_output_video_framebuffers;
    source->memory_replacement = init_data->memory_replacement;
    source->max_num_video_framebuffers =
        (u32)(framebuffers < 2 ? 2 : (framebuffers > 16 ? 16 : framebuffers));

    if (init_data->file_replacement.open != NULL) {
        c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_INFO, "Init",
              "Opening %.*s through the game-provided file replacement", (int)length, path);
        avplayer_streamer_init_file_replacement(&source->streamer, &init_data->file_replacement);
        source->has_streamer = true;
    } else {
        char host_path[PATH_MAX_LENGTH];
        void* handle = NULL;
        host_path[0] = '\0';
        if (fs_open_guest_read(path, length, host_path, sizeof(host_path), &handle) ==
            FS_OPEN_GUEST_HANDLE) {
            c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_INFO, "Init",
                  "Opening %.*s through a non-host backend handle", (int)length, path);
            avplayer_streamer_init_handle(&source->streamer, handle);
            source->has_streamer = true;
        }
        if (!source->has_streamer) {
            s64 mapped = 0;
            c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_INFO, "Init", "Opening %.*s from host path %s",
                  (int)length, path, host_path);
            /* Mapped, the samples are read in place */
            source->file = filestream_open(host_path, RETRO_VFS_FILE_ACCESS_READ,
                                           RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS);
            if (!source->file) {
                c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_ERROR, "Init", "Could not open %s",
                      host_path);
                return false;
            }
            source->data = filestream_get_mapped_ptr((RFILE*)source->file, &mapped);
            if (source->data && mapped > 0) {
                source->size = (size_t)mapped;
                source->loaded_hi = source->size;
                source->fully_loaded = true;
            } else {
                source->data = NULL;
                avplayer_streamer_init_vfs(&source->streamer, source->file);
                source->has_streamer = true;
            }
        }
    }

    if (source->has_streamer) {
        if (!avplayer_streamer_open(&source->streamer, path)) {
            return false;
        }
        source->size = (size_t)avplayer_streamer_size(&source->streamer);
        /* One buffer of the file's size, filled as the demuxer reaches each part: samples are
         * read into place and handed to the decoders from there */
        source->owned = (u8*)malloc(max_size(source->size, 1));
        if (!source->owned) {
            c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_ERROR, "Init",
                  "Could not allocate %zu bytes for %.*s", source->size, (int)length, path);
            return false;
        }
        source->data = source->owned;
    }
    return source_open_demuxer(source);
}

static u64 track_duration_millis(const rmp4_t* demuxer, const rmp4_track* track) {
    if (track->presentation_duration_ns > 0) {
        return nanos_to_millis(track->presentation_duration_ns);
    }
    return nanos_to_millis(rmp4_duration_ns(demuxer));
}

static AvPlayerStreamInfo source_create_stream_info(AvPlayerSource* source, u32 stream_index) {
    AvPlayerStreamInfo info;
    const int track_index = source->streams[stream_index].track;
    const rmp4_track* track = rmp4_get_track((rmp4_t*)source->demuxer, track_index);
    size_t lang_len = 0;
    enum rmp4_language_kind kind = RMP4_LANGUAGE_NONE;
    const u8* lang;
    size_t lang_copy;

    memset(&info, 0, sizeof(info));
    info.type =
        track->type == RMP4_TRACK_VIDEO ? AV_PLAYER_STREAM_TYPE_VIDEO : AV_PLAYER_STREAM_TYPE_AUDIO;
    info.start_time = 0; /* start_time is currently unused and defaults to 0. */
    info.duration = track_duration_millis((rmp4_t*)source->demuxer, track);

    lang = rmp4_get_track_language((rmp4_t*)source->demuxer, track_index, &lang_len, &kind);
    lang_copy = lang ? min_size(lang_len, 3) : 0;
    if (lang_copy) {
        c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_INFO, "CreateStreamInfo",
              "Stream %u language = %.*s", stream_index, (int)lang_len, (const char*)lang);
    } else {
        c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_WARNING, "CreateStreamInfo",
              "Stream %u language is unknown", stream_index);
    }

    if (info.type == AV_PLAYER_STREAM_TYPE_VIDEO) {
        c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_INFO, "CreateStreamInfo",
              "Stream %u is a video stream.", stream_index);
        info.details.video.aspect_ratio =
            track->height ? (f32)track->width / (f32)track->height : 0.0f;
        info.details.video.width = align_up_u32(track->width, 16);
        info.details.video.height = align_up_u32(track->height, 16);
        if (lang_copy) {
            memcpy(info.details.video.language_code, lang, lang_copy);
        }
    } else {
        c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_INFO, "CreateStreamInfo",
              "Stream %u is an audio stream.", stream_index);
        info.details.audio.channel_count = (u16)track->channels;
        info.details.audio.sample_rate = track->sample_rate;
        info.details.audio.size = 0; /* sceAvPlayerGetStreamInfo() is expected to set this to 0 */
        if (lang_copy) {
            memcpy(info.details.audio.language_code, lang, lang_copy);
        }
    }
    return info;
}

static u64 source_duration_millis(const AvPlayerSource* source) {
    u64 duration = 0;
    u32 i;
    if (source->demuxer == NULL) {
        return 0;
    }
    for (i = 0; i < source->num_streams; i++) {
        if (source->streams[i].info.duration > duration) {
            duration = source->streams[i].info.duration;
        }
    }
    return duration ? duration : nanos_to_millis(rmp4_duration_ns((rmp4_t*)source->demuxer));
}

bool avplayer_source_find_streams(AvPlayerSource* source) {
    if (source->demuxer) {
        const int count = rmp4_num_tracks((rmp4_t*)source->demuxer);
        int track;
        free(source->streams);
        source->streams = NULL;
        source->num_streams = 0;
        if (count > 0) {
            source->streams = (avplayer_stream_t*)calloc((size_t)count, sizeof(avplayer_stream_t));
        }
        for (track = 0; source->streams && track < count; ++track) {
            const rmp4_track* t = rmp4_get_track((rmp4_t*)source->demuxer, track);
            if (t && is_track_supported(t)) {
                const u32 index = source->num_streams++;
                source->streams[index].track = track;
                source->streams[index].info = source_create_stream_info(source, index);
            }
        }
    }
    source->duration = source_duration_millis(source);
    return source->num_streams > 0;
}

s32 avplayer_source_get_stream_count(AvPlayerSource* source) {
    c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_INFO, "GetStreamCount", "Stream Count: %zu",
          (size_t)source->num_streams);
    return (s32)source->num_streams;
}

bool avplayer_source_get_stream_info(AvPlayerSource* source, u32 stream_index,
                                     AvPlayerStreamInfo* info) {
    if (stream_index >= source->num_streams) {
        c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_ERROR, "GetStreamInfo",
              "Could not get stream %u info.", stream_index);
        return false;
    }
    *info = source->streams[stream_index].info;
    return true;
}

bool avplayer_source_enable_stream(AvPlayerSource* source, u32 stream_index) {
    if (source->demuxer == NULL || stream_index >= source->num_streams) {
        return false;
    }
    if (source->streams[stream_index].info.type == AV_PLAYER_STREAM_TYPE_VIDEO) {
        source->video_stream_index = (s32)stream_index;
        source->has_video_stream = true;
        c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_INFO, "EnableStream", "Video stream %u enabled",
              stream_index);
    } else {
        source->audio_stream_index = (s32)stream_index;
        source->has_audio_stream = true;
        c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_INFO, "EnableStream", "Audio stream %u enabled",
              stream_index);
    }
    return true;
}

void avplayer_source_set_looping(AvPlayerSource* source, bool is_looping) {
    (void)retro_atomic_exchange_int(&source->is_looping, is_looping ? 1 : 0);
}

bool avplayer_source_has_frames(AvPlayerSource* source, u32 num_frames) {
    return spsc_size(&source->video_packets) > num_frames ||
           (retro_atomic_load_seq_cst_int(&source->is_eof) != 0);
}

static void demuxer_thread(void* arg);
static void video_decoder_thread(void* arg);
static void audio_decoder_thread(void* arg);

bool avplayer_source_start(AvPlayerSource* source) {
    u32 index;
    if (!source->has_video_stream && !source->has_audio_stream) {
        c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_ERROR, "Start",
              "Could not start playback. No streams.");
        return false;
    }
    rmp4_rewind((rmp4_t*)source->demuxer);
    if (source->has_video_stream) {
        const rmp4_track* track = rmp4_get_track((rmp4_t*)source->demuxer,
                                                 source->streams[source->video_stream_index].track);
        u32 pitch;
        u32 height;
        u32 size;
        if (source->has_video_decoder) {
            video_sw_decoder_free(&source->video_decoder);
        }
        memset(&source->video_decoder, 0, sizeof(source->video_decoder));
        source->has_video_decoder = true;
        if (!video_sw_decoder_init(&source->video_decoder, track->codec == RMP4_CODEC_H265) ||
            !video_sw_decoder_set_extradata(&source->video_decoder, track->codec_private,
                                            track->codec_private_size)) {
            c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_ERROR, "Start",
                  "Could not open the decoder for video stream %d.", source->video_stream_index);
            return false;
        }
        pitch = align_up_u32(track->width, 64);
        height = align_up_u32(track->height, 16);
        size = (pitch * height * 3) / 2;
        guest_pool_clear(&source->memory_replacement, source->video_pool, &source->video_pool_size);
        for (index = 0; index < source->max_num_video_framebuffers; ++index) {
            guest_buffer_make(&source->memory_replacement, &source->video_pool[index], 0x100, size,
                              true);
            source->video_pool_size = index + 1;
        }
    }
    if (source->has_audio_stream) {
        const rmp4_track* track = rmp4_get_track((rmp4_t*)source->demuxer,
                                                 source->streams[source->audio_stream_index].track);
        /* A frame of HE-AAC at the most channels, as 16-bit samples */
        const u32 size = MAX_AUDIO_CHANNELS * (u32)sizeof(s16) * MAX_AUDIO_FRAME;
        if (source->audio_decoder) {
            raac_close((raac_t*)source->audio_decoder);
        }
        source->audio_decoder = raac_open(track->codec_private, track->codec_private_size);
        if (!source->audio_decoder) {
            c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_ERROR, "Start",
                  "Could not open the decoder for audio stream %d.", source->audio_stream_index);
            return false;
        }
        raac_set_channel_order((raac_t*)source->audio_decoder, RAAC_ORDER_WAVE);
        source->audio_skip = track->media_skip;
        guest_pool_clear(&source->memory_replacement, source->audio_pool, &source->audio_pool_size);
        for (index = 0; index < source->max_num_video_framebuffers * 2; ++index) {
            guest_buffer_make(&source->memory_replacement, &source->audio_pool[index], 0x10, size,
                              false);
            source->audio_pool_size = index + 1;
        }
    }
    /* Fresh queues: the last playback's were closed. A frame queue holds every buffer, so a
     * decoder never waits on it. */
    if (!spsc_init(&source->video_buffers, source->video_pool_size, sizeof(u32)) ||
        !spsc_init(&source->video_frames, source->video_pool_size, sizeof(avplayer_frame_t)) ||
        !spsc_init(&source->audio_buffers, source->audio_pool_size, sizeof(u32)) ||
        !spsc_init(&source->audio_frames, source->audio_pool_size, sizeof(avplayer_frame_t)) ||
        !spsc_init(&source->video_packets, MAX_QUEUED_PACKETS, sizeof(avplayer_packet_t)) ||
        !spsc_init(&source->audio_packets, MAX_QUEUED_PACKETS, sizeof(avplayer_packet_t))) {
        c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_ERROR, "Start",
              "Could not allocate the playback queues.");
        return false;
    }
    for (index = 0; index < source->video_pool_size; ++index) {
        spsc_push(&source->video_buffers, &index);
    }
    for (index = 0; index < source->audio_pool_size; ++index) {
        spsc_push(&source->audio_buffers, &index);
    }
    thread_arm(&source->demuxer_thread);
    thread_arm(&source->video_decoder_thread);
    thread_arm(&source->audio_decoder_thread);
    thread_run(&source->demuxer_thread, demuxer_thread, source);
    thread_run(&source->video_decoder_thread, video_decoder_thread, source);
    thread_run(&source->audio_decoder_thread, audio_decoder_thread, source);
    avplayer_clock_start(&source->clock, kernel_game_now());
    return true;
}

static void source_close_queues(AvPlayerSource* source) {
    spsc_close(&source->video_buffers);
    spsc_close(&source->audio_buffers);
    spsc_close(&source->audio_packets);
    spsc_close(&source->video_packets);
    spsc_close(&source->audio_frames);
    spsc_close(&source->video_frames);
}

static bool source_has_running_threads(AvPlayerSource* source) {
    return thread_joinable(&source->demuxer_thread) ||
           thread_joinable(&source->video_decoder_thread) ||
           thread_joinable(&source->audio_decoder_thread);
}

bool avplayer_source_stop(AvPlayerSource* source) {
    if (!source_has_running_threads(source)) {
        c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_WARNING, "Stop",
              "Could not stop playback: already stopped.");
        return false;
    }
    /* Closed queues wake every thread waiting on one */
    source_close_queues(source);
    thread_stop(&source->video_decoder_thread);
    thread_stop(&source->audio_decoder_thread);
    thread_stop(&source->demuxer_thread);
    source->has_current_audio_frame = false;
    source->has_current_video_frame = false;
    /* The memory stays until the next Start, as other threads may still ask for a size */
    spsc_drain(&source->video_buffers);
    spsc_drain(&source->audio_buffers);
    spsc_drain(&source->audio_packets);
    spsc_drain(&source->video_packets);
    spsc_drain(&source->audio_frames);
    spsc_drain(&source->video_frames);
    guest_pool_clear(&source->memory_replacement, source->video_pool, &source->video_pool_size);
    guest_pool_clear(&source->memory_replacement, source->audio_pool, &source->audio_pool_size);
    source->has_last_audio_ts = false;
    avplayer_clock_reset(&source->clock);
    (void)retro_atomic_exchange_int(&source->is_paused, 0);
    (void)retro_atomic_exchange_int(&source->is_eof, 0);
    return true;
}

void avplayer_source_pause(AvPlayerSource* source) {
    avplayer_clock_pause(&source->clock, kernel_game_now());
    (void)retro_atomic_exchange_int(&source->is_paused, 1);
}

void avplayer_source_resume(AvPlayerSource* source) {
    avplayer_clock_resume(&source->clock, kernel_game_now());
    (void)retro_atomic_exchange_int(&source->is_paused, 0);
}

bool avplayer_source_get_video_data(AvPlayerSource* source, AvPlayerFrameInfo* video_info) {
    AvPlayerFrameInfoEx info;
    memset(&info, 0, sizeof(info));
    if (!avplayer_source_get_video_data_ex(source, &info)) {
        return false;
    }
    memset(video_info, 0, sizeof(*video_info));
    video_info->timestamp = info.timestamp;
    video_info->p_data = (u8*)info.p_data;
    video_info->details.video.aspect_ratio = info.details.video.aspect_ratio;
    video_info->details.video.width = info.details.video.width;
    video_info->details.video.height = info.details.video.height;
    return true;
}

bool avplayer_source_get_video_data_ex(AvPlayerSource* source, AvPlayerFrameInfoEx* video_info) {
    avplayer_frame_t frame;
    if (!avplayer_source_is_active(source) ||
        (retro_atomic_load_seq_cst_int(&source->is_paused) != 0)) {
        return false;
    }
    if (!spsc_peek(&source->video_frames, &frame)) {
        return false;
    }
    if (avplayer_state_get_sync_mode(source->state) == AV_PLAYER_AV_SYNC_MODE_DEFAULT &&
        frame.info.timestamp > avplayer_source_current_time(source)) {
        return false;
    }
    /* The frame handed out last time is done with */
    if (source->has_current_video_frame) {
        spsc_push(&source->video_buffers, &source->current_video_frame.buffer);
        source->has_current_video_frame = false;
    }
    spsc_try_pop(&source->video_frames, &frame);
    *video_info = frame.info;
    source->current_video_frame = frame;
    source->has_current_video_frame = true;
    return true;
}

bool avplayer_source_get_audio_data(AvPlayerSource* source, AvPlayerFrameInfo* audio_info) {
    avplayer_frame_t frame;
    if (!avplayer_source_is_active(source) ||
        (retro_atomic_load_seq_cst_int(&source->is_paused) != 0)) {
        return false;
    }
    if (!spsc_try_pop(&source->audio_frames, &frame)) {
        return false;
    }
    if (source->has_current_audio_frame) {
        spsc_push(&source->audio_buffers, &source->current_audio_frame.buffer);
        source->has_current_audio_frame = false;
    }
    source->last_audio_ts = frame.info.timestamp;
    source->has_last_audio_ts = true;
    avplayer_clock_data(&source->clock, kernel_game_now());
    memset(audio_info, 0, sizeof(*audio_info));
    audio_info->timestamp = frame.info.timestamp;
    audio_info->p_data = (u8*)frame.info.p_data;
    audio_info->details.audio.sample_rate = frame.info.details.audio.sample_rate;
    audio_info->details.audio.size = frame.info.details.audio.size;
    audio_info->details.audio.channel_count = frame.info.details.audio.channel_count;
    source->current_audio_frame = frame;
    source->has_current_audio_frame = true;
    return true;
}

u64 avplayer_source_current_time(AvPlayerSource* source) {
    u64 now;
    int paused;
    s64 elapsed;
    if (!source->clock.started) {
        return 0;
    }
    if ((retro_atomic_load_seq_cst_int(&source->is_eof) != 0) &&
        !avplayer_source_is_active(source)) {
        return source->duration;
    }
    if (!avplayer_source_is_active(source)) {
        return 0;
    }
    /* Milliseconds of the game's time, less the time spent paused (avplayer_clock.c) */
    now = kernel_game_now();
    paused = retro_atomic_load_seq_cst_int(&source->is_paused) != 0;
    if (source->has_audio_stream) {
        return (source->has_last_audio_ts ? source->last_audio_ts : 0) +
               avplayer_clock_since_data_ms(&source->clock, now, paused);
    }
    elapsed = avplayer_clock_since_start_ms(&source->clock, now, paused);
    if (elapsed <= 0) {
        return 0;
    }
    return source->duration != 0 && (u64)elapsed > source->duration ? source->duration
                                                                    : (u64)elapsed;
}

bool avplayer_source_is_active(AvPlayerSource* source) {
    return retro_atomic_load_seq_cst_int(&source->is_eof) == 0 ||
           spsc_size(&source->audio_packets) != 0 || spsc_size(&source->video_packets) != 0 ||
           spsc_size(&source->video_frames) != 0 || spsc_size(&source->audio_frames) != 0;
}

static void demuxer_thread(void* arg) {
    AvPlayerSource* source = (AvPlayerSource*)arg;
    int video_track;
    int audio_track;
    thread_name_set("shadPS4:AvDemuxer");
    if (!source->has_audio_stream && !source->has_video_stream) {
        c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_WARNING, "DemuxerThread",
              "Could not start DEMUXER thread. No streams enabled.");
        return;
    }
    c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_INFO, "DemuxerThread", "Demuxer Thread started");
    video_track = source->has_video_stream ? source->streams[source->video_stream_index].track : -1;
    audio_track = source->has_audio_stream ? source->streams[source->audio_stream_index].track : -1;

    while (!thread_stopping(&source->demuxer_thread)) {
        size_t lo = 0;
        size_t hi = 0;
        rmp4_packet packet;
        avplayer_packet_t out;
        int res;
        if (spsc_size(&source->video_packets) > 30 &&
            (!source->has_audio_stream || spsc_size(&source->audio_packets) > 8)) {
            /* The video decoder taking a packet wakes it; the audio side is looked at again
             * within 5 ms, as it was */
            spsc_wait_down_to(&source->video_packets, 30, 5000);
            continue;
        }
        /* The next sample's bytes are put in place first, for a file that is not mapped */
        rmp4_read_span((rmp4_t*)source->demuxer, &lo, &hi);
        if (hi > lo && !source_load(source, hi)) {
            avplayer_state_on_error(source->state);
            return;
        }
        memset(&packet, 0, sizeof(packet));
        res = rmp4_read_packet((rmp4_t*)source->demuxer, &packet);
        if (res == 0) {
            if (retro_atomic_load_seq_cst_int(&source->is_looping) != 0) {
                c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_INFO, "DemuxerThread",
                      "EOF reached in demuxer. Looping the source...");
                avplayer_state_on_warning(source->state, ORBIS_AVPLAYER_ERROR_WAR_LOOPING_BACK);
                /* Every sample has been read once: the bytes stay in place */
                source->fully_loaded = true;
                rmp4_rewind((rmp4_t*)source->demuxer);
                continue;
            }
            c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_INFO, "DemuxerThread",
                  "EOF reached in demuxer. Exiting.");
            break;
        }
        if (res < 0 || res == RMP4_READ_AGAIN) {
            c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_ERROR, "DemuxerThread",
                  "Could not read a sample: error = %d", res);
            avplayer_state_on_error(source->state);
            return;
        }
        out.data = packet.data;
        out.size = packet.size;
        out.timestamp = nanos_to_millis(packet.timestamp);
        /* A full queue waits for its decoder; a closed one means the player stopped */
        if (packet.track == video_track) {
            if (!spsc_push(&source->video_packets, &out)) {
                break;
            }
        } else if (packet.track == audio_track) {
            if (!spsc_push(&source->audio_packets, &out)) {
                break;
            }
        }
    }
    (void)retro_atomic_exchange_int(&source->is_eof, 1);
    /* The decoders take what is left, then find the queues closed */
    spsc_close(&source->video_packets);
    spsc_close(&source->audio_packets);
    thread_wait_end(&source->video_decoder_thread);
    thread_wait_end(&source->audio_decoder_thread);
    avplayer_state_on_eof(source->state);
    c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_INFO, "DemuxerThread", "Demuxer Thread exited normally");
    thread_done(&source->demuxer_thread);
}

/* Writes the ready picture straight into a guest buffer: false once the player stopped */
static bool video_decoder_output(AvPlayerSource* source, u32* spare, bool* has_spare) {
    video_sw_decoder_t* const decoder = &source->video_decoder;
    const avplayer_guest_buffer_t* buffer;
    u8* p_buffer;
    u32 width;
    u32 pitch;
    u32 height;
    avplayer_frame_t frame;
    if (!*has_spare) {
        u32 index = 0;
        if (!spsc_pop(&source->video_buffers, &index)) {
            return false; /* the player stopped */
        }
        *spare = index;
        *has_spare = true;
    }
    buffer = &source->video_pool[*spare];
    p_buffer = buffer->data;
    if (!video_sw_decoder_write_nv12(decoder, p_buffer, buffer->size)) {
        c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_ERROR, "VideoDecoderThread",
              "The %ux%u picture does not fit the frame buffer", decoder->width, decoder->height);
        return true; /* the buffer is kept for the next picture */
    }
    width = align_up_u32(decoder->width, 16);
    pitch = video_sw_decoder_pitch(decoder);
    height = video_sw_decoder_aligned_height(decoder);
    kernel_invalidate_memory((uintptr_t)p_buffer, video_sw_decoder_nv12_size(decoder));
    memset(&frame, 0, sizeof(frame));
    frame.buffer = *spare;
    frame.info.p_data = p_buffer;
    frame.info.timestamp = decoder->meta.pts;
    frame.info.details.video.width = width;
    frame.info.details.video.height = height;
    frame.info.details.video.aspect_ratio = 1.0f; /* square samples */
    frame.info.details.video.crop_left_offset = 0;
    frame.info.details.video.crop_right_offset = pitch - decoder->width;
    frame.info.details.video.crop_top_offset = 0;
    frame.info.details.video.crop_bottom_offset = height - decoder->height;
    frame.info.details.video.pitch = pitch;
    frame.info.details.video.luma_bit_depth = 8;
    frame.info.details.video.chroma_bit_depth = 8;
    *has_spare = false;
    return spsc_push(&source->video_frames, &frame);
}

static void video_decoder_thread(void* arg) {
    AvPlayerSource* source = (AvPlayerSource*)arg;
    video_sw_decoder_t* decoder;
    u32 spare = 0; /* a buffer taken and not filled */
    bool has_spare = false;
    avplayer_packet_t packet;
    thread_name_set("shadPS4:AvVideoDecoder");
    if (!source->has_video_decoder) {
        return; /* no video stream enabled */
    }
    c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_INFO, "VideoDecoderThread",
          "Video Decoder Thread started");
    decoder = &source->video_decoder;
    while (!thread_stopping(&source->video_decoder_thread) &&
           spsc_pop(&source->video_packets, &packet)) {
        video_picture_meta_t meta;
        int res;
        memset(&meta, 0, sizeof(meta));
        meta.pts = packet.timestamp;
        res = video_sw_decoder_decode(decoder, packet.data, packet.size, &meta);
        if (res < 0) {
            c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_ERROR, "VideoDecoderThread",
                  "Could not decode a video sample. Error = %d", res);
            avplayer_state_on_error(source->state);
            return;
        }
        if (res && !video_decoder_output(source, &spare, &has_spare)) {
            break;
        }
    }
    /* The pictures still held for display order */
    while (!thread_stopping(&source->video_decoder_thread) && video_sw_decoder_drain(decoder)) {
        if (!video_decoder_output(source, &spare, &has_spare)) {
            break;
        }
    }
    c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_INFO, "VideoDecoderThread",
          "Video Decoder Thread exited normally");
    thread_done(&source->video_decoder_thread);
}

static void audio_decoder_thread(void* arg) {
    AvPlayerSource* source = (AvPlayerSource*)arg;
    u32 channels;
    u32 sample_rate;
    avplayer_packet_t packet;
    u32 spare = 0; /* a buffer taken and not filled */
    bool has_spare = false;
    thread_name_set("shadPS4:AvAudioDecoder");
    if (!source->audio_decoder || !source->has_audio_stream) {
        return; /* no audio stream enabled */
    }
    c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_INFO, "AudioDecoderThread",
          "Audio Decoder Thread started");
    channels = raac_channels((raac_t*)source->audio_decoder);
    sample_rate = raac_sample_rate((raac_t*)source->audio_decoder);
    if (channels == 0 || channels > MAX_AUDIO_CHANNELS ||
        raac_frame_len((raac_t*)source->audio_decoder) > MAX_AUDIO_FRAME) {
        c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_ERROR, "AudioDecoderThread",
              "Unsupported audio: %u channels, %u samples a frame", channels,
              raac_frame_len((raac_t*)source->audio_decoder));
        avplayer_state_on_error(source->state);
        return;
    }
    while (!thread_stopping(&source->audio_decoder_thread) &&
           spsc_pop(&source->audio_packets, &packet)) {
        s16* pcm;
        int samples;
        u32 skip;
        u32 count;
        avplayer_frame_t frame;
        if (!has_spare) {
            u32 index = 0;
            if (!spsc_pop(&source->audio_buffers, &index)) {
                break; /* the player stopped */
            }
            spare = index;
            has_spare = true;
        }
        /* Decoded straight into the guest buffer */
        pcm = (s16*)source->audio_pool[spare].data;
        samples = raac_decode_s16((raac_t*)source->audio_decoder, packet.data, packet.size, pcm);
        if (samples < 0) {
            c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_ERROR, "AudioDecoderThread",
                  "Could not decode an audio sample. Error = %d", samples);
            avplayer_state_on_error(source->state);
            return;
        }
        /* The encoder delay the edit list trims comes off the start */
        skip = (u32)(source->audio_skip < (u64)samples ? source->audio_skip : (u64)samples);
        source->audio_skip -= skip;
        count = (u32)samples - skip;
        if (count == 0) {
            continue; /* the buffer is kept for the next frame */
        }
        memset(&frame, 0, sizeof(frame));
        frame.buffer = spare;
        frame.info.p_data = (u8*)(pcm + skip * channels);
        frame.info.timestamp = packet.timestamp;
        frame.info.details.audio.channel_count = (u16)channels;
        frame.info.details.audio.sample_rate = sample_rate;
        frame.info.details.audio.size = count * channels * (u32)sizeof(s16);
        has_spare = false;
        if (!spsc_push(&source->audio_frames, &frame)) {
            break;
        }
    }
    c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_INFO, "AudioDecoderThread",
          "Audio Decoder Thread exited normally");
    thread_done(&source->audio_decoder_thread);
}
