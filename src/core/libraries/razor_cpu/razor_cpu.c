/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <boolean.h>
#include <stddef.h>
#include <string.h>

#include "common/c_bridge.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/razor_cpu/razor_cpu.h"

s32 PS4_SYSV_ABI sceRazorCpuBeginLogicalFileAccess(void) {
    c_log(C_LOG_LIB_RAZOR_CPU, C_LOG_LEVEL_DEBUG, "sceRazorCpuBeginLogicalFileAccess",
          "(STUBBED) called");
    return ORBIS_OK;
}

void PS4_SYSV_ABI sceRazorCpuDisableFiberUserMarkers(void) {
    c_log(C_LOG_LIB_RAZOR_CPU, C_LOG_LEVEL_DEBUG, "sceRazorCpuDisableFiberUserMarkers",
          "(STUBBED) called");
}

s32 PS4_SYSV_ABI sceRazorCpuEndLogicalFileAccess(void) {
    c_log(C_LOG_LIB_RAZOR_CPU, C_LOG_LEVEL_DEBUG, "sceRazorCpuEndLogicalFileAccess",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceRazorCpuFiberLogNameChange(void) {
    c_log(C_LOG_LIB_RAZOR_CPU, C_LOG_LEVEL_DEBUG, "sceRazorCpuFiberLogNameChange",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceRazorCpuFiberSwitch(void) {
    c_log(C_LOG_LIB_RAZOR_CPU, C_LOG_LEVEL_DEBUG, "sceRazorCpuFiberSwitch", "(STUBBED) called");
    return ORBIS_OK;
}

bool PS4_SYSV_ABI sceRazorCpuFlushOccurred(void) {
    c_log(C_LOG_LIB_RAZOR_CPU, C_LOG_LEVEL_DEBUG, "sceRazorCpuFlushOccurred", "(STUBBED) called");
    return false;
}

s32 PS4_SYSV_ABI sceRazorCpuGetDataTagStorageSize(void) {
    c_log(C_LOG_LIB_RAZOR_CPU, C_LOG_LEVEL_DEBUG, "sceRazorCpuGetDataTagStorageSize",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceRazorCpuGpuMarkerSync(void) {
    c_log(C_LOG_LIB_RAZOR_CPU, C_LOG_LEVEL_DEBUG, "sceRazorCpuGpuMarkerSync", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceRazorCpuInitDataTags(void) {
    c_log(C_LOG_LIB_RAZOR_CPU, C_LOG_LEVEL_DEBUG, "sceRazorCpuInitDataTags", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceRazorCpuInitializeGpuMarkerContext(void) {
    c_log(C_LOG_LIB_RAZOR_CPU, C_LOG_LEVEL_DEBUG, "sceRazorCpuInitializeGpuMarkerContext",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceRazorCpuInitializeInternal(void) {
    c_log(C_LOG_LIB_RAZOR_CPU, C_LOG_LEVEL_DEBUG, "sceRazorCpuInitializeInternal",
          "(STUBBED) called");
    return ORBIS_OK;
}

bool PS4_SYSV_ABI sceRazorCpuIsCapturing(void) {
    c_log(C_LOG_LIB_RAZOR_CPU, C_LOG_LEVEL_DEBUG, "sceRazorCpuIsCapturing", "(STUBBED) called");
    return false;
}

s32 PS4_SYSV_ABI sceRazorCpuJobManagerDispatch(void) {
    c_log(C_LOG_LIB_RAZOR_CPU, C_LOG_LEVEL_DEBUG, "sceRazorCpuJobManagerDispatch",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceRazorCpuJobManagerJob(void) {
    c_log(C_LOG_LIB_RAZOR_CPU, C_LOG_LEVEL_DEBUG, "sceRazorCpuJobManagerJob", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceRazorCpuJobManagerSequence(void) {
    c_log(C_LOG_LIB_RAZOR_CPU, C_LOG_LEVEL_DEBUG, "sceRazorCpuJobManagerSequence",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceRazorCpuNamedSync(void) {
    c_log(C_LOG_LIB_RAZOR_CPU, C_LOG_LEVEL_DEBUG, "sceRazorCpuNamedSync", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceRazorCpuPlotValue(void) {
    c_log(C_LOG_LIB_RAZOR_CPU, C_LOG_LEVEL_DEBUG, "sceRazorCpuPlotValue", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceRazorCpuPopMarker(void) {
    c_log(C_LOG_LIB_RAZOR_CPU, C_LOG_LEVEL_DEBUG, "sceRazorCpuPopMarker", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceRazorCpuPushMarker(void) {
    c_log(C_LOG_LIB_RAZOR_CPU, C_LOG_LEVEL_DEBUG, "sceRazorCpuPushMarker", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceRazorCpuPushMarkerStatic(void) {
    c_log(C_LOG_LIB_RAZOR_CPU, C_LOG_LEVEL_DEBUG, "sceRazorCpuPushMarkerStatic",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceRazorCpuResizeTaggedBuffer(void) {
    c_log(C_LOG_LIB_RAZOR_CPU, C_LOG_LEVEL_DEBUG, "sceRazorCpuResizeTaggedBuffer",
          "(STUBBED) called");
    return ORBIS_OK;
}

void PS4_SYSV_ABI sceRazorCpuSetPopMarkerCallback(void) {
    c_log(C_LOG_LIB_RAZOR_CPU, C_LOG_LEVEL_DEBUG, "sceRazorCpuSetPopMarkerCallback",
          "(STUBBED) called");
}

void PS4_SYSV_ABI sceRazorCpuSetPushMarkerCallback(void) {
    c_log(C_LOG_LIB_RAZOR_CPU, C_LOG_LEVEL_DEBUG, "sceRazorCpuSetPushMarkerCallback",
          "(STUBBED) called");
}

void PS4_SYSV_ABI sceRazorCpuSetPushMarkerStaticCallback(void) {
    c_log(C_LOG_LIB_RAZOR_CPU, C_LOG_LEVEL_DEBUG, "sceRazorCpuSetPushMarkerStaticCallback",
          "(STUBBED) called");
}

s32 PS4_SYSV_ABI sceRazorCpuShutdownDataTags(void) {
    c_log(C_LOG_LIB_RAZOR_CPU, C_LOG_LEVEL_DEBUG, "sceRazorCpuShutdownDataTags",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceRazorCpuStartCaptureInternal(void) {
    c_log(C_LOG_LIB_RAZOR_CPU, C_LOG_LEVEL_DEBUG, "sceRazorCpuStartCaptureInternal",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceRazorCpuStopCaptureInternal(void) {
    c_log(C_LOG_LIB_RAZOR_CPU, C_LOG_LEVEL_DEBUG, "sceRazorCpuStopCaptureInternal",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceRazorCpuSync(void) {
    c_log(C_LOG_LIB_RAZOR_CPU, C_LOG_LEVEL_DEBUG, "sceRazorCpuSync", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceRazorCpuTagArray(void) {
    c_log(C_LOG_LIB_RAZOR_CPU, C_LOG_LEVEL_DEBUG, "sceRazorCpuTagArray", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceRazorCpuTagBuffer(void) {
    c_log(C_LOG_LIB_RAZOR_CPU, C_LOG_LEVEL_DEBUG, "sceRazorCpuTagBuffer", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceRazorCpuUnTagBuffer(void) {
    c_log(C_LOG_LIB_RAZOR_CPU, C_LOG_LEVEL_DEBUG, "sceRazorCpuUnTagBuffer", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceRazorCpuWorkloadRunBegin(void) {
    c_log(C_LOG_LIB_RAZOR_CPU, C_LOG_LEVEL_DEBUG, "sceRazorCpuWorkloadRunBegin",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceRazorCpuWorkloadRunEnd(void) {
    c_log(C_LOG_LIB_RAZOR_CPU, C_LOG_LEVEL_DEBUG, "sceRazorCpuWorkloadRunEnd", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceRazorCpuWorkloadSubmit(void) {
    c_log(C_LOG_LIB_RAZOR_CPU, C_LOG_LEVEL_DEBUG, "sceRazorCpuWorkloadSubmit", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceRazorCpuWriteBookmark(void) {
    c_log(C_LOG_LIB_RAZOR_CPU, C_LOG_LEVEL_DEBUG, "sceRazorCpuWriteBookmark", "(STUBBED) called");
    return ORBIS_OK;
}
