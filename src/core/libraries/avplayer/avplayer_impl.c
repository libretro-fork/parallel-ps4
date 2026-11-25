/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdlib.h>
#include <string.h>

#include "common/c_bridge.h"
#include "core/libraries/avplayer/avplayer_common.h"
#include "core/libraries/avplayer/avplayer_error.h"
#include "core/libraries/avplayer/avplayer_impl.h"

/* Memory Replacement */
static void* PS4_SYSV_ABI Allocate(void* handle, u32 alignment, u32 size) {
    const struct AvPlayer* const self = (const struct AvPlayer*)handle;
    return self->init_data_original.memory_replacement.allocate(
        self->init_data_original.memory_replacement.object_ptr, alignment, size);
}

static void PS4_SYSV_ABI Deallocate(void* handle, void* memory) {
    const struct AvPlayer* const self = (const struct AvPlayer*)handle;
    self->init_data_original.memory_replacement.deallocate(
        self->init_data_original.memory_replacement.object_ptr, memory);
}

static void* PS4_SYSV_ABI AllocateTexture(void* handle, u32 alignment, u32 size) {
    const struct AvPlayer* const self = (const struct AvPlayer*)handle;
    return self->init_data_original.memory_replacement.allocate_texture(
        self->init_data_original.memory_replacement.object_ptr, alignment, size);
}

static void PS4_SYSV_ABI DeallocateTexture(void* handle, void* memory) {
    const struct AvPlayer* const self = (const struct AvPlayer*)handle;
    self->init_data_original.memory_replacement.deallocate_texture(
        self->init_data_original.memory_replacement.object_ptr, memory);
}

/* File Replacement */
static s32 PS4_SYSV_ABI OpenFile(void* handle, const char* filename) {
    const struct AvPlayer* const self = (const struct AvPlayer*)handle;
    return self->init_data_original.file_replacement.open(
        self->init_data_original.file_replacement.object_ptr, filename);
}

static s32 PS4_SYSV_ABI CloseFile(void* handle) {
    const struct AvPlayer* const self = (const struct AvPlayer*)handle;
    return self->init_data_original.file_replacement.close(
        self->init_data_original.file_replacement.object_ptr);
}

static s32 PS4_SYSV_ABI ReadOffsetFile(void* handle, u8* buffer, u64 position, u32 length) {
    const struct AvPlayer* const self = (const struct AvPlayer*)handle;
    return self->init_data_original.file_replacement.read_offset(
        self->init_data_original.file_replacement.object_ptr, buffer, position, length);
}

static u64 PS4_SYSV_ABI SizeFile(void* handle) {
    const struct AvPlayer* const self = (const struct AvPlayer*)handle;
    return self->init_data_original.file_replacement.size(
        self->init_data_original.file_replacement.object_ptr);
}

static void avplayer_stub_init_data(struct AvPlayer* player, const AvPlayerInitData* data,
                                    AvPlayerInitData* result) {
    *result = *data;
    result->memory_replacement.object_ptr = player;
    result->memory_replacement.allocate = Allocate;
    result->memory_replacement.deallocate = Deallocate;
    result->memory_replacement.allocate_texture = AllocateTexture;
    result->memory_replacement.deallocate_texture = DeallocateTexture;
    if (data->file_replacement.open == NULL || data->file_replacement.close == NULL ||
        data->file_replacement.read_offset == NULL || data->file_replacement.size == NULL) {
        memset(&result->file_replacement, 0, sizeof(result->file_replacement));
    } else {
        result->file_replacement.object_ptr = player;
        result->file_replacement.open = OpenFile;
        result->file_replacement.close = CloseFile;
        result->file_replacement.read_offset = ReadOffsetFile;
        result->file_replacement.size = SizeFile;
    }
}

struct AvPlayer* avplayer_new(const AvPlayerInitData* data) {
    struct AvPlayer* player = (struct AvPlayer*)calloc(1, sizeof(*player));
    if (!player) {
        return NULL;
    }
    avplayer_stub_init_data(player, data, &player->init_data);
    player->init_data_original = *data;
    avplayer_state_init(&player->state, &player->init_data);
    return player;
}

void avplayer_delete(struct AvPlayer* player) {
    avplayer_state_deinit(&player->state);
    free(player);
}

s32 avplayer_post_init(struct AvPlayer* player, const AvPlayerPostInitData* data) {
    avplayer_state_post_init(&player->state, data);
    return ORBIS_OK;
}

s32 avplayer_add_source(struct AvPlayer* player, const char* path, size_t length) {
    return avplayer_add_source_ex(player, path, length, AV_PLAYER_SOURCE_TYPE_UNKNOWN);
}

s32 avplayer_add_source_ex(struct AvPlayer* player, const char* path, size_t length,
                           s32 source_type) {
    if (source_type == AV_PLAYER_SOURCE_TYPE_UNKNOWN) {
        source_type = avplayer_get_source_type(path, length);
    }
    if (source_type == AV_PLAYER_SOURCE_TYPE_HLS) {
        c_log(C_LOG_LIB_AV_PLAYER, C_LOG_LEVEL_ERROR, "AddSourceEx",
              "HTTP Live Streaming is not implemented");
        return ORBIS_AVPLAYER_ERROR_NOT_SUPPORTED;
    }
    if (!avplayer_state_add_source(&player->state, path, length,
                                   avplayer_get_source_type(path, length))) {
        return ORBIS_AVPLAYER_ERROR_OPERATION_FAILED;
    }
    return ORBIS_OK;
}

s32 avplayer_get_stream_count(struct AvPlayer* player) {
    const s32 res = avplayer_state_get_stream_count(&player->state);
    if (AVPLAYER_IS_ERROR(res)) {
        return ORBIS_AVPLAYER_ERROR_OPERATION_FAILED;
    }
    return res;
}

s32 avplayer_get_stream_info(struct AvPlayer* player, u32 stream_index, AvPlayerStreamInfo* info) {
    if (!avplayer_state_get_stream_info(&player->state, stream_index, info)) {
        return ORBIS_AVPLAYER_ERROR_OPERATION_FAILED;
    }
    return ORBIS_OK;
}

s32 avplayer_enable_stream(struct AvPlayer* player, u32 stream_index) {
    if (!avplayer_state_enable_stream(&player->state, stream_index)) {
        return ORBIS_AVPLAYER_ERROR_OPERATION_FAILED;
    }
    return ORBIS_OK;
}

s32 avplayer_start(struct AvPlayer* player) {
    if (!avplayer_state_start(&player->state)) {
        return ORBIS_AVPLAYER_ERROR_OPERATION_FAILED;
    }
    return ORBIS_OK;
}

s32 avplayer_pause(struct AvPlayer* player) {
    if (!avplayer_state_pause(&player->state)) {
        return ORBIS_AVPLAYER_ERROR_OPERATION_FAILED;
    }
    return ORBIS_OK;
}

s32 avplayer_resume(struct AvPlayer* player) {
    if (!avplayer_state_resume(&player->state)) {
        return ORBIS_AVPLAYER_ERROR_OPERATION_FAILED;
    }
    return ORBIS_OK;
}

s32 avplayer_set_av_sync_mode(struct AvPlayer* player, s32 sync_mode) {
    avplayer_state_set_av_sync_mode(&player->state, sync_mode);
    return ORBIS_OK;
}

bool avplayer_get_video_data(struct AvPlayer* player, AvPlayerFrameInfo* video_info) {
    return avplayer_state_get_video_data(&player->state, video_info);
}

bool avplayer_get_video_data_ex(struct AvPlayer* player, AvPlayerFrameInfoEx* video_info) {
    return avplayer_state_get_video_data_ex(&player->state, video_info);
}

bool avplayer_get_audio_data(struct AvPlayer* player, AvPlayerFrameInfo* audio_info) {
    return avplayer_state_get_audio_data(&player->state, audio_info);
}

bool avplayer_is_active(struct AvPlayer* player) {
    return avplayer_state_is_active(&player->state);
}

u64 avplayer_current_time(struct AvPlayer* player) {
    return avplayer_state_current_time(&player->state);
}

s32 avplayer_stop(struct AvPlayer* player) {
    if (!avplayer_state_stop(&player->state)) {
        return ORBIS_AVPLAYER_ERROR_OPERATION_FAILED;
    }
    return ORBIS_OK;
}

bool avplayer_set_looping(struct AvPlayer* player, bool is_looping) {
    return avplayer_state_set_looping(&player->state, is_looping);
}
