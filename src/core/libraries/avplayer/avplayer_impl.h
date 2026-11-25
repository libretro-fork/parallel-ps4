/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_AVPLAYER_AVPLAYER_IMPL_H
#define SHADPS4_AVPLAYER_AVPLAYER_IMPL_H

/* A player the game holds: its init data, with its allocator and file replacement called
 * through the player, and its state. Defined in avplayer_impl.c. */

#include <stddef.h>

#include <boolean.h>
#include <retro_common_api.h>

#include "core/libraries/avplayer/avplayer.h"
#include "core/libraries/avplayer/avplayer_state.h"

RETRO_BEGIN_DECLS

struct AvPlayer {
    AvPlayerInitData init_data;
    AvPlayerInitData init_data_original;
    AvPlayerState state;
};

/* NULL if it cannot be made */
struct AvPlayer* avplayer_new(const AvPlayerInitData* data);
void avplayer_delete(struct AvPlayer* player);
s32 avplayer_post_init(struct AvPlayer* player, const AvPlayerPostInitData* data);
s32 avplayer_add_source(struct AvPlayer* player, const char* path, size_t length);
s32 avplayer_add_source_ex(struct AvPlayer* player, const char* path, size_t length,
                           s32 source_type);
s32 avplayer_get_stream_count(struct AvPlayer* player);
s32 avplayer_get_stream_info(struct AvPlayer* player, u32 stream_index, AvPlayerStreamInfo* info);
s32 avplayer_enable_stream(struct AvPlayer* player, u32 stream_index);
s32 avplayer_start(struct AvPlayer* player);
s32 avplayer_pause(struct AvPlayer* player);
s32 avplayer_resume(struct AvPlayer* player);
s32 avplayer_set_av_sync_mode(struct AvPlayer* player, s32 sync_mode);
bool avplayer_get_audio_data(struct AvPlayer* player, AvPlayerFrameInfo* audio_info);
bool avplayer_get_video_data(struct AvPlayer* player, AvPlayerFrameInfo* video_info);
bool avplayer_get_video_data_ex(struct AvPlayer* player, AvPlayerFrameInfoEx* video_info);
bool avplayer_is_active(struct AvPlayer* player);
u64 avplayer_current_time(struct AvPlayer* player);
s32 avplayer_stop(struct AvPlayer* player);
bool avplayer_set_looping(struct AvPlayer* player, bool is_looping);

RETRO_END_DECLS

#endif
