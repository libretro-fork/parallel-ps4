/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_NGS2_LIB_H
#define SHADPS4_NGS2_LIB_H

/* libSceNgs2's calls (ngs2.c), for its registration in sysmodule_internal.cpp */

#include <retro_common_api.h>

#include "core/libraries/ngs2/ngs2.h"
#include "core/libraries/ngs2/ngs2_custom.h"
#include "core/libraries/ngs2/ngs2_geom.h"
#include "core/libraries/ngs2/ngs2_mastering.h"
#include "core/libraries/ngs2/ngs2_pan.h"
#include "core/libraries/ngs2/ngs2_report.h"
#include "core/libraries/ngs2/ngs2_reverb.h"
#include "core/libraries/ngs2/ngs2_sampler.h"
#include "core/libraries/ngs2/ngs2_submixer.h"

RETRO_BEGIN_DECLS

s32 PS4_SYSV_ABI sceNgs2CalcWaveformBlock(const OrbisNgs2WaveformFormat* format, u32 samplePos,
                                          u32 numSamples, OrbisNgs2WaveformBlock* outBlock);
s32 PS4_SYSV_ABI sceNgs2GetWaveformFrameInfo(const OrbisNgs2WaveformFormat* format,
                                             u32* outFrameSize, u32* outNumFrameSamples,
                                             u32* outUnitsPerFrame, u32* outNumDelaySamples);
s32 PS4_SYSV_ABI sceNgs2ParseWaveformData(const void* data, size_t dataSize,
                                          OrbisNgs2WaveformInfo* outInfo);
s32 PS4_SYSV_ABI sceNgs2ParseWaveformFile(const char* path, u64 offset,
                                          OrbisNgs2WaveformInfo* outInfo);
s32 PS4_SYSV_ABI sceNgs2ParseWaveformUser(OrbisNgs2ParseReadHandler handler, uintptr_t userData,
                                          OrbisNgs2WaveformInfo* outInfo);
s32 PS4_SYSV_ABI sceNgs2RackCreate(OrbisNgs2Handle systemHandle, u32 rackId,
                                   const OrbisNgs2RackOption* option,
                                   const OrbisNgs2ContextBufferInfo* bufferInfo,
                                   OrbisNgs2Handle* outHandle);
s32 PS4_SYSV_ABI sceNgs2RackCreateWithAllocator(OrbisNgs2Handle systemHandle, u32 rackId,
                                                const OrbisNgs2RackOption* option,
                                                const OrbisNgs2BufferAllocator* allocator,
                                                OrbisNgs2Handle* outHandle);
s32 PS4_SYSV_ABI sceNgs2RackDestroy(OrbisNgs2Handle rackHandle,
                                    OrbisNgs2ContextBufferInfo* outBufferInfo);
s32 PS4_SYSV_ABI sceNgs2RackGetInfo(OrbisNgs2Handle rackHandle, OrbisNgs2RackInfo* outInfo,
                                    size_t infoSize);
s32 PS4_SYSV_ABI sceNgs2RackGetUserData(OrbisNgs2Handle rackHandle, uintptr_t* outUserData);
s32 PS4_SYSV_ABI sceNgs2RackGetVoiceHandle(OrbisNgs2Handle rackHandle, u32 voiceIndex,
                                           OrbisNgs2Handle* outHandle);
s32 PS4_SYSV_ABI sceNgs2RackLock(OrbisNgs2Handle rackHandle);
s32 PS4_SYSV_ABI sceNgs2RackQueryBufferSize(u32 rackId, const OrbisNgs2RackOption* option,
                                            OrbisNgs2ContextBufferInfo* outBufferInfo);
s32 PS4_SYSV_ABI sceNgs2RackSetUserData(OrbisNgs2Handle rackHandle, uintptr_t userData);
s32 PS4_SYSV_ABI sceNgs2RackUnlock(OrbisNgs2Handle rackHandle);
s32 PS4_SYSV_ABI sceNgs2SystemCreate(const OrbisNgs2SystemOption* option,
                                     const OrbisNgs2ContextBufferInfo* bufferInfo,
                                     OrbisNgs2Handle* outHandle);
s32 PS4_SYSV_ABI sceNgs2SystemCreateWithAllocator(const OrbisNgs2SystemOption* option,
                                                  const OrbisNgs2BufferAllocator* allocator,
                                                  OrbisNgs2Handle* outHandle);
s32 PS4_SYSV_ABI sceNgs2SystemDestroy(OrbisNgs2Handle systemHandle,
                                      OrbisNgs2ContextBufferInfo* outBufferInfo);
s32 PS4_SYSV_ABI sceNgs2SystemEnumHandles(OrbisNgs2Handle* aOutHandle, u32 maxHandles);
s32 PS4_SYSV_ABI sceNgs2SystemEnumRackHandles(OrbisNgs2Handle systemHandle,
                                              OrbisNgs2Handle* aOutHandle, u32 maxHandles);
s32 PS4_SYSV_ABI sceNgs2SystemGetInfo(OrbisNgs2Handle systemHandle, OrbisNgs2SystemInfo* outInfo,
                                      size_t infoSize);
s32 PS4_SYSV_ABI sceNgs2SystemGetUserData(OrbisNgs2Handle systemHandle, uintptr_t* outUserData);
s32 PS4_SYSV_ABI sceNgs2SystemLock(OrbisNgs2Handle systemHandle);
s32 PS4_SYSV_ABI sceNgs2SystemQueryBufferSize(const OrbisNgs2SystemOption* option,
                                              OrbisNgs2ContextBufferInfo* outBufferInfo);
s32 PS4_SYSV_ABI sceNgs2SystemRender(OrbisNgs2Handle systemHandle,
                                     const OrbisNgs2RenderBufferInfo* aBufferInfo,
                                     u32 numBufferInfo);
s32 PS4_SYSV_ABI sceNgs2SystemResetOption(OrbisNgs2SystemOption* outOption);
s32 PS4_SYSV_ABI sceNgs2SystemSetGrainSamples(OrbisNgs2Handle systemHandle, u32 numSamples);
s32 PS4_SYSV_ABI sceNgs2SystemSetSampleRate(OrbisNgs2Handle systemHandle, u32 sampleRate);
s32 PS4_SYSV_ABI sceNgs2SystemSetUserData(OrbisNgs2Handle systemHandle, uintptr_t userData);
s32 PS4_SYSV_ABI sceNgs2SystemUnlock(OrbisNgs2Handle systemHandle);
s32 PS4_SYSV_ABI sceNgs2VoiceControl(OrbisNgs2Handle voiceHandle,
                                     const OrbisNgs2VoiceParamHeader* paramList);
s32 PS4_SYSV_ABI sceNgs2VoiceGetMatrixInfo(OrbisNgs2Handle voiceHandle, u32 matrixId,
                                           OrbisNgs2VoiceMatrixInfo* outInfo, size_t outInfoSize);
s32 PS4_SYSV_ABI sceNgs2VoiceGetOwner(OrbisNgs2Handle voiceHandle, OrbisNgs2Handle* outRackHandle,
                                      u32* outVoiceId);
s32 PS4_SYSV_ABI sceNgs2VoiceGetPortInfo(OrbisNgs2Handle voiceHandle, u32 port,
                                         OrbisNgs2VoicePortInfo* outInfo, size_t outInfoSize);
s32 PS4_SYSV_ABI sceNgs2VoiceGetState(OrbisNgs2Handle voiceHandle, OrbisNgs2VoiceState* outState,
                                      size_t stateSize);
s32 PS4_SYSV_ABI sceNgs2VoiceGetStateFlags(OrbisNgs2Handle voiceHandle, u32* outStateFlags);
s32 PS4_SYSV_ABI sceNgs2CustomRackGetModuleInfo(OrbisNgs2Handle rackHandle, u32 moduleIndex,
                                                OrbisNgs2CustomModuleInfo* outInfo,
                                                size_t infoSize);
s32 PS4_SYSV_ABI sceNgs2GeomResetListenerParam(OrbisNgs2GeomListenerParam* outListenerParam);
s32 PS4_SYSV_ABI sceNgs2GeomResetSourceParam(OrbisNgs2GeomSourceParam* outSourceParam);
s32 PS4_SYSV_ABI sceNgs2GeomCalcListener(const OrbisNgs2GeomListenerParam* param,
                                         OrbisNgs2GeomListenerWork* outWork, u32 flags);
s32 PS4_SYSV_ABI sceNgs2GeomApply(const OrbisNgs2GeomListenerWork* listener,
                                  const OrbisNgs2GeomSourceParam* source,
                                  OrbisNgs2GeomAttribute* outAttrib, u32 flags);
s32 PS4_SYSV_ABI sceNgs2PanInit(OrbisNgs2PanWork* work, const float* aSpeakerAngle, float unitAngle,
                                u32 numSpeakers);
s32 PS4_SYSV_ABI sceNgs2PanGetVolumeMatrix(OrbisNgs2PanWork* work, const OrbisNgs2PanParam* aParam,
                                           u32 numParams, u32 matrixFormat, float* outVolumeMatrix);
s32 PS4_SYSV_ABI sceNgs2ReportRegisterHandler(u32 reportType, OrbisNgs2ReportHandler handler,
                                              uintptr_t userData, OrbisNgs2Handle* outHandle);
s32 PS4_SYSV_ABI sceNgs2ReportUnregisterHandler(OrbisNgs2Handle reportHandle);
s32 PS4_SYSV_ABI sceNgs2FftInit(void);
s32 PS4_SYSV_ABI sceNgs2FftProcess(void);
s32 PS4_SYSV_ABI sceNgs2FftQuerySize(void);
s32 PS4_SYSV_ABI sceNgs2JobSchedulerResetOption(void);
s32 PS4_SYSV_ABI sceNgs2ModuleArrayEnumItems(void);
s32 PS4_SYSV_ABI sceNgs2ModuleEnumConfigs(void);
s32 PS4_SYSV_ABI sceNgs2ModuleQueueEnumItems(void);
s32 PS4_SYSV_ABI sceNgs2RackQueryInfo(void);
s32 PS4_SYSV_ABI sceNgs2RackRunCommands(void);
s32 PS4_SYSV_ABI sceNgs2SystemQueryInfo(void);
s32 PS4_SYSV_ABI sceNgs2SystemRunCommands(void);
s32 PS4_SYSV_ABI sceNgs2SystemSetLoudThreshold(void);
s32 PS4_SYSV_ABI sceNgs2StreamCreate(void);
s32 PS4_SYSV_ABI sceNgs2StreamCreateWithAllocator(void);
s32 PS4_SYSV_ABI sceNgs2StreamDestroy(void);
s32 PS4_SYSV_ABI sceNgs2StreamQueryBufferSize(void);
s32 PS4_SYSV_ABI sceNgs2StreamQueryInfo(void);
s32 PS4_SYSV_ABI sceNgs2StreamResetOption(void);
s32 PS4_SYSV_ABI sceNgs2StreamRunCommands(void);
s32 PS4_SYSV_ABI sceNgs2VoiceQueryInfo(void);
s32 PS4_SYSV_ABI sceNgs2VoiceRunCommands(void);

RETRO_END_DECLS

#endif
