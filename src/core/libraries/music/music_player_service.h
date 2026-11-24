/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_MUSIC_PLAYER_SERVICE_H
#define SHADPS4_MUSIC_PLAYER_SERVICE_H

/* libSceMusicPlayerService: the game's track lists, played into the core's mix by a thread of
 * its own. MSVC C89. */

#include <stddef.h>

#include <boolean.h>
#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

/* Error codes for module 0x12f (libSceMusicPlayerService) */
#define ORBIS_MUSIC_PLAYER_SERVICE_ERROR_NOT_INITIALIZED ((s32)0x812f0001)
#define ORBIS_MUSIC_PLAYER_SERVICE_ERROR_ALREADY_INITIALIZED ((s32)0x812f0002)
#define ORBIS_MUSIC_PLAYER_SERVICE_ERROR_INVALID_PARAMETER ((s32)0x812f0004)
#define ORBIS_MUSIC_PLAYER_SERVICE_ERROR_ALREADY_IN_TRANSACTION ((s32)0x812f0101)
#define ORBIS_MUSIC_PLAYER_SERVICE_ERROR_NOT_IN_TRANSACTION ((s32)0x812f0102)
#define ORBIS_MUSIC_PLAYER_SERVICE_ERROR_TIMED_OUT ((s32)0x812f010a)

typedef struct OrbisMusicPlayerPlayStatusExtension {
    u64 size;              /* +0x00 (0x48) */
    u32 error_code;        /* +0x08 (0: ok, non-zero: error) */
    u32 play_state;        /* +0x0c (0: playing, 1: paused, 2: stopped) */
    u64 total_duration_s;  /* +0x10 (total duration in seconds) */
    u64 current_time_s;    /* +0x18 (position in seconds) */
    u32 tracklist_id;      /* +0x20 */
    u32 track_index;       /* +0x24 (0-based current track index) */
    u32 play_index;        /* +0x28 (0-based current playback sequence index) */
    u32 total_track_count; /* +0x2c (total tracks in tracklist) */
    u32 volume;            /* +0x30 (0 - 25) */
    u32 repeat_mode;       /* +0x34 (0: off, 1: all, 2: one) */
    u32 shuffle_mode;      /* +0x38 (0: off, 1: on) */
    u32 can_skip_next;     /* +0x3c (0: can skip next, 1: cannot) */
    u32 can_skip_previous; /* +0x40 (0: can skip previous, 1: cannot) */
    u32 is_loading;        /* +0x44 (0: not loading) */
} OrbisMusicPlayerPlayStatusExtension;

typedef struct SceMusicPlayerServicePlayStatus {
    u32 result;        /* +0x00 (0: ok) */
    u32 play_status;   /* +0x04 (0: stopped, 1: playing, 2: paused) */
    u32 track_index;   /* +0x08 (0-based current track index) */
    u32 tracklist_id;  /* +0x0c */
    u64 playback_time; /* +0x10 (position in seconds) */
    u32 shuffle;       /* +0x18 (0: off, 1: on) */
    u32 repeat;        /* +0x1c (0: off, 1: all, 2: one) */
} SceMusicPlayerServicePlayStatus;

typedef struct OrbisMusicPlayerUsbDeviceInfo {
    u32 device_id;           /* +0x00 */
    char serial_number[256]; /* +0x04 */
    u8 pad0[4];              /* +0x104 */
    u64 serial_number_len;   /* +0x108 */
} OrbisMusicPlayerUsbDeviceInfo;

/* The layouts the game passes, checked when built */
typedef char music_player_status_ext_size_check[sizeof(OrbisMusicPlayerPlayStatusExtension) == 0x48
                                                    ? 1
                                                    : -1];
typedef char
    music_player_status_size_check[sizeof(SceMusicPlayerServicePlayStatus) == 0x20 ? 1 : -1];
typedef char music_player_usb_serial_check
    [offsetof(OrbisMusicPlayerUsbDeviceInfo, serial_number) == 0x04 ? 1 : -1];
typedef char music_player_usb_len_check
    [offsetof(OrbisMusicPlayerUsbDeviceInfo, serial_number_len) == 0x108 ? 1 : -1];
typedef char music_player_usb_size_check[sizeof(OrbisMusicPlayerUsbDeviceInfo) == 0x110 ? 1 : -1];

/* The player made, once, before the game calls it (libs.cpp registers the library) */
void music_player_library_init(void);

/* Whether the player is playing or paused (the system service's BGM state) */
bool music_player_is_bgm_playing(void);

s32 PS4_SYSV_ABI sceMusicPlayerServiceInitialize(size_t pool_size, void* pool_addr);
s32 PS4_SYSV_ABI sceMusicPlayerServiceInitialize2(size_t pool_size, void* pool_addr);
s32 PS4_SYSV_ABI sceMusicPlayerServiceInitialize3(size_t pool_size, void* pool_addr, u64 unk1,
                                                  u64 unk2);
s32 PS4_SYSV_ABI sceMusicPlayerServiceTerminate(void);
s32 PS4_SYSV_ABI sceMusicPlayerServiceSetUsbStorageDeviceInfo(
    const OrbisMusicPlayerUsbDeviceInfo* devices, u32 count);
s32 PS4_SYSV_ABI sceMusicPlayerServiceRemoveAllData(s32 player_id);
s32 PS4_SYSV_ABI sceMusicPlayerServiceSetTrackList(s32 player_id, const void* tracks, u32 count);
s32 PS4_SYSV_ABI sceMusicPlayerServiceSetTrackList2(s32 player_id, const void* tracks, u32 count,
                                                    u32 flags);
s32 PS4_SYSV_ABI sceMusicPlayerServicePlay(s32 player_id, u32 track_index, s64 start_time_s,
                                           s32 flags);
s32 PS4_SYSV_ABI sceMusicPlayerServicePlayStartByTime(s32 player_id, u32 track_index,
                                                      s64 start_time_s);
s32 PS4_SYSV_ABI sceMusicPlayerServiceStop(void);
s32 PS4_SYSV_ABI sceMusicPlayerServicePause(void);
s32 PS4_SYSV_ABI sceMusicPlayerServiceUnpause(void);
s32 PS4_SYSV_ABI sceMusicPlayerServiceNextTrack(void);
s32 PS4_SYSV_ABI sceMusicPlayerServicePreviousTrack(void);
s32 PS4_SYSV_ABI sceMusicPlayerServiceSeek(s32 player_id, u32 track_index, s64 time_s);
s32 PS4_SYSV_ABI sceMusicPlayerServiceSetAudioVolume(s32 volume);
s32 PS4_SYSV_ABI sceMusicPlayerServiceGetAudioVolume(s32* volume, s32* mute);
s32 PS4_SYSV_ABI sceMusicPlayerServiceSetShuffle(u32 mode);
s32 PS4_SYSV_ABI sceMusicPlayerServiceSetRepeatMode(u32 mode);
s32 PS4_SYSV_ABI
sceMusicPlayerServiceGetPlayStatusExtension(OrbisMusicPlayerPlayStatusExtension* status);
s32 PS4_SYSV_ABI sceMusicPlayerServiceGetCurrentPlayStatus(SceMusicPlayerServicePlayStatus* status);
s32 PS4_SYSV_ABI sceMusicPlayerServiceReceiveEvent(void* buffer);
s32 PS4_SYSV_ABI sceMusicPlayerServiceGetPlaybackResults(void* a, void* b);
s32 PS4_SYSV_ABI sceMusicPlayerServiceGetTrackInfo(void* a, void* b);
s32 PS4_SYSV_ABI sceMusicPlayerServiceBeginTransaction(s32 a);
s32 PS4_SYSV_ABI sceMusicPlayerServiceEndTransaction(s32 a);
s32 PS4_SYSV_ABI sceMusicPlayerServiceCreateTrackList(u32 tracklist_id);
s32 PS4_SYSV_ABI sceMusicPlayerServiceRemoveTrackList(u32 tracklist_id);
s32 PS4_SYSV_ABI sceMusicPlayerServiceInsertTrackEntry(u32 tracklist_id, u32 track_index,
                                                       const void* track_entry);
s32 PS4_SYSV_ABI sceMusicPlayerServiceRemoveTrackEntry(u32 tracklist_id, u32 track_index);
s32 PS4_SYSV_ABI sceMusicPlayerServiceGetTrackListVersion(s32 player_id, u64* version);

RETRO_END_DECLS

#endif
