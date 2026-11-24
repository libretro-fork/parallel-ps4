/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <compat/strl.h>
#include <file/file_path.h>
#include <retro_atomic.h>
#include <rthreads/retro_eventcount.h>

#include "common/audio_stream.h"
#include "common/c_bridge.h"
#include "common/combine.h"
#include "common/mpsc_ring.h"
#include "common/stop_thread.h"
#include "core/emulator_settings.h"
#include "core/file_sys/fs_c.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/kernel/kernel_c.h"
#include "core/libraries/music/music_player_service.h"
#include "libretro/audio_mix.h"

/* Track list ids the game may use, and the most entries one may have */
#define MUSIC_TRACKLISTS 100
#define MUSIC_MAX_TRACK_INDEX 3999
#define MUSIC_MAX_USB_DEVICES 8
/* The longest track URI the playback thread works on, and the paths made from it */
#define MUSIC_PATH_SIZE 4096
/* The track decoded a block at a time, in frames of up to 2 channels */
#define MUSIC_WINDOW_FRAMES 8192
#define MUSIC_CHUNK_FRAMES 1024

/* A track list: the URIs the game gave it, each a string of its own. Made when the game sets
 * it, never while a track plays. */
typedef struct music_list {
    char** uris;
    u32 count;
    u32 cap;
    bool exists;
} music_list_t;

typedef struct music_usb_device {
    char serial[256];
    u32 device_id;
} music_usb_device_t;

typedef struct music_event {
    u32 event_id;
    u32 track_index;
} music_event_t;

/* Runs the control calls (play, stop, seek, next, previous) one at a time */
static combiner_t g_control;
/* Runs the calls on the lists and devices one at a time */
static combiner_t g_data;
static bool g_inited;

/* The list played (the game's list 0 as it last set it or ended a transaction), the game's
 * lists by id, and its USB devices: g_data's */
static music_list_t g_tracks;
static music_list_t g_lists[MUSIC_TRACKLISTS];
static music_usb_device_t g_usb[MUSIC_MAX_USB_DEVICES];
static unsigned g_usb_count;

static retro_atomic_int_t g_play_state; /* 0 stopped, 1 playing, 2 paused */
static retro_atomic_int_t g_current_track_idx;
static retro_atomic_64_t g_current_time_ms;
static retro_atomic_64_t g_total_duration_ms;
static retro_atomic_int_t g_volume;
static retro_atomic_int_t g_repeat_mode;
static retro_atomic_int_t g_shuffle_mode;
static retro_atomic_int_t g_stop_requested;
static retro_atomic_int_t g_pause_requested;
static retro_atomic_64_t g_seek_offset_ms;
static retro_atomic_int_t g_in_transaction;
static retro_atomic_64_t g_tracklist_version;
/* What a paused playback thread parks on: notified by unpause, stop and seek */
static retro_eventcount_t g_control_ec;

static stop_thread_t g_worker_thread = STOP_THREAD_INIT;
/* What the thread starts with, read by it as it starts */
static u32 g_worker_track;
static u64 g_worker_start_ms;

/* Taken by the game's call for its events; nothing puts one yet */
static mpsc_ring_t g_events;
static retro_eventcount_t g_events_ec;

/* The playback thread's own: the track's URI and the paths made from it, and the window it is
 * decoded into. Made with the player, so playing allocates nothing. */
static char g_uri[MUSIC_PATH_SIZE];
static char g_guest[MUSIC_PATH_SIZE];
static char g_resolved[MUSIC_PATH_SIZE];
static char g_host[MUSIC_PATH_SIZE];
static float g_window[MUSIC_WINDOW_FRAMES * 2];

static s32 atomic_load_s32(retro_atomic_int_t* a) {
    return (s32)retro_atomic_load_seq_cst_int(a);
}

static char* music_strdup(const char* s) {
    const size_t len = strlen(s);
    char* copy = (char*)malloc(len + 1);
    if (copy) {
        memcpy(copy, s, len + 1);
    }
    return copy;
}

static void list_clear(music_list_t* list) {
    u32 i;
    for (i = 0; i < list->count; i++) {
        free(list->uris[i]);
    }
    list->count = 0;
}

static void list_free(music_list_t* list) {
    list_clear(list);
    free(list->uris);
    list->uris = NULL;
    list->cap = 0;
    list->exists = false;
}

/* Room for one more: false if there is no memory */
static bool list_reserve(music_list_t* list, u32 want) {
    char** grown;
    u32 cap;
    if (want <= list->cap) {
        return true;
    }
    cap = list->cap ? list->cap : 8;
    while (cap < want) {
        cap *= 2;
    }
    grown = (char**)realloc(list->uris, cap * sizeof(*grown));
    if (!grown) {
        return false;
    }
    list->uris = grown;
    list->cap = cap;
    return true;
}

/* @uri at @index, or at the end if @index is past it */
static void list_insert(music_list_t* list, u32 index, const char* uri, const char* func) {
    char* copy;
    if (!list_reserve(list, list->count + 1) || !(copy = music_strdup(uri))) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, func,
              "no memory for a track entry: it is dropped");
        return;
    }
    if (index < list->count) {
        memmove(list->uris + index + 1, list->uris + index,
                (list->count - index) * sizeof(*list->uris));
        list->uris[index] = copy;
    } else {
        list->uris[list->count] = copy;
    }
    list->count++;
}

static void list_erase(music_list_t* list, u32 index) {
    free(list->uris[index]);
    memmove(list->uris + index, list->uris + index + 1,
            (list->count - index - 1) * sizeof(*list->uris));
    list->count--;
}

/* @dst made a copy of @src */
static void list_copy(music_list_t* dst, const music_list_t* src, const char* func) {
    u32 i;
    list_clear(dst);
    for (i = 0; i < src->count; i++) {
        list_insert(dst, dst->count, src->uris[i], func);
    }
}

/* ---- The lists and devices, run by g_data ---- */

typedef struct set_tracks_call {
    const u8* base;
    u32 count;
    size_t stride;
} set_tracks_call_t;

static void set_tracks_run(void* ctx) {
    const set_tracks_call_t* call = (const set_tracks_call_t*)ctx;
    u32 i;
    list_clear(&g_tracks);
    for (i = 0; i < call->count; i++) {
        list_insert(&g_tracks, g_tracks.count, (const char*)(call->base + i * call->stride),
                    "SetTrackList");
    }
    list_copy(&g_lists[0], &g_tracks, "SetTrackList");
    g_lists[0].exists = true;
    retro_atomic_fetch_add_64(&g_tracklist_version, 1);
}

static void clear_tracks_run(void* ctx) {
    u32 i;
    (void)ctx;
    list_clear(&g_tracks);
    for (i = 0; i < MUSIC_TRACKLISTS; i++) {
        list_free(&g_lists[i]);
    }
    retro_atomic_fetch_add_64(&g_tracklist_version, 1);
}

typedef struct usb_call {
    const OrbisMusicPlayerUsbDeviceInfo* devices;
    u32 count;
    s32 result;
} usb_call_t;

static void usb_run(void* ctx) {
    usb_call_t* call = (usb_call_t*)ctx;
    u32 i;
    g_usb_count = 0;
    call->result = ORBIS_OK;
    for (i = 0; i < call->count; ++i) {
        const OrbisMusicPlayerUsbDeviceInfo* dev = &call->devices[i];
        size_t str_len = 0;
        unsigned d;
        if (dev->serial_number_len > 255) {
            c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "SetUsbStorageDeviceInfo",
                  "device[%u] serial_number_len %llu > 255", i,
                  (unsigned long long)dev->serial_number_len);
            call->result = ORBIS_MUSIC_PLAYER_SERVICE_ERROR_INVALID_PARAMETER;
            return;
        }
        while (str_len < sizeof(dev->serial_number) && dev->serial_number[str_len]) {
            str_len++;
        }
        if (str_len > dev->serial_number_len) {
            c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "SetUsbStorageDeviceInfo",
                  "device[%u] strlen %llu > serial_number_len %llu", i, (unsigned long long)str_len,
                  (unsigned long long)dev->serial_number_len);
            call->result = ORBIS_MUSIC_PLAYER_SERVICE_ERROR_INVALID_PARAMETER;
            return;
        }
        /* At most 255: serial_number_len is */
        for (d = 0; d < g_usb_count; d++) {
            if (strlen(g_usb[d].serial) == str_len &&
                memcmp(g_usb[d].serial, dev->serial_number, str_len) == 0) {
                break;
            }
        }
        memcpy(g_usb[d].serial, dev->serial_number, str_len);
        g_usb[d].serial[str_len] = '\0';
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_INFO, "SetUsbStorageDeviceInfo",
              "registered USB device_id %u with serial '%s'", dev->device_id, g_usb[d].serial);
        g_usb[d].device_id = dev->device_id;
        if (d == g_usb_count) {
            g_usb_count++;
        }
    }
}

/* A call on a list, inside a transaction */
enum { LIST_BEGIN, LIST_END, LIST_CREATE, LIST_REMOVE, LIST_INSERT, LIST_REMOVE_ENTRY };

typedef struct list_call {
    int op;
    u32 tracklist_id;
    u32 track_index;
    const char* uri;
    s32 result;
} list_call_t;

static void list_call_run(void* ctx) {
    list_call_t* call = (list_call_t*)ctx;
    music_list_t* list;
    call->result = ORBIS_OK;
    switch (call->op) {
    case LIST_BEGIN:
        if (atomic_load_s32(&g_in_transaction) != 0) {
            c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "BeginTransaction",
                  "BeginTransaction: already in transaction");
            call->result = ORBIS_MUSIC_PLAYER_SERVICE_ERROR_ALREADY_IN_TRANSACTION;
            return;
        }
        (void)retro_atomic_exchange_int(&g_in_transaction, 1);
        return;
    case LIST_END:
        if (atomic_load_s32(&g_in_transaction) == 0) {
            c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "EndTransaction",
                  "EndTransaction: not in transaction");
            call->result = ORBIS_MUSIC_PLAYER_SERVICE_ERROR_NOT_IN_TRANSACTION;
            return;
        }
        (void)retro_atomic_exchange_int(&g_in_transaction, 0);
        retro_atomic_fetch_add_64(&g_tracklist_version, 1);
        if (g_lists[0].exists) {
            list_copy(&g_tracks, &g_lists[0], "EndTransaction");
        }
        return;
    default:
        break;
    }

    if (atomic_load_s32(&g_in_transaction) == 0) {
        static const char* const names[] = {
            "", "", "CreateTrackList", "RemoveTrackList", "InsertTrackEntry", "RemoveTrackEntry"};
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, names[call->op], "%s: not in transaction",
              names[call->op]);
        call->result = ORBIS_MUSIC_PLAYER_SERVICE_ERROR_NOT_IN_TRANSACTION;
        return;
    }
    list = &g_lists[call->tracklist_id];
    switch (call->op) {
    case LIST_CREATE:
        list_clear(list);
        list->exists = true;
        return;
    case LIST_REMOVE:
        list_free(list);
        if (call->tracklist_id == 0) {
            list_clear(&g_tracks);
        }
        return;
    case LIST_INSERT:
        list->exists = true;
        list_insert(list, call->track_index, call->uri, "InsertTrackEntry");
        return;
    case LIST_REMOVE_ENTRY:
        if (list->exists && call->track_index < list->count) {
            list_erase(list, call->track_index);
        }
        return;
    default:
        return;
    }
}

static s32 list_call(int op, u32 tracklist_id, u32 track_index, const char* uri) {
    list_call_t call;
    call.op = op;
    call.tracklist_id = tracklist_id;
    call.track_index = track_index;
    call.uri = uri;
    call.result = ORBIS_OK;
    combiner_run(&g_data, list_call_run, &call);
    return call.result;
}

typedef struct count_call {
    u32 count;
} count_call_t;

static void count_run(void* ctx) {
    ((count_call_t*)ctx)->count = g_tracks.count;
}

static u32 track_count(void) {
    count_call_t call;
    call.count = 0;
    combiner_run(&g_data, count_run, &call);
    return call.count;
}

/* ---- Playback ---- */

typedef struct play_track_call {
    u32 track_index;
    bool out_of_range;
} play_track_call_t;

static void play_track_run(void* ctx) {
    play_track_call_t* call = (play_track_call_t*)ctx;
    call->out_of_range = call->track_index >= g_tracks.count;
    if (!call->out_of_range) {
        (void)retro_atomic_exchange_int(&g_current_track_idx, (int)call->track_index);
    }
}

typedef struct fetch_track_call {
    u32 track_index;
    bool have;
} fetch_track_call_t;

static void fetch_track_run(void* ctx) {
    fetch_track_call_t* call = (fetch_track_call_t*)ctx;
    call->have = call->track_index < g_tracks.count;
    if (call->have) {
        strlcpy(g_uri, g_tracks.uris[call->track_index], sizeof(g_uri));
        (void)retro_atomic_exchange_int(&g_current_track_idx, (int)call->track_index);
    }
}

/* g_guest on a USB device the game named: g_resolved on its /mnt/usbN */
static void resolve_usb_run(void* ctx) {
    unsigned d;
    (void)ctx;
    for (d = 0; d < g_usb_count; d++) {
        const size_t len = strlen(g_usb[d].serial);
        if (g_guest[0] == '/' && strncmp(g_guest + 1, g_usb[d].serial, len) == 0 &&
            g_guest[1 + len] == '/') {
            snprintf(g_resolved, sizeof(g_resolved), "/mnt/usb%u/%s", g_usb[d].device_id,
                     g_guest + len + 2);
            break;
        }
    }
}

/* The URI as a guest path: the scheme dropped, rooted, %XX decoded */
static void uri_to_guest_path(void) {
    const char* p = strstr(g_uri, "://");
    char rooted[MUSIC_PATH_SIZE];
    size_t n;
    size_t i;
    size_t o = 0;
    p = p ? p + 3 : g_uri;
    if (p[0] != '/') {
        rooted[0] = '/';
        strlcpy(rooted + 1, p, sizeof(rooted) - 1);
    } else {
        strlcpy(rooted, p, sizeof(rooted));
    }
    n = strlen(rooted);
    for (i = 0; i < n; ++i) {
        if (rooted[i] == '%' && i + 2 < n && isxdigit((unsigned char)rooted[i + 1]) &&
            isxdigit((unsigned char)rooted[i + 2])) {
            char hex_str[3];
            hex_str[0] = rooted[i + 1];
            hex_str[1] = rooted[i + 2];
            hex_str[2] = '\0';
            g_guest[o++] = (char)strtol(hex_str, NULL, 16);
            i += 2;
            continue;
        }
        g_guest[o++] = rooted[i];
    }
    g_guest[o] = '\0';
}

/* The stream kind of @path by its extension (as std::filesystem::path takes one: from the
 * file name's last dot, never a dot that starts it), or -1 */
static int stream_kind_of(const char* path) {
    const char* name = path;
    const char* dot;
    const char* s;
    char ext[8];
    size_t len;
    size_t i;
    for (s = path; *s; s++) {
        if (*s == '/'
#ifdef _WIN32
            || *s == '\\'
#endif
        ) {
            name = s + 1;
        }
    }
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        return -1;
    }
    dot = strrchr(name, '.');
    if (!dot || dot == name) {
        return -1;
    }
    len = strlen(dot);
    if (len >= sizeof(ext)) {
        return -1;
    }
    for (i = 0; i <= len; i++) {
        ext[i] = (char)tolower((unsigned char)dot[i]);
    }
    if (strcmp(ext, ".flac") == 0) {
        return AUDIO_STREAM_FLAC;
    }
    if (strcmp(ext, ".mp3") == 0) {
        return AUDIO_STREAM_MP3;
    }
    if (strcmp(ext, ".aac") == 0) {
        return AUDIO_STREAM_ADTS;
    }
    if (strcmp(ext, ".m4a") == 0 || strcmp(ext, ".mp4") == 0) {
        return AUDIO_STREAM_MP4;
    }
    return -1;
}

/* One track played from the mapped file */
typedef struct track_play {
    int kind;
    u64 start_time_ms;
    bool opened; /* the stream could be decoded */
    bool go_on;  /* the playlist may go on after it */
    audio_stream_t* stream;
    audio_mix_input_t* input;
    unsigned hz;
    unsigned channels;
    u64 total_frames;
    /* The window: frames [base, base + have) of the track, and the position in it (32.32) */
    u64 base;
    u64 have;
    u64 rel;
    bool eof;
} track_play_t;

static void track_seek_to(track_play_t* t, u64 ms) {
    u64 frame = ms * t->hz / 1000;
    u64 start;
    if (frame > t->total_frames) {
        frame = t->total_frames;
    }
    start = frame > AUDIO_MIX_RESAMPLE_BEHIND ? frame - AUDIO_MIX_RESAMPLE_BEHIND : 0;
    t->eof = !audio_stream_seek(t->stream, start);
    t->base = start;
    t->have = 0;
    t->rel = (frame - start) << 32;
}

/* The resampler reads a few frames either side of the position, so the window is decoded on
 * into only once nothing more can be written from it, and keeps the frames behind the
 * position when it moves on */
static void track_decode_on(track_play_t* t) {
    const u64 at = t->rel >> 32;
    u64 keep_from = at > AUDIO_MIX_RESAMPLE_BEHIND ? at - AUDIO_MIX_RESAMPLE_BEHIND : 0;
    size_t want;
    size_t got;
    if (keep_from > t->have) {
        keep_from = t->have;
    }
    memmove(g_window, g_window + keep_from * t->channels,
            (size_t)(t->have - keep_from) * t->channels * sizeof(float));
    t->have -= keep_from;
    t->base += keep_from;
    t->rel -= keep_from << 32;
    want = (size_t)(MUSIC_WINDOW_FRAMES - t->have);
    got = audio_stream_read(t->stream, g_window + t->have * t->channels, want);
    t->have += got;
    t->eof = got < want;
}

static int input_ready(void* ctx) {
    return audio_mix_input_ready((audio_mix_input_t*)ctx, MUSIC_CHUNK_FRAMES,
                                 AUDIO_MIX_FRAME_FRAMES) != 0;
}

static void track_feed(track_play_t* t, audio_mix_resampler_t* resampler) {
    while (atomic_load_s32(&g_stop_requested) == 0) {
        const s64 seek_target = (s64)retro_atomic_exchange_64(&g_seek_offset_ms, -1);
        float vol;
        unsigned done = 0;
        bool ended = false;
        if (seek_target >= 0) {
            track_seek_to(t, (u64)seek_target);
            (void)retro_atomic_exchange_64(&g_current_time_ms, (u64)seek_target);
        }

        if (atomic_load_s32(&g_pause_requested) != 0) {
            /* Parked until unpaused, stopped or seeked, each of which notifies */
            const int key = retro_eventcount_prepare_wait(&g_control_ec);
            if (atomic_load_s32(&g_pause_requested) != 0 &&
                atomic_load_s32(&g_stop_requested) == 0 &&
                (s64)retro_atomic_load_acquire_64(&g_seek_offset_ms) < 0) {
                retro_eventcount_commit_wait(&g_control_ec, key);
            } else {
                retro_eventcount_cancel_wait(&g_control_ec);
            }
            continue;
        }

        if (t->eof && (t->rel >> 32) >= t->have) {
            break;
        }
        /* Paced by the mix's frames; bounded by 20 ms of the game's time, so stop, pause and
         * seek are seen while the frontend takes nothing */
        if (!kernel_game_wait(audio_mix_input_room_ec(t->input), kernel_game_deadline_in(20000000),
                              input_ready, t->input)) {
            continue;
        }

        vol = ((float)atomic_load_s32(&g_volume) / 25.0f) *
              ((float)emu_settings_get_volume_slider() * 0.01f);
        while (done < MUSIC_CHUNK_FRAMES && !ended) {
            float* dst = NULL;
            unsigned wrote = 0;
            const unsigned room =
                audio_mix_input_reserve(t->input, &dst, MUSIC_CHUNK_FRAMES - done);
            if (room == 0) {
                break;
            }
            while (wrote < room) {
                /* At the track's end the last frames are read with silence past them */
                const unsigned ready =
                    t->eof ? room - wrote
                           : audio_mix_resample_ready(resampler, t->rel, t->have, room - wrote);
                unsigned got;
                if (ready == 0) {
                    track_decode_on(t);
                    continue;
                }
                got = audio_mix_resample_f32(resampler, g_window, t->have, &t->rel, dst + wrote * 2,
                                             ready, vol);
                wrote += got;
                if (got < ready) {
                    ended = true; /* the track has ended */
                    break;
                }
            }
            audio_mix_input_commit(t->input, wrote);
            done += wrote;
        }
        (void)retro_atomic_exchange_64(&g_current_time_ms,
                                       (t->base + (t->rel >> 32)) * 1000 / t->hz);
    }
}

/* Decoded where it lies a block at a time as it is played: the track is never held decoded
 * whole, and its samples reach the mix as floats */
static void track_play_lent(const u8* data, size_t size, void* ctx) {
    track_play_t* t = (track_play_t*)ctx;
    audio_mix_resampler_t resampler;
    u64 total_duration;
    t->stream = audio_stream_open(data, size, (enum audio_stream_kind)t->kind);
    if (!t->stream) {
        return;
    }
    t->opened = true;
    t->hz = audio_stream_rate(t->stream);
    t->channels = audio_stream_channels(t->stream);
    t->total_frames = audio_stream_frames(t->stream);
    total_duration = t->total_frames * 1000 / t->hz;
    (void)retro_atomic_exchange_64(&g_total_duration_ms, total_duration);

    if (atomic_load_s32(&g_stop_requested) != 0) {
        audio_stream_close(t->stream);
        return;
    }

    /* The track goes to the frontend through the core's mix, resampled to its 48 kHz straight
     * from the window it is decoded into, into the input's ring */
    if (t->channels > 2) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "PlaybackWorker",
              "Music with %u channels is not played", t->channels);
        audio_stream_close(t->stream);
        return;
    }
    t->input = audio_mix_input_open();
    if (!t->input) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "PlaybackWorker",
              "No input left in the core's mix for music");
        audio_stream_close(t->stream);
        return;
    }
    audio_mix_resampler_init(&resampler, t->hz, t->channels);

    t->base = 0;
    t->have = 0;
    t->rel = 0;
    t->eof = false;
    if (t->start_time_ms > 0 && total_duration > 0) {
        track_seek_to(t, t->start_time_ms);
    }

    track_feed(t, &resampler);

    audio_mix_input_close(t->input);
    audio_stream_close(t->stream);
    t->go_on = true;
}

/* Plays @track_idx: whether the playlist may go on after it */
static bool play_one(u32 track_idx, u64 start_time_ms) {
    fetch_track_call_t fetch;
    track_play_t t;
    int kind;

    fetch.track_index = track_idx;
    fetch.have = false;
    combiner_run(&g_data, fetch_track_run, &fetch);
    if (!fetch.have) {
        return false;
    }

    uri_to_guest_path();
    strlcpy(g_resolved, g_guest, sizeof(g_resolved));
    combiner_run(&g_data, resolve_usb_run, NULL);

    if (!fs_guest_host_path(g_resolved, g_host, sizeof(g_host)) || !path_is_valid(g_host)) {
        strlcpy(g_host, g_resolved, sizeof(g_host));
    }

    if (!path_is_valid(g_host)) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "PlaybackWorker",
              "Music file does not exist: %s", g_host);
        return false;
    }

    kind = stream_kind_of(g_host);
    if (kind < 0) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "PlaybackWorker",
              "Not a format the music player decodes: %s", g_host);
        return false;
    }

    memset(&t, 0, sizeof(t));
    t.kind = kind;
    t.start_time_ms = start_time_ms;
    if (!fs_lend_host_file(g_host, track_play_lent, &t) || !t.opened) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "PlaybackWorker",
              "Failed to decode audio file: %s", g_host);
        return false;
    }
    return t.go_on;
}

static void playback_worker(void* arg) {
    u32 track_idx = g_worker_track;
    u64 start_time_ms = g_worker_start_ms;
    (void)arg;

    while (atomic_load_s32(&g_stop_requested) == 0) {
        u32 count;
        u32 rep;
        u32 shuf;
        u32 next_track;
        int retry;
        if (!play_one(track_idx, start_time_ms)) {
            break;
        }

        if (atomic_load_s32(&g_stop_requested) != 0) {
            break;
        }

        count = track_count();
        if (count == 0) {
            break;
        }

        rep = (u32)atomic_load_s32(&g_repeat_mode);
        shuf = (u32)atomic_load_s32(&g_shuffle_mode);

        next_track = track_idx;
        if (rep == 2) {
            /* Repeat one */
            next_track = track_idx;
        } else if (shuf == 1) {
            /* Shuffle */
            if (count > 1) {
                for (retry = 0; retry < 10 && next_track == track_idx; ++retry) {
                    next_track = (u32)(rand() % count);
                }
            }
        } else if (rep == 1) {
            /* Repeat all */
            next_track = (track_idx + 1) % count;
        } else {
            /* Repeat off */
            if (track_idx + 1 < count) {
                next_track = track_idx + 1;
            } else {
                c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_INFO, "PlaybackWorker",
                      "Playback reached end of playlist");
                break;
            }
        }

        track_idx = next_track;
        start_time_ms = 0;
        (void)retro_atomic_exchange_int(&g_current_track_idx, (int)track_idx);
        (void)retro_atomic_exchange_64(&g_current_time_ms, 0);
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_INFO, "PlaybackWorker",
              "Auto-advancing to track_index=%u", track_idx);
    }

    if (atomic_load_s32(&g_stop_requested) == 0) {
        (void)retro_atomic_exchange_int(&g_play_state, 0);
        (void)retro_atomic_exchange_64(&g_current_time_ms, 0);
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_INFO, "PlaybackWorker", "Playback finished");
    }
    /* The event the thread's waits cached goes with it */
    retro_eventcount_thread_cleanup();
}

/* ---- The control calls, run by g_control ---- */

static void stop_playback_internal(void) {
    (void)retro_atomic_exchange_int(&g_stop_requested, 1);
    (void)retro_atomic_exchange_int(&g_pause_requested, 0);
    (void)retro_atomic_exchange_int(&g_play_state, 0);
    (void)retro_atomic_exchange_64(&g_current_time_ms, 0);
    (void)retro_atomic_exchange_64(&g_total_duration_ms, 0);
    retro_eventcount_notify(&g_control_ec);

    stop_thread_join(&g_worker_thread);
}

static void play_locked(u32 track_index, u64 start_time_ms) {
    play_track_call_t call;
    stop_playback_internal();

    call.track_index = track_index;
    call.out_of_range = true;
    combiner_run(&g_data, play_track_run, &call);
    if (call.out_of_range) {
        return;
    }

    (void)retro_atomic_exchange_int(&g_stop_requested, 0);
    (void)retro_atomic_exchange_int(&g_pause_requested, 0);
    (void)retro_atomic_exchange_64(&g_seek_offset_ms, -1);
    (void)retro_atomic_exchange_64(&g_current_time_ms, start_time_ms);
    (void)retro_atomic_exchange_64(&g_total_duration_ms, 0);
    (void)retro_atomic_exchange_int(&g_play_state, 1);

    /* stop_playback_internal above has joined the thread before */
    g_worker_track = track_index;
    g_worker_start_ms = start_time_ms;
    stop_thread_start(&g_worker_thread, playback_worker, NULL);
}

static void unpause(void) {
    (void)retro_atomic_exchange_int(&g_pause_requested, 0);
    (void)retro_atomic_exchange_int(&g_play_state, 1);
    retro_eventcount_notify(&g_control_ec);
}

static void seek(u64 time_ms) {
    (void)retro_atomic_exchange_64(&g_seek_offset_ms, (s64)time_ms);
    (void)retro_atomic_exchange_64(&g_current_time_ms, time_ms);
    retro_eventcount_notify(&g_control_ec);
}

static bool is_track_active(u32 track_index) {
    return (u32)atomic_load_s32(&g_current_track_idx) == track_index &&
           (u32)atomic_load_s32(&g_play_state) != 0 && stop_thread_started(&g_worker_thread) &&
           atomic_load_s32(&g_stop_requested) == 0;
}

enum { CONTROL_STOP, CONTROL_PLAY_OR_SEEK, CONTROL_SEEK_TRACK, CONTROL_NEXT, CONTROL_PREVIOUS };

typedef struct control_call {
    int op;
    u32 track_index;
    u64 time_ms;
} control_call_t;

static void control_run(void* ctx) {
    const control_call_t* call = (const control_call_t*)ctx;
    u32 count;
    u32 cur;
    switch (call->op) {
    case CONTROL_STOP:
        stop_playback_internal();
        return;
    case CONTROL_PLAY_OR_SEEK:
        if (is_track_active(call->track_index)) {
            if (call->time_ms > 0) {
                seek(call->time_ms);
            }
            unpause();
        } else {
            play_locked(call->track_index, call->time_ms);
        }
        return;
    case CONTROL_SEEK_TRACK:
        if (is_track_active(call->track_index)) {
            seek(call->time_ms);
        } else {
            play_locked(call->track_index, call->time_ms);
        }
        return;
    case CONTROL_NEXT:
        count = track_count();
        if (count > 0) {
            u32 next_idx;
            int retry;
            cur = (u32)atomic_load_s32(&g_current_track_idx);
            next_idx = (cur + 1) % count;
            if ((u32)atomic_load_s32(&g_shuffle_mode) == 1 && count > 1) {
                for (retry = 0; retry < 10 && next_idx == cur; ++retry) {
                    next_idx = (u32)(rand() % count);
                }
            }
            play_locked(next_idx, 0);
        }
        return;
    case CONTROL_PREVIOUS:
        count = track_count();
        if (count > 0) {
            cur = (u32)atomic_load_s32(&g_current_track_idx);
            play_locked(cur > 0 ? cur - 1 : 0, 0);
        }
        return;
    default:
        return;
    }
}

static void control(int op, u32 track_index, u64 time_ms) {
    control_call_t call;
    call.op = op;
    call.track_index = track_index;
    call.time_ms = time_ms;
    combiner_run(&g_control, control_run, &call);
}

static void stop_playback(void) {
    control(CONTROL_STOP, 0, 0);
}

/* ---- The player ---- */

static void music_player_library_deinit(void) {
    u32 i;
    stop_playback();
    mpsc_ring_free(&g_events);
    retro_eventcount_free(&g_events_ec);
    retro_eventcount_free(&g_control_ec);
    list_free(&g_tracks);
    for (i = 0; i < MUSIC_TRACKLISTS; i++) {
        list_free(&g_lists[i]);
    }
    combiner_free(&g_data);
    combiner_free(&g_control);
    g_inited = false;
}

void music_player_library_init(void) {
    if (g_inited) {
        return;
    }
    g_inited = true;
    combiner_init(&g_control);
    combiner_init(&g_data);
    retro_eventcount_init(&g_control_ec);
    retro_eventcount_init(&g_events_ec);
    mpsc_ring_init(&g_events, 64, sizeof(music_event_t));
    retro_atomic_store_release_int(&g_volume, 25);
    retro_atomic_store_release_64(&g_seek_offset_ms, -1);
    retro_atomic_store_release_64(&g_tracklist_version, 1);
    /* As the C++ player's static went: stopped before the core goes */
    atexit(music_player_library_deinit);
}

bool music_player_is_bgm_playing(void) {
    return atomic_load_s32(&g_play_state) != 0;
}

static u32 play_status_of(u32 state) {
    if (state == 1) {
        return 0; /* playing */
    }
    if (state == 2) {
        return 1; /* paused */
    }
    return 2; /* stopped */
}

s32 PS4_SYSV_ABI sceMusicPlayerServiceInitialize(size_t pool_size, void* pool_addr) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceMusicPlayerServiceInitialize",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceMusicPlayerServiceInitialize2(size_t pool_size, void* pool_addr) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceMusicPlayerServiceInitialize2",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceMusicPlayerServiceInitialize3(size_t pool_size, void* pool_addr, u64 unk1,
                                                  u64 unk2) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_INFO, "sceMusicPlayerServiceInitialize3",
          "pool_size=0x%llx, pool_addr=0x%llx", (unsigned long long)pool_size,
          (unsigned long long)(uintptr_t)pool_addr);
    if (pool_size < 0x200000 || !pool_addr) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceMusicPlayerServiceInitialize3",
              "invalid arguments: pool_size=0x%llx, pool_addr=0x%llx",
              (unsigned long long)pool_size, (unsigned long long)(uintptr_t)pool_addr);
        return ORBIS_MUSIC_PLAYER_SERVICE_ERROR_INVALID_PARAMETER;
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceMusicPlayerServiceTerminate(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_INFO, "sceMusicPlayerServiceTerminate", "called");
    stop_playback();
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceMusicPlayerServiceSetUsbStorageDeviceInfo(
    const OrbisMusicPlayerUsbDeviceInfo* devices, u32 count) {
    usb_call_t call;
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_INFO, "sceMusicPlayerServiceSetUsbStorageDeviceInfo",
          "devices=0x%llx, count=%u", (unsigned long long)(uintptr_t)devices, count);
    if (!devices || count == 0 || count > MUSIC_MAX_USB_DEVICES) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "SetUsbStorageDeviceInfo",
              "invalid arguments: devices=0x%llx, count=%u", (unsigned long long)(uintptr_t)devices,
              count);
        return ORBIS_MUSIC_PLAYER_SERVICE_ERROR_INVALID_PARAMETER;
    }
    call.devices = devices;
    call.count = count;
    call.result = ORBIS_OK;
    combiner_run(&g_data, usb_run, &call);
    return call.result;
}

s32 PS4_SYSV_ABI sceMusicPlayerServiceRemoveAllData(s32 player_id) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_INFO, "sceMusicPlayerServiceRemoveAllData",
          "player_id=%d", player_id);
    stop_playback();
    combiner_run(&g_data, clear_tracks_run, NULL);
    return ORBIS_OK;
}

static s32 set_track_list(s32 player_id, const void* tracks, u32 count, u32 flags,
                          size_t track_entry_size) {
    set_tracks_call_t call;
    u32 i;
    if (!tracks || count == 0) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "SetTrackListInternal",
              "invalid arguments: tracks=0x%llx, count=%u", (unsigned long long)(uintptr_t)tracks,
              count);
        return ORBIS_MUSIC_PLAYER_SERVICE_ERROR_INVALID_PARAMETER;
    }

    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_INFO, "SetTrackListInternal",
          "player_id=%d, count=%u, flags=%u, stride=0x%llx", player_id, count, flags,
          (unsigned long long)track_entry_size);

    call.base = (const u8*)tracks;
    call.count = count;
    call.stride = track_entry_size;
    for (i = 0; i < count; i++) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_DEBUG, "SetTrackListInternal", "  track[%u]: %s", i,
              (const char*)(call.base + i * track_entry_size));
    }

    /* Straight from the game's entries into the lists */
    combiner_run(&g_data, set_tracks_run, &call);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceMusicPlayerServiceSetTrackList(s32 player_id, const void* tracks, u32 count) {
    return set_track_list(player_id, tracks, count, 0, 0xc2f);
}

s32 PS4_SYSV_ABI sceMusicPlayerServiceSetTrackList2(s32 player_id, const void* tracks, u32 count,
                                                    u32 flags) {
    return set_track_list(player_id, tracks, count, flags, 0xc48);
}

s32 PS4_SYSV_ABI sceMusicPlayerServicePlay(s32 player_id, u32 track_index, s64 start_time_s,
                                           s32 flags) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_INFO, "sceMusicPlayerServicePlay",
          "player_id=%d, track_index=%u, start_time_s=%lld, flags=%d", player_id, track_index,
          (long long)start_time_s, flags);
    control(CONTROL_PLAY_OR_SEEK, track_index, start_time_s > 0 ? (u64)(start_time_s * 1000) : 0);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceMusicPlayerServicePlayStartByTime(s32 player_id, u32 track_index,
                                                      s64 start_time_s) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_INFO, "sceMusicPlayerServicePlayStartByTime",
          "player_id=%d, track_index=%u, start_time_s=%lld", player_id, track_index,
          (long long)start_time_s);
    control(CONTROL_PLAY_OR_SEEK, track_index, start_time_s > 0 ? (u64)(start_time_s * 1000) : 0);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceMusicPlayerServiceStop(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_INFO, "sceMusicPlayerServiceStop", "called");
    stop_playback();
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceMusicPlayerServicePause(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_INFO, "sceMusicPlayerServicePause", "called");
    (void)retro_atomic_exchange_int(&g_pause_requested, 1);
    (void)retro_atomic_exchange_int(&g_play_state, 2);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceMusicPlayerServiceUnpause(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_INFO, "sceMusicPlayerServiceUnpause", "called");
    unpause();
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceMusicPlayerServiceNextTrack(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_INFO, "sceMusicPlayerServiceNextTrack", "called");
    control(CONTROL_NEXT, 0, 0);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceMusicPlayerServicePreviousTrack(void) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_INFO, "sceMusicPlayerServicePreviousTrack", "called");
    control(CONTROL_PREVIOUS, 0, 0);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceMusicPlayerServiceSeek(s32 player_id, u32 track_index, s64 time_s) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_INFO, "sceMusicPlayerServiceSeek",
          "player_id=%d, track_index=%u, time_s=%lld", player_id, track_index, (long long)time_s);
    control(CONTROL_SEEK_TRACK, track_index, time_s > 0 ? (u64)(time_s * 1000) : 0);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceMusicPlayerServiceSetAudioVolume(s32 volume) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_INFO, "sceMusicPlayerServiceSetAudioVolume", "volume=%d",
          volume);
    if (volume < 0 || volume > 25) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceMusicPlayerServiceSetAudioVolume",
              "invalid volume=%d", volume);
        return ORBIS_MUSIC_PLAYER_SERVICE_ERROR_INVALID_PARAMETER;
    }
    (void)retro_atomic_exchange_int(&g_volume, volume);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceMusicPlayerServiceGetAudioVolume(s32* volume, s32* mute) {
    if (volume) {
        *volume = atomic_load_s32(&g_volume);
    }
    if (mute) {
        *mute = 0;
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceMusicPlayerServiceSetShuffle(u32 mode) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_INFO, "sceMusicPlayerServiceSetShuffle", "mode=%u",
          mode);
    if (mode > 1) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceMusicPlayerServiceSetShuffle",
              "invalid mode=%u", mode);
        return ORBIS_MUSIC_PLAYER_SERVICE_ERROR_INVALID_PARAMETER;
    }
    (void)retro_atomic_exchange_int(&g_shuffle_mode, (int)mode);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceMusicPlayerServiceSetRepeatMode(u32 mode) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_INFO, "sceMusicPlayerServiceSetRepeatMode", "mode=%u",
          mode);
    if (mode > 2) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceMusicPlayerServiceSetRepeatMode",
              "invalid mode=%u", mode);
        return ORBIS_MUSIC_PLAYER_SERVICE_ERROR_INVALID_PARAMETER;
    }
    (void)retro_atomic_exchange_int(&g_repeat_mode, (int)mode);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI
sceMusicPlayerServiceGetPlayStatusExtension(OrbisMusicPlayerPlayStatusExtension* status) {
    u32 track_total;
    u32 cur_track;
    u32 rep_mode;
    u32 shuf_mode;
    bool can_next;
    bool can_prev;
    if (!status || status->size != 0x48) {
        return ORBIS_MUSIC_PLAYER_SERVICE_ERROR_INVALID_PARAMETER;
    }

    track_total = track_count();
    cur_track = (u32)atomic_load_s32(&g_current_track_idx);
    rep_mode = (u32)atomic_load_s32(&g_repeat_mode);
    shuf_mode = (u32)atomic_load_s32(&g_shuffle_mode);

    status->error_code = 0;
    status->play_state = play_status_of((u32)atomic_load_s32(&g_play_state));
    status->total_duration_s = (u64)retro_atomic_load_acquire_64(&g_total_duration_ms) / 1000;
    status->current_time_s = (u64)retro_atomic_load_acquire_64(&g_current_time_ms) / 1000;
    status->tracklist_id = 0;
    status->track_index = cur_track;
    status->play_index = cur_track;
    status->total_track_count = track_total;
    status->volume = (u32)atomic_load_s32(&g_volume);
    status->repeat_mode = rep_mode;
    status->shuffle_mode = shuf_mode;
    can_next = track_total > 0 && (cur_track + 1 < track_total || rep_mode != 0 || shuf_mode != 0);
    can_prev = track_total > 0 && (cur_track > 0 || rep_mode != 0 || shuf_mode != 0);

    /* 0 = skip allowed, 1 = skip disabled (playlist boundary reached with repeat/shuffle off) */
    status->can_skip_next = can_next ? 0 : 1;
    status->can_skip_previous = can_prev ? 0 : 1;
    status->is_loading = 0;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI
sceMusicPlayerServiceGetCurrentPlayStatus(SceMusicPlayerServicePlayStatus* status) {
    if (!status) {
        return ORBIS_MUSIC_PLAYER_SERVICE_ERROR_INVALID_PARAMETER;
    }
    status->result = 0;
    status->play_status = play_status_of((u32)atomic_load_s32(&g_play_state));
    status->track_index = (u32)atomic_load_s32(&g_current_track_idx);
    status->tracklist_id = 0;
    status->playback_time = (u64)retro_atomic_load_acquire_64(&g_current_time_ms) / 1000;
    status->shuffle = (u32)atomic_load_s32(&g_shuffle_mode);
    status->repeat = (u32)atomic_load_s32(&g_repeat_mode);
    return ORBIS_OK;
}

static int event_take(void* ctx) {
    return mpsc_ring_take(&g_events, ctx) != 0;
}

s32 PS4_SYSV_ABI sceMusicPlayerServiceReceiveEvent(void* buffer) {
    music_event_t ev;
    if (!buffer) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceMusicPlayerServiceReceiveEvent",
              "invalid buffer");
        return ORBIS_MUSIC_PLAYER_SERVICE_ERROR_INVALID_PARAMETER;
    }
    ev.event_id = 0;
    ev.track_index = 0;
    /* The next event, waiting up to 50 ms of the game's time for one */
    if (kernel_game_wait(&g_events_ec, kernel_game_deadline_in((u64)50 * 1000000), event_take,
                         &ev)) {
        u8* buf = (u8*)buffer;
        const u32 one = 1;
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_DEBUG, "sceMusicPlayerServiceReceiveEvent",
              "delivering event_id=%u, track_idx=%u", ev.event_id, ev.track_index);
        memcpy(buf + 0x08, &one, sizeof(one));
        memcpy(buf + 0x20, &one, sizeof(one));
        memcpy(buf + 0x24, &ev.event_id, sizeof(ev.event_id));
        memcpy(buf + 0x28, &ev.track_index, sizeof(ev.track_index));
        return ORBIS_OK;
    }
    return ORBIS_MUSIC_PLAYER_SERVICE_ERROR_TIMED_OUT;
}

s32 PS4_SYSV_ABI sceMusicPlayerServiceGetPlaybackResults(void* a, void* b) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceMusicPlayerServiceGetPlaybackResults",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceMusicPlayerServiceGetTrackInfo(void* a, void* b) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceMusicPlayerServiceGetTrackInfo",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceMusicPlayerServiceBeginTransaction(s32 a) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_INFO, "sceMusicPlayerServiceBeginTransaction",
          "called: a=%d", a);
    return list_call(LIST_BEGIN, 0, 0, NULL);
}

s32 PS4_SYSV_ABI sceMusicPlayerServiceEndTransaction(s32 a) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_INFO, "sceMusicPlayerServiceEndTransaction",
          "called: a=%d", a);
    return list_call(LIST_END, 0, 0, NULL);
}

s32 PS4_SYSV_ABI sceMusicPlayerServiceCreateTrackList(u32 tracklist_id) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_INFO, "sceMusicPlayerServiceCreateTrackList",
          "called: tracklist_id=%u", tracklist_id);
    if (tracklist_id >= MUSIC_TRACKLISTS) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "CreateTrackList",
              "CreateTrackList: invalid tracklist_id=%u", tracklist_id);
        return ORBIS_MUSIC_PLAYER_SERVICE_ERROR_INVALID_PARAMETER;
    }
    return list_call(LIST_CREATE, tracklist_id, 0, NULL);
}

s32 PS4_SYSV_ABI sceMusicPlayerServiceRemoveTrackList(u32 tracklist_id) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_INFO, "sceMusicPlayerServiceRemoveTrackList",
          "called: tracklist_id=%u", tracklist_id);
    if (tracklist_id >= MUSIC_TRACKLISTS) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "RemoveTrackList",
              "RemoveTrackList: invalid tracklist_id=%u", tracklist_id);
        return ORBIS_MUSIC_PLAYER_SERVICE_ERROR_INVALID_PARAMETER;
    }
    return list_call(LIST_REMOVE, tracklist_id, 0, NULL);
}

s32 PS4_SYSV_ABI sceMusicPlayerServiceInsertTrackEntry(u32 tracklist_id, u32 track_index,
                                                       const void* track_entry) {
    const char* uri_str = (const char*)track_entry;
    if (!track_entry) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceMusicPlayerServiceInsertTrackEntry",
              "InsertTrackEntry: null track_entry");
        return ORBIS_MUSIC_PLAYER_SERVICE_ERROR_INVALID_PARAMETER;
    }
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_INFO, "sceMusicPlayerServiceInsertTrackEntry",
          "called: tracklist_id=%u, track_index=%u, uri=%s", tracklist_id, track_index, uri_str);
    if (tracklist_id >= MUSIC_TRACKLISTS || track_index > MUSIC_MAX_TRACK_INDEX) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "InsertTrackEntry",
              "InsertTrackEntry: invalid tracklist_id=%u or track_index=%u", tracklist_id,
              track_index);
        return ORBIS_MUSIC_PLAYER_SERVICE_ERROR_INVALID_PARAMETER;
    }
    return list_call(LIST_INSERT, tracklist_id, track_index, uri_str);
}

s32 PS4_SYSV_ABI sceMusicPlayerServiceRemoveTrackEntry(u32 tracklist_id, u32 track_index) {
    c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_INFO, "sceMusicPlayerServiceRemoveTrackEntry",
          "called: tracklist_id=%u, track_index=%u", tracklist_id, track_index);
    if (tracklist_id >= MUSIC_TRACKLISTS || track_index > MUSIC_MAX_TRACK_INDEX) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "RemoveTrackEntry",
              "RemoveTrackEntry: invalid tracklist_id=%u or track_index=%u", tracklist_id,
              track_index);
        return ORBIS_MUSIC_PLAYER_SERVICE_ERROR_INVALID_PARAMETER;
    }
    return list_call(LIST_REMOVE_ENTRY, tracklist_id, track_index, NULL);
}

s32 PS4_SYSV_ABI sceMusicPlayerServiceGetTrackListVersion(s32 player_id, u64* version) {
    if (!version) {
        c_log(C_LOG_LIB_AUDIO_OUT, C_LOG_LEVEL_ERROR, "sceMusicPlayerServiceGetTrackListVersion",
              "GetTrackListVersion: null version pointer");
        return ORBIS_MUSIC_PLAYER_SERVICE_ERROR_INVALID_PARAMETER;
    }
    *version = (u64)retro_atomic_load_acquire_64(&g_tracklist_version);
    return ORBIS_OK;
}
