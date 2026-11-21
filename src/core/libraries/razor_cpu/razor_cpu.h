/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_RAZOR_CPU_H
#define SHADPS4_LIBRARIES_RAZOR_CPU_H

/* libSceRazorCpu. Registered in libs.cpp. */

#include <retro_common_api.h>

#include <boolean.h>
#include "common/types.h"

RETRO_BEGIN_DECLS

s32 PS4_SYSV_ABI sceRazorCpuBeginLogicalFileAccess(void);
void PS4_SYSV_ABI sceRazorCpuDisableFiberUserMarkers(void);
s32 PS4_SYSV_ABI sceRazorCpuEndLogicalFileAccess(void);
s32 PS4_SYSV_ABI sceRazorCpuFiberLogNameChange(void);
s32 PS4_SYSV_ABI sceRazorCpuFiberSwitch(void);
bool PS4_SYSV_ABI sceRazorCpuFlushOccurred(void);
s32 PS4_SYSV_ABI sceRazorCpuGetDataTagStorageSize(void);
s32 PS4_SYSV_ABI sceRazorCpuGpuMarkerSync(void);
s32 PS4_SYSV_ABI sceRazorCpuInitDataTags(void);
s32 PS4_SYSV_ABI sceRazorCpuInitializeGpuMarkerContext(void);
s32 PS4_SYSV_ABI sceRazorCpuInitializeInternal(void);
bool PS4_SYSV_ABI sceRazorCpuIsCapturing(void);
s32 PS4_SYSV_ABI sceRazorCpuJobManagerDispatch(void);
s32 PS4_SYSV_ABI sceRazorCpuJobManagerJob(void);
s32 PS4_SYSV_ABI sceRazorCpuJobManagerSequence(void);
s32 PS4_SYSV_ABI sceRazorCpuNamedSync(void);
s32 PS4_SYSV_ABI sceRazorCpuPlotValue(void);
s32 PS4_SYSV_ABI sceRazorCpuPopMarker(void);
s32 PS4_SYSV_ABI sceRazorCpuPushMarker(void);
s32 PS4_SYSV_ABI sceRazorCpuPushMarkerStatic(void);
s32 PS4_SYSV_ABI sceRazorCpuResizeTaggedBuffer(void);
void PS4_SYSV_ABI sceRazorCpuSetPopMarkerCallback(void);
void PS4_SYSV_ABI sceRazorCpuSetPushMarkerCallback(void);
void PS4_SYSV_ABI sceRazorCpuSetPushMarkerStaticCallback(void);
s32 PS4_SYSV_ABI sceRazorCpuShutdownDataTags(void);
s32 PS4_SYSV_ABI sceRazorCpuStartCaptureInternal(void);
s32 PS4_SYSV_ABI sceRazorCpuStopCaptureInternal(void);
s32 PS4_SYSV_ABI sceRazorCpuSync(void);
s32 PS4_SYSV_ABI sceRazorCpuTagArray(void);
s32 PS4_SYSV_ABI sceRazorCpuTagBuffer(void);
s32 PS4_SYSV_ABI sceRazorCpuUnTagBuffer(void);
s32 PS4_SYSV_ABI sceRazorCpuWorkloadRunBegin(void);
s32 PS4_SYSV_ABI sceRazorCpuWorkloadRunEnd(void);
s32 PS4_SYSV_ABI sceRazorCpuWorkloadSubmit(void);
s32 PS4_SYSV_ABI sceRazorCpuWriteBookmark(void);

RETRO_END_DECLS

#endif
