/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_VIDEO_RECORDING_H
#define SHADPS4_LIBRARIES_VIDEO_RECORDING_H

/* libSceVideoRecording. Registered in libs.cpp. */

#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

typedef struct OrbisVideoRecordingParam2 {
    u64 size;
    u64 affinity_mask;
    s32 thread_priority;
    s32 ring_sec;
} OrbisVideoRecordingParam2;

typedef struct OrbisVideoRecordingInfoUserMeta {
    u64 size;
    s32 flags;
    char name[32];
    char data[128];
} OrbisVideoRecordingInfoUserMeta;

typedef struct OrbisVideoRecordingInfoGuardArea {
    float x1, y1;
    float x2, y2;
} OrbisVideoRecordingInfoGuardArea;

enum OrbisVideoRecordingStatus {
    ORBIS_VIDEO_RECORDING_STATUS_NONE = 0,
    ORBIS_VIDEO_RECORDING_STATUS_RUNNING = 1,
    ORBIS_VIDEO_RECORDING_STATUS_PAUSED = 2
};

enum OrbisVideoRecordingInfo {
    ORBIS_VIDEO_RECORDING_INFO_SUBTITLE = 0x2,
    ORBIS_VIDEO_RECORDING_INFO_DESCRIPTION = 0x6,
    ORBIS_VIDEO_RECORDING_INFO_COMMENTS = 0x7,
    ORBIS_VIDEO_RECORDING_INFO_KEYWORDS = 0x8,
    ORBIS_VIDEO_RECORDING_INFO_CHAPTER = 0xd,
    ORBIS_VIDEO_RECORDING_INFO_COPYRIGHT = 0xa01,
    ORBIS_VIDEO_RECORDING_INFO_PERMISSION_LEVEL = 0xa007,
    ORBIS_VIDEO_RECORDING_INFO_GUARD_AREA = 0xa008,
    ORBIS_VIDEO_RECORDING_INFO_USER_META = 0xa009
};

enum OrbisVideoRecordingChapter {
    ORBIS_VIDEO_RECORDING_CHAPTER_CHANGE = 0,
    ORBIS_VIDEO_RECORDING_CHAPTER_PROHIBIT = 1
};

enum OrbisVideoRecordingUserMeta {
    ORBIS_VIDEO_RECORDING_USER_META_CONSTANT = 1,
    ORBIS_VIDEO_RECORDING_USER_META_TIMELINE = 2,
    ORBIS_VIDEO_RECORDING_USER_META_VARIABLE = 3,
    ORBIS_VIDEO_RECORDING_USER_META_DEBUG = 0x8000
};

s32 PS4_SYSV_ABI sceVideoRecordingSetInfo(enum OrbisVideoRecordingInfo set_info, const void* info,
                                          u64 info_len);

RETRO_END_DECLS

#endif
