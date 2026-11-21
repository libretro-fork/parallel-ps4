/* SPDX-FileCopyrightText: Copyright 2025-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_CAMERA_H
#define SHADPS4_LIBRARIES_CAMERA_H

/* libSceCamera. No camera is attached until the frontend's is passed through (camera_backend.h).
 * Registered in libs.cpp. */

#include <boolean.h>
#include <retro_common_api.h>

#include "common/types.h"
#include "core/libraries/camera/camera_config_types.h"
#include "core/libraries/camera/camera_error.h"

RETRO_BEGIN_DECLS

/* The library's state, shared by its files */
extern bool g_library_opened;
extern s32 g_firmware_version;

#define ORBIS_CAMERA_MAX_DEVICE_NUM ((int32_t)2)
/* The system user (ORBIS_USER_SERVICE_USER_ID_SYSTEM) */
#define CAMERA_USER_ID_SYSTEM 0xFF
#define ORBIS_CAMERA_MAX_FORMAT_LEVEL_NUM ((int32_t)4)
#define ORBIS_CAMERA_CALIBRATION_DATA_MESH_NUMBER_OF_TOTAL_VERTICIES ((int32_t)1276)

typedef enum OrbisCameraChannel {
    ORBIS_CAMERA_CHANNEL_0 = 1,
    ORBIS_CAMERA_CHANNEL_1 = 2,
    ORBIS_CAMERA_CHANNEL_BOTH = 3
} OrbisCameraChannel;

typedef struct OrbisCameraOpenParameter {
    u32 sizeThis;
    u32 reserved1;
    u32 reserved2;
    u32 reserved3;
} OrbisCameraOpenParameter;

typedef enum OrbisCameraCalibrationDataFunctionType {
    ORBIS_CAMERA_CALIBRATION_DATA_FUNCTION_TYPE_IMAGE_RECTIFICATION = 0,
    ORBIS_CAMERA_CALIBRATION_DATA_FUNCTION_TYPE_IMAGE_INVERSE_RECTIFICATION = 1
} OrbisCameraCalibrationDataFunctionType;

typedef struct OrbisCameraConfig {
    u32 sizeThis;
    OrbisCameraConfigType configType;
    OrbisCameraConfigExtention configExtention[ORBIS_CAMERA_MAX_DEVICE_NUM];
} OrbisCameraConfig;

extern OrbisCameraConfigExtention output_config0, output_config1;

typedef enum OrbisCameraAecAgcTarget {
    ORBIS_CAMERA_ATTRIBUTE_AECAGC_TARGET_DEF = 0x00,
    ORBIS_CAMERA_ATTRIBUTE_AECAGC_TARGET_2_0 = 0x20,
    ORBIS_CAMERA_ATTRIBUTE_AECAGC_TARGET_1_6 = 0x16,
    ORBIS_CAMERA_ATTRIBUTE_AECAGC_TARGET_1_4 = 0x14,
    ORBIS_CAMERA_ATTRIBUTE_AECAGC_TARGET_1_2 = 0x12,
    ORBIS_CAMERA_ATTRIBUTE_AECAGC_TARGET_1_0 = 0x10,
    ORBIS_CAMERA_ATTRIBUTE_AECAGC_TARGET_0_8 = 0x08,
    ORBIS_CAMERA_ATTRIBUTE_AECAGC_TARGET_0_6 = 0x06,
    ORBIS_CAMERA_ATTRIBUTE_AECAGC_TARGET_0_4 = 0x04,
    ORBIS_CAMERA_ATTRIBUTE_AECAGC_TARGET_0_2 = 0x02
} OrbisCameraAecAgcTarget;

typedef struct OrbisCameraDeviceInfo {
    u32 sizeThis;
    u32 infoRevision;
    u32 deviceRevision;
    u32 padding;
} OrbisCameraDeviceInfo;

typedef struct OrbisCameraStartParameter {
    u32 sizeThis;
    u32 formatLevel[ORBIS_CAMERA_MAX_DEVICE_NUM];
    void* pStartOption;
} OrbisCameraStartParameter;

typedef struct OrbisCameraVideoSyncParameter {
    u32 sizeThis;
    u32 videoSyncMode;
    void* pModeOption;
} OrbisCameraVideoSyncParameter;

typedef struct OrbisCameraFramePosition {
    u32 x;
    u32 y;
    u32 xSize;
    u32 ySize;
} OrbisCameraFramePosition;

typedef struct OrbisCameraAutoExposureGainTarget {
    u32 sizeThis;
    OrbisCameraAecAgcTarget target;
} OrbisCameraAutoExposureGainTarget;

typedef struct OrbisCameraExposureGain {
    u32 exposureControl;
    u32 exposure;
    u32 gain;
    u32 mode;
} OrbisCameraExposureGain;

typedef struct OrbisCameraWhiteBalance {
    u32 whiteBalanceControl;
    u32 gainRed;
    u32 gainBlue;
    u32 gainGreen;
} OrbisCameraWhiteBalance;

typedef struct OrbisCameraGamma {
    u32 gammaControl;
    u32 value;
    u8 reserved[16];
} OrbisCameraGamma;

typedef struct OrbisCameraMeta {
    u32 metaMode;
    u32 format[ORBIS_CAMERA_MAX_DEVICE_NUM][ORBIS_CAMERA_MAX_FORMAT_LEVEL_NUM];
    u64 frame[ORBIS_CAMERA_MAX_DEVICE_NUM];
    u64 timestamp[ORBIS_CAMERA_MAX_DEVICE_NUM];
    u32 deviceTimestamp[ORBIS_CAMERA_MAX_DEVICE_NUM];
    OrbisCameraExposureGain exposureGain[ORBIS_CAMERA_MAX_DEVICE_NUM];
    OrbisCameraWhiteBalance whiteBalance[ORBIS_CAMERA_MAX_DEVICE_NUM];
    OrbisCameraGamma gamma[ORBIS_CAMERA_MAX_DEVICE_NUM];
    u32 luminance[ORBIS_CAMERA_MAX_DEVICE_NUM];
    float acceleration_x;
    float acceleration_y;
    float acceleration_z;
    u64 vcounter;
    u32 reserved[14];
} OrbisCameraMeta;

typedef struct OrbisCameraFrameData {
    u32 sizeThis;
    u32 readMode;
    OrbisCameraFramePosition framePosition[ORBIS_CAMERA_MAX_DEVICE_NUM]
                                          [ORBIS_CAMERA_MAX_FORMAT_LEVEL_NUM];
    void* pFramePointerList[ORBIS_CAMERA_MAX_DEVICE_NUM][ORBIS_CAMERA_MAX_FORMAT_LEVEL_NUM];
    u32 frameSize[ORBIS_CAMERA_MAX_DEVICE_NUM][ORBIS_CAMERA_MAX_FORMAT_LEVEL_NUM];
    u32 status[ORBIS_CAMERA_MAX_DEVICE_NUM];
    OrbisCameraMeta meta;
    void* pFramePointerListGarlic[ORBIS_CAMERA_MAX_DEVICE_NUM][ORBIS_CAMERA_MAX_FORMAT_LEVEL_NUM];
} OrbisCameraFrameData;

typedef struct OrbisCameraAttribute {
    u32 sizeThis;
    OrbisCameraChannel channel;
    OrbisCameraFramePosition framePosition;
    OrbisCameraExposureGain exposureGain;
    OrbisCameraWhiteBalance whiteBalance;
    OrbisCameraGamma gamma;
    u32 saturation;
    u32 contrast;
    u32 sharpness;
    s32 hue;
    u32 reserved1;
    u32 reserved2;
    u32 reserved3;
    u32 reserved4;
} OrbisCameraAttribute;

typedef struct OrbisCameraGetCalibrationDataParameter {
    u32 size;
    u32 format_type;
    OrbisCameraCalibrationDataFunctionType function_type;
} OrbisCameraGetCalibrationDataParameter;

typedef struct OrbisCameraCalibrationDataElement {
    u32 total_horizontal_verticies;
    u32 total_vertical_verticies;
    u32 left_margin;
    u32 right_margin;
    u32 top_margin;
    u32 bottom_margin;
    float x_table[ORBIS_CAMERA_CALIBRATION_DATA_MESH_NUMBER_OF_TOTAL_VERTICIES];
    float y_table[ORBIS_CAMERA_CALIBRATION_DATA_MESH_NUMBER_OF_TOTAL_VERTICIES];
} OrbisCameraCalibrationDataElement;

typedef struct OrbisCameraCalibrationData {
    u32 format_type;
    OrbisCameraCalibrationDataFunctionType function_type;
    OrbisCameraCalibrationDataElement data[2];
} OrbisCameraCalibrationData;

s32 PS4_SYSV_ABI sceCameraAccGetData(void);
s32 PS4_SYSV_ABI sceCameraAudioClose(void);
s32 PS4_SYSV_ABI sceCameraAudioGetData(void* data);
s32 PS4_SYSV_ABI sceCameraAudioGetData2(void* data);
s32 PS4_SYSV_ABI sceCameraAudioOpen(void);
s32 PS4_SYSV_ABI sceCameraAudioReset(void);
s32 PS4_SYSV_ABI sceCameraChangeAppModuleState(void);
s32 PS4_SYSV_ABI sceCameraClose(s32 handle);
s32 PS4_SYSV_ABI sceCameraCloseByHandle(s32 handle);
s32 PS4_SYSV_ABI sceCameraDeviceOpen(void);
s32 PS4_SYSV_ABI sceCameraGetAttribute(s32 handle, OrbisCameraAttribute* attribute);
s32 PS4_SYSV_ABI sceCameraGetAutoExposureGain(s32 handle, OrbisCameraChannel channel, u32* enable,
                                              OrbisCameraAutoExposureGainTarget* option);
s32 PS4_SYSV_ABI sceCameraGetAutoWhiteBalance(s32 handle, OrbisCameraChannel channel, u32* enable,
                                              void* option);
s32 PS4_SYSV_ABI sceCameraGetCalibData(s32 handle, void* calib_data, void* maybe_reserved);
s32 PS4_SYSV_ABI sceCameraGetCalibDataFromDevice(void);
s32 PS4_SYSV_ABI sceCameraGetCalibrationData(const OrbisCameraGetCalibrationDataParameter* param,
                                             OrbisCameraCalibrationData* calibration_data);
s32 PS4_SYSV_ABI sceCameraGetConfig(s32 handle, OrbisCameraConfig* config);
s32 PS4_SYSV_ABI sceCameraGetContrast(s32 handle, OrbisCameraChannel channel, u32* contrast,
                                      void* option);
s32 PS4_SYSV_ABI sceCameraGetDefectivePixelCancellation(s32 handle, OrbisCameraChannel channel,
                                                        u32* enable, void* option);
s32 PS4_SYSV_ABI sceCameraGetDeviceConfig(s32 handle, OrbisCameraConfig* config);
s32 PS4_SYSV_ABI sceCameraGetDeviceConfigWithoutHandle(OrbisCameraConfig* config);
s32 PS4_SYSV_ABI sceCameraGetDeviceID(void);
s32 PS4_SYSV_ABI sceCameraGetDeviceIDWithoutOpen(void);
s32 PS4_SYSV_ABI sceCameraGetDeviceInfo(s32 reserved, OrbisCameraDeviceInfo* device_info);
s32 PS4_SYSV_ABI sceCameraGetExposureGain(s32 handle, OrbisCameraChannel channel,
                                          OrbisCameraExposureGain* exposure_gain, void* option);
s32 PS4_SYSV_ABI sceCameraGetFrameData(s32 handle, OrbisCameraFrameData* frame_data);
s32 PS4_SYSV_ABI sceCameraGetGamma(s32 handle, OrbisCameraChannel channel, OrbisCameraGamma* gamma,
                                   void* option);
s32 PS4_SYSV_ABI sceCameraGetHue(s32 handle, OrbisCameraChannel channel, s32* hue, void* option);
s32 PS4_SYSV_ABI sceCameraGetLensCorrection(s32 handle, OrbisCameraChannel channel, u32* enable,
                                            void* option);
s32 PS4_SYSV_ABI sceCameraGetMmapConnectedCount(u32* count);
s32 PS4_SYSV_ABI sceCameraGetProductInfo(void* product_info);
s32 PS4_SYSV_ABI sceCameraGetRegister(void);
s32 PS4_SYSV_ABI sceCameraGetRegistryInfo(void* registry_info);
s32 PS4_SYSV_ABI sceCameraGetSaturation(s32 handle, OrbisCameraChannel channel, u32* saturation,
                                        void* option);
s32 PS4_SYSV_ABI sceCameraGetSharpness(s32 handle, OrbisCameraChannel channel, u32* sharpness,
                                       void* option);
s32 PS4_SYSV_ABI sceCameraGetVrCaptureInfo(void);
s32 PS4_SYSV_ABI sceCameraGetWhiteBalance(s32 handle, OrbisCameraChannel channel,
                                          OrbisCameraWhiteBalance* white_balance, void* option);
s32 PS4_SYSV_ABI sceCameraInitializeRegistryCalibData(void);
s32 PS4_SYSV_ABI sceCameraIsAttached(s32 index);
s32 PS4_SYSV_ABI sceCameraIsConfigChangeDone(void);
s32 PS4_SYSV_ABI sceCameraIsValidFrameData(s32 handle, OrbisCameraFrameData* frame_data);
s32 PS4_SYSV_ABI sceCameraOpen(s32 user_id, s32 type, s32 index, OrbisCameraOpenParameter* param);
s32 PS4_SYSV_ABI sceCameraOpenByModuleId(void);
s32 PS4_SYSV_ABI sceCameraRemoveAppModuleFocus(void);
s32 PS4_SYSV_ABI sceCameraSetAppModuleFocus(void);
s32 PS4_SYSV_ABI sceCameraSetAttribute(s32 handle, OrbisCameraAttribute* attribute);
s32 PS4_SYSV_ABI sceCameraSetAttributeInternal(void);
s32 PS4_SYSV_ABI sceCameraSetAutoExposureGain(s32 handle, OrbisCameraChannel channel, u32 enable,
                                              OrbisCameraAutoExposureGainTarget* option);
s32 PS4_SYSV_ABI sceCameraSetAutoWhiteBalance(s32 handle, OrbisCameraChannel channel, u32 enable,
                                              void* option);
s32 PS4_SYSV_ABI sceCameraSetCalibData(void);
s32 PS4_SYSV_ABI sceCameraSetConfig(s32 handle, OrbisCameraConfig* config);
s32 PS4_SYSV_ABI sceCameraSetConfigInternal(s32 handle, OrbisCameraConfig* config);
s32 PS4_SYSV_ABI sceCameraSetContrast(s32 handle, OrbisCameraChannel channel, u32 contrast,
                                      void* option);
s32 PS4_SYSV_ABI sceCameraSetDebugStop(u32 debug_stop_enable);
s32 PS4_SYSV_ABI sceCameraSetDefectivePixelCancellation(s32 handle, OrbisCameraChannel channel,
                                                        u32 enable, void* option);
s32 PS4_SYSV_ABI sceCameraSetDefectivePixelCancellationInternal(s32 handle,
                                                                OrbisCameraChannel channel,
                                                                u32 enable, void* option);
s32 PS4_SYSV_ABI sceCameraSetExposureGain(s32 handle, OrbisCameraChannel channel,
                                          OrbisCameraExposureGain* exposure_gain, void* option);
s32 PS4_SYSV_ABI sceCameraSetForceActivate(void);
s32 PS4_SYSV_ABI sceCameraSetGamma(s32 handle, OrbisCameraChannel channel, OrbisCameraGamma* gamma,
                                   void* option);
s32 PS4_SYSV_ABI sceCameraSetHue(s32 handle, OrbisCameraChannel channel, s32 hue, void* option);
s32 PS4_SYSV_ABI sceCameraSetLensCorrection(s32 handle, OrbisCameraChannel channel, u32 enable,
                                            void* option);
s32 PS4_SYSV_ABI sceCameraSetLensCorrectionInternal(s32 handle, OrbisCameraChannel channel,
                                                    u32 enable, void* option);
s32 PS4_SYSV_ABI sceCameraSetProcessFocus(void);
s32 PS4_SYSV_ABI sceCameraSetProcessFocusByHandle(void);
s32 PS4_SYSV_ABI sceCameraSetRegister(void);
s32 PS4_SYSV_ABI sceCameraSetSaturation(s32 handle, OrbisCameraChannel channel, u32 saturation,
                                        void* option);
s32 PS4_SYSV_ABI sceCameraSetSharpness(s32 handle, OrbisCameraChannel channel, u32 sharpness,
                                       void* option);
s32 PS4_SYSV_ABI sceCameraSetTrackerMode(void);
s32 PS4_SYSV_ABI sceCameraSetUacModeInternal(void);
s32 PS4_SYSV_ABI sceCameraSetVideoSync(s32 handle, OrbisCameraVideoSyncParameter* video_sync);
s32 PS4_SYSV_ABI sceCameraSetVideoSyncInternal(s32 handle,
                                               OrbisCameraVideoSyncParameter* video_sync);
s32 PS4_SYSV_ABI sceCameraSetWhiteBalance(s32 handle, OrbisCameraChannel channel,
                                          OrbisCameraWhiteBalance* white_balance, void* option);
s32 PS4_SYSV_ABI sceCameraStart(s32 handle, OrbisCameraStartParameter* param);
s32 PS4_SYSV_ABI sceCameraStartByHandle(s32 handle, OrbisCameraStartParameter* param);
s32 PS4_SYSV_ABI sceCameraStop(s32 handle);
s32 PS4_SYSV_ABI sceCameraStopByHandle(s32 handle);

RETRO_END_DECLS

#endif
