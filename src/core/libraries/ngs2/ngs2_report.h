/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_NGS2_REPORT_H
#define SHADPS4_NGS2_REPORT_H

#include "core/libraries/ngs2/ngs2.h"

#include <stdarg.h> /* va_list */

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

typedef struct OrbisNgs2ReportDataHeader {
    size_t size;
    OrbisNgs2Handle handle;
    u32 type;
    s32 result;
} OrbisNgs2ReportDataHeader;

typedef void(PS4_SYSV_ABI* OrbisNgs2ReportHandler)(const OrbisNgs2ReportDataHeader* data,
                                                   uintptr_t user_data);

typedef struct OrbisNgs2ReportMessageData {
    OrbisNgs2ReportDataHeader header;
    const char* message;
} OrbisNgs2ReportMessageData;

typedef struct OrbisNgs2ReportApiData {
    OrbisNgs2ReportDataHeader header;
    const char* functionName;
    const char* format;
    va_list argument;
} OrbisNgs2ReportApiData;

typedef struct OrbisNgs2ReportControlData {
    OrbisNgs2ReportDataHeader header;
    const OrbisNgs2VoiceParamHeader* param;
} OrbisNgs2ReportControlData;

typedef struct OrbisNgs2ReportOutputData {
    OrbisNgs2ReportDataHeader header;
    const OrbisNgs2RenderBufferInfo* bufferInfo;

    u32 bufferIndex;
    u32 sampleRate;
    u32 numGrainSamples;
    u32 reserved;
} OrbisNgs2ReportOutputData;

typedef struct OrbisNgs2ReportCpuLoadData {
    OrbisNgs2ReportDataHeader header;
    float totalRatio;
    float flushRatio;
    float processRatio;
    float feedbackRatio;
} OrbisNgs2ReportCpuLoadData;

typedef struct OrbisNgs2ReportRenderStateData {
    OrbisNgs2ReportDataHeader header;
    u32 state;
    u32 reserved;
} OrbisNgs2ReportRenderStateData;

typedef struct OrbisNgs2ReportVoiceWaveformData {
    OrbisNgs2ReportDataHeader header;
    u32 location;
    u32 waveformType;
    u32 numChannels;
    u32 sampleRate;
    u32 numGrainSamples;
    u32 reserved;
    void* const* aData;
} OrbisNgs2ReportVoiceWaveformData;

s32 PS4_SYSV_ABI sceNgs2ReportRegisterHandler(u32 reportType, OrbisNgs2ReportHandler handler,
                                              uintptr_t userData, OrbisNgs2Handle* outHandle);

RETRO_END_DECLS

#endif
