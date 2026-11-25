/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_AVPLAYER_AVPLAYER_SOURCE_H
#define SHADPS4_AVPLAYER_AVPLAYER_SOURCE_H

/* The player's one source: an MP4 file, its demuxer and decoders on threads of their own, and
 * the queues between them and the game. Defined in avplayer_source.c. */

#include <stddef.h>

#include <boolean.h>
#include <retro_atomic.h>
#include <retro_common_api.h>
#include <retro_waitable_spsc.h>
#include <rthreads/retro_eventcount.h>

#include "common/types.h"
#include "core/libraries/avplayer/avplayer.h"
#include "core/libraries/avplayer/avplayer_clock.h"
#include "core/libraries/avplayer/avplayer_common.h"
#include "core/libraries/avplayer/avplayer_data_streamer.h"
#include "core/libraries/kernel/kernel_c.h"
#include "core/libraries/videodec/video_utils.h"

RETRO_BEGIN_DECLS

struct RFILE;
struct rmp4;
struct raac;
struct AvPlayerState;

#define AVPLAYER_MAX_VIDEO_BUFFERS 16
#define AVPLAYER_MAX_AUDIO_BUFFERS (AVPLAYER_MAX_VIDEO_BUFFERS * 2)

/* A frame buffer of the game's, from its allocator */
typedef struct avplayer_guest_buffer {
    u8* data;
    u64 size;
    bool is_texture;
} avplayer_guest_buffer_t;

/* A queue between two threads, on libretro-common's waitable SPSC ring: no lock, and a
 * waiting end sleeps on an eventcount. Closing it releases both ends for good; the records
 * still in it can be taken. */
typedef struct avplayer_spsc {
    retro_waitable_spsc_t queue;
    size_t capacity; /* bytes */
    size_t item;     /* a record's bytes */
    bool live;
} avplayer_spsc_t;

/* A thread of the player's: a service thread, asked to stop with a flag. It is done (as if
 * joined) once it ended of itself or a thread of the player's waited for its end; it is
 * collected by the one who stops it. */
typedef struct avplayer_thread {
    kernel_service_thread_t thread;
    void (*run)(void* arg);
    void* arg;
    retro_atomic_int_t stop;
    retro_atomic_int_t done;
    retro_atomic_int_t exited;
    /* Set before its start: a thread waiting for its end then waits */
    retro_atomic_int_t armed;
    retro_eventcount_t exited_ec;
} avplayer_thread_t;

/* An MP4 sample, read in place from the file's bytes. */
typedef struct avplayer_packet {
    const u8* data;
    size_t size;
    u64 timestamp; /* milliseconds */
} avplayer_packet_t;

/* A decoded frame in a guest buffer of the pool. */
typedef struct avplayer_frame {
    u32 buffer;
    AvPlayerFrameInfoEx info;
} avplayer_frame_t;

typedef struct avplayer_stream {
    int track; /* rmp4's track index */
    AvPlayerStreamInfo info;
} avplayer_stream_t;

typedef struct AvPlayerSource {
    struct AvPlayerState* state;

    /* The supported tracks, found once */
    avplayer_stream_t* streams;
    u32 num_streams;
    u64 duration;

    AvPlayerMemAllocator memory_replacement;
    u32 max_num_video_framebuffers;
    retro_atomic_int_t is_looping;
    retro_atomic_int_t is_paused;
    retro_atomic_int_t is_eof;
    avplayer_streamer_t streamer;
    bool has_streamer;

    /* The guest buffers, made at Start; the queues carry their indices. Every queue has one
     * thread at each end: buffers go from the game to a decoder, packets from the demuxer to a
     * decoder, frames from a decoder to the game. */
    avplayer_guest_buffer_t video_pool[AVPLAYER_MAX_VIDEO_BUFFERS];
    u32 video_pool_size;
    avplayer_guest_buffer_t audio_pool[AVPLAYER_MAX_AUDIO_BUFFERS];
    u32 audio_pool_size;
    avplayer_spsc_t audio_buffers;
    avplayer_spsc_t video_buffers;
    avplayer_spsc_t audio_packets;
    avplayer_spsc_t video_packets;
    avplayer_spsc_t audio_frames;
    avplayer_spsc_t video_frames;
    avplayer_frame_t current_video_frame;
    bool has_current_video_frame;
    avplayer_frame_t current_audio_frame;
    bool has_current_audio_frame;
    s32 video_stream_index;
    bool has_video_stream;
    s32 audio_stream_index;
    bool has_audio_stream;

    avplayer_thread_t demuxer_thread;
    avplayer_thread_t video_decoder_thread;
    avplayer_thread_t audio_decoder_thread;

    /* The file's bytes: mapped, or for a file read through callbacks, a buffer of its size
     * filled as the demuxer reaches each sample. */
    struct RFILE* file;
    const u8* data;
    u8* owned;
    size_t size;
    size_t loaded_hi; /* the file is read in order: what lies below is in place */
    bool fully_loaded;
    struct rmp4* demuxer;

    /* The video stream's decoder, freed with the source */
    video_sw_decoder_t video_decoder;
    bool has_video_decoder;
    struct raac* audio_decoder;
    u64 audio_skip; /* samples a channel the edit list trims, still to drop */
    u64 last_audio_ts;
    bool has_last_audio_ts;
    /* The player's clock is the game's time (nanoseconds), which the frontend's frames move:
     * the picture keeps step with the sound, which goes out by the same frames */
    avplayer_clock_t clock;
} AvPlayerSource;

/* NULL if it cannot be made */
AvPlayerSource* avplayer_source_new(struct AvPlayerState* state);
void avplayer_source_delete(AvPlayerSource* source);
/* @path: @length bytes */
bool avplayer_source_init(AvPlayerSource* source, const AvPlayerInitData* init_data,
                          const char* path, size_t length);
bool avplayer_source_find_streams(AvPlayerSource* source);
s32 avplayer_source_get_stream_count(AvPlayerSource* source);
bool avplayer_source_get_stream_info(AvPlayerSource* source, u32 stream_index,
                                     AvPlayerStreamInfo* info);
bool avplayer_source_enable_stream(AvPlayerSource* source, u32 stream_index);
void avplayer_source_set_looping(AvPlayerSource* source, bool is_looping);
bool avplayer_source_has_frames(AvPlayerSource* source, u32 num_frames);
bool avplayer_source_start(AvPlayerSource* source);
bool avplayer_source_stop(AvPlayerSource* source);
void avplayer_source_pause(AvPlayerSource* source);
void avplayer_source_resume(AvPlayerSource* source);
bool avplayer_source_get_audio_data(AvPlayerSource* source, AvPlayerFrameInfo* audio_info);
bool avplayer_source_get_video_data(AvPlayerSource* source, AvPlayerFrameInfo* video_info);
bool avplayer_source_get_video_data_ex(AvPlayerSource* source, AvPlayerFrameInfoEx* video_info);
u64 avplayer_source_current_time(AvPlayerSource* source);
bool avplayer_source_is_active(AvPlayerSource* source);

RETRO_END_DECLS

#endif
