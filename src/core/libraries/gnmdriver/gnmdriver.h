/* SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_GNMDRIVER_GNMDRIVER_H
#define SHADPS4_LIBRARIES_GNMDRIVER_GNMDRIVER_H

/* libSceGnmDriver. Defined in gnmdriver.c, registered in libs.cpp. */

#include <boolean.h>
#include <retro_common_api.h>

#include "common/types.h"
#include "core/libraries/kernel/kernel_c.h"

RETRO_BEGIN_DECLS

/* Before the functions below are registered: makes the GPU and the presenter */
void gnm_driver_init(void);

s32 PS4_SYSV_ABI sceGnmAddEqEvent(s64 eq, u64 id, void* udata);
int PS4_SYSV_ABI sceGnmAreSubmitsAllowed(void);
int PS4_SYSV_ABI sceGnmBeginWorkload(u32 workload_stream, u64* workload);
s32 PS4_SYSV_ABI sceGnmComputeWaitOnAddress(u32* cmdbuf, u32 size, uintptr_t addr, u32 mask,
                                            u32 cmp_func, u32 ref);
s32 PS4_SYSV_ABI sceGnmComputeWaitSemaphore(u32* cmdbuf, u32 size, u64 sem_addr, u32 flags);
int PS4_SYSV_ABI sceGnmCreateWorkloadStream(u64 param1, u32* workload_stream);
int PS4_SYSV_ABI sceGnmDebuggerGetAddressWatch(void);
int PS4_SYSV_ABI sceGnmDebuggerHaltWavefront(void);
int PS4_SYSV_ABI sceGnmDebuggerReadGds(void);
int PS4_SYSV_ABI sceGnmDebuggerReadSqIndirectRegister(void);
int PS4_SYSV_ABI sceGnmDebuggerResumeWavefront(void);
int PS4_SYSV_ABI sceGnmDebuggerResumeWavefrontCreation(void);
int PS4_SYSV_ABI sceGnmDebuggerSetAddressWatch(void);
int PS4_SYSV_ABI sceGnmDebuggerWriteGds(void);
int PS4_SYSV_ABI sceGnmDebuggerWriteSqIndirectRegister(void);
int PS4_SYSV_ABI sceGnmDebugHardwareStatus(void);
s32 PS4_SYSV_ABI sceGnmDeleteEqEvent(s64 eq, u64 id);
int PS4_SYSV_ABI sceGnmDestroyWorkloadStream(void);
void PS4_SYSV_ABI sceGnmDingDong(u32 gnm_vqid, u32 next_offs_dw);
void PS4_SYSV_ABI sceGnmDingDongForWorkload(u32 gnm_vqid, u32 next_offs_dw, u64 workload_id);
int PS4_SYSV_ABI sceGnmDisableMipStatsReport(void);
s32 PS4_SYSV_ABI sceGnmDispatchDirect(u32* cmdbuf, u32 size, u32 threads_x, u32 threads_y,
                                      u32 threads_z, u32 flags);
s32 PS4_SYSV_ABI sceGnmDispatchIndirect(u32* cmdbuf, u32 size, u32 data_offset, u32 flags);
s32 PS4_SYSV_ABI sceGnmDispatchIndirectOnMec(u32* cmdbuf, u32 size, uintptr_t args, u32 modifier);
u32 PS4_SYSV_ABI sceGnmDispatchInitDefaultHardwareState(u32* cmdbuf, u32 size);
s32 PS4_SYSV_ABI sceGnmDrawIndex(u32* cmdbuf, u32 size, u32 index_count, uintptr_t index_addr,
                                 u32 flags, u32 type);
s32 PS4_SYSV_ABI sceGnmDrawIndexAuto(u32* cmdbuf, u32 size, u32 index_count, u32 flags);
s32 PS4_SYSV_ABI sceGnmDrawIndexIndirect(u32* cmdbuf, u32 size, u32 data_offset, u32 shader_stage,
                                         u32 vertex_sgpr_offset, u32 instance_sgpr_offset,
                                         u32 flags);
s32 PS4_SYSV_ABI sceGnmDrawIndexIndirectCountMulti(u32* cmdbuf, u32 size, u32 data_offset,
                                                   u32 max_count, u64 count_addr, u32 shader_stage,
                                                   u32 vertex_sgpr_offset, u32 instance_sgpr_offset,
                                                   u32 flags);
int PS4_SYSV_ABI sceGnmDrawIndexIndirectMulti(u32* cmdbuf, u32 size, u32 data_offset, u32 max_count,
                                              u32 shader_stage, u32 vertex_sgpr_offset,
                                              u32 instance_sgpr_offset, u32 flags);
int PS4_SYSV_ABI sceGnmDrawIndexMultiInstanced(void);
s32 PS4_SYSV_ABI sceGnmDrawIndexOffset(u32* cmdbuf, u32 size, u32 index_offset, u32 index_count,
                                       u32 flags);
s32 PS4_SYSV_ABI sceGnmDrawIndirect(u32* cmdbuf, u32 size, u32 data_offset, u32 shader_stage,
                                    u32 vertex_sgpr_offset, u32 instance_sgpr_offset, u32 flags);
s32 PS4_SYSV_ABI sceGnmDrawIndirectCountMulti(u32* cmdbuf, u32 size, u32 data_offset, u32 max_count,
                                              u64 count_addr, u32 shader_stage,
                                              u32 vertex_sgpr_offset, u32 instance_sgpr_offset,
                                              u32 flags);
s32 PS4_SYSV_ABI sceGnmDrawIndirectMulti(u32* cmdbuf, u32 size, u32 data_offset, u32 max_count,
                                         u32 shader_stage, u32 vertex_sgpr_offset,
                                         u32 instance_sgpr_offset, u32 flags);
u32 PS4_SYSV_ABI sceGnmDrawInitDefaultHardwareState(u32* cmdbuf, u32 size);
u32 PS4_SYSV_ABI sceGnmDrawInitDefaultHardwareState175(u32* cmdbuf, u32 size);
u32 PS4_SYSV_ABI sceGnmDrawInitDefaultHardwareState200(u32* cmdbuf, u32 size);
u32 PS4_SYSV_ABI sceGnmDrawInitDefaultHardwareState350(u32* cmdbuf, u32 size);
u32 PS4_SYSV_ABI sceGnmDrawInitToDefaultContextState(u32* cmdbuf, u32 size);
u32 PS4_SYSV_ABI sceGnmDrawInitToDefaultContextState400(u32* cmdbuf, u32 size);
int PS4_SYSV_ABI sceGnmDrawOpaqueAuto(void);
bool PS4_SYSV_ABI sceGnmDriverCaptureInProgress(void);
u32 PS4_SYSV_ABI sceGnmDriverInternalRetrieveGnmInterface(void);
u32 PS4_SYSV_ABI sceGnmDriverInternalRetrieveGnmInterfaceForGpuDebugger(void);
u32 PS4_SYSV_ABI sceGnmDriverInternalRetrieveGnmInterfaceForGpuException(void);
u32 PS4_SYSV_ABI sceGnmDriverInternalRetrieveGnmInterfaceForHDRScopes(void);
u32 PS4_SYSV_ABI sceGnmDriverInternalRetrieveGnmInterfaceForReplay(void);
u32 PS4_SYSV_ABI sceGnmDriverInternalRetrieveGnmInterfaceForResourceRegistration(void);
u32 PS4_SYSV_ABI sceGnmDriverInternalRetrieveGnmInterfaceForValidation(void);
int PS4_SYSV_ABI sceGnmDriverInternalVirtualQuery(void);
bool PS4_SYSV_ABI sceGnmDriverTraceInProgress(void);
int PS4_SYSV_ABI sceGnmDriverTriggerCapture(void);
int PS4_SYSV_ABI sceGnmEndWorkload(u64 workload);
s32 PS4_SYSV_ABI sceGnmFindResourcesPublic(void);
void PS4_SYSV_ABI sceGnmFlushGarlic(void);
int PS4_SYSV_ABI sceGnmGetCoredumpAddress(void);
int PS4_SYSV_ABI sceGnmGetCoredumpMode(void);
int PS4_SYSV_ABI sceGnmGetCoredumpProtectionFaultTimestamp(void);
int PS4_SYSV_ABI sceGnmGetDbgGcHandle(void);
int PS4_SYSV_ABI sceGnmGetDebugTimestamp(void);
int PS4_SYSV_ABI sceGnmGetEqEventType(const kernel_event_t* ev);
int PS4_SYSV_ABI sceGnmGetEqTimeStamp(void);
int PS4_SYSV_ABI sceGnmGetGpuBlockStatus(void);
u32 PS4_SYSV_ABI sceGnmGetGpuCoreClockFrequency(void);
int PS4_SYSV_ABI sceGnmGetGpuInfoStatus(void);
int PS4_SYSV_ABI sceGnmGetLastWaitedAddress(void);
int PS4_SYSV_ABI sceGnmGetNumTcaUnits(void);
int PS4_SYSV_ABI sceGnmGetOffChipTessellationBufferSize(void);
int PS4_SYSV_ABI sceGnmGetOwnerName(void);
int PS4_SYSV_ABI sceGnmGetPhysicalCounterFromVirtualized(void);
u32 PS4_SYSV_ABI sceGnmGetProtectionFaultTimeStamp(void);
int PS4_SYSV_ABI sceGnmGetResourceBaseAddressAndSizeInBytes(void);
int PS4_SYSV_ABI sceGnmGetResourceName(void);
int PS4_SYSV_ABI sceGnmGetResourceShaderGuid(void);
int PS4_SYSV_ABI sceGnmGetResourceType(void);
int PS4_SYSV_ABI sceGnmGetResourceUserData(void);
int PS4_SYSV_ABI sceGnmGetShaderProgramBaseAddress(void);
int PS4_SYSV_ABI sceGnmGetShaderStatus(void);
uintptr_t PS4_SYSV_ABI sceGnmGetTheTessellationFactorRingBufferBaseAddress(void);
void PS4_SYSV_ABI sceGnmGpuPaDebugEnter(void);
void PS4_SYSV_ABI sceGnmGpuPaDebugLeave(void);
s32 PS4_SYSV_ABI sceGnmInsertDingDongMarker(u32* cmdbuf, u32 size);
s32 PS4_SYSV_ABI sceGnmInsertPopMarker(u32* cmdbuf, u32 size);
s32 PS4_SYSV_ABI sceGnmInsertPushColorMarker(u32* cmdbuf, u32 size, const char* marker, u32 color);
s32 PS4_SYSV_ABI sceGnmInsertPushMarker(u32* cmdbuf, u32 size, const char* marker);
int PS4_SYSV_ABI sceGnmInsertSetColorMarker(void);
s32 PS4_SYSV_ABI sceGnmInsertSetMarker(u32* cmdbuf, u32 size, const char* marker);
int PS4_SYSV_ABI sceGnmInsertThreadTraceMarker(void);
s32 PS4_SYSV_ABI sceGnmInsertWaitFlipDone(u32* cmdbuf, u32 size, s32 vo_handle, u32 buf_idx);
int PS4_SYSV_ABI sceGnmIsCoredumpValid(void);
bool PS4_SYSV_ABI sceGnmIsUserPaEnabled(void);
int PS4_SYSV_ABI sceGnmLogicalCuIndexToPhysicalCuIndex(void);
s32 PS4_SYSV_ABI sceGnmLogicalCuMaskToPhysicalCuMask(s64, s32 logical_cu_mask);
int PS4_SYSV_ABI sceGnmLogicalTcaUnitToPhysical(void);
int PS4_SYSV_ABI sceGnmMapComputeQueue(u32 pipe_id, u32 queue_id, uintptr_t ring_base_addr,
                                       u32 ring_size_dw, u32* read_ptr_addr);
int PS4_SYSV_ABI sceGnmMapComputeQueueWithPriority(u32 pipe_id, u32 queue_id,
                                                   uintptr_t ring_base_addr, u32 ring_size_dw,
                                                   u32* read_ptr_addr, u32 pipePriority);
int PS4_SYSV_ABI sceGnmPaDisableFlipCallbacks(void);
int PS4_SYSV_ABI sceGnmPaEnableFlipCallbacks(void);
int PS4_SYSV_ABI sceGnmPaHeartbeat(void);
int PS4_SYSV_ABI sceGnmQueryResourceRegistrationUserMemoryRequirements(void);
int PS4_SYSV_ABI sceGnmRaiseUserExceptionEvent(void);
int PS4_SYSV_ABI sceGnmRegisterGdsResource(void);
void PS4_SYSV_ABI sceGnmRegisterGnmLiveCallbackConfig(void);
s32 PS4_SYSV_ABI sceGnmRegisterOwner(void* handle, const char* name);
s32 PS4_SYSV_ABI sceGnmRegisterResource(void* res_handle, void* owner_handle, const void* addr,
                                        size_t size, const char* name, int res_type, u64 user_data);
s32 PS4_SYSV_ABI sceGnmRequestFlipAndSubmitDone(u32* gpu_addr, u32 gpu_addr_size_in_bytes,
                                                u32 video_out_handle, u32 display_buffer_index,
                                                u32 flip_mode, s64 flip_arg);
s32 PS4_SYSV_ABI sceGnmRequestFlipAndSubmitDoneForWorkload(u64 workload, u32* gpu_addr,
                                                           u32 gpu_addr_size_in_bytes,
                                                           u32 vo_handle, u32 buf_idx,
                                                           u32 flip_mode, s64 flip_arg);
int PS4_SYSV_ABI sceGnmRequestMipStatsReportAndReset(void);
s32 PS4_SYSV_ABI sceGnmResetVgtControl(u32* cmdbuf, u32 size);
int PS4_SYSV_ABI sceGnmSdmaClose(void);
int PS4_SYSV_ABI sceGnmSdmaConstFill(void);
int PS4_SYSV_ABI sceGnmSdmaCopyLinear(void);
int PS4_SYSV_ABI sceGnmSdmaCopyTiled(void);
int PS4_SYSV_ABI sceGnmSdmaCopyWindow(void);
int PS4_SYSV_ABI sceGnmSdmaFlush(void);
int PS4_SYSV_ABI sceGnmSdmaGetMinCmdSize(void);
int PS4_SYSV_ABI sceGnmSdmaOpen(void);
s32 PS4_SYSV_ABI sceGnmSetCsShader(u32* cmdbuf, u32 size, const u32* cs_regs);
s32 PS4_SYSV_ABI sceGnmSetCsShaderWithModifier(u32* cmdbuf, u32 size, const u32* cs_regs,
                                               u32 modifier);
s32 PS4_SYSV_ABI sceGnmSetEmbeddedPsShader(u32* cmdbuf, u32 size, u32 shader_id,
                                           u32 shader_modifier);
s32 PS4_SYSV_ABI sceGnmSetEmbeddedVsShader(u32* cmdbuf, u32 size, u32 shader_id, u32 modifier);
s32 PS4_SYSV_ABI sceGnmSetEsShader(u32* cmdbuf, u32 size, const u32* es_regs, u32 shader_modifier);
s32 PS4_SYSV_ABI sceGnmSetGsRingSizes(u32 esgs_ring_size, u32 gsvs_ring_size);
s32 PS4_SYSV_ABI sceGnmSetGsShader(u32* cmdbuf, u32 size, const u32* gs_regs);
s32 PS4_SYSV_ABI sceGnmSetHsShader(u32* cmdbuf, u32 size, const u32* hs_regs, u32 param4);
s32 PS4_SYSV_ABI sceGnmSetLsShader(u32* cmdbuf, u32 size, const u32* ls_regs, u32 shader_modifier);
s32 PS4_SYSV_ABI sceGnmSetPsShader(u32* cmdbuf, u32 size, const u32* ps_regs);
s32 PS4_SYSV_ABI sceGnmSetPsShader350(u32* cmdbuf, u32 size, const u32* ps_regs);
int PS4_SYSV_ABI sceGnmSetResourceRegistrationUserMemory(void);
int PS4_SYSV_ABI sceGnmSetResourceUserData(void);
int PS4_SYSV_ABI sceGnmSetSpiEnableSqCounters(void);
int PS4_SYSV_ABI sceGnmSetSpiEnableSqCountersForUnitInstance(void);
s32 PS4_SYSV_ABI sceGnmSetupMipStatsReport(void* output_buffer, u32 size_in_bytes,
                                           u8 intervals_between_reports,
                                           u8 num_reports_before_reset, u32 mip_stats_reset_force);
s32 PS4_SYSV_ABI sceGnmSetVgtControl(u32* cmdbuf, u32 size, u32 prim_group_sz_minus_one,
                                     u32 partial_vs_wave_mode, u32 wd_switch_only_on_eop_mode);
s32 PS4_SYSV_ABI sceGnmSetVsShader(u32* cmdbuf, u32 size, const u32* vs_regs, u32 shader_modifier);
int PS4_SYSV_ABI sceGnmSetWaveLimitMultiplier(void);
int PS4_SYSV_ABI sceGnmSetWaveLimitMultipliers(void);
int PS4_SYSV_ABI sceGnmSpmEndSpm(void);
int PS4_SYSV_ABI sceGnmSpmInit(void);
int PS4_SYSV_ABI sceGnmSpmInit2(void);
int PS4_SYSV_ABI sceGnmSpmSetDelay(void);
int PS4_SYSV_ABI sceGnmSpmSetMuxRam(void);
int PS4_SYSV_ABI sceGnmSpmSetMuxRam2(void);
int PS4_SYSV_ABI sceGnmSpmSetSelectCounter(void);
int PS4_SYSV_ABI sceGnmSpmSetSpmSelects(void);
int PS4_SYSV_ABI sceGnmSpmSetSpmSelects2(void);
int PS4_SYSV_ABI sceGnmSpmStartSpm(void);
int PS4_SYSV_ABI sceGnmSqttFini(void);
int PS4_SYSV_ABI sceGnmSqttFinishTrace(void);
int PS4_SYSV_ABI sceGnmSqttGetBcInfo(void);
int PS4_SYSV_ABI sceGnmSqttGetGpuClocks(void);
int PS4_SYSV_ABI sceGnmSqttGetHiWater(void);
int PS4_SYSV_ABI sceGnmSqttGetStatus(void);
int PS4_SYSV_ABI sceGnmSqttGetTraceCounter(void);
int PS4_SYSV_ABI sceGnmSqttGetTraceWptr(void);
int PS4_SYSV_ABI sceGnmSqttGetWrapCounts(void);
int PS4_SYSV_ABI sceGnmSqttGetWrapCounts2(void);
int PS4_SYSV_ABI sceGnmSqttGetWritebackLabels(void);
int PS4_SYSV_ABI sceGnmSqttInit(void);
int PS4_SYSV_ABI sceGnmSqttSelectMode(void);
int PS4_SYSV_ABI sceGnmSqttSelectTarget(void);
int PS4_SYSV_ABI sceGnmSqttSelectTokens(void);
int PS4_SYSV_ABI sceGnmSqttSetCuPerfMask(void);
int PS4_SYSV_ABI sceGnmSqttSetDceEventWrite(void);
int PS4_SYSV_ABI sceGnmSqttSetHiWater(void);
int PS4_SYSV_ABI sceGnmSqttSetTraceBuffer2(void);
int PS4_SYSV_ABI sceGnmSqttSetTraceBuffers(void);
int PS4_SYSV_ABI sceGnmSqttSetUserData(void);
int PS4_SYSV_ABI sceGnmSqttSetUserdataTimer(void);
int PS4_SYSV_ABI sceGnmSqttStartTrace(void);
int PS4_SYSV_ABI sceGnmSqttStopTrace(void);
int PS4_SYSV_ABI sceGnmSqttSwitchTraceBuffer(void);
int PS4_SYSV_ABI sceGnmSqttSwitchTraceBuffer2(void);
int PS4_SYSV_ABI sceGnmSqttWaitForEvent(void);
s32 PS4_SYSV_ABI sceGnmSubmitAndFlipCommandBuffers(u32 count, u32* dcb_gpu_addrs[],
                                                   u32* dcb_sizes_in_bytes, u32* ccb_gpu_addrs[],
                                                   u32* ccb_sizes_in_bytes, u32 vo_handle,
                                                   u32 buf_idx, u32 flip_mode, s64 flip_arg);
s32 PS4_SYSV_ABI sceGnmSubmitAndFlipCommandBuffersForWorkload(
    u32 workload, u32 count, u32* dcb_gpu_addrs[], u32* dcb_sizes_in_bytes, u32* ccb_gpu_addrs[],
    u32* ccb_sizes_in_bytes, u32 vo_handle, u32 buf_idx, u32 flip_mode, s64 flip_arg);
s32 PS4_SYSV_ABI sceGnmSubmitCommandBuffers(u32 count, const u32* dcb_gpu_addrs[],
                                            u32* dcb_sizes_in_bytes, const u32* ccb_gpu_addrs[],
                                            u32* ccb_sizes_in_bytes);
s32 PS4_SYSV_ABI sceGnmSubmitCommandBuffersForWorkload(u32 workload, u32 count,
                                                       const u32* dcb_gpu_addrs[],
                                                       u32* dcb_sizes_in_bytes,
                                                       const u32* ccb_gpu_addrs[],
                                                       u32* ccb_sizes_in_bytes);
s32 PS4_SYSV_ABI sceGnmSubmitDone(void);
int PS4_SYSV_ABI sceGnmUnmapComputeQueue(u32 vqid);
int PS4_SYSV_ABI sceGnmUnregisterAllResourcesForOwner(void);
int PS4_SYSV_ABI sceGnmUnregisterOwnerAndResources(void);
int PS4_SYSV_ABI sceGnmUnregisterResource(void);
s32 PS4_SYSV_ABI sceGnmUpdateGsShader(u32* cmdbuf, u32 size, const u32* gs_regs);
int PS4_SYSV_ABI sceGnmUpdateHsShader(u32* cmdbuf, u32 size, const u32* ps_regs, u32 ls_hs_config);
s32 PS4_SYSV_ABI sceGnmUpdatePsShader(u32* cmdbuf, u32 size, const u32* ps_regs);
s32 PS4_SYSV_ABI sceGnmUpdatePsShader350(u32* cmdbuf, u32 size, const u32* ps_regs);
s32 PS4_SYSV_ABI sceGnmUpdateVsShader(u32* cmdbuf, u32 size, const u32* vs_regs,
                                      u32 shader_modifier);
s32 PS4_SYSV_ABI sceGnmValidateCommandBuffers(void);
int PS4_SYSV_ABI sceGnmValidateDisableDiagnostics(void);
int PS4_SYSV_ABI sceGnmValidateDisableDiagnostics2(void);
int PS4_SYSV_ABI sceGnmValidateDispatchCommandBuffers(void);
int PS4_SYSV_ABI sceGnmValidateDrawCommandBuffers(void);
int PS4_SYSV_ABI sceGnmValidateGetDiagnosticInfo(void);
int PS4_SYSV_ABI sceGnmValidateGetDiagnostics(void);
int PS4_SYSV_ABI sceGnmValidateGetVersion(void);
bool PS4_SYSV_ABI sceGnmValidateOnSubmitEnabled(void);
int PS4_SYSV_ABI sceGnmValidateResetState(void);
int PS4_SYSV_ABI sceGnmValidationRegisterMemoryCheckCallback(void);
int PS4_SYSV_ABI sceRazorCaptureCommandBuffersOnlyImmediate(void);
int PS4_SYSV_ABI sceRazorCaptureCommandBuffersOnlySinceLastFlip(void);
int PS4_SYSV_ABI sceRazorCaptureImmediate(void);
int PS4_SYSV_ABI sceRazorCaptureSinceLastFlip(void);
bool PS4_SYSV_ABI sceRazorIsLoaded(void);
int PS4_SYSV_ABI Gnm_Func_063D065A2D6359C3(void);
int PS4_SYSV_ABI Gnm_Func_0CABACAFB258429D(void);
int PS4_SYSV_ABI Gnm_Func_150CF336FC2E99A3(void);
int PS4_SYSV_ABI Gnm_Func_17CA687F9EE52D49(void);
int PS4_SYSV_ABI Gnm_Func_1870B89F759C6B45(void);
int PS4_SYSV_ABI Gnm_Func_26F9029EF68A955E(void);
int PS4_SYSV_ABI Gnm_Func_301E3DBBAB092DB0(void);
int PS4_SYSV_ABI Gnm_Func_30BAFE172AF17FEF(void);
int PS4_SYSV_ABI Gnm_Func_3E6A3E8203D95317(void);
int PS4_SYSV_ABI Gnm_Func_40FEEF0C6534C434(void);
int PS4_SYSV_ABI Gnm_Func_416B9079DE4CBACE(void);
int PS4_SYSV_ABI Gnm_Func_4774D83BB4DDBF9A(void);
int PS4_SYSV_ABI Gnm_Func_50678F1CCEEB9A00(void);
int PS4_SYSV_ABI Gnm_Func_54A2EC5FA4C62413(void);
int PS4_SYSV_ABI Gnm_Func_5A9C52C83138AE6B(void);
int PS4_SYSV_ABI Gnm_Func_5D22193A31EA1142(void);
int PS4_SYSV_ABI Gnm_Func_725A36DEBB60948D(void);
int PS4_SYSV_ABI Gnm_Func_8021A502FA61B9BB(void);
int PS4_SYSV_ABI Gnm_Func_9D002FE0FA40F0E6(void);
int PS4_SYSV_ABI Gnm_Func_9D297F36A7028B71(void);
int PS4_SYSV_ABI Gnm_Func_A2D7EC7A7BCF79B3(void);
int PS4_SYSV_ABI Gnm_Func_AA12A3CB8990854A(void);
int PS4_SYSV_ABI Gnm_Func_ADC8DDC005020BC6(void);
int PS4_SYSV_ABI Gnm_Func_B0A8688B679CB42D(void);
int PS4_SYSV_ABI Gnm_Func_B489020B5157A5FF(void);
int PS4_SYSV_ABI Gnm_Func_BADE7B4C199140DD(void);
int PS4_SYSV_ABI Gnm_Func_D1511B9DCFFB3DD9(void);
int PS4_SYSV_ABI Gnm_Func_D53446649B02E58E(void);
int PS4_SYSV_ABI Gnm_Func_D8B6E8E28E1EF0A3(void);
int PS4_SYSV_ABI Gnm_Func_D93D733A19DD7454(void);
int PS4_SYSV_ABI Gnm_Func_DE995443BC2A8317(void);
int PS4_SYSV_ABI Gnm_Func_DF6E9528150C23FF(void);
int PS4_SYSV_ABI Gnm_Func_ECB4C6BA41FE3350(void);
int PS4_SYSV_ABI sceGnmDebugModuleReset(void);
int PS4_SYSV_ABI sceGnmDebugReset(void);
int PS4_SYSV_ABI Gnm_Func_C4C328B7CF3B4171(void);
int PS4_SYSV_ABI sceGnmDrawInitToDefaultContextStateInternalCommand(u32* cmdbuf, u32 size);
int PS4_SYSV_ABI sceGnmDrawInitToDefaultContextStateInternalSize(void);
int PS4_SYSV_ABI sceGnmFindResources(void);
int PS4_SYSV_ABI sceGnmGetResourceRegistrationBuffers(void);
int PS4_SYSV_ABI sceGnmRegisterOwnerForSystem(void);
int PS4_SYSV_ABI Gnm_Func_1C43886B16EE5530(void);
int PS4_SYSV_ABI Gnm_Func_81037019ECCD0E01(void);
int PS4_SYSV_ABI Gnm_Func_BFB41C057478F0BF(void);
int PS4_SYSV_ABI Gnm_Func_E51D44DB8151238C(void);
int PS4_SYSV_ABI Gnm_Func_F916890425496553(void);

RETRO_END_DECLS

#endif
