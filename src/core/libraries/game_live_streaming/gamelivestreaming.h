/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_GAME_LIVE_STREAMING_H
#define SHADPS4_LIBRARIES_GAME_LIVE_STREAMING_H

/* libSceGameLiveStreaming. Registered in libs.cpp. */

#include <retro_common_api.h>

#include <boolean.h>
#include "common/types.h"

RETRO_BEGIN_DECLS

typedef struct OrbisGameLiveStreamingStatus {
    bool isOnAir;
    u8 align[3];
    u32 spectatorCounts;
    s32 userId;
    u8 reserved[60];
} OrbisGameLiveStreamingStatus;

typedef struct OrbisGameLiveStreamingStatus2 {
    s32 userId;
    bool isOnAir;
    u8 align[3];
    u32 spectatorCounts;
    u32 textMessageCounts;
    u32 commandMessageCounts;
    u32 broadcastVideoResolution;
    u8 reserved[48];
} OrbisGameLiveStreamingStatus2;

int PS4_SYSV_ABI sceGameLiveStreamingStartDebugBroadcast(void);
int PS4_SYSV_ABI sceGameLiveStreamingStopDebugBroadcast(void);
int PS4_SYSV_ABI sceGameLiveStreamingApplySocialFeedbackMessageFilter(void);
int PS4_SYSV_ABI sceGameLiveStreamingCheckCallback(void);
int PS4_SYSV_ABI sceGameLiveStreamingClearPresetSocialFeedbackCommands(void);
int PS4_SYSV_ABI sceGameLiveStreamingClearSocialFeedbackMessages(void);
int PS4_SYSV_ABI sceGameLiveStreamingClearSpoilerTag(void);
int PS4_SYSV_ABI sceGameLiveStreamingEnableLiveStreaming(void);
int PS4_SYSV_ABI sceGameLiveStreamingEnableSocialFeedback(void);
int PS4_SYSV_ABI sceGameLiveStreamingGetCurrentBroadcastScreenLayout(void);
int PS4_SYSV_ABI sceGameLiveStreamingGetCurrentStatus(OrbisGameLiveStreamingStatus* status);
int PS4_SYSV_ABI sceGameLiveStreamingGetCurrentStatus2(void);
int PS4_SYSV_ABI sceGameLiveStreamingGetProgramInfo(void);
int PS4_SYSV_ABI sceGameLiveStreamingGetSocialFeedbackMessages(void);
int PS4_SYSV_ABI sceGameLiveStreamingGetSocialFeedbackMessagesCount(void);
int PS4_SYSV_ABI sceGameLiveStreamingInitialize(void);
int PS4_SYSV_ABI sceGameLiveStreamingLaunchLiveViewer(void);
int PS4_SYSV_ABI sceGameLiveStreamingLaunchLiveViewerA(void);
int PS4_SYSV_ABI sceGameLiveStreamingPermitLiveStreaming(void);
int PS4_SYSV_ABI sceGameLiveStreamingPermitServerSideRecording(void);
int PS4_SYSV_ABI sceGameLiveStreamingPostSocialMessage(void);
int PS4_SYSV_ABI sceGameLiveStreamingRegisterCallback(void);
int PS4_SYSV_ABI sceGameLiveStreamingScreenCloseSeparateMode(void);
int PS4_SYSV_ABI sceGameLiveStreamingScreenConfigureSeparateMode(void);
int PS4_SYSV_ABI sceGameLiveStreamingScreenInitialize(void);
int PS4_SYSV_ABI sceGameLiveStreamingScreenInitializeSeparateModeParameter(void);
int PS4_SYSV_ABI sceGameLiveStreamingScreenOpenSeparateMode(void);
int PS4_SYSV_ABI sceGameLiveStreamingScreenSetMode(void);
int PS4_SYSV_ABI sceGameLiveStreamingScreenTerminate(void);
int PS4_SYSV_ABI sceGameLiveStreamingSetCameraFrameSetting(void);
int PS4_SYSV_ABI sceGameLiveStreamingSetDefaultServiceProviderPermission(void);
int PS4_SYSV_ABI sceGameLiveStreamingSetGuardAreas(void);
int PS4_SYSV_ABI sceGameLiveStreamingSetInvitationSessionId(void);
int PS4_SYSV_ABI sceGameLiveStreamingSetLinkCommentPreset(void);
int PS4_SYSV_ABI sceGameLiveStreamingSetMaxBitrate(void);
int PS4_SYSV_ABI sceGameLiveStreamingSetMetadata(void);
int PS4_SYSV_ABI sceGameLiveStreamingSetPresetSocialFeedbackCommands(void);
int PS4_SYSV_ABI sceGameLiveStreamingSetPresetSocialFeedbackCommandsDescription(void);
int PS4_SYSV_ABI sceGameLiveStreamingSetServiceProviderPermission(void);
int PS4_SYSV_ABI sceGameLiveStreamingSetSpoilerTag(void);
int PS4_SYSV_ABI sceGameLiveStreamingSetStandbyScreenResource(void);
int PS4_SYSV_ABI sceGameLiveStreamingStartGenerateStandbyScreenResource(void);
int PS4_SYSV_ABI sceGameLiveStreamingStartSocialFeedbackMessageFiltering(void);
int PS4_SYSV_ABI sceGameLiveStreamingStopGenerateStandbyScreenResource(void);
int PS4_SYSV_ABI sceGameLiveStreamingStopSocialFeedbackMessageFiltering(void);
int PS4_SYSV_ABI sceGameLiveStreamingTerminate(void);
int PS4_SYSV_ABI sceGameLiveStreamingUnregisterCallback(void);

RETRO_END_DECLS

#endif
