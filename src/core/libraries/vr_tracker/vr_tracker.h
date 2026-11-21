/* SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_VR_TRACKER_H
#define SHADPS4_LIBRARIES_VR_TRACKER_H

/* libSceVrTracker. No headset is ever tracked. Registered in libs.cpp. */

#include <boolean.h>
#include <retro_common_api.h>

#include "common/types.h"
#include "core/libraries/camera/camera.h"

RETRO_BEGIN_DECLS

#define ORBIS_VR_TRACKER_MAX_LED_NUM ((int32_t)16)
#define ORBIS_VR_TRACKER_MEMORY_ALIGNMENT ((uint32_t)0x10000)
#define ORBIS_VR_TRACKER_BASE_ONION_SIZE ((uint32_t)0x400000)
#define ORBIS_VR_TRACKER_GARLIC_SIZE ((uint32_t)0x3000000)
#define ORBIS_VR_TRACKER_WORK_SIZE ((uint32_t)0x1000000)

typedef enum OrbisVrTrackerProfile {
    ORBIS_VR_TRACKER_PROFILE_000 = 0,
    ORBIS_VR_TRACKER_PROFILE_100 = 100
} OrbisVrTrackerProfile;

typedef enum OrbisVrTrackerCalibrationMode {
    ORBIS_VR_TRACKER_CALIBRATION_MANUAL = 0,
    ORBIS_VR_TRACKER_CALIBRATION_AUTO = 1
} OrbisVrTrackerCalibrationMode;

typedef enum OrbisVrTrackerExecutionMode {
    ORBIS_VR_TRACKER_EXECUTION_MODE_SERIAL = 0,
    ORBIS_VR_TRACKER_EXECUTION_MODE_PARALLEL = 1
} OrbisVrTrackerExecutionMode;

typedef enum OrbisVrTrackerDeviceType {
    ORBIS_VR_TRACKER_DEVICE_HMD = 0,
    ORBIS_VR_TRACKER_DEVICE_DUALSHOCK4 = 1,
    ORBIS_VR_TRACKER_DEVICE_MOVE = 2,
    ORBIS_VR_TRACKER_DEVICE_GUN = 3
} OrbisVrTrackerDeviceType;

typedef enum OrbisVrTrackerCalibrationType {
    ORBIS_VR_TRACKER_CALIBRATION_POSITION = 0,
    ORBIS_VR_TRACKER_CALIBRATION_ALL = 2
} OrbisVrTrackerCalibrationType;

typedef enum OrbisVrTrackerPreferenceType {
    ORBIS_VR_TRACKER_PREFERENCE_FAR_POSITION = 0,
    ORBIS_VR_TRACKER_PREFERENCE_STABLE_POSITION = 1
} OrbisVrTrackerPreferenceType;

typedef enum OrbisVrTrackerCameraMetaCheckMode {
    ORBIS_VR_TRACKER_CAMERA_META_CHECK_ENABLE = 0,
    ORBIS_VR_TRACKER_CAMERA_META_CHECK_DISABLE = 1
} OrbisVrTrackerCameraMetaCheckMode;

typedef enum OrbisVrTrackerDevicePermitType {
    ORBIS_VR_TRACKER_DEVICE_PERMIT_ALL = 0,
    ORBIS_VR_TRACKER_DEVICE_PERMIT_HMD_ONLY = 1
} OrbisVrTrackerDevicePermitType;

typedef enum OrbisVrTrackerRobustnessLevel {
    ORBIS_VR_TRACKER_ROBUSTNESS_LEVEL_HIGH = 0,
    ORBIS_VR_TRACKER_ROBUSTNESS_LEVEL_LOW = 3,
    ORBIS_VR_TRACKER_ROBUSTNESS_LEVEL_MEDIUM = 6,
    ORBIS_VR_TRACKER_ROBUSTNESS_LEVEL_LEGACY = 99
} OrbisVrTrackerRobustnessLevel;

typedef enum OrbisVrTrackerCpuProcessOperationMode {
    ORBIS_VR_TRACKER_CPU_PROCESS_OPERATION_MODE_WHOLE = 0,
    ORBIS_VR_TRACKER_CPU_PROCESS_OPERATION_MODE_HANDLE = 1
} OrbisVrTrackerCpuProcessOperationMode;

typedef enum OrbisVrTrackerUpdateMotionSensorDataOperationMode {
    ORBIS_VR_TRACKER_UPDATE_MOTION_SENSORDATA_OPERATION_MODE_DEVICE_TYPE = 0,
    ORBIS_VR_TRACKER_UPDATE_MOTION_SENSORDATA_OPERATION_MODE_HANDLE = 1
} OrbisVrTrackerUpdateMotionSensorDataOperationMode;

typedef enum OrbisVrTrackerResultType {
    ORBIS_VR_TRACKER_RESULT_PREDICTED = 0,
    ORBIS_VR_TRACKER_RESULT_RAW = 1
} OrbisVrTrackerResultType;

typedef enum OrbisVrTrackerOrientationType {
    ORBIS_VR_TRACKER_ORIENTATION_ABSOLUTE = 0,
    ORBIS_VR_TRACKER_ORIENTATION_RELATIVE = 1
} OrbisVrTrackerOrientationType;

typedef enum OrbisVrTrackerUsageType {
    ORBIS_VR_TRACKER_USAGE_DEFAULT = 0,
    ORBIS_VR_TRACKER_USAGE_OPTIMIZED_FOR_HMD_USER = 1
} OrbisVrTrackerUsageType;

typedef enum OrbisVrTrackerDebugMarkerType {
    ORBIS_VR_TRACKER_DEBUG_MARKER_UNSPECIFIED = 0,
    ORBIS_VR_TRACKER_DEBUG_MARKER_DEFAULT_UPDATE = 1,
    ORBIS_VR_TRACKER_DEBUG_MARKER_FINAL_UPDATE = 2,
    ORBIS_VR_TRACKER_DEBUG_MARKER_OTHER = 3
} OrbisVrTrackerDebugMarkerType;

typedef enum OrbisVrTrackerRecalibrateNecessityType {
    ORBIS_VR_TRACKER_RECALIBRATE_NECESSITY_NOTHING = 0,
    ORBIS_VR_TRACKER_RECALIBRATE_NECESSITY_POSITION = 4
} OrbisVrTrackerRecalibrateNecessityType;

typedef enum OrbisVrTrackerPlayareaBrightnessRiskType {
    ORBIS_VR_TRACKER_PLAYAREA_BRIGHTNESS_RISK_LOW = 0,
    ORBIS_VR_TRACKER_PLAYAREA_BRIGHTNESS_RISK_HIGH = 5,
    ORBIS_VR_TRACKER_PLAYAREA_BRIGHTNESS_RISK_MAX = 10
} OrbisVrTrackerPlayareaBrightnessRiskType;

typedef enum OrbisVrTrackerLedColor {
    ORBIS_VR_TRACKER_LED_COLOR_BLUE = 0,
    ORBIS_VR_TRACKER_LED_COLOR_RED = 1,
    ORBIS_VR_TRACKER_LED_COLOR_CYAN = 2,
    ORBIS_VR_TRACKER_LED_COLOR_MAGENTA = 3,
    ORBIS_VR_TRACKER_LED_COLOR_YELLOW = 4,
    ORBIS_VR_TRACKER_LED_COLOR_GREEN = 2
} OrbisVrTrackerLedColor;

typedef enum OrbisVrTrackerStatus {
    ORBIS_VR_TRACKER_STATUS_NOT_STARTED = 0,
    ORBIS_VR_TRACKER_STATUS_TRACKING = 1,
    ORBIS_VR_TRACKER_STATUS_NOT_TRACKING = 2,
    ORBIS_VR_TRACKER_STATUS_CALIBRATING = 3
} OrbisVrTrackerStatus;

typedef enum OrbisVrTrackerQuality {
    ORBIS_VR_TRACKER_QUALITY_NONE = 0,
    ORBIS_VR_TRACKER_QUALITY_NOT_VISIBLE = 3,
    ORBIS_VR_TRACKER_QUALITY_PARTIAL = 6,
    ORBIS_VR_TRACKER_QUALITY_FULL = 9
} OrbisVrTrackerQuality;

typedef enum OrbisVrTrackerLedAdjustmentStatus {
    ORBIS_VR_TRACKER_LED_ADJUSTMENT_NOT_USED = 0,
    ORBIS_VR_TRACKER_LED_ADJUSTMENT_USED = 1
} OrbisVrTrackerLedAdjustmentStatus;

typedef enum OrbisVrTrackerHmdRearTrackingStatus {
    ORBIS_VR_TRACKER_REAR_TRACKING_NOT_READY = 0,
    ORBIS_VR_TRACKER_REAR_TRACKING_READY = 1
} OrbisVrTrackerHmdRearTrackingStatus;

typedef struct OrbisVrTrackerCalibrationSettings {
    OrbisVrTrackerCalibrationMode hmd_position;
    OrbisVrTrackerCalibrationMode pad_position;
    OrbisVrTrackerCalibrationMode move_position;
    OrbisVrTrackerCalibrationMode gun_position;
    u32 reserved[4];
} OrbisVrTrackerCalibrationSettings;

typedef struct OrbisVrTrackerQueryMemoryParam {
    u32 size;
    OrbisVrTrackerProfile profile;
    u32 reserved[6];
    OrbisVrTrackerCalibrationSettings calibration_settings;
} OrbisVrTrackerQueryMemoryParam;

typedef struct OrbisVrTrackerQueryMemoryResult {
    u32 size;
    u32 direct_memory_onion_size;
    u32 direct_memory_onion_alignment;
    u32 direct_memory_garlic_size;
    u32 direct_memory_garlic_alignment;
    u32 work_memory_size;
    u32 work_memory_alignment;
    u32 reserved[9];
} OrbisVrTrackerQueryMemoryResult;

typedef struct OrbisVrTrackerInitParam {
    u32 size;
    OrbisVrTrackerProfile profile;
    OrbisVrTrackerExecutionMode execution_mode;
    s32 hmd_thread_priority;
    s32 pad_thread_priority;
    s32 move_thread_priority;
    s32 gun_thread_priority;
    s32 reserved;
    u64 cpu_mask;
    OrbisVrTrackerCalibrationSettings calibration_settings;
    void* direct_memory_onion;
    u32 direct_memory_onion_size;
    u32 direct_memory_onion_alignment;
    void* direct_memory_garlic;
    u32 direct_memory_garlic_size;
    u32 direct_memory_garlic_alignment;
    void* work_memory;
    u32 work_memory_size;
    u32 work_memory_alignment;
    s32 gpu_pipe_id;
    s32 gpu_queue_id;
} OrbisVrTrackerInitParam;

typedef struct OrbisVrTrackerRecalibrateParam {
    u32 size;
    OrbisVrTrackerDeviceType device_type;
    OrbisVrTrackerCalibrationType calibration_type;
    u32 reserved[5];
} OrbisVrTrackerRecalibrateParam;

typedef struct OrbisVrTrackerGpuSubmitParam {
    u32 size;
    OrbisVrTrackerPreferenceType pad_tracking_preference;
    OrbisVrTrackerCameraMetaCheckMode camera_meta_check_mode;
    OrbisVrTrackerDevicePermitType tracking_device_permit_type;
    OrbisVrTrackerRobustnessLevel robustness_level;
    u32 reserved0[10];
    u32 reserved1;
    OrbisCameraFrameData camera_frame_data;
} OrbisVrTrackerGpuSubmitParam;

typedef struct OrbisVrTrackerGpuWaitParam {
    u32 size;
    u32 reserved[7];
} OrbisVrTrackerGpuWaitParam;

typedef struct OrbisVrTrackerCpuProcessParam {
    u32 size;
    OrbisVrTrackerCpuProcessOperationMode operation_mode;
    s32 handle;
    u32 reserved[5];
} OrbisVrTrackerCpuProcessParam;

typedef struct OrbisVrTrackerNotifyEndOfCpuProcessParam {
    u32 size;
    u32 reserved[7];
} OrbisVrTrackerNotifyEndOfCpuProcessParam;

typedef struct OrbisVrTrackerUpdateMotionSensorDataParam {
    u32 size;
    OrbisVrTrackerDeviceType device_type;
    OrbisVrTrackerUpdateMotionSensorDataOperationMode operation_mode;
    s32 handle;
    s32 reserved[4];
} OrbisVrTrackerUpdateMotionSensorDataParam;

typedef struct OrbisVrTrackerGetResultParam {
    u32 size;
    s32 handle;
    OrbisVrTrackerResultType result_type;
    u32 reserved0;
    u64 prediction_time;
    OrbisVrTrackerOrientationType orientation_type;
    u32 reserved1;
    OrbisVrTrackerUsageType usage_type;
    u32 user_frame_number;
    OrbisVrTrackerDebugMarkerType debug_marker_type;
    u32 reserved2[2];
} OrbisVrTrackerGetResultParam;

typedef struct OrbisVrTrackerPoseData {
    float position_x;
    float position_y;
    float position_z;
    u32 reserved0[1];
    float orientation_x;
    float orientation_y;
    float orientation_z;
    float orientation_w;
    u32 reserved1[8];
} OrbisVrTrackerPoseData;

typedef struct OrbisVrTrackerHmdInfo {
    OrbisVrTrackerPoseData device_pose;
    OrbisVrTrackerPoseData left_eye_pose;
    OrbisVrTrackerPoseData right_eye_pose;
    OrbisVrTrackerPoseData head_pose;
    OrbisVrTrackerHmdRearTrackingStatus rear_tracking_status;
    u32 reserved0[3];
    u64 sensor_read_system_timestamp;
    u32 reserved1[10];
} OrbisVrTrackerHmdInfo;

typedef struct OrbisVrTrackerPadInfo {
    OrbisVrTrackerPoseData device_pose;
    u32 reserved[64];
} OrbisVrTrackerPadInfo;

typedef struct OrbisVrTrackerMoveInfo {
    OrbisVrTrackerPoseData device_pose;
    u32 reserved[64];
} OrbisVrTrackerMoveInfo;

typedef struct OrbisVrTrackerGunInfo {
    OrbisVrTrackerPoseData device_pose;
    u32 reserved[64];
} OrbisVrTrackerGunInfo;

typedef struct OrbisVrTrackerLedResult {
    float x;
    float y;
    float rx;
    float ry;
    u32 reserved[4];
} OrbisVrTrackerLedResult;

typedef struct OrbisVrTrackerResultData {
    s32 handle;
    u32 connected;
    u32 reserved0[2];
    u64 timestamp;
    u64 device_timestamp;
    OrbisVrTrackerRecalibrateNecessityType recalibrate_necessity;
    OrbisVrTrackerPlayareaBrightnessRiskType playarea_brightness_risk;
    u32 reserved1[2];
    OrbisVrTrackerLedColor led_color;
    OrbisVrTrackerStatus status;
    OrbisVrTrackerQuality position_quality;
    OrbisVrTrackerQuality orientation_quality;
    float velocity_x;
    float velocity_y;
    float velocity_z;
    float acceleration_x;
    float acceleration_y;
    float acceleration_z;
    float angular_velocity_x;
    float angular_velocity_y;
    float angular_velocity_z;
    float angular_acceleration_x;
    float angular_acceleration_y;
    float angular_acceleration_z;
    float camera_orientation_x;
    float camera_orientation_y;
    float camera_orientation_z;
    float camera_orientation_w;
    union {
        OrbisVrTrackerHmdInfo hmd_info;
        OrbisVrTrackerPadInfo pad_info;
        OrbisVrTrackerMoveInfo move_info;
        OrbisVrTrackerGunInfo gun_info;
    } device_info; /* named, as C89 has no anonymous unions; the layout is the same */
    u32 user_frame_number;
    OrbisVrTrackerLedAdjustmentStatus led_adjustment_status;
    u64 timestamp_of_led_result;
    u32 reserved2[2];
    s32 number_of_led_result[ORBIS_CAMERA_MAX_DEVICE_NUM];
    u32 reserved3[4];
    OrbisVrTrackerLedResult led[ORBIS_CAMERA_MAX_DEVICE_NUM][ORBIS_VR_TRACKER_MAX_LED_NUM];
} OrbisVrTrackerResultData;

typedef struct OrbisVrTrackerPlayAreaWarningInfo {
    u32 size;
    u32 reserved0[3];
    bool is_out_of_play_area;
    u8 reserved1[3];
    u32 reserved2[3];
    bool is_distance_data_valid;
    u8 reserved3[3];
    float distance_from_vertical_boundary;
    float distance_from_horizontal_boundary;
    u32 reserved4[5];
} OrbisVrTrackerPlayAreaWarningInfo;

s32 PS4_SYSV_ABI sceVrTrackerQueryMemory(const OrbisVrTrackerQueryMemoryParam* param,
                                         OrbisVrTrackerQueryMemoryResult* result);
s32 PS4_SYSV_ABI sceVrTrackerInit(const OrbisVrTrackerInitParam* param);
s32 PS4_SYSV_ABI sceVrTrackerRegisterDevice(const OrbisVrTrackerDeviceType device_type,
                                            const s32 handle);
s32 PS4_SYSV_ABI sceVrTrackerRegisterDevice2(const OrbisVrTrackerDeviceType device_type,
                                             const s32 handle);
s32 PS4_SYSV_ABI sceVrTrackerRegisterDeviceInternal(const OrbisVrTrackerDeviceType device_type,
                                                    const s32 handle, s32 unk0, s32 unk1);
s32 PS4_SYSV_ABI sceVrTrackerCpuProcess(const OrbisVrTrackerCpuProcessParam* param);
s32 PS4_SYSV_ABI sceVrTrackerGetPlayAreaWarningInfo(OrbisVrTrackerPlayAreaWarningInfo* info);
s32 PS4_SYSV_ABI sceVrTrackerGetResult(const OrbisVrTrackerGetResultParam* param,
                                       OrbisVrTrackerResultData* result);
s32 PS4_SYSV_ABI sceVrTrackerGetTime(u64* time);
s32 PS4_SYSV_ABI sceVrTrackerGpuSubmit(const OrbisVrTrackerGpuSubmitParam* param);
s32 PS4_SYSV_ABI sceVrTrackerGpuWait(const OrbisVrTrackerGpuWaitParam* param);
s32 PS4_SYSV_ABI sceVrTrackerGpuWaitAndCpuProcess(void);
s32 PS4_SYSV_ABI
sceVrTrackerNotifyEndOfCpuProcess(const OrbisVrTrackerNotifyEndOfCpuProcessParam* param);
s32 PS4_SYSV_ABI sceVrTrackerRecalibrate(const OrbisVrTrackerRecalibrateParam* param);
s32 PS4_SYSV_ABI sceVrTrackerResetAll(void);
s32 PS4_SYSV_ABI sceVrTrackerResetOrientationRelative(const OrbisVrTrackerDeviceType device_type,
                                                      const s32 handle);
s32 PS4_SYSV_ABI sceVrTrackerSaveInternalBuffers(void);
s32 PS4_SYSV_ABI sceVrTrackerSetDurationUntilStatusNotTracking(
    const OrbisVrTrackerDeviceType device_type, const u32 duration_camera_frames);
s32 PS4_SYSV_ABI sceVrTrackerSetExtendedMode(void);
s32 PS4_SYSV_ABI sceVrTrackerSetLEDBrightness(void);
s32 PS4_SYSV_ABI sceVrTrackerSetRestingMode(void);
s32 PS4_SYSV_ABI
sceVrTrackerUpdateMotionSensorData(const OrbisVrTrackerUpdateMotionSensorDataParam* param);
s32 PS4_SYSV_ABI VrTracker_Func_0FA4C949F8D3024E(void);
s32 PS4_SYSV_ABI VrTracker_Func_285C6AFC09C42F7E(void);
s32 PS4_SYSV_ABI VrTracker_Func_9A6CDB2103664F8A(void);
s32 PS4_SYSV_ABI VrTracker_Func_B4D26B7D8B18DF06(void);
s32 PS4_SYSV_ABI sceVrTrackerSetDeviceRejection(void);
s32 PS4_SYSV_ABI VrTracker_Func_1119B0BE399F37E7(void);
s32 PS4_SYSV_ABI VrTracker_Func_4928B43816BC440D(void);
s32 PS4_SYSV_ABI VrTracker_Func_863EF32EFCB0FA9C(void);
s32 PS4_SYSV_ABI VrTracker_Func_E6E726CBC85C48F9(void);
s32 PS4_SYSV_ABI VrTracker_Func_F6407E46C66DF383(void);
s32 PS4_SYSV_ABI sceVrTrackerCpuPopMarker(void);
s32 PS4_SYSV_ABI sceVrTrackerCpuPushMarker(void);
s32 PS4_SYSV_ABI sceVrTrackerGetLiveCaptureId(void);
s32 PS4_SYSV_ABI sceVrTrackerStartLiveCapture(void);
s32 PS4_SYSV_ABI sceVrTrackerStopLiveCapture(void);
s32 PS4_SYSV_ABI sceVrTrackerUnregisterDevice(const s32 handle);
s32 PS4_SYSV_ABI sceVrTrackerTerm(void);

RETRO_END_DECLS

#endif
