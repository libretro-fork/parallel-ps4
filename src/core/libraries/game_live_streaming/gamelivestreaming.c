/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <boolean.h>
#include <stddef.h>
#include <string.h>

#include "common/c_bridge.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/game_live_streaming/gamelivestreaming.h"

int PS4_SYSV_ABI sceGameLiveStreamingStartDebugBroadcast(void) {
    c_log(C_LOG_LIB_GAME_LIVE_STREAMING, C_LOG_LEVEL_ERROR,
          "sceGameLiveStreamingStartDebugBroadcast", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGameLiveStreamingStopDebugBroadcast(void) {
    c_log(C_LOG_LIB_GAME_LIVE_STREAMING, C_LOG_LEVEL_ERROR,
          "sceGameLiveStreamingStopDebugBroadcast", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGameLiveStreamingApplySocialFeedbackMessageFilter(void) {
    c_log(C_LOG_LIB_GAME_LIVE_STREAMING, C_LOG_LEVEL_ERROR,
          "sceGameLiveStreamingApplySocialFeedbackMessageFilter", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGameLiveStreamingCheckCallback(void) {
    c_log(C_LOG_LIB_GAME_LIVE_STREAMING, C_LOG_LEVEL_ERROR, "sceGameLiveStreamingCheckCallback",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGameLiveStreamingClearPresetSocialFeedbackCommands(void) {
    c_log(C_LOG_LIB_GAME_LIVE_STREAMING, C_LOG_LEVEL_ERROR,
          "sceGameLiveStreamingClearPresetSocialFeedbackCommands", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGameLiveStreamingClearSocialFeedbackMessages(void) {
    c_log(C_LOG_LIB_GAME_LIVE_STREAMING, C_LOG_LEVEL_ERROR,
          "sceGameLiveStreamingClearSocialFeedbackMessages", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGameLiveStreamingClearSpoilerTag(void) {
    c_log(C_LOG_LIB_GAME_LIVE_STREAMING, C_LOG_LEVEL_ERROR, "sceGameLiveStreamingClearSpoilerTag",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGameLiveStreamingEnableLiveStreaming(void) {
    c_log(C_LOG_LIB_GAME_LIVE_STREAMING, C_LOG_LEVEL_ERROR,
          "sceGameLiveStreamingEnableLiveStreaming", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGameLiveStreamingEnableSocialFeedback(void) {
    c_log(C_LOG_LIB_GAME_LIVE_STREAMING, C_LOG_LEVEL_ERROR,
          "sceGameLiveStreamingEnableSocialFeedback", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGameLiveStreamingGetCurrentBroadcastScreenLayout(void) {
    c_log(C_LOG_LIB_GAME_LIVE_STREAMING, C_LOG_LEVEL_ERROR,
          "sceGameLiveStreamingGetCurrentBroadcastScreenLayout", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGameLiveStreamingGetCurrentStatus(OrbisGameLiveStreamingStatus* status) {
    memset(status, 0, sizeof(*status));
    status->isOnAir = false;
    c_log(C_LOG_LIB_GAME_LIVE_STREAMING, C_LOG_LEVEL_DEBUG, "sceGameLiveStreamingGetCurrentStatus",
          "(STUBBED) called userid = %d", status->userId);
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGameLiveStreamingGetCurrentStatus2(void) {
    c_log(C_LOG_LIB_GAME_LIVE_STREAMING, C_LOG_LEVEL_ERROR, "sceGameLiveStreamingGetCurrentStatus2",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGameLiveStreamingGetProgramInfo(void) {
    c_log(C_LOG_LIB_GAME_LIVE_STREAMING, C_LOG_LEVEL_ERROR, "sceGameLiveStreamingGetProgramInfo",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGameLiveStreamingGetSocialFeedbackMessages(void) {
    c_log(C_LOG_LIB_GAME_LIVE_STREAMING, C_LOG_LEVEL_ERROR,
          "sceGameLiveStreamingGetSocialFeedbackMessages", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGameLiveStreamingGetSocialFeedbackMessagesCount(void) {
    c_log(C_LOG_LIB_GAME_LIVE_STREAMING, C_LOG_LEVEL_ERROR,
          "sceGameLiveStreamingGetSocialFeedbackMessagesCount", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGameLiveStreamingInitialize(void) {
    c_log(C_LOG_LIB_GAME_LIVE_STREAMING, C_LOG_LEVEL_ERROR, "sceGameLiveStreamingInitialize",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGameLiveStreamingLaunchLiveViewer(void) {
    c_log(C_LOG_LIB_GAME_LIVE_STREAMING, C_LOG_LEVEL_ERROR, "sceGameLiveStreamingLaunchLiveViewer",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGameLiveStreamingLaunchLiveViewerA(void) {
    c_log(C_LOG_LIB_GAME_LIVE_STREAMING, C_LOG_LEVEL_ERROR, "sceGameLiveStreamingLaunchLiveViewerA",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGameLiveStreamingPermitLiveStreaming(void) {
    c_log(C_LOG_LIB_GAME_LIVE_STREAMING, C_LOG_LEVEL_ERROR,
          "sceGameLiveStreamingPermitLiveStreaming", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGameLiveStreamingPermitServerSideRecording(void) {
    c_log(C_LOG_LIB_GAME_LIVE_STREAMING, C_LOG_LEVEL_ERROR,
          "sceGameLiveStreamingPermitServerSideRecording", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGameLiveStreamingPostSocialMessage(void) {
    c_log(C_LOG_LIB_GAME_LIVE_STREAMING, C_LOG_LEVEL_ERROR, "sceGameLiveStreamingPostSocialMessage",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGameLiveStreamingRegisterCallback(void) {
    c_log(C_LOG_LIB_GAME_LIVE_STREAMING, C_LOG_LEVEL_ERROR, "sceGameLiveStreamingRegisterCallback",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGameLiveStreamingScreenCloseSeparateMode(void) {
    c_log(C_LOG_LIB_GAME_LIVE_STREAMING, C_LOG_LEVEL_ERROR,
          "sceGameLiveStreamingScreenCloseSeparateMode", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGameLiveStreamingScreenConfigureSeparateMode(void) {
    c_log(C_LOG_LIB_GAME_LIVE_STREAMING, C_LOG_LEVEL_ERROR,
          "sceGameLiveStreamingScreenConfigureSeparateMode", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGameLiveStreamingScreenInitialize(void) {
    c_log(C_LOG_LIB_GAME_LIVE_STREAMING, C_LOG_LEVEL_ERROR, "sceGameLiveStreamingScreenInitialize",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGameLiveStreamingScreenInitializeSeparateModeParameter(void) {
    c_log(C_LOG_LIB_GAME_LIVE_STREAMING, C_LOG_LEVEL_ERROR,
          "sceGameLiveStreamingScreenInitializeSeparateModeParameter", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGameLiveStreamingScreenOpenSeparateMode(void) {
    c_log(C_LOG_LIB_GAME_LIVE_STREAMING, C_LOG_LEVEL_ERROR,
          "sceGameLiveStreamingScreenOpenSeparateMode", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGameLiveStreamingScreenSetMode(void) {
    c_log(C_LOG_LIB_GAME_LIVE_STREAMING, C_LOG_LEVEL_ERROR, "sceGameLiveStreamingScreenSetMode",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGameLiveStreamingScreenTerminate(void) {
    c_log(C_LOG_LIB_GAME_LIVE_STREAMING, C_LOG_LEVEL_ERROR, "sceGameLiveStreamingScreenTerminate",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGameLiveStreamingSetCameraFrameSetting(void) {
    c_log(C_LOG_LIB_GAME_LIVE_STREAMING, C_LOG_LEVEL_ERROR,
          "sceGameLiveStreamingSetCameraFrameSetting", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGameLiveStreamingSetDefaultServiceProviderPermission(void) {
    c_log(C_LOG_LIB_GAME_LIVE_STREAMING, C_LOG_LEVEL_ERROR,
          "sceGameLiveStreamingSetDefaultServiceProviderPermission", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGameLiveStreamingSetGuardAreas(void) {
    c_log(C_LOG_LIB_GAME_LIVE_STREAMING, C_LOG_LEVEL_ERROR, "sceGameLiveStreamingSetGuardAreas",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGameLiveStreamingSetInvitationSessionId(void) {
    c_log(C_LOG_LIB_GAME_LIVE_STREAMING, C_LOG_LEVEL_ERROR,
          "sceGameLiveStreamingSetInvitationSessionId", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGameLiveStreamingSetLinkCommentPreset(void) {
    c_log(C_LOG_LIB_GAME_LIVE_STREAMING, C_LOG_LEVEL_ERROR,
          "sceGameLiveStreamingSetLinkCommentPreset", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGameLiveStreamingSetMaxBitrate(void) {
    c_log(C_LOG_LIB_GAME_LIVE_STREAMING, C_LOG_LEVEL_ERROR, "sceGameLiveStreamingSetMaxBitrate",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGameLiveStreamingSetMetadata(void) {
    c_log(C_LOG_LIB_GAME_LIVE_STREAMING, C_LOG_LEVEL_ERROR, "sceGameLiveStreamingSetMetadata",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGameLiveStreamingSetPresetSocialFeedbackCommands(void) {
    c_log(C_LOG_LIB_GAME_LIVE_STREAMING, C_LOG_LEVEL_ERROR,
          "sceGameLiveStreamingSetPresetSocialFeedbackCommands", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGameLiveStreamingSetPresetSocialFeedbackCommandsDescription(void) {
    c_log(C_LOG_LIB_GAME_LIVE_STREAMING, C_LOG_LEVEL_ERROR,
          "sceGameLiveStreamingSetPresetSocialFeedbackCommandsDescription", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGameLiveStreamingSetServiceProviderPermission(void) {
    c_log(C_LOG_LIB_GAME_LIVE_STREAMING, C_LOG_LEVEL_ERROR,
          "sceGameLiveStreamingSetServiceProviderPermission", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGameLiveStreamingSetSpoilerTag(void) {
    c_log(C_LOG_LIB_GAME_LIVE_STREAMING, C_LOG_LEVEL_ERROR, "sceGameLiveStreamingSetSpoilerTag",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGameLiveStreamingSetStandbyScreenResource(void) {
    c_log(C_LOG_LIB_GAME_LIVE_STREAMING, C_LOG_LEVEL_ERROR,
          "sceGameLiveStreamingSetStandbyScreenResource", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGameLiveStreamingStartGenerateStandbyScreenResource(void) {
    c_log(C_LOG_LIB_GAME_LIVE_STREAMING, C_LOG_LEVEL_ERROR,
          "sceGameLiveStreamingStartGenerateStandbyScreenResource", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGameLiveStreamingStartSocialFeedbackMessageFiltering(void) {
    c_log(C_LOG_LIB_GAME_LIVE_STREAMING, C_LOG_LEVEL_ERROR,
          "sceGameLiveStreamingStartSocialFeedbackMessageFiltering", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGameLiveStreamingStopGenerateStandbyScreenResource(void) {
    c_log(C_LOG_LIB_GAME_LIVE_STREAMING, C_LOG_LEVEL_ERROR,
          "sceGameLiveStreamingStopGenerateStandbyScreenResource", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGameLiveStreamingStopSocialFeedbackMessageFiltering(void) {
    c_log(C_LOG_LIB_GAME_LIVE_STREAMING, C_LOG_LEVEL_ERROR,
          "sceGameLiveStreamingStopSocialFeedbackMessageFiltering", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGameLiveStreamingTerminate(void) {
    c_log(C_LOG_LIB_GAME_LIVE_STREAMING, C_LOG_LEVEL_ERROR, "sceGameLiveStreamingTerminate",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceGameLiveStreamingUnregisterCallback(void) {
    c_log(C_LOG_LIB_GAME_LIVE_STREAMING, C_LOG_LEVEL_ERROR,
          "sceGameLiveStreamingUnregisterCallback", "(STUBBED) called");
    return ORBIS_OK;
}
