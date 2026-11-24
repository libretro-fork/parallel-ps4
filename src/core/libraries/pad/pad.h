/* SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_PAD_H
#define SHADPS4_LIBRARIES_PAD_H

/* libScePad: the controllers of input/controller.h, by the handles the game opens. Defined in
 * pad.c, registered in libs.cpp. C++ also has the button bits as
 * Libraries::Pad::OrbisPadButtonDataOffset, at the end. */

#include <boolean.h>
#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

#define ORBIS_PAD_MAX_TOUCH_NUM 2
#define ORBIS_PAD_MAX_DATA_NUM 64
#define ORBIS_PAD_MAX_DEVICE_UNIQUE_DATA_SIZE 12

#define ORBIS_PAD_PORT_TYPE_STANDARD 0
#define ORBIS_PAD_PORT_TYPE_SPECIAL 2
#define ORBIS_PAD_PORT_TYPE_REMOTE_CONTROL 16

#define ORBIS_PAD_CONNECTION_TYPE_LOCAL 0

/* OrbisPadDeviceClass: an int */
typedef s32 OrbisPadDeviceClass;
#define ORBIS_PAD_DEVICE_CLASS_INVALID (-1)
#define ORBIS_PAD_DEVICE_CLASS_STANDARD 0
#define ORBIS_PAD_DEVICE_CLASS_GUITAR 1
#define ORBIS_PAD_DEVICE_CLASS_DRUM 2
#define ORBIS_PAD_DEVICE_CLASS_DJ_TURNTABLE 3
#define ORBIS_PAD_DEVICE_CLASS_DANCEMAT 4
#define ORBIS_PAD_DEVICE_CLASS_NAVIGATION 5
#define ORBIS_PAD_DEVICE_CLASS_STEERING_WHEEL 6
#define ORBIS_PAD_DEVICE_CLASS_STICK 7
#define ORBIS_PAD_DEVICE_CLASS_FIGHT_STICK 8
#define ORBIS_PAD_DEVICE_CLASS_GUN 9

typedef struct OrbisPadDeviceClassExtendedInformation {
    OrbisPadDeviceClass deviceClass;
    u8 reserved[4];
    union {
        struct {
            u8 capability;
            u8 reserved1[1];
            u16 maxPhysicalWheelAngle;
            u8 reserved2[8];
        } steeringWheel;

        struct {
            u8 capability;
            u8 quantityOfSelectorSwitch;
            u8 reserved[10];
        } guitar;

        struct {
            u8 capability;
            u8 reserved[11];
        } drum;

        struct {
            u8 capability;
            u8 reserved[11];
        } flightStick;

        u8 data[12];
    } classData;
} OrbisPadDeviceClassExtendedInformation;

typedef struct OrbisPadDeviceClassData {
    OrbisPadDeviceClass deviceClass;
    bool bDataValid;
    union {
        struct {
            float steeringWheelAngle;
            u16 steeringWheel;
            u16 acceleratorPedal;
            u16 brakePedal;
            u16 clutchPedal;
            u16 handBrake;
            u8 gear;
            u8 reserved[1];
        } steeringWheel;

        struct {
            u8 toneNumber;
            u8 whammyBar;
            u8 tilt;
            u8 fret;
            u8 fretSolo;
            u8 reserved[11];
        } guitar;

        struct {
            u8 snare;
            u8 tom1;
            u8 tom2;
            u8 floorTom;
            u8 hihatCymbal;
            u8 rideCymbal;
            u8 crashCymbal;
            u8 reserved[9];
        } drum;

        struct {
            u16 stickAxisX;
            u16 stickAxisY;
            u8 stickTwist;
            u8 throttle;
            u8 trigger;
            u8 rudderPedal;
            u8 brakePedalLeft;
            u8 brakePedalRight;
            u8 antennaKnob;
            u8 rangeKnob;
            u8 reserved[4];
        } flightStick;

        struct {
            u8 dataLen;
            u8 reserved[3];
            u8 data[ORBIS_PAD_MAX_DEVICE_UNIQUE_DATA_SIZE];
        } others;
    } classData;
} OrbisPadDeviceClassData;

typedef struct OrbisPadAnalogButtons {
    u8 l2;
    u8 r2;
    u8 padding[2];
} OrbisPadAnalogButtons;

typedef struct OrbisPadAnalogStick {
    u8 x;
    u8 y;
} OrbisPadAnalogStick;

/* OrbisPadButtonDataOffset: u32 bits */
#define ORBIS_PAD_BUTTON_NONE 0u
#define ORBIS_PAD_BUTTON_L3 0x2u
#define ORBIS_PAD_BUTTON_R3 0x4u
#define ORBIS_PAD_BUTTON_OPTIONS 0x8u
#define ORBIS_PAD_BUTTON_UP 0x10u
#define ORBIS_PAD_BUTTON_RIGHT 0x20u
#define ORBIS_PAD_BUTTON_DOWN 0x40u
#define ORBIS_PAD_BUTTON_LEFT 0x80u
#define ORBIS_PAD_BUTTON_L2 0x100u
#define ORBIS_PAD_BUTTON_R2 0x200u
#define ORBIS_PAD_BUTTON_L1 0x400u
#define ORBIS_PAD_BUTTON_R1 0x800u
#define ORBIS_PAD_BUTTON_TRIANGLE 0x1000u
#define ORBIS_PAD_BUTTON_CIRCLE 0x2000u
#define ORBIS_PAD_BUTTON_CROSS 0x4000u
#define ORBIS_PAD_BUTTON_SQUARE 0x8000u
#define ORBIS_PAD_BUTTON_TOUCH_PAD 0x100000u
#define ORBIS_PAD_BUTTON_INTERCEPTED 0x80000000u

typedef struct OrbisFQuaternion {
    float x, y, z, w;
} OrbisFQuaternion;

typedef struct OrbisFVector3 {
    float x, y, z;
} OrbisFVector3;

typedef struct OrbisPadTouch {
    u16 x;
    u16 y;
    u8 id;
    u8 reserve[3];
} OrbisPadTouch;

typedef struct OrbisPadTouchData {
    u8 touchNum;
    u8 reserve[3];
    u32 time_since_touch_held_down;
    OrbisPadTouch touch[ORBIS_PAD_MAX_TOUCH_NUM];
} OrbisPadTouchData;

typedef struct OrbisPadExtensionUnitData {
    u32 extensionUnitId;
    u8 reserve[1];
    u8 dataLength;
    u8 data[10];
} OrbisPadExtensionUnitData;

typedef struct OrbisPadData {
    u32 buttons; /* ORBIS_PAD_BUTTON_* */
    OrbisPadAnalogStick leftStick;
    OrbisPadAnalogStick rightStick;
    OrbisPadAnalogButtons analogButtons;
    OrbisFQuaternion orientation;
    OrbisFVector3 acceleration;
    OrbisFVector3 angularVelocity;
    OrbisPadTouchData touchData;
    bool connected;
    u64 timestamp;
    OrbisPadExtensionUnitData extensionUnitData;
    u8 connectedCount;
    u8 reserve[2];
    u8 deviceUniqueDataLen;
    u8 deviceUniqueData[ORBIS_PAD_MAX_DEVICE_UNIQUE_DATA_SIZE];
} OrbisPadData;

typedef struct OrbisPadTouchPadInformation {
    float pixelDensity;
    struct {
        u16 x;
        u16 y;
    } resolution;
} OrbisPadTouchPadInformation;

typedef struct OrbisPadStickInformation {
    u8 deadZoneLeft;
    u8 deadZoneRight;
} OrbisPadStickInformation;

typedef struct OrbisPadControllerInformation {
    OrbisPadTouchPadInformation touchPadInfo;
    OrbisPadStickInformation stickInfo;
    u8 connectionType;
    u8 connectedCount;
    bool connected;
    OrbisPadDeviceClass deviceClass;
    u8 reserve[8];
} OrbisPadControllerInformation;

typedef struct OrbisPadExtendedControllerInformation {
    OrbisPadControllerInformation base;
    u16 padType1;
    u16 padType2;
    u8 capability;
    union {
        u8 quantityOfSelectorSwitch;
        int maxPhysicalWheelAngle;
        u8 data[8];
    } u;
} OrbisPadExtendedControllerInformation;

typedef struct OrbisPadOpenParam {
    u8 reserve[8];
} OrbisPadOpenParam;

typedef struct OrbisPadOpenExtParam {
    u16 vendorId;
    u16 productId;
    u16 productId_2;
    u8 reserve[10];
} OrbisPadOpenExtParam;

typedef struct OrbisPadLightBarParam {
    u8 r;
    u8 g;
    u8 b;
    u8 reserve[1];
} OrbisPadLightBarParam;

typedef struct OrbisPadVibrationParam {
    u8 largeMotor;
    u8 smallMotor;
} OrbisPadVibrationParam;

/* Thanks to OpenOrbis
 * https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/blob/master/include/orbis/_types/pad.h */
typedef struct OrbisPadInfo {
    u32 unk1;
    u32 unk2;
    u32 pad_handle;
    u32 unk3;
    u32 unk4;
    u32 unk5;
    u32 colour;
    u32 unk6;
    u32 unk[30];
} OrbisPadInfo;

/* The sizes the game sees */
typedef char pad_data_size_check[sizeof(OrbisPadData) == 0x78 ? 1 : -1];
typedef char
    pad_controller_information_size_check[sizeof(OrbisPadControllerInformation) == 0x1c ? 1 : -1];
typedef char pad_extended_controller_information_size_check
    [sizeof(OrbisPadExtendedControllerInformation) == 44 ? 1 : -1];
typedef char pad_device_class_extended_information_size_check
    [sizeof(OrbisPadDeviceClassExtendedInformation) == 20 ? 1 : -1];
typedef char pad_device_class_data_size_check[sizeof(OrbisPadDeviceClassData) == 24 ? 1 : -1];
typedef char pad_info_size_check[sizeof(OrbisPadInfo) == 152 ? 1 : -1];

int PS4_SYSV_ABI scePadClose(s32 handle);
int PS4_SYSV_ABI scePadConnectPort(void);
int PS4_SYSV_ABI scePadDeviceClassGetExtendedInformation(
    s32 handle, OrbisPadDeviceClassExtendedInformation* pExtInfo);
int PS4_SYSV_ABI scePadDeviceClassParseData(s32 handle, const OrbisPadData* pData,
                                            OrbisPadDeviceClassData* pDeviceClassData);
int PS4_SYSV_ABI scePadDeviceOpen(void);
int PS4_SYSV_ABI scePadDisableVibration(void);
int PS4_SYSV_ABI scePadDisconnectDevice(void);
int PS4_SYSV_ABI scePadDisconnectPort(void);
int PS4_SYSV_ABI scePadEnableAutoDetect(void);
int PS4_SYSV_ABI scePadEnableExtensionPort(void);
int PS4_SYSV_ABI scePadEnableSpecificDeviceClass(void);
int PS4_SYSV_ABI scePadEnableUsbConnection(void);
int PS4_SYSV_ABI scePadGetBluetoothAddress(void);
int PS4_SYSV_ABI scePadGetCapability(void);
int PS4_SYSV_ABI scePadGetControllerInformation(s32 handle, OrbisPadControllerInformation* pInfo);
int PS4_SYSV_ABI scePadGetDataInternal(void);
int PS4_SYSV_ABI scePadGetDeviceId(void);
int PS4_SYSV_ABI scePadGetDeviceInfo(void);
int PS4_SYSV_ABI scePadGetExtControllerInformation(s32 handle,
                                                   OrbisPadExtendedControllerInformation* pInfo);
int PS4_SYSV_ABI scePadGetExtensionUnitInfo(void);
int PS4_SYSV_ABI scePadGetFeatureReport(void);
int PS4_SYSV_ABI scePadGetHandle(s32 userId, s32 type, s32 index);
int PS4_SYSV_ABI scePadGetIdleCount(void);
int PS4_SYSV_ABI scePadGetInfo(OrbisPadInfo* data);
int PS4_SYSV_ABI scePadGetInfoByPortType(void);
int PS4_SYSV_ABI scePadGetLicenseControllerInformation(void);
int PS4_SYSV_ABI scePadGetMotionSensorPosition(void);
int PS4_SYSV_ABI scePadGetMotionTimerUnit(void);
int PS4_SYSV_ABI scePadGetSphereRadius(void);
int PS4_SYSV_ABI scePadGetVersionInfo(void);
int PS4_SYSV_ABI scePadInit(void);
int PS4_SYSV_ABI scePadIsBlasterConnected(void);
int PS4_SYSV_ABI scePadIsDS4Connected(void);
int PS4_SYSV_ABI scePadIsLightBarBaseBrightnessControllable(void);
int PS4_SYSV_ABI scePadIsMoveConnected(void);
int PS4_SYSV_ABI scePadIsMoveReproductionModel(void);
int PS4_SYSV_ABI scePadIsValidHandle(void);
int PS4_SYSV_ABI scePadMbusInit(void);
int PS4_SYSV_ABI scePadMbusTerm(void);
int PS4_SYSV_ABI scePadOpen(s32 userId, s32 type, s32 index, const OrbisPadOpenParam* pParam);
int PS4_SYSV_ABI scePadOpenExt(s32 userId, s32 type, s32 index, const OrbisPadOpenExtParam* pParam);
int PS4_SYSV_ABI scePadOpenExt2(void);
int PS4_SYSV_ABI scePadOutputReport(void);
int PS4_SYSV_ABI scePadRead(s32 handle, OrbisPadData* pData, s32 num);
int PS4_SYSV_ABI scePadReadBlasterForTracker(void);
int PS4_SYSV_ABI scePadReadExt(s32 handle, OrbisPadData* pData, s32 num);
int PS4_SYSV_ABI scePadReadForTracker(void);
int PS4_SYSV_ABI scePadReadHistory(void);
int PS4_SYSV_ABI scePadReadState(s32 handle, OrbisPadData* pData);
int PS4_SYSV_ABI scePadReadStateExt(s32 handle, OrbisPadData* pData);
int PS4_SYSV_ABI scePadResetLightBar(s32 handle);
int PS4_SYSV_ABI scePadResetLightBarAll(void);
int PS4_SYSV_ABI scePadResetLightBarAllByPortType(void);
int PS4_SYSV_ABI scePadResetOrientation(s32 handle);
int PS4_SYSV_ABI scePadResetOrientationForTracker(void);
int PS4_SYSV_ABI scePadSetAngularVelocityDeadbandState(s32 handle, bool bEnable);
int PS4_SYSV_ABI scePadSetAutoPowerOffCount(void);
int PS4_SYSV_ABI scePadSetButtonRemappingInfo(void);
int PS4_SYSV_ABI scePadSetConnection(void);
int PS4_SYSV_ABI scePadSetExtensionReport(void);
int PS4_SYSV_ABI scePadSetFeatureReport(void);
int PS4_SYSV_ABI scePadSetForceIntercepted(void);
int PS4_SYSV_ABI scePadSetLightBar(s32 handle, const OrbisPadLightBarParam* pParam);
int PS4_SYSV_ABI scePadSetLightBarBaseBrightness(void);
int PS4_SYSV_ABI scePadSetLightBarBlinking(void);
int PS4_SYSV_ABI scePadSetLightBarForTracker(s32 handle, const OrbisPadLightBarParam* pParam);
int PS4_SYSV_ABI scePadSetLoginUserNumber(void);
int PS4_SYSV_ABI scePadSetMotionSensorState(s32 handle, bool bEnable);
int PS4_SYSV_ABI scePadSetProcessFocus(void);
int PS4_SYSV_ABI scePadSetProcessPrivilege(void);
int PS4_SYSV_ABI scePadSetProcessPrivilegeOfButtonRemapping(void);
int PS4_SYSV_ABI scePadSetShareButtonMaskForRemotePlay(void);
int PS4_SYSV_ABI scePadSetTiltCorrectionState(s32 handle, bool bEnable);
int PS4_SYSV_ABI scePadSetUserColor(void);
int PS4_SYSV_ABI scePadSetVibration(s32 handle, const OrbisPadVibrationParam* pParam);
int PS4_SYSV_ABI scePadSetVibrationForce(void);
int PS4_SYSV_ABI scePadSetVrTrackingMode(void);
int PS4_SYSV_ABI scePadShareOutputData(void);
int PS4_SYSV_ABI scePadStartRecording(void);
int PS4_SYSV_ABI scePadStopRecording(void);
int PS4_SYSV_ABI scePadSwitchConnection(void);
int PS4_SYSV_ABI scePadVertualDeviceAddDevice(void);
int PS4_SYSV_ABI scePadVirtualDeviceAddDevice(void);
int PS4_SYSV_ABI scePadVirtualDeviceDeleteDevice(void);
int PS4_SYSV_ABI scePadVirtualDeviceDisableButtonRemapping(void);
int PS4_SYSV_ABI scePadVirtualDeviceGetRemoteSetting(void);
int PS4_SYSV_ABI scePadVirtualDeviceInsertData(void);
int PS4_SYSV_ABI Pad_Func_28B998C7D8A3DA1D(void);
int PS4_SYSV_ABI Pad_Func_298D21481F94C9FA(void);
int PS4_SYSV_ABI Pad_Func_51E514BCD3A05CA5(void);
int PS4_SYSV_ABI Pad_Func_89C9237E393DA243(void);
int PS4_SYSV_ABI Pad_Func_EF103E845B6F0420(void);

int PS4_SYSV_ABI scePadSetAngularVelocityBiasCorrectionState(void);

/* Connects the first controller, once, before any call: libs.cpp's registration calls it */
void pad_library_init(void);

RETRO_END_DECLS

#ifdef __cplusplus

#include "common/enum.h"

namespace Libraries::Pad {

/// The button bits, for the C++ that names them (the IME's on-screen keyboard, the presenter)
enum class OrbisPadButtonDataOffset : u32 {
    None = ORBIS_PAD_BUTTON_NONE,
    L3 = ORBIS_PAD_BUTTON_L3,
    R3 = ORBIS_PAD_BUTTON_R3,
    Options = ORBIS_PAD_BUTTON_OPTIONS,
    Up = ORBIS_PAD_BUTTON_UP,
    Right = ORBIS_PAD_BUTTON_RIGHT,
    Down = ORBIS_PAD_BUTTON_DOWN,
    Left = ORBIS_PAD_BUTTON_LEFT,
    L2 = ORBIS_PAD_BUTTON_L2,
    R2 = ORBIS_PAD_BUTTON_R2,
    L1 = ORBIS_PAD_BUTTON_L1,
    R1 = ORBIS_PAD_BUTTON_R1,
    Triangle = ORBIS_PAD_BUTTON_TRIANGLE,
    Circle = ORBIS_PAD_BUTTON_CIRCLE,
    Cross = ORBIS_PAD_BUTTON_CROSS,
    Square = ORBIS_PAD_BUTTON_SQUARE,
    TouchPad = ORBIS_PAD_BUTTON_TOUCH_PAD,
    Intercepted = ORBIS_PAD_BUTTON_INTERCEPTED,
};

DECLARE_ENUM_FLAG_OPERATORS(OrbisPadButtonDataOffset)

} // namespace Libraries::Pad

#endif

#endif
