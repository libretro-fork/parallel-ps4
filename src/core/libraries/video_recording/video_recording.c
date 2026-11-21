/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "common/c_bridge.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/video_recording/video_recording.h"
#include "core/libraries/video_recording/video_recording_error.h"

s32 PS4_SYSV_ABI sceVideoRecordingSetInfo(enum OrbisVideoRecordingInfo set_info, const void* info,
                                          u64 info_len) {
    c_log(C_LOG_LIB_VIDEO_RECORDING, C_LOG_LEVEL_ERROR, "sceVideoRecordingSetInfo",
          "(STUBBED) called, set_info = 0x%x, info_len = 0x%llx", (u32)set_info,
          (unsigned long long)info_len);
    if (set_info < ORBIS_VIDEO_RECORDING_INFO_SUBTITLE ||
        (set_info > ORBIS_VIDEO_RECORDING_INFO_SUBTITLE &&
         set_info < ORBIS_VIDEO_RECORDING_INFO_DESCRIPTION) ||
        (set_info > ORBIS_VIDEO_RECORDING_INFO_KEYWORDS &&
         set_info < ORBIS_VIDEO_RECORDING_INFO_CHAPTER) ||
        (set_info > ORBIS_VIDEO_RECORDING_INFO_CHAPTER &&
         set_info < ORBIS_VIDEO_RECORDING_INFO_COPYRIGHT) ||
        (set_info > ORBIS_VIDEO_RECORDING_INFO_CHAPTER &&
         set_info < ORBIS_VIDEO_RECORDING_INFO_PERMISSION_LEVEL) ||
        set_info > ORBIS_VIDEO_RECORDING_INFO_USER_META) {
        return ORBIS_VIDEO_RECORDING_ERROR_INVALID_VALUE;
    }

    /* The library seems to rely on libSceAvcap to provide errors for other parameters. */
    if (!info) {
        return ORBIS_VIDEO_RECORDING_ERROR_INVALID_VALUE;
    }
    return ORBIS_OK;
}
