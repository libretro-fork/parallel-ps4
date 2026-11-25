/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <string.h>

#include "common/c_bridge.h"
#include "core/libraries/avplayer/avplayer.h"
#include "core/libraries/avplayer/avplayer_error.h"
#include "core/libraries/avplayer/avplayer_impl.h"

s32 PS4_SYSV_ABI sceAvPlayerAddSource(AvPlayerHandle handle, const char* filename) {
#ifndef NDEBUG
    c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_TRACE, "sceAvPlayerAddSource", "filename = %s",
          filename);
#endif
    if (handle == NULL) {
        return ORBIS_AVPLAYER_ERROR_INVALID_PARAMS;
    }
    return avplayer_add_source(handle, filename, strlen(filename));
}

s32 PS4_SYSV_ABI sceAvPlayerAddSourceEx(AvPlayerHandle handle, u32 uri_type,
                                        AvPlayerSourceDetails* source_details) {
#ifndef NDEBUG
    c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_TRACE, "sceAvPlayerAddSourceEx", "called");
#endif
    if (handle == NULL || uri_type != AV_PLAYER_URI_TYPE_SOURCE || source_details == NULL) {
        return ORBIS_AVPLAYER_ERROR_INVALID_PARAMS;
    }
    return avplayer_add_source_ex(handle, source_details->uri.name, source_details->uri.length,
                                  source_details->source_type);
}

int PS4_SYSV_ABI sceAvPlayerChangeStream(void) {
    c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_ERROR, "sceAvPlayerChangeStream", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAvPlayerClose(AvPlayerHandle handle) {
#ifndef NDEBUG
    c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_TRACE, "sceAvPlayerClose", "called");
#endif
    if (handle == NULL) {
        return ORBIS_AVPLAYER_ERROR_INVALID_PARAMS;
    }
    avplayer_delete(handle);
    return ORBIS_OK;
}

u64 PS4_SYSV_ABI sceAvPlayerCurrentTime(AvPlayerHandle handle) {
#ifndef NDEBUG
    c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_TRACE, "sceAvPlayerCurrentTime", "called");
#endif
    if (handle == NULL) {
        return (u64)(s64)ORBIS_AVPLAYER_ERROR_INVALID_PARAMS;
    }
    return avplayer_current_time(handle);
}

s32 PS4_SYSV_ABI sceAvPlayerDisableStream(AvPlayerHandle handle, u32 stream_id) {
    (void)stream_id;
    c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_ERROR, "sceAvPlayerDisableStream", "(STUBBED) called");
    if (handle == NULL) {
        return ORBIS_AVPLAYER_ERROR_INVALID_PARAMS;
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAvPlayerEnableStream(AvPlayerHandle handle, u32 stream_id) {
#ifndef NDEBUG
    c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_TRACE, "sceAvPlayerEnableStream", "stream_id = %u",
          stream_id);
#endif
    if (handle == NULL) {
        return ORBIS_AVPLAYER_ERROR_INVALID_PARAMS;
    }
    return avplayer_enable_stream(handle, stream_id);
}

bool PS4_SYSV_ABI sceAvPlayerGetAudioData(AvPlayerHandle handle, AvPlayerFrameInfo* p_info) {
#ifndef NDEBUG
    c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_TRACE, "sceAvPlayerGetAudioData", "called");
#endif
    if (handle == NULL || p_info == NULL) {
        return false;
    }
    return avplayer_get_audio_data(handle, p_info);
}

s32 PS4_SYSV_ABI sceAvPlayerGetStreamInfo(AvPlayerHandle handle, u32 stream_id,
                                          AvPlayerStreamInfo* p_info) {
#ifndef NDEBUG
    c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_TRACE, "sceAvPlayerGetStreamInfo", "stream_id = %u",
          stream_id);
#endif
    if (handle == NULL || p_info == NULL) {
        return ORBIS_AVPLAYER_ERROR_INVALID_PARAMS;
    }
    return avplayer_get_stream_info(handle, stream_id, p_info);
}

bool PS4_SYSV_ABI sceAvPlayerGetVideoData(AvPlayerHandle handle, AvPlayerFrameInfo* video_info) {
#ifndef NDEBUG
    c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_TRACE, "sceAvPlayerGetVideoData", "called");
#endif
    if (handle == NULL || video_info == NULL) {
        return false;
    }
    return avplayer_get_video_data(handle, video_info);
}

bool PS4_SYSV_ABI sceAvPlayerGetVideoDataEx(AvPlayerHandle handle,
                                            AvPlayerFrameInfoEx* video_info) {
#ifndef NDEBUG
    c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_TRACE, "sceAvPlayerGetVideoDataEx", "called");
#endif
    if (handle == NULL || video_info == NULL) {
        return false;
    }
    return avplayer_get_video_data_ex(handle, video_info);
}

static bool avplayer_has_allocators(const AvPlayerMemAllocator* memory) {
    return memory->allocate != NULL && memory->allocate_texture != NULL &&
           memory->deallocate != NULL && memory->deallocate_texture != NULL;
}

AvPlayerHandle PS4_SYSV_ABI sceAvPlayerInit(AvPlayerInitData* data) {
#ifndef NDEBUG
    c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_TRACE, "sceAvPlayerInit", "called");
#endif
    if (data == NULL) {
        return NULL;
    }
    if (!avplayer_has_allocators(&data->memory_replacement)) {
        c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_ERROR, "sceAvPlayerInit",
              "All allocators are required for AvPlayer Initialisation.");
        return NULL;
    }
    return avplayer_new(data);
}

s32 PS4_SYSV_ABI sceAvPlayerInitEx(const AvPlayerInitDataEx* p_data, AvPlayerHandle* p_player) {
    AvPlayerInitData data;
#ifndef NDEBUG
    c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_TRACE, "sceAvPlayerInitEx", "called");
#endif
    if (p_data == NULL || p_player == NULL) {
        return ORBIS_AVPLAYER_ERROR_INVALID_PARAMS;
    }
    if (!avplayer_has_allocators(&p_data->memory_replacement)) {
        c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_ERROR, "sceAvPlayerInitEx",
              "All allocators are required for AvPlayer Initialisation.");
        return ORBIS_AVPLAYER_ERROR_INVALID_PARAMS;
    }
    memset(&data, 0, sizeof(data));
    data.memory_replacement = p_data->memory_replacement;
    data.file_replacement = p_data->file_replacement;
    data.event_replacement = p_data->event_replacement;
    data.default_language = p_data->default_language;
    data.num_output_video_framebuffers = p_data->num_output_video_framebuffers;
    data.auto_start = p_data->auto_start;
    *p_player = avplayer_new(&data);
    if (*p_player == NULL) {
        return ORBIS_AVPLAYER_ERROR_NO_MEMORY;
    }
    return ORBIS_OK;
}

bool PS4_SYSV_ABI sceAvPlayerIsActive(AvPlayerHandle handle) {
#ifndef NDEBUG
    c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_TRACE, "sceAvPlayerIsActive", "called");
#endif
    if (handle == NULL) {
        return false;
    }
    return avplayer_is_active(handle);
}

s32 PS4_SYSV_ABI sceAvPlayerJumpToTime(AvPlayerHandle handle, uint64_t time) {
    c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_ERROR, "sceAvPlayerJumpToTime",
          "(STUBBED) called, time (msec) = %llu", (unsigned long long)time);
    if (handle == NULL) {
        return ORBIS_AVPLAYER_ERROR_INVALID_PARAMS;
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAvPlayerPause(AvPlayerHandle handle) {
#ifndef NDEBUG
    c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_TRACE, "sceAvPlayerPause", "called");
#endif
    if (handle == NULL) {
        return ORBIS_AVPLAYER_ERROR_INVALID_PARAMS;
    }
    return avplayer_pause(handle);
}

s32 PS4_SYSV_ABI sceAvPlayerPostInit(AvPlayerHandle handle, AvPlayerPostInitData* data) {
#ifndef NDEBUG
    c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_TRACE, "sceAvPlayerPostInit", "called");
#endif
    if (handle == NULL || data == NULL) {
        return ORBIS_AVPLAYER_ERROR_INVALID_PARAMS;
    }
    return avplayer_post_init(handle, data);
}

s32 PS4_SYSV_ABI sceAvPlayerPrintf(const char* format, ...) {
    (void)format;
    c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_ERROR, "sceAvPlayerPrintf", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAvPlayerResume(AvPlayerHandle handle) {
#ifndef NDEBUG
    c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_TRACE, "sceAvPlayerResume", "called");
#endif
    if (handle == NULL) {
        return ORBIS_AVPLAYER_ERROR_INVALID_PARAMS;
    }
    return avplayer_resume(handle);
}

s32 PS4_SYSV_ABI sceAvPlayerSetAvSyncMode(AvPlayerHandle handle, s32 sync_mode) {
#ifndef NDEBUG
    c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_TRACE, "sceAvPlayerSetAvSyncMode", "called");
#endif
    if (handle == NULL) {
        return ORBIS_AVPLAYER_ERROR_INVALID_PARAMS;
    }
    return avplayer_set_av_sync_mode(handle, sync_mode);
}

s32 PS4_SYSV_ABI sceAvPlayerSetLogCallback(AvPlayerLogCallback log_cb, void* user_data) {
    (void)log_cb;
    (void)user_data;
    c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_ERROR, "sceAvPlayerSetLogCallback", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAvPlayerSetLooping(AvPlayerHandle handle, bool loop_flag) {
#ifndef NDEBUG
    c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_TRACE, "sceAvPlayerSetLooping", "called, looping = %s",
          loop_flag ? "true" : "false");
#endif
    if (handle == NULL) {
        return ORBIS_AVPLAYER_ERROR_INVALID_PARAMS;
    }
    if (!avplayer_set_looping(handle, loop_flag)) {
        return ORBIS_AVPLAYER_ERROR_OPERATION_FAILED;
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAvPlayerSetTrickSpeed(AvPlayerHandle handle, s32 trick_speed) {
    c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_ERROR, "sceAvPlayerSetTrickSpeed",
          "(STUBBED) called speed = %d", trick_speed);
    if (handle == NULL) {
        return ORBIS_AVPLAYER_ERROR_INVALID_PARAMS;
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAvPlayerStart(AvPlayerHandle handle) {
#ifndef NDEBUG
    c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_TRACE, "sceAvPlayerStart", "called");
#endif
    if (handle == NULL) {
        return ORBIS_AVPLAYER_ERROR_INVALID_PARAMS;
    }
    return avplayer_start(handle);
}

s32 PS4_SYSV_ABI sceAvPlayerStop(AvPlayerHandle handle) {
#ifndef NDEBUG
    c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_TRACE, "sceAvPlayerStop", "called");
#endif
    if (handle == NULL) {
        return ORBIS_AVPLAYER_ERROR_INVALID_PARAMS;
    }
    return avplayer_stop(handle);
}

s32 PS4_SYSV_ABI sceAvPlayerStreamCount(AvPlayerHandle handle) {
#ifndef NDEBUG
    c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_TRACE, "sceAvPlayerStreamCount", "called");
#endif
    if (handle == NULL) {
        return ORBIS_AVPLAYER_ERROR_INVALID_PARAMS;
    }
    return avplayer_get_stream_count(handle);
}

s32 PS4_SYSV_ABI sceAvPlayerVprintf(const char* format, va_list args) {
    (void)format;
    (void)args;
    c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_ERROR, "sceAvPlayerVprintf", "(STUBBED) called");
    return ORBIS_OK;
}
