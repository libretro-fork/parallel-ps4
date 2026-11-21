/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_REMOTEPLAY_H
#define SHADPS4_LIBRARIES_REMOTEPLAY_H

/* libSceRemoteplay. Registered in libs.cpp. */

#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

/* returning codes in sceRemoteplayGetConnectionStatus pstatus */
#define ORBIS_REMOTEPLAY_CONNECTION_STATUS_DISCONNECT 0
#define ORBIS_REMOTEPLAY_CONNECTION_STATUS_CONNECT 1

int PS4_SYSV_ABI sceRemoteplayApprove(void);
int PS4_SYSV_ABI sceRemoteplayChangeEnterKey(void);
int PS4_SYSV_ABI sceRemoteplayClearAllRegistData(void);
int PS4_SYSV_ABI sceRemoteplayClearConnectHistory(void);
int PS4_SYSV_ABI sceRemoteplayConfirmDeviceRegist(void);
int PS4_SYSV_ABI sceRemoteplayDisconnect(void);
int PS4_SYSV_ABI sceRemoteplayGeneratePinCode(void);
int PS4_SYSV_ABI sceRemoteplayGetApMode(void);
int PS4_SYSV_ABI sceRemoteplayGetConnectHistory(void);
int PS4_SYSV_ABI sceRemoteplayGetConnectionStatus(s32 userId, int* pStatus);
int PS4_SYSV_ABI sceRemoteplayGetConnectUserId(void);
int PS4_SYSV_ABI sceRemoteplayGetMbusDeviceInfo(void);
int PS4_SYSV_ABI sceRemoteplayGetOperationStatus(void);
int PS4_SYSV_ABI sceRemoteplayGetRemoteplayStatus(void);
int PS4_SYSV_ABI sceRemoteplayGetRpMode(void);
int PS4_SYSV_ABI sceRemoteplayImeClose(void);
int PS4_SYSV_ABI sceRemoteplayImeFilterResult(void);
int PS4_SYSV_ABI sceRemoteplayImeGetEvent(void);
int PS4_SYSV_ABI sceRemoteplayImeNotify(void);
int PS4_SYSV_ABI sceRemoteplayImeNotifyEventResult(void);
int PS4_SYSV_ABI sceRemoteplayImeOpen(void);
int PS4_SYSV_ABI sceRemoteplayImeSetCaret(void);
int PS4_SYSV_ABI sceRemoteplayImeSetText(void);
int PS4_SYSV_ABI sceRemoteplayInitialize(void);
int PS4_SYSV_ABI sceRemoteplayIsRemoteOskReady(void);
int PS4_SYSV_ABI sceRemoteplayIsRemotePlaying(void);
int PS4_SYSV_ABI sceRemoteplayNotifyMbusDeviceRegistComplete(void);
int PS4_SYSV_ABI sceRemoteplayNotifyNpPushWakeup(void);
int PS4_SYSV_ABI sceRemoteplayNotifyPinCodeError(void);
int PS4_SYSV_ABI sceRemoteplayNotifyUserDelete(void);
int PS4_SYSV_ABI sceRemoteplayPrintAllRegistData(void);
int PS4_SYSV_ABI sceRemoteplayProhibit(void);
int PS4_SYSV_ABI sceRemoteplayProhibitStreaming(void);
int PS4_SYSV_ABI sceRemoteplayServerLock(void);
int PS4_SYSV_ABI sceRemoteplayServerUnLock(void);
int PS4_SYSV_ABI sceRemoteplaySetApMode(void);
int PS4_SYSV_ABI sceRemoteplaySetLogLevel(void);
int PS4_SYSV_ABI sceRemoteplaySetProhibition(void);
int PS4_SYSV_ABI sceRemoteplaySetProhibitionForVsh(void);
int PS4_SYSV_ABI sceRemoteplaySetRpMode(void);
int PS4_SYSV_ABI sceRemoteplayTerminate(void);
int PS4_SYSV_ABI Remoteplay_Func_1D5EE365ED5FADB3(void);

RETRO_END_DECLS

#endif
