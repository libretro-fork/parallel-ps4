/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_SHARE_PLAY_H
#define SHADPS4_LIBRARIES_SHARE_PLAY_H

/* libSceSharePlay. Registered in libs.cpp. */

#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

#define ORBIS_SHARE_PLAY_CONNECTION_STATUS_DORMANT 0x00
#define ORBIS_SHARE_PLAY_CONNECTION_STATUS_READY 0x01
#define ORBIS_SHARE_PLAY_CONNECTION_STATUS_CONNECTED 0x02

/* The layout of OrbisNpOnlineId (np_types.h) */
typedef struct OrbisSharePlayOnlineId {
    char data[16];
    s8 term;
    s8 dummy[3];
} OrbisSharePlayOnlineId;

typedef struct OrbisSharePlayConnectionInfo {
    int status;
    int mode;
    OrbisSharePlayOnlineId hostOnlineId;
    OrbisSharePlayOnlineId visitorOnlineId;
    s32 hostUserId;
    s32 visitorUserId;
} OrbisSharePlayConnectionInfo;

int PS4_SYSV_ABI sceSharePlayCrashDaemon(void);
int PS4_SYSV_ABI sceSharePlayGetCurrentConnectionInfo(OrbisSharePlayConnectionInfo* pInfo);
int PS4_SYSV_ABI sceSharePlayGetCurrentConnectionInfoA(void);
int PS4_SYSV_ABI sceSharePlayGetCurrentInfo(void);
int PS4_SYSV_ABI sceSharePlayGetEvent(void);
int PS4_SYSV_ABI sceSharePlayInitialize(void);
int PS4_SYSV_ABI sceSharePlayNotifyDialogOpen(void);
int PS4_SYSV_ABI sceSharePlayNotifyForceCloseForCdlg(void);
int PS4_SYSV_ABI sceSharePlayNotifyOpenQuickMenu(void);
int PS4_SYSV_ABI sceSharePlayResumeScreenForCdlg(void);
int PS4_SYSV_ABI sceSharePlayServerLock(void);
int PS4_SYSV_ABI sceSharePlayServerUnLock(void);
int PS4_SYSV_ABI sceSharePlaySetMode(void);
int PS4_SYSV_ABI sceSharePlaySetProhibition(void);
int PS4_SYSV_ABI sceSharePlaySetProhibitionModeWithAppId(void);
int PS4_SYSV_ABI sceSharePlayStartStandby(void);
int PS4_SYSV_ABI sceSharePlayStartStreaming(void);
int PS4_SYSV_ABI sceSharePlayStopStandby(void);
int PS4_SYSV_ABI sceSharePlayStopStreaming(void);
int PS4_SYSV_ABI sceSharePlayTerminate(void);
int PS4_SYSV_ABI SharePlay_Func_2E93C0EA6A6B67C4(void);
int PS4_SYSV_ABI SharePlay_Func_C1C236728D88E177(void);
int PS4_SYSV_ABI SharePlay_Func_E9E80C474781F115(void);
int PS4_SYSV_ABI SharePlay_Func_F3DD6199DA15ED44(void);

RETRO_END_DECLS

#endif
