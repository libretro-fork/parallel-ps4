/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <string.h>

#include "common/c_bridge.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/share_play/shareplay.h"

int PS4_SYSV_ABI sceSharePlayCrashDaemon(void) {
    c_log(C_LOG_LIB_SHARE_PLAY, C_LOG_LEVEL_ERROR, "sceSharePlayCrashDaemon", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceSharePlayGetCurrentConnectionInfo(OrbisSharePlayConnectionInfo* pInfo) {
    memset(pInfo, 0, sizeof(*pInfo));
    pInfo->status = ORBIS_SHARE_PLAY_CONNECTION_STATUS_DORMANT;
    c_log(C_LOG_LIB_SHARE_PLAY, C_LOG_LEVEL_DEBUG, "sceSharePlayGetCurrentConnectionInfo",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceSharePlayGetCurrentConnectionInfoA(void) {
    c_log(C_LOG_LIB_SHARE_PLAY, C_LOG_LEVEL_ERROR, "sceSharePlayGetCurrentConnectionInfoA",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceSharePlayGetCurrentInfo(void) {
    c_log(C_LOG_LIB_SHARE_PLAY, C_LOG_LEVEL_ERROR, "sceSharePlayGetCurrentInfo",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceSharePlayGetEvent(void) {
    c_log(C_LOG_LIB_SHARE_PLAY, C_LOG_LEVEL_ERROR, "sceSharePlayGetEvent", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceSharePlayInitialize(void) {
    c_log(C_LOG_LIB_SHARE_PLAY, C_LOG_LEVEL_ERROR, "sceSharePlayInitialize", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceSharePlayNotifyDialogOpen(void) {
    c_log(C_LOG_LIB_SHARE_PLAY, C_LOG_LEVEL_ERROR, "sceSharePlayNotifyDialogOpen",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceSharePlayNotifyForceCloseForCdlg(void) {
    c_log(C_LOG_LIB_SHARE_PLAY, C_LOG_LEVEL_ERROR, "sceSharePlayNotifyForceCloseForCdlg",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceSharePlayNotifyOpenQuickMenu(void) {
    c_log(C_LOG_LIB_SHARE_PLAY, C_LOG_LEVEL_ERROR, "sceSharePlayNotifyOpenQuickMenu",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceSharePlayResumeScreenForCdlg(void) {
    c_log(C_LOG_LIB_SHARE_PLAY, C_LOG_LEVEL_ERROR, "sceSharePlayResumeScreenForCdlg",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceSharePlayServerLock(void) {
    c_log(C_LOG_LIB_SHARE_PLAY, C_LOG_LEVEL_ERROR, "sceSharePlayServerLock", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceSharePlayServerUnLock(void) {
    c_log(C_LOG_LIB_SHARE_PLAY, C_LOG_LEVEL_ERROR, "sceSharePlayServerUnLock", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceSharePlaySetMode(void) {
    c_log(C_LOG_LIB_SHARE_PLAY, C_LOG_LEVEL_ERROR, "sceSharePlaySetMode", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceSharePlaySetProhibition(void) {
    c_log(C_LOG_LIB_SHARE_PLAY, C_LOG_LEVEL_ERROR, "sceSharePlaySetProhibition",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceSharePlaySetProhibitionModeWithAppId(void) {
    c_log(C_LOG_LIB_SHARE_PLAY, C_LOG_LEVEL_ERROR, "sceSharePlaySetProhibitionModeWithAppId",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceSharePlayStartStandby(void) {
    c_log(C_LOG_LIB_SHARE_PLAY, C_LOG_LEVEL_ERROR, "sceSharePlayStartStandby", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceSharePlayStartStreaming(void) {
    c_log(C_LOG_LIB_SHARE_PLAY, C_LOG_LEVEL_ERROR, "sceSharePlayStartStreaming",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceSharePlayStopStandby(void) {
    c_log(C_LOG_LIB_SHARE_PLAY, C_LOG_LEVEL_ERROR, "sceSharePlayStopStandby", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceSharePlayStopStreaming(void) {
    c_log(C_LOG_LIB_SHARE_PLAY, C_LOG_LEVEL_ERROR, "sceSharePlayStopStreaming", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceSharePlayTerminate(void) {
    c_log(C_LOG_LIB_SHARE_PLAY, C_LOG_LEVEL_ERROR, "sceSharePlayTerminate", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI SharePlay_Func_2E93C0EA6A6B67C4(void) {
    c_log(C_LOG_LIB_SHARE_PLAY, C_LOG_LEVEL_ERROR, "SharePlay_Func_2E93C0EA6A6B67C4",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI SharePlay_Func_C1C236728D88E177(void) {
    c_log(C_LOG_LIB_SHARE_PLAY, C_LOG_LEVEL_ERROR, "SharePlay_Func_C1C236728D88E177",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI SharePlay_Func_E9E80C474781F115(void) {
    c_log(C_LOG_LIB_SHARE_PLAY, C_LOG_LEVEL_ERROR, "SharePlay_Func_E9E80C474781F115",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI SharePlay_Func_F3DD6199DA15ED44(void) {
    c_log(C_LOG_LIB_SHARE_PLAY, C_LOG_LEVEL_ERROR, "SharePlay_Func_F3DD6199DA15ED44",
          "(STUBBED) called");
    return ORBIS_OK;
}
