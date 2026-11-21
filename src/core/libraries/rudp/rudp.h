/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_RUDP_H
#define SHADPS4_LIBRARIES_RUDP_H

/* libSceRudp. Registered in sysmodule_internal.cpp. */

#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

s32 PS4_SYSV_ABI sceRudpAccept(void);
s32 PS4_SYSV_ABI sceRudpActivate(void);
s32 PS4_SYSV_ABI sceRudpBind(void);
s32 PS4_SYSV_ABI sceRudpCreateContext(void);
s32 PS4_SYSV_ABI sceRudpEnableInternalIOThread(void);
s32 PS4_SYSV_ABI sceRudpEnableInternalIOThread2(void);
s32 PS4_SYSV_ABI sceRudpEnd(void);
s32 PS4_SYSV_ABI sceRudpFlush(void);
s32 PS4_SYSV_ABI sceRudpGetContextStatus(void);
s32 PS4_SYSV_ABI sceRudpGetLocalInfo(void);
s32 PS4_SYSV_ABI sceRudpGetMaxSegmentSize(void);
s32 PS4_SYSV_ABI sceRudpGetNumberOfPacketsToRead(void);
s32 PS4_SYSV_ABI sceRudpGetOption(void);
s32 PS4_SYSV_ABI sceRudpGetRemoteInfo(void);
s32 PS4_SYSV_ABI sceRudpGetSizeReadable(void);
s32 PS4_SYSV_ABI sceRudpGetSizeWritable(void);
s32 PS4_SYSV_ABI sceRudpGetStatus(void);
s32 PS4_SYSV_ABI sceRudpInit(void);
s32 PS4_SYSV_ABI sceRudpInitiate(void);
s32 PS4_SYSV_ABI sceRudpListen(void);
s32 PS4_SYSV_ABI sceRudpNetFlush(void);
s32 PS4_SYSV_ABI sceRudpNetReceived(void);
s32 PS4_SYSV_ABI sceRudpPollCancel(void);
s32 PS4_SYSV_ABI sceRudpPollControl(void);
s32 PS4_SYSV_ABI sceRudpPollCreate(void);
s32 PS4_SYSV_ABI sceRudpPollDestroy(void);
s32 PS4_SYSV_ABI sceRudpPollWait(void);
s32 PS4_SYSV_ABI sceRudpProcessEvents(void);
s32 PS4_SYSV_ABI sceRudpRead(void);
s32 PS4_SYSV_ABI sceRudpSetEventHandler(void);
s32 PS4_SYSV_ABI sceRudpSetMaxSegmentSize(void);
s32 PS4_SYSV_ABI sceRudpSetOption(void);
s32 PS4_SYSV_ABI sceRudpTerminate(void);
s32 PS4_SYSV_ABI sceRudpWrite(void);

RETRO_END_DECLS

#endif
