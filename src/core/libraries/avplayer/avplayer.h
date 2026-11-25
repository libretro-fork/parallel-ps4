/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_AVPLAYER_AVPLAYER_H
#define SHADPS4_AVPLAYER_AVPLAYER_H

/* libSceAvPlayer. Defined in avplayer.c, registered in sysmodule_internal.cpp. */

#include <stdarg.h>
#include <stddef.h>

#include <boolean.h>
#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

struct AvPlayer;
typedef struct AvPlayer* AvPlayerHandle;

/* The enums: the values the game passes. Fields and parameters that hold them are u32 or s32. */
enum AvPlayerUriType { AV_PLAYER_URI_TYPE_SOURCE = 0 };

typedef struct AvPlayerUri {
    const char* name;
    u32 length;
} AvPlayerUri;

enum AvPlayerSourceType {
    AV_PLAYER_SOURCE_TYPE_UNKNOWN = 0,
    AV_PLAYER_SOURCE_TYPE_FILE_MP4 = 1,
    AV_PLAYER_SOURCE_TYPE_HLS = 8
};

enum AvPlayerStreamType {
    AV_PLAYER_STREAM_TYPE_VIDEO = 0,
    AV_PLAYER_STREAM_TYPE_AUDIO = 1,
    AV_PLAYER_STREAM_TYPE_TIMED_TEXT = 2,
    AV_PLAYER_STREAM_TYPE_UNKNOWN = 3
};

typedef struct AvPlayerSourceDetails {
    AvPlayerUri uri;
    u8 reserved1[64];
    s32 source_type; /* AvPlayerSourceType */
    u8 reserved2[44];
} AvPlayerSourceDetails;

typedef struct AvPlayerAudio {
    u16 channel_count;
    u8 reserved1[2];
    u32 sample_rate;
    u32 size;
    char language_code[4];
} AvPlayerAudio;

typedef struct AvPlayerVideo {
    u32 width;
    u32 height;
    f32 aspect_ratio;
    char language_code[4];
} AvPlayerVideo;

typedef struct AvPlayerTextPosition {
    u16 top;
    u16 left;
    u16 bottom;
    u16 right;
} AvPlayerTextPosition;

typedef struct AvPlayerTimedText {
    char language_code[4];
    u16 text_size;
    u16 font_size;
    AvPlayerTextPosition position;
} AvPlayerTimedText;

typedef union AvPlayerStreamDetails {
    u8 reserved[16];
    AvPlayerAudio audio;
    AvPlayerVideo video;
    AvPlayerTimedText subs;
} AvPlayerStreamDetails;

typedef struct AvPlayerFrameInfo {
    u8* p_data;
    u8 reserved[4];
    u64 timestamp;
    AvPlayerStreamDetails details;
} AvPlayerFrameInfo;

typedef struct AvPlayerStreamInfo {
    u32 type; /* AvPlayerStreamType */
    u8 reserved[4];
    AvPlayerStreamDetails details;
    u64 duration;
    u64 start_time;
} AvPlayerStreamInfo;

typedef struct AvPlayerAudioEx {
    u16 channel_count;
    u8 reserved[2];
    u32 sample_rate;
    u32 size;
    u8 language_code[4];
    u8 reserved1[64];
} AvPlayerAudioEx;

typedef struct AvPlayerVideoEx {
    u32 width;
    u32 height;
    f32 aspect_ratio;
    u8 language_code[4];
    u32 framerate;
    u32 crop_left_offset;
    u32 crop_right_offset;
    u32 crop_top_offset;
    u32 crop_bottom_offset;
    u32 pitch;
    u8 luma_bit_depth;
    u8 chroma_bit_depth;
    bool video_full_range_flag;
    u8 reserved1[37];
} AvPlayerVideoEx;

typedef struct AvPlayerTimedTextEx {
    u8 language_code[4];
    u8 reserved[12];
    u8 reserved1[64];
} AvPlayerTimedTextEx;

typedef union AvPlayerStreamDetailsEx {
    AvPlayerAudioEx audio;
    AvPlayerVideoEx video;
    AvPlayerTimedTextEx subs;
    u8 reserved1[80];
} AvPlayerStreamDetailsEx;

typedef struct AvPlayerFrameInfoEx {
    void* p_data;
    u8 reserved[4];
    u64 timestamp;
    AvPlayerStreamDetailsEx details;
} AvPlayerFrameInfoEx;

typedef void* PS4_SYSV_ABI (*AvPlayerAllocate)(void* p, u32 align, u32 size);
typedef void PS4_SYSV_ABI (*AvPlayerDeallocate)(void* p, void* mem);
typedef void* PS4_SYSV_ABI (*AvPlayerAllocateTexture)(void* p, u32 align, u32 size);
typedef void PS4_SYSV_ABI (*AvPlayerDeallocateTexture)(void* p, void* mem);

typedef struct AvPlayerMemAllocator {
    void* object_ptr;
    AvPlayerAllocate allocate;
    AvPlayerDeallocate deallocate;
    AvPlayerAllocateTexture allocate_texture;
    AvPlayerDeallocateTexture deallocate_texture;
} AvPlayerMemAllocator;

typedef s32 PS4_SYSV_ABI (*AvPlayerOpenFile)(void* p, const char* name);
typedef s32 PS4_SYSV_ABI (*AvPlayerCloseFile)(void* p);
typedef s32 PS4_SYSV_ABI (*AvPlayerReadOffsetFile)(void* p, u8* buf, u64 pos, u32 len);
typedef u64 PS4_SYSV_ABI (*AvPlayerSizeFile)(void* p);

typedef struct AvPlayerFileReplacement {
    void* object_ptr;
    AvPlayerOpenFile open;
    AvPlayerCloseFile close;
    AvPlayerReadOffsetFile read_offset;
    AvPlayerSizeFile size;
} AvPlayerFileReplacement;

enum AvPlayerEvents {
    AV_PLAYER_EVENTS_STATE_STOP = 0x01,
    AV_PLAYER_EVENTS_STATE_READY = 0x02,
    AV_PLAYER_EVENTS_STATE_PLAY = 0x03,
    AV_PLAYER_EVENTS_STATE_PAUSE = 0x04,
    AV_PLAYER_EVENTS_STATE_BUFFERING = 0x05,
    AV_PLAYER_EVENTS_TIMED_TEXT_DELIVERY = 0x10,
    AV_PLAYER_EVENTS_WARNING_ID = 0x20,
    AV_PLAYER_EVENTS_ENCRYPTION = 0x30,
    AV_PLAYER_EVENTS_DRM_ERROR = 0x40
};

/* @event: AvPlayerEvents */
typedef void PS4_SYSV_ABI (*AvPlayerEventCallback)(void* p, s32 event, s32 src_id, void* data);

typedef struct AvPlayerEventReplacement {
    void* object_ptr;
    AvPlayerEventCallback event_callback;
} AvPlayerEventReplacement;

enum AvPlayerDebuglevels {
    AV_PLAYER_DEBUGLEVELS_NONE = 0,
    AV_PLAYER_DEBUGLEVELS_INFO,
    AV_PLAYER_DEBUGLEVELS_WARNINGS,
    AV_PLAYER_DEBUGLEVELS_ALL
};

typedef struct AvPlayerInitData {
    AvPlayerMemAllocator memory_replacement;
    AvPlayerFileReplacement file_replacement;
    AvPlayerEventReplacement event_replacement;
    s32 debug_level; /* AvPlayerDebuglevels */
    u32 base_priority;
    s32 num_output_video_framebuffers;
    bool auto_start;
    u8 reserved[3];
    const char* default_language;
} AvPlayerInitData;

typedef struct AvPlayerInitDataEx {
    size_t this_size;
    AvPlayerMemAllocator memory_replacement;
    AvPlayerFileReplacement file_replacement;
    AvPlayerEventReplacement event_replacement;
    const char* default_language;
    s32 debug_level; /* AvPlayerDebuglevels */
    u32 audio_decoder_priority;
    u32 audio_decoder_affinity;
    u32 video_decoder_priority;
    u32 video_decoder_affinity;
    u32 demuxer_priority;
    u32 demuxer_affinity;
    u32 controller_priority;
    u32 controller_affinity;
    u32 http_streaming_priority;
    u32 http_streaming_affinity;
    u32 file_streaming_priority;
    u32 file_streaming_affinity;
    s32 num_output_video_framebuffers;
    bool auto_start;
    u8 reserved[3];
} AvPlayerInitDataEx;

enum AvPlayerVideoDecoderType {
    AV_PLAYER_VIDEO_DECODER_TYPE_DEFAULT = 0,
    AV_PLAYER_VIDEO_DECODER_TYPE_RESERVED1,
    AV_PLAYER_VIDEO_DECODER_TYPE_SOFTWARE,
    AV_PLAYER_VIDEO_DECODER_TYPE_SOFTWARE2
};

enum AvPlayerAudioDecoderType {
    AV_PLAYER_AUDIO_DECODER_TYPE_DEFAULT = 0,
    AV_PLAYER_AUDIO_DECODER_TYPE_RESERVED1,
    AV_PLAYER_AUDIO_DECODER_TYPE_RESERVED2
};

typedef struct AvPlayerDecoderInit {
    union {
        u32 video_type; /* AvPlayerVideoDecoderType */
        u32 audio_type; /* AvPlayerAudioDecoderType */
        u8 reserved[4];
    } decoder_type;
    union {
        struct {
            s32 cpu_affinity_mask;
            s32 cpu_thread_priority;
            u8 decode_pipeline_depth;
            u8 compute_pipe_id;
            u8 compute_queue_id;
            u8 enable_interlaced;
            u8 reserved[16];
        } avc_sw2;
        struct {
            u8 audio_channel_order;
            u8 reserved[27];
        } aac;
        u8 reserved[28];
    } decoder_params;
} AvPlayerDecoderInit;

typedef struct AvPlayerHTTPCtx {
    u32 http_context_id;
    u32 ssl_context_id;
} AvPlayerHTTPCtx;

typedef struct AvPlayerPostInitData {
    u32 demux_video_buffer_size;
    AvPlayerDecoderInit video_decoder_init;
    AvPlayerDecoderInit audio_decoder_init;
    AvPlayerHTTPCtx http_context;
    u8 reserved[56];
} AvPlayerPostInitData;

enum AvPlayerAvSyncMode { AV_PLAYER_AV_SYNC_MODE_DEFAULT = 0, AV_PLAYER_AV_SYNC_MODE_NONE };

typedef int PS4_SYSV_ABI (*AvPlayerLogCallback)(void* p, const char* format, va_list args);

s32 PS4_SYSV_ABI sceAvPlayerAddSource(AvPlayerHandle handle, const char* filename);
s32 PS4_SYSV_ABI sceAvPlayerAddSourceEx(AvPlayerHandle handle, u32 uri_type,
                                        AvPlayerSourceDetails* source_details);
int PS4_SYSV_ABI sceAvPlayerChangeStream(void);
s32 PS4_SYSV_ABI sceAvPlayerClose(AvPlayerHandle handle);
u64 PS4_SYSV_ABI sceAvPlayerCurrentTime(AvPlayerHandle handle);
s32 PS4_SYSV_ABI sceAvPlayerDisableStream(AvPlayerHandle handle, u32 stream_id);
s32 PS4_SYSV_ABI sceAvPlayerEnableStream(AvPlayerHandle handle, u32 stream_id);
bool PS4_SYSV_ABI sceAvPlayerGetAudioData(AvPlayerHandle handle, AvPlayerFrameInfo* p_info);
s32 PS4_SYSV_ABI sceAvPlayerGetStreamInfo(AvPlayerHandle handle, u32 stream_id,
                                          AvPlayerStreamInfo* p_info);
bool PS4_SYSV_ABI sceAvPlayerGetVideoData(AvPlayerHandle handle, AvPlayerFrameInfo* video_info);
bool PS4_SYSV_ABI sceAvPlayerGetVideoDataEx(AvPlayerHandle handle, AvPlayerFrameInfoEx* video_info);
AvPlayerHandle PS4_SYSV_ABI sceAvPlayerInit(AvPlayerInitData* data);
s32 PS4_SYSV_ABI sceAvPlayerInitEx(const AvPlayerInitDataEx* p_data, AvPlayerHandle* p_player);
bool PS4_SYSV_ABI sceAvPlayerIsActive(AvPlayerHandle handle);
s32 PS4_SYSV_ABI sceAvPlayerJumpToTime(AvPlayerHandle handle, uint64_t time);
s32 PS4_SYSV_ABI sceAvPlayerPause(AvPlayerHandle handle);
s32 PS4_SYSV_ABI sceAvPlayerPostInit(AvPlayerHandle handle, AvPlayerPostInitData* data);
s32 PS4_SYSV_ABI sceAvPlayerPrintf(const char* format, ...);
s32 PS4_SYSV_ABI sceAvPlayerResume(AvPlayerHandle handle);
s32 PS4_SYSV_ABI sceAvPlayerSetAvSyncMode(AvPlayerHandle handle, s32 sync_mode);
s32 PS4_SYSV_ABI sceAvPlayerSetLogCallback(AvPlayerLogCallback log_cb, void* user_data);
s32 PS4_SYSV_ABI sceAvPlayerSetLooping(AvPlayerHandle handle, bool loop_flag);
s32 PS4_SYSV_ABI sceAvPlayerSetTrickSpeed(AvPlayerHandle handle, s32 trick_speed);
s32 PS4_SYSV_ABI sceAvPlayerStart(AvPlayerHandle handle);
s32 PS4_SYSV_ABI sceAvPlayerStop(AvPlayerHandle handle);
s32 PS4_SYSV_ABI sceAvPlayerStreamCount(AvPlayerHandle handle);
s32 PS4_SYSV_ABI sceAvPlayerVprintf(const char* format, va_list args);

RETRO_END_DECLS

#endif
