/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_HMD_H
#define SHADPS4_HMD_H

/* libSceHmd, its distortion and reprojection parts: no headset, answered as the library answers
 * without one. MSVC C89. */

#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

/* OrbisHmdDeviceStatus */
typedef u32 OrbisHmdDeviceStatus;
#define ORBIS_HMD_DEVICE_STATUS_READY 0u
#define ORBIS_HMD_DEVICE_STATUS_NOT_READY 1u
#define ORBIS_HMD_DEVICE_STATUS_NOT_DETECTED 2u
#define ORBIS_HMD_DEVICE_STATUS_NOT_READY_HMU_DISCONNECT 3u

/* ORBIS_USER_SERVICE_USER_ID_INVALID and ORBIS_USER_SERVICE_USER_ID_SYSTEM */
#define HMD_USER_ID_INVALID (-1)
#define HMD_USER_ID_SYSTEM 0xFF

typedef struct OrbisHmdInitializeParam {
    void* reserved0;
    u8 reserved[8];
} OrbisHmdInitializeParam;

typedef struct OrbisHmdOpenParam {
    u8 reserve[32];
} OrbisHmdOpenParam;

typedef struct OrbisHmdFieldOfView {
    float tan_out;
    float tan_in;
    float tan_top;
    float tan_bottom;
} OrbisHmdFieldOfView;

typedef struct OrbisHmdPanelResolution {
    u32 width;
    u32 height;
} OrbisHmdPanelResolution;

typedef struct OrbisHmdFlipToDisplayLatency {
    u16 refresh_rate_90hz;
    u16 refresh_rate_120hz;
} OrbisHmdFlipToDisplayLatency;

typedef struct OrbisHmdDeviceInfo {
    OrbisHmdPanelResolution panel_resolution;
    OrbisHmdFlipToDisplayLatency flip_to_display_latency;
} OrbisHmdDeviceInfo;

typedef struct OrbisHmdDeviceInformation {
    OrbisHmdDeviceStatus status;
    s32 user_id;
    u8 reserve0[4];
    OrbisHmdDeviceInfo device_info;
    u8 hmu_mount;
    u8 reserve1[7];
} OrbisHmdDeviceInformation;

typedef struct OrbisHmdEyeOffset {
    float offset_x;
    float offset_y;
    float offset_z;
    u8 reserve[20];
} OrbisHmdEyeOffset;

/* The game's firmware version, read once before the game calls the library (libs.cpp) */
void hmd_library_init(void);

s32 PS4_SYSV_ABI sceHmdReprojectionStartMultilayer(void);
s32 PS4_SYSV_ABI sceHmdReprojectionAddDisplayBuffer(void);
s32 PS4_SYSV_ABI sceHmdReprojectionClearUserEventEnd(void);
s32 PS4_SYSV_ABI sceHmdReprojectionClearUserEventStart(void);
s32 PS4_SYSV_ABI sceHmdReprojectionDebugGetLastInfo(void);
s32 PS4_SYSV_ABI sceHmdReprojectionDebugGetLastInfoMultilayer(void);
s32 PS4_SYSV_ABI sceHmdReprojectionFinalize(void);
s32 PS4_SYSV_ABI sceHmdReprojectionFinalizeCapture(void);
s32 PS4_SYSV_ABI sceHmdReprojectionInitialize(void);
s32 PS4_SYSV_ABI sceHmdReprojectionInitializeCapture(void);
s32 PS4_SYSV_ABI sceHmdReprojectionQueryGarlicBuffAlign(void);
s32 PS4_SYSV_ABI sceHmdReprojectionQueryGarlicBuffSize(void);
s32 PS4_SYSV_ABI sceHmdReprojectionQueryOnionBuffAlign(void);
s32 PS4_SYSV_ABI sceHmdReprojectionQueryOnionBuffSize(void);
s32 PS4_SYSV_ABI sceHmdReprojectionSetCallback(void);
s32 PS4_SYSV_ABI sceHmdReprojectionSetDisplayBuffers(void);
s32 PS4_SYSV_ABI sceHmdReprojectionSetOutputMinColor(void);
s32 PS4_SYSV_ABI sceHmdReprojectionSetUserEventEnd(void);
s32 PS4_SYSV_ABI sceHmdReprojectionSetUserEventStart(void);
s32 PS4_SYSV_ABI sceHmdReprojectionStart(void);
s32 PS4_SYSV_ABI sceHmdReprojectionStart2dVr(void);
s32 PS4_SYSV_ABI sceHmdReprojectionStartCapture(void);
s32 PS4_SYSV_ABI sceHmdReprojectionStartLiveCapture(void);
s32 PS4_SYSV_ABI sceHmdReprojectionStartMultilayer2(void);
s32 PS4_SYSV_ABI sceHmdReprojectionStartWideNear(void);
s32 PS4_SYSV_ABI sceHmdReprojectionStartWideNearWithOverlay(void);
s32 PS4_SYSV_ABI sceHmdReprojectionStartWithOverlay(void);
s32 PS4_SYSV_ABI sceHmdReprojectionStop(void);
s32 PS4_SYSV_ABI sceHmdReprojectionStopCapture(void);
s32 PS4_SYSV_ABI sceHmdReprojectionStopLiveCapture(void);
s32 PS4_SYSV_ABI sceHmdReprojectionUnsetCallback(void);
s32 PS4_SYSV_ABI sceHmdReprojectionUnsetDisplayBuffers(void);
s32 PS4_SYSV_ABI sceHmdDistortionGet2dVrCommand(void);
s32 PS4_SYSV_ABI sceHmdDistortionGetCompoundEyeCorrectionCommand(void);
s32 PS4_SYSV_ABI sceHmdDistortionGetCorrectionCommand(void);
s32 PS4_SYSV_ABI sceHmdDistortionGetWideNearCorrectionCommand(void);
s32 PS4_SYSV_ABI sceHmdDistortionGetWorkMemoryAlign(void);
s32 PS4_SYSV_ABI sceHmdDistortionGetWorkMemorySize(void);
s32 PS4_SYSV_ABI sceHmdDistortionInitialize(void* reserved);
s32 PS4_SYSV_ABI sceHmdDistortionSetOutputMinColor(void);
s32 PS4_SYSV_ABI sceHmdDistortionTerminate(void);
s32 PS4_SYSV_ABI sceHmdGetDistortionParams(void);
s32 PS4_SYSV_ABI Hmd_Func_B26430EA74FC3DC0(void);
s32 PS4_SYSV_ABI Hmd_Func_B614F290B67FB59B(void);
s32 PS4_SYSV_ABI sceHmdClose(s32 handle);
s32 PS4_SYSV_ABI sceHmdGet2DEyeOffset(s32 handle, OrbisHmdEyeOffset* left_offset,
                                      OrbisHmdEyeOffset* right_offset);
s32 PS4_SYSV_ABI sceHmdGetAssyError(void* data);
s32 PS4_SYSV_ABI sceHmdGetDeviceInformation(OrbisHmdDeviceInformation* info);
s32 PS4_SYSV_ABI sceHmdGetDeviceInformationByHandle(s32 handle, OrbisHmdDeviceInformation* info);
s32 PS4_SYSV_ABI sceHmdGetFieldOfView(s32 handle, OrbisHmdFieldOfView* field_of_view);
s32 PS4_SYSV_ABI sceHmdGetInertialSensorData(s32 handle, void* data, s32 unk);
s32 PS4_SYSV_ABI sceHmdInitialize(const OrbisHmdInitializeParam* param);
s32 PS4_SYSV_ABI sceHmdInitialize315(const OrbisHmdInitializeParam* param);
s32 PS4_SYSV_ABI sceHmdOpen(s32 user_id, s32 type, s32 index, OrbisHmdOpenParam* param);
s32 PS4_SYSV_ABI sceHmdTerminate(void);
s32 PS4_SYSV_ABI sceHmdInternal3dAudioClose(void);
s32 PS4_SYSV_ABI sceHmdInternal3dAudioOpen(void);
s32 PS4_SYSV_ABI sceHmdInternal3dAudioSendData(void);
s32 PS4_SYSV_ABI sceHmdInternalAnotherScreenClose(void);
s32 PS4_SYSV_ABI sceHmdInternalAnotherScreenGetAudioStatus(void);
s32 PS4_SYSV_ABI sceHmdInternalAnotherScreenGetFadeState(void);
s32 PS4_SYSV_ABI sceHmdInternalAnotherScreenGetVideoStatus(void);
s32 PS4_SYSV_ABI sceHmdInternalAnotherScreenOpen(void);
s32 PS4_SYSV_ABI sceHmdInternalAnotherScreenSendAudio(void);
s32 PS4_SYSV_ABI sceHmdInternalAnotherScreenSendVideo(void);
s32 PS4_SYSV_ABI sceHmdInternalAnotherScreenSetFadeAndSwitch(void);
s32 PS4_SYSV_ABI sceHmdInternalBindDeviceWithUserId(void);
s32 PS4_SYSV_ABI sceHmdInternalCheckDeviceModelMk3(void);
s32 PS4_SYSV_ABI sceHmdInternalCheckS3dPassModeAvailable(void);
s32 PS4_SYSV_ABI sceHmdInternalCrashReportCancel(void);
s32 PS4_SYSV_ABI sceHmdInternalCrashReportClose(void);
s32 PS4_SYSV_ABI sceHmdInternalCrashReportOpen(void);
s32 PS4_SYSV_ABI sceHmdInternalCrashReportReadData(void);
s32 PS4_SYSV_ABI sceHmdInternalCrashReportReadDataSize(void);
s32 PS4_SYSV_ABI sceHmdInternalCreateSharedMemory(void);
s32 PS4_SYSV_ABI sceHmdInternalDfuCheckAfterPvt(void);
s32 PS4_SYSV_ABI sceHmdInternalDfuCheckPartialUpdateAvailable(void);
s32 PS4_SYSV_ABI sceHmdInternalDfuClose(void);
s32 PS4_SYSV_ABI sceHmdInternalDfuGetStatus(void);
s32 PS4_SYSV_ABI sceHmdInternalDfuOpen(void);
s32 PS4_SYSV_ABI sceHmdInternalDfuReset(void);
s32 PS4_SYSV_ABI sceHmdInternalDfuSend(void);
s32 PS4_SYSV_ABI sceHmdInternalDfuSendSize(void);
s32 PS4_SYSV_ABI sceHmdInternalDfuSetMode(void);
s32 PS4_SYSV_ABI sceHmdInternalDfuStart(void);
s32 PS4_SYSV_ABI sceHmdInternalEventInitialize(void);
s32 PS4_SYSV_ABI sceHmdInternalGetBrightness(void);
s32 PS4_SYSV_ABI sceHmdInternalGetCrashDumpInfo(void);
s32 PS4_SYSV_ABI sceHmdInternalGetDebugMode(void);
s32 PS4_SYSV_ABI sceHmdInternalGetDebugSocialScreenMode(void);
s32 PS4_SYSV_ABI sceHmdInternalGetDebugTextMode(void);
s32 PS4_SYSV_ABI sceHmdInternalGetDefaultLedData(void);
s32 PS4_SYSV_ABI sceHmdInternalGetDemoMode(void);
s32 PS4_SYSV_ABI sceHmdInternalGetDeviceInformation(void);
s32 PS4_SYSV_ABI sceHmdInternalGetDeviceInformationByHandle(void);
s32 PS4_SYSV_ABI sceHmdInternalGetDeviceStatus(OrbisHmdDeviceStatus* status);
s32 PS4_SYSV_ABI sceHmdInternalGetEyeStatus(void);
s32 PS4_SYSV_ABI sceHmdInternalGetHmuOpticalParam(void);
s32 PS4_SYSV_ABI sceHmdInternalGetHmuPowerStatusForDebug(void);
s32 PS4_SYSV_ABI sceHmdInternalGetHmuSerialNumber(void);
s32 PS4_SYSV_ABI sceHmdInternalGetIPD(void);
s32 PS4_SYSV_ABI sceHmdInternalGetIpdSettingEnableForSystemService(void);
s32 PS4_SYSV_ABI sceHmdInternalGetPuBuildNumber(void);
s32 PS4_SYSV_ABI sceHmdInternalGetPuPositionParam(void);
s32 PS4_SYSV_ABI sceHmdInternalGetPuRevision(void);
s32 PS4_SYSV_ABI sceHmdInternalGetPUSerialNumber(void);
s32 PS4_SYSV_ABI sceHmdInternalGetPUVersion(void);
s32 PS4_SYSV_ABI sceHmdInternalGetRequiredPUPVersion(void);
s32 PS4_SYSV_ABI sceHmdInternalGetStatusReport(void);
s32 PS4_SYSV_ABI sceHmdInternalGetTv4kCapability(void);
s32 PS4_SYSV_ABI sceHmdInternalGetVirtualDisplayDepth(void);
s32 PS4_SYSV_ABI sceHmdInternalGetVirtualDisplayHeight(void);
s32 PS4_SYSV_ABI sceHmdInternalGetVirtualDisplaySize(void);
s32 PS4_SYSV_ABI sceHmdInternalGetVr2dData(void);
s32 PS4_SYSV_ABI sceHmdInternalIsCommonDlgMiniAppVr2d(void);
s32 PS4_SYSV_ABI sceHmdInternalIsCommonDlgVr2d(void);
s32 PS4_SYSV_ABI sceHmdInternalIsGameVr2d(void);
s32 PS4_SYSV_ABI sceHmdInternalIsMiniAppVr2d(void);
s32 PS4_SYSV_ABI sceHmdInternalMapSharedMemory(void);
s32 PS4_SYSV_ABI sceHmdInternalMirroringModeSetAspect(void);
s32 PS4_SYSV_ABI sceHmdInternalMirroringModeSetAspectDebug(void);
s32 PS4_SYSV_ABI sceHmdInternalMmapGetCount(void);
s32 PS4_SYSV_ABI sceHmdInternalMmapGetModeId(void);
s32 PS4_SYSV_ABI sceHmdInternalMmapGetSensorCalibrationData(void);
s32 PS4_SYSV_ABI sceHmdInternalMmapIsConnect(void);
s32 PS4_SYSV_ABI sceHmdInternalPushVr2dData(void);
s32 PS4_SYSV_ABI sceHmdInternalRegisterEventCallback(void);
s32 PS4_SYSV_ABI sceHmdInternalResetInertialSensor(void);
s32 PS4_SYSV_ABI sceHmdInternalResetLedForVrTracker(void);
s32 PS4_SYSV_ABI sceHmdInternalResetLedForVsh(void);
s32 PS4_SYSV_ABI sceHmdInternalSeparateModeClose(void);
s32 PS4_SYSV_ABI sceHmdInternalSeparateModeGetAudioStatus(void);
s32 PS4_SYSV_ABI sceHmdInternalSeparateModeGetVideoStatus(void);
s32 PS4_SYSV_ABI sceHmdInternalSeparateModeOpen(void);
s32 PS4_SYSV_ABI sceHmdInternalSeparateModeSendAudio(void);
s32 PS4_SYSV_ABI sceHmdInternalSeparateModeSendVideo(void);
s32 PS4_SYSV_ABI sceHmdInternalSetBrightness(void);
s32 PS4_SYSV_ABI sceHmdInternalSetCrashReportCommand(void);
s32 PS4_SYSV_ABI sceHmdInternalSetDebugGpo(void);
s32 PS4_SYSV_ABI sceHmdInternalSetDebugMode(void);
s32 PS4_SYSV_ABI sceHmdInternalSetDebugSocialScreenMode(void);
s32 PS4_SYSV_ABI sceHmdInternalSetDebugTextMode(void);
s32 PS4_SYSV_ABI sceHmdInternalSetDefaultLedData(void);
s32 PS4_SYSV_ABI sceHmdInternalSetDemoMode(void);
s32 PS4_SYSV_ABI sceHmdInternalSetDeviceConnection(void);
s32 PS4_SYSV_ABI sceHmdInternalSetForcedCrash(void);
s32 PS4_SYSV_ABI sceHmdInternalSetHmuPowerControl(void);
s32 PS4_SYSV_ABI sceHmdInternalSetHmuPowerControlForDebug(void);
s32 PS4_SYSV_ABI sceHmdInternalSetIPD(void);
s32 PS4_SYSV_ABI sceHmdInternalSetIpdSettingEnableForSystemService(void);
s32 PS4_SYSV_ABI sceHmdInternalSetLedOn(void);
s32 PS4_SYSV_ABI sceHmdInternalSetM2LedBrightness(void);
s32 PS4_SYSV_ABI sceHmdInternalSetM2LedOn(void);
s32 PS4_SYSV_ABI sceHmdInternalSetPortConnection(void);
s32 PS4_SYSV_ABI sceHmdInternalSetPortStatus(void);
s32 PS4_SYSV_ABI sceHmdInternalSetS3dPassMode(void);
s32 PS4_SYSV_ABI sceHmdInternalSetSidetone(void);
s32 PS4_SYSV_ABI sceHmdInternalSetUserType(void);
s32 PS4_SYSV_ABI sceHmdInternalSetVirtualDisplayDepth(void);
s32 PS4_SYSV_ABI sceHmdInternalSetVirtualDisplayHeight(void);
s32 PS4_SYSV_ABI sceHmdInternalSetVirtualDisplaySize(void);
s32 PS4_SYSV_ABI sceHmdInternalSetVRMode(void);
s32 PS4_SYSV_ABI sceHmdInternalSocialScreenGetFadeState(void);
s32 PS4_SYSV_ABI sceHmdInternalSocialScreenSetFadeAndSwitch(void);
s32 PS4_SYSV_ABI sceHmdInternalSocialScreenSetOutput(void);
s32 PS4_SYSV_ABI Hmd_Func_202D0D1A687FCD2F(void);
s32 PS4_SYSV_ABI Hmd_Func_358DBF818A3D8A12(void);
s32 PS4_SYSV_ABI Hmd_Func_5CCBADA76FE8F40E(void);
s32 PS4_SYSV_ABI Hmd_Func_63D403167DC08CF0(void);
s32 PS4_SYSV_ABI Hmd_Func_69383B2B4E3AEABF(s32 handle, void* data, s32 unk);
s32 PS4_SYSV_ABI Hmd_Func_791560C32F4F6D68(void);
s32 PS4_SYSV_ABI Hmd_Func_7C955961EA85B6D3(void);
s32 PS4_SYSV_ABI Hmd_Func_9952277839236BA7(void);
s32 PS4_SYSV_ABI Hmd_Func_9A276E739E54EEAF(void);
s32 PS4_SYSV_ABI Hmd_Func_9E501994E289CBE7(void);
s32 PS4_SYSV_ABI Hmd_Func_A31A0320D80EAD99(void);
s32 PS4_SYSV_ABI Hmd_Func_A31F4DA8B3BD2E12(void);
s32 PS4_SYSV_ABI Hmd_Func_A92D7C23AC364993(void);
s32 PS4_SYSV_ABI Hmd_Func_ADCCC25CB876FDBE(void);
s32 PS4_SYSV_ABI Hmd_Func_B16652641FE69F0E(void);
s32 PS4_SYSV_ABI Hmd_Func_B9A6FA0735EC7E49(void);
s32 PS4_SYSV_ABI Hmd_Func_FC193BD653F2AF2E(void);
s32 PS4_SYSV_ABI Hmd_Func_FF2E0E53015FE231(void);

RETRO_END_DECLS

#endif
