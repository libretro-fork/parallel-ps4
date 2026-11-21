/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <boolean.h>
#include <stddef.h>
#include <string.h>

#include "common/c_bridge.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/remote_play/remoteplay.h"

int PS4_SYSV_ABI sceRemoteplayApprove(void) {
    c_log(C_LOG_LIB_REMOTEPLAY, C_LOG_LEVEL_ERROR, "sceRemoteplayApprove", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRemoteplayChangeEnterKey(void) {
    c_log(C_LOG_LIB_REMOTEPLAY, C_LOG_LEVEL_ERROR, "sceRemoteplayChangeEnterKey",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRemoteplayClearAllRegistData(void) {
    c_log(C_LOG_LIB_REMOTEPLAY, C_LOG_LEVEL_ERROR, "sceRemoteplayClearAllRegistData",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRemoteplayClearConnectHistory(void) {
    c_log(C_LOG_LIB_REMOTEPLAY, C_LOG_LEVEL_ERROR, "sceRemoteplayClearConnectHistory",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRemoteplayConfirmDeviceRegist(void) {
    c_log(C_LOG_LIB_REMOTEPLAY, C_LOG_LEVEL_ERROR, "sceRemoteplayConfirmDeviceRegist",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRemoteplayDisconnect(void) {
    c_log(C_LOG_LIB_REMOTEPLAY, C_LOG_LEVEL_ERROR, "sceRemoteplayDisconnect", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRemoteplayGeneratePinCode(void) {
    c_log(C_LOG_LIB_REMOTEPLAY, C_LOG_LEVEL_ERROR, "sceRemoteplayGeneratePinCode",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRemoteplayGetApMode(void) {
    c_log(C_LOG_LIB_REMOTEPLAY, C_LOG_LEVEL_ERROR, "sceRemoteplayGetApMode", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRemoteplayGetConnectHistory(void) {
    c_log(C_LOG_LIB_REMOTEPLAY, C_LOG_LEVEL_ERROR, "sceRemoteplayGetConnectHistory",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRemoteplayGetConnectionStatus(s32 userId, int* pStatus) {
    *pStatus = ORBIS_REMOTEPLAY_CONNECTION_STATUS_DISCONNECT;
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRemoteplayGetConnectUserId(void) {
    c_log(C_LOG_LIB_REMOTEPLAY, C_LOG_LEVEL_ERROR, "sceRemoteplayGetConnectUserId",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRemoteplayGetMbusDeviceInfo(void) {
    c_log(C_LOG_LIB_REMOTEPLAY, C_LOG_LEVEL_ERROR, "sceRemoteplayGetMbusDeviceInfo",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRemoteplayGetOperationStatus(void) {
    c_log(C_LOG_LIB_REMOTEPLAY, C_LOG_LEVEL_ERROR, "sceRemoteplayGetOperationStatus",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRemoteplayGetRemoteplayStatus(void) {
    c_log(C_LOG_LIB_REMOTEPLAY, C_LOG_LEVEL_ERROR, "sceRemoteplayGetRemoteplayStatus",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRemoteplayGetRpMode(void) {
    c_log(C_LOG_LIB_REMOTEPLAY, C_LOG_LEVEL_ERROR, "sceRemoteplayGetRpMode", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRemoteplayImeClose(void) {
    c_log(C_LOG_LIB_REMOTEPLAY, C_LOG_LEVEL_ERROR, "sceRemoteplayImeClose", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRemoteplayImeFilterResult(void) {
    c_log(C_LOG_LIB_REMOTEPLAY, C_LOG_LEVEL_ERROR, "sceRemoteplayImeFilterResult",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRemoteplayImeGetEvent(void) {
    c_log(C_LOG_LIB_REMOTEPLAY, C_LOG_LEVEL_ERROR, "sceRemoteplayImeGetEvent", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRemoteplayImeNotify(void) {
    c_log(C_LOG_LIB_REMOTEPLAY, C_LOG_LEVEL_ERROR, "sceRemoteplayImeNotify", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRemoteplayImeNotifyEventResult(void) {
    c_log(C_LOG_LIB_REMOTEPLAY, C_LOG_LEVEL_ERROR, "sceRemoteplayImeNotifyEventResult",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRemoteplayImeOpen(void) {
    c_log(C_LOG_LIB_REMOTEPLAY, C_LOG_LEVEL_ERROR, "sceRemoteplayImeOpen", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRemoteplayImeSetCaret(void) {
    c_log(C_LOG_LIB_REMOTEPLAY, C_LOG_LEVEL_ERROR, "sceRemoteplayImeSetCaret", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRemoteplayImeSetText(void) {
    c_log(C_LOG_LIB_REMOTEPLAY, C_LOG_LEVEL_ERROR, "sceRemoteplayImeSetText", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRemoteplayInitialize(void) {
    c_log(C_LOG_LIB_REMOTEPLAY, C_LOG_LEVEL_ERROR, "sceRemoteplayInitialize", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRemoteplayIsRemoteOskReady(void) {
    c_log(C_LOG_LIB_REMOTEPLAY, C_LOG_LEVEL_ERROR, "sceRemoteplayIsRemoteOskReady",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRemoteplayIsRemotePlaying(void) {
    c_log(C_LOG_LIB_REMOTEPLAY, C_LOG_LEVEL_ERROR, "sceRemoteplayIsRemotePlaying",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRemoteplayNotifyMbusDeviceRegistComplete(void) {
    c_log(C_LOG_LIB_REMOTEPLAY, C_LOG_LEVEL_ERROR, "sceRemoteplayNotifyMbusDeviceRegistComplete",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRemoteplayNotifyNpPushWakeup(void) {
    c_log(C_LOG_LIB_REMOTEPLAY, C_LOG_LEVEL_ERROR, "sceRemoteplayNotifyNpPushWakeup",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRemoteplayNotifyPinCodeError(void) {
    c_log(C_LOG_LIB_REMOTEPLAY, C_LOG_LEVEL_ERROR, "sceRemoteplayNotifyPinCodeError",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRemoteplayNotifyUserDelete(void) {
    c_log(C_LOG_LIB_REMOTEPLAY, C_LOG_LEVEL_ERROR, "sceRemoteplayNotifyUserDelete",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRemoteplayPrintAllRegistData(void) {
    c_log(C_LOG_LIB_REMOTEPLAY, C_LOG_LEVEL_ERROR, "sceRemoteplayPrintAllRegistData",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRemoteplayProhibit(void) {
    c_log(C_LOG_LIB_REMOTEPLAY, C_LOG_LEVEL_ERROR, "sceRemoteplayProhibit", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRemoteplayProhibitStreaming(void) {
    c_log(C_LOG_LIB_REMOTEPLAY, C_LOG_LEVEL_ERROR, "sceRemoteplayProhibitStreaming",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRemoteplayServerLock(void) {
    c_log(C_LOG_LIB_REMOTEPLAY, C_LOG_LEVEL_ERROR, "sceRemoteplayServerLock", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRemoteplayServerUnLock(void) {
    c_log(C_LOG_LIB_REMOTEPLAY, C_LOG_LEVEL_ERROR, "sceRemoteplayServerUnLock", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRemoteplaySetApMode(void) {
    c_log(C_LOG_LIB_REMOTEPLAY, C_LOG_LEVEL_ERROR, "sceRemoteplaySetApMode", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRemoteplaySetLogLevel(void) {
    c_log(C_LOG_LIB_REMOTEPLAY, C_LOG_LEVEL_ERROR, "sceRemoteplaySetLogLevel", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRemoteplaySetProhibition(void) {
    c_log(C_LOG_LIB_REMOTEPLAY, C_LOG_LEVEL_ERROR, "sceRemoteplaySetProhibition",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRemoteplaySetProhibitionForVsh(void) {
    c_log(C_LOG_LIB_REMOTEPLAY, C_LOG_LEVEL_ERROR, "sceRemoteplaySetProhibitionForVsh",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRemoteplaySetRpMode(void) {
    c_log(C_LOG_LIB_REMOTEPLAY, C_LOG_LEVEL_ERROR, "sceRemoteplaySetRpMode", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceRemoteplayTerminate(void) {
    c_log(C_LOG_LIB_REMOTEPLAY, C_LOG_LEVEL_ERROR, "sceRemoteplayTerminate", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI Remoteplay_Func_1D5EE365ED5FADB3(void) {
    c_log(C_LOG_LIB_REMOTEPLAY, C_LOG_LEVEL_ERROR, "Remoteplay_Func_1D5EE365ED5FADB3",
          "(STUBBED) called");
    return ORBIS_OK;
}
