/* SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stddef.h>
#include <string.h>

#include <boolean.h>

#include "common/c_bridge.h"
#include "core/emulator_settings.h"
#include "core/libraries/kernel/kernel_c.h"
#include "core/libraries/pad/pad.h"
#include "core/libraries/pad/pad_errors.h"
#include "core/user_manager.h"
#include "input/controller.h"
#include "video_core/overlay/ovl.h"

/* ORBIS_USER_SERVICE_USER_ID_SYSTEM */
#define PAD_USER_ID_SYSTEM 0xFF

/* The handles open: as many as a game could want, made once. A handle is the game's (user,
 * port type, index) and the controller it reads; handles are numbered from 1, never again. */
#define PAD_HANDLES 64

typedef struct pad_handle_slot {
    bool used;
    s32 user_id;
    s32 device_class;
    s32 index;
    s32 handle;
    input_controller_t* controller;
} pad_handle_slot;

static bool g_initialized = false;
static u64 pad_handle_counter = 1;
static pad_handle_slot g_handles[PAD_HANDLES];

static pad_handle_slot* FindHandle(s32 handle) {
    s32 i;
    for (i = 0; i < PAD_HANDLES; i++) {
        if (g_handles[i].used && g_handles[i].handle == handle) {
            return &g_handles[i];
        }
    }
    return NULL;
}

static pad_handle_slot* FindKey(s32 user_id, s32 device_class, s32 index) {
    s32 i;
    for (i = 0; i < PAD_HANDLES; i++) {
        if (g_handles[i].used && g_handles[i].user_id == user_id &&
            g_handles[i].device_class == device_class && g_handles[i].index == index) {
            return &g_handles[i];
        }
    }
    return NULL;
}

/* A new handle for the key, reading @controller; an error if all are open */
static s32 AddHandle(s32 user_id, s32 device_class, s32 index, input_controller_t* controller) {
    s32 i;
    for (i = 0; i < PAD_HANDLES; i++) {
        pad_handle_slot* slot = &g_handles[i];
        if (!slot->used) {
            slot->used = true;
            slot->user_id = user_id;
            slot->device_class = device_class;
            slot->index = index;
            slot->handle = (s32)pad_handle_counter++;
            slot->controller = controller;
            return slot->handle;
        }
    }
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadOpen", "All %d pad handles are open",
          PAD_HANDLES);
    return ORBIS_PAD_ERROR_FATAL;
}

int PS4_SYSV_ABI scePadClose(s32 handle) {
    pad_handle_slot* slot;
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_WARNING, "scePadClose", "called, handle: %d", (int)handle);
    slot = FindHandle(handle);
    if (slot == NULL) {
        return ORBIS_PAD_ERROR_INVALID_HANDLE;
    }
    slot->used = false;
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadConnectPort(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadConnectPort", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadDeviceClassGetExtendedInformation(
    s32 handle, OrbisPadDeviceClassExtendedInformation* pExtInfo) {
    if (FindHandle(handle) == NULL) {
        return ORBIS_PAD_ERROR_INVALID_HANDLE;
    }
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadDeviceClassGetExtendedInformation",
          "(STUBBED) called");
    memset(pExtInfo, 0, sizeof(OrbisPadDeviceClassExtendedInformation));
    if (emu_settings_get_use_special_pad()) {
        pExtInfo->deviceClass = (OrbisPadDeviceClass)emu_settings_get_special_pad_class();
    }
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadDeviceClassParseData(s32 handle, const OrbisPadData* pData,
                                            OrbisPadDeviceClassData* pDeviceClassData) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadDeviceClassParseData", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadDeviceOpen(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadDeviceOpen", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadDisableVibration(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadDisableVibration", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadDisconnectDevice(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadDisconnectDevice", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadDisconnectPort(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadDisconnectPort", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadEnableAutoDetect(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadEnableAutoDetect", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadEnableExtensionPort(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadEnableExtensionPort", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadEnableSpecificDeviceClass(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadEnableSpecificDeviceClass", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadEnableUsbConnection(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadEnableUsbConnection", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadGetBluetoothAddress(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadGetBluetoothAddress", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadGetCapability(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadGetCapability", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadGetControllerInformation(s32 handle, OrbisPadControllerInformation* pInfo) {
    const pad_handle_slot* slot;
    input_state_t state;
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_DEBUG, "scePadGetControllerInformation", "called handle = %d",
          (int)handle);
    slot = FindHandle(handle);
    if (slot == NULL) {
        return ORBIS_PAD_ERROR_INVALID_HANDLE;
    }
    state = input_controller_read_state(slot->controller);

    memset(pInfo, 0, sizeof(OrbisPadControllerInformation));
    pInfo->touchPadInfo.pixelDensity = 1;
    pInfo->touchPadInfo.resolution.x = 1920;
    pInfo->touchPadInfo.resolution.y = 950;
    pInfo->stickInfo.deadZoneLeft = 1;
    pInfo->stickInfo.deadZoneRight = 1;
    pInfo->connectionType = ORBIS_PAD_CONNECTION_TYPE_LOCAL;
    pInfo->connectedCount = state.connected_count;
    pInfo->deviceClass = ORBIS_PAD_DEVICE_CLASS_STANDARD;
    pInfo->connected = state.connected;
    if (state.connected) {
        pInfo->deviceClass = emu_settings_get_use_special_pad()
                                 ? (OrbisPadDeviceClass)emu_settings_get_special_pad_class()
                                 : ORBIS_PAD_DEVICE_CLASS_STANDARD;
    }
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_DEBUG, "scePadGetControllerInformation",
          "c: %s cc: %u, ct: %u, dc: %d", (pInfo->connected) ? "true" : "false",
          (unsigned)pInfo->connectedCount, (unsigned)pInfo->connectionType,
          (int)pInfo->deviceClass);
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadGetDataInternal(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadGetDataInternal", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadGetDeviceId(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadGetDeviceId", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadGetDeviceInfo(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadGetDeviceInfo", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadGetExtControllerInformation(s32 handle,
                                                   OrbisPadExtendedControllerInformation* pInfo) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_INFO, "scePadGetExtControllerInformation",
          "called handle = %d", (int)handle);
    memset(pInfo, 0, sizeof(OrbisPadExtendedControllerInformation));
    return scePadGetControllerInformation(handle, &pInfo->base);
}

int PS4_SYSV_ABI scePadGetExtensionUnitInfo(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadGetExtensionUnitInfo", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadGetFeatureReport(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadGetFeatureReport", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadGetHandle(s32 userId, s32 type, s32 index) {
    const pad_handle_slot* slot;
    if (!g_initialized) {
        return ORBIS_PAD_ERROR_NOT_INITIALIZED;
    }
    if (userId == -1) {
        return ORBIS_PAD_ERROR_DEVICE_NO_HANDLE;
    }
    slot = FindKey(userId, type, index);
    if (slot == NULL) {
        return ORBIS_PAD_ERROR_DEVICE_NO_HANDLE;
    }
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_DEBUG, "scePadGetHandle",
          "called, userid: %d, out pad handle: %d", (int)userId, (int)slot->handle);
    return slot->handle;
}

int PS4_SYSV_ABI scePadGetIdleCount(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadGetIdleCount", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadGetInfo(OrbisPadInfo* data) {
    input_colour_t col;
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_WARNING, "scePadGetInfo", "(DUMMY) called");
    if (!data) {
        return ORBIS_PAD_ERROR_INVALID_ARG;
    }
    col = input_controller_get_light_bar(input_controller_get(0));
    memset(data, 0, sizeof(OrbisPadInfo));
    data->unk1 = 0x1;
    data->pad_handle = 1;
    data->unk3 = 0x00000101;
    data->colour = (u32)col.r + ((u32)col.g << 8) + ((u32)col.b << 16);
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadGetInfoByPortType(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadGetInfoByPortType", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadGetLicenseControllerInformation(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadGetLicenseControllerInformation",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadGetMotionSensorPosition(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadGetMotionSensorPosition", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadGetMotionTimerUnit(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadGetMotionTimerUnit", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadGetSphereRadius(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadGetSphereRadius", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadGetVersionInfo(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadGetVersionInfo", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadInit(void) {
    g_initialized = true;
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadIsBlasterConnected(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadIsBlasterConnected", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadIsDS4Connected(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadIsDS4Connected", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadIsLightBarBaseBrightnessControllable(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadIsLightBarBaseBrightnessControllable",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadIsMoveConnected(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadIsMoveConnected", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadIsMoveReproductionModel(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadIsMoveReproductionModel", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadIsValidHandle(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadIsValidHandle", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadMbusInit(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadMbusInit", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadMbusTerm(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadMbusTerm", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadOpen(s32 userId, s32 type, s32 index, const OrbisPadOpenParam* pParam) {
    struct user* u;
    s32 new_handle;
    (void)pParam;
    if (!g_initialized) {
        return ORBIS_PAD_ERROR_NOT_INITIALIZED;
    }
    if (userId < 0) {
        return ORBIS_DEVICE_SERVICE_ERROR_INVALID_USER;
    }
    if (FindKey(userId, type, index) != NULL) {
        return ORBIS_PAD_ERROR_ALREADY_OPENED;
    }
    if (userId == PAD_USER_ID_SYSTEM) {
        if (type == ORBIS_PAD_PORT_TYPE_REMOTE_CONTROL) {
            new_handle = AddHandle(userId, type, index, input_controller_get(4));
            if (new_handle < 0) {
                return new_handle;
            }
            c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_INFO, "scePadOpen",
                  "Opened a TV remote device, out handle: %d", (int)new_handle);
            return new_handle;
        }
        return ORBIS_DEVICE_SERVICE_ERROR_INVALID_USER;
    }
    if (type == ORBIS_PAD_PORT_TYPE_REMOTE_CONTROL) {
        return ORBIS_PAD_ERROR_INVALID_ARG;
    }
    u = user_manager_get_by_id(userId);
    if (!u) {
        return ORBIS_DEVICE_SERVICE_ERROR_USER_NOT_LOGIN;
    }
    new_handle = AddHandle(userId, type, index,
                           input_controller_get(type == (emu_settings_get_use_special_pad() ? 2 : 0)
                                                    ? u->player_index - 1
                                                    : 4));
    if (new_handle < 0) {
        return new_handle;
    }
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_INFO, "scePadOpen",
          "called user_id = %d, type = %d, index = %d, player index = %d, out handle = %d",
          (int)userId, (int)type, (int)index, u->player_index, (int)new_handle);
    scePadResetLightBar(new_handle);
    scePadResetOrientation(new_handle);
    return new_handle;
}

int PS4_SYSV_ABI scePadOpenExt(s32 userId, s32 type, s32 index,
                               const OrbisPadOpenExtParam* pParam) {
    (void)pParam;
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_WARNING, "scePadOpenExt", "Redirect to scePadOpen");
    return scePadOpen(userId, type, index, NULL);
}

int PS4_SYSV_ABI scePadOpenExt2(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadOpenExt2", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadOutputReport(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadOutputReport", "(STUBBED) called");
    return ORBIS_OK;
}

/* The controller's states as the game's pad data: neutral while the overlay has the pad */
static int ProcessStates(OrbisPadData* pData, const input_state_t* states, s32 num) {
    bool gamepad_input_intercepted;
    const bool touch_held_time = kernel_firmware_version() > KERNEL_FW_350;
    s32 i;
    if (num > 0 && !states[0].connected) {
        memset(&pData[0], 0, sizeof(pData[0]));
        pData[0].orientation.w = 1.0f;
        pData[0].connected = false;
        return 1;
    }

    gamepad_input_intercepted = ovl_pad_captured() != 0;
    for (i = 0; i < num; i++) {
        OrbisPadData* d = &pData[i];
        const input_state_t* st = &states[i];
        memset(d, 0, sizeof(*d));
        if (gamepad_input_intercepted) {
            d->buttons = ORBIS_PAD_BUTTON_INTERCEPTED;
            d->leftStick.x = 128;
            d->leftStick.y = 128;
            d->rightStick.x = 128;
            d->rightStick.y = 128;
            d->orientation.w = 1.0f;
            d->connected = st->connected;
            d->timestamp = st->time;
            d->connectedCount = st->connected_count;
            d->deviceUniqueDataLen = 0;
            continue;
        }

        d->buttons = st->buttons;
        d->leftStick.x = (u8)st->axes[INPUT_AXIS_LEFT_X];
        d->leftStick.y = (u8)st->axes[INPUT_AXIS_LEFT_Y];
        d->rightStick.x = (u8)st->axes[INPUT_AXIS_RIGHT_X];
        d->rightStick.y = (u8)st->axes[INPUT_AXIS_RIGHT_Y];
        d->analogButtons.l2 = (u8)st->axes[INPUT_AXIS_TRIGGER_LEFT];
        d->analogButtons.r2 = (u8)st->axes[INPUT_AXIS_TRIGGER_RIGHT];
        d->acceleration.x = (float)(st->acceleration.x * 0.098);
        d->acceleration.y = (float)(st->acceleration.y * 0.098);
        d->acceleration.z = (float)(st->acceleration.z * 0.098);
        d->angularVelocity.x = st->angular_velocity.x;
        d->angularVelocity.y = st->angular_velocity.y;
        d->angularVelocity.z = st->angular_velocity.z;
        d->orientation.x = st->orientation.x;
        d->orientation.y = st->orientation.y;
        d->orientation.z = st->orientation.z;
        d->orientation.w = st->orientation.w;
        d->touchData.touchNum =
            (u8)((st->touchpad[0].state ? 1 : 0) + (st->touchpad[1].state ? 1 : 0));

        if (!st->touchpad[0].state && st->touchpad[1].state) {
            d->touchData.touch[0].x = st->touchpad[1].x;
            d->touchData.touch[0].y = st->touchpad[1].y;
            d->touchData.touch[0].id = st->touchpad[1].id;
        } else {
            d->touchData.touch[0].x = st->touchpad[0].x;
            d->touchData.touch[0].y = st->touchpad[0].y;
            d->touchData.touch[0].id = st->touchpad[0].id;
            d->touchData.touch[1].x = st->touchpad[1].x;
            d->touchData.touch[1].y = st->touchpad[1].y;
            d->touchData.touch[1].id = st->touchpad[1].id;
        }
        if (touch_held_time) {
            d->touchData.time_since_touch_held_down = (u32)st->touch_time_since_held_down;
        }
        d->connected = st->connected;
        d->timestamp = st->time;
        d->connectedCount = st->connected_count;
        d->deviceUniqueDataLen = 0;
    }

    return num;
}

int PS4_SYSV_ABI scePadRead(s32 handle, OrbisPadData* pData, s32 num) {
    const pad_handle_slot* slot;
    input_state_t states[ORBIS_PAD_MAX_DATA_NUM];
    int ret_num;
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_TRACE, "scePadRead", "called");
    if (pData == NULL || num < 1 || num > ORBIS_PAD_MAX_DATA_NUM) {
        return ORBIS_PAD_ERROR_INVALID_ARG;
    }
    slot = FindHandle(handle);
    if (slot == NULL) {
        return ORBIS_PAD_ERROR_INVALID_HANDLE;
    }
    ret_num = input_controller_read_states(slot->controller, states, num);
    return ProcessStates(pData, states, ret_num);
}

int PS4_SYSV_ABI scePadReadBlasterForTracker(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadReadBlasterForTracker", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadReadExt(s32 handle, OrbisPadData* pData, s32 num) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_TRACE, "scePadReadExt", "handle: %d, num: %d", (int)handle,
          (int)num);
    /* We don't currently support extensions, pass through. */
    return scePadRead(handle, pData, num);
}

int PS4_SYSV_ABI scePadReadForTracker(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadReadForTracker", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadReadHistory(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadReadHistory", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadReadState(s32 handle, OrbisPadData* pData) {
    int result;
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_TRACE, "scePadReadState", "handle: %d", (int)handle);
    result = scePadRead(handle, pData, 1);
    return result < 0 ? result : ORBIS_OK;
}

int PS4_SYSV_ABI scePadReadStateExt(s32 handle, OrbisPadData* pData) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_TRACE, "scePadReadStateExt", "handle: %d", (int)handle);
    /* We don't currently support extensions, pass through. */
    return scePadReadState(handle, pData);
}

int PS4_SYSV_ABI scePadResetLightBar(s32 handle) {
    const pad_handle_slot* slot;
    input_controller_t* controller;
    struct user* u;
    s32 colour_index;
    input_colour_t colour;
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_DEBUG, "scePadResetLightBar", "called, handle: %d",
          (int)handle);
    slot = FindHandle(handle);
    if (slot == NULL) {
        return ORBIS_PAD_ERROR_INVALID_HANDLE;
    }
    controller = slot->controller;
    u = user_manager_get_by_player_index(controller->user_id);
    colour_index = u ? (s32)u->user_color - 1 : 0;
    colour.r = 255;
    colour.g = 0;
    colour.b = 0;
    if (colour_index >= 0 && colour_index <= 3) {
        colour = input_user_colours[colour_index];
    } else {
        c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadResetLightBar",
              "Invalid user colour value %d for controller %d, falling back to blue",
              (int)colour_index, (int)handle);
    }
    input_controller_set_light_bar(controller, colour);
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadResetLightBarAll(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadResetLightBarAll", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadResetLightBarAllByPortType(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadResetLightBarAllByPortType", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadResetOrientation(s32 handle) {
    const pad_handle_slot* slot;
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_INFO, "scePadResetOrientation",
          "scePadResetOrientation called handle = %d", (int)handle);

    slot = FindHandle(handle);
    if (slot == NULL) {
        return ORBIS_PAD_ERROR_INVALID_HANDLE;
    }
    input_controller_reset_orientation(slot->controller);

    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadResetOrientationForTracker(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadResetOrientationForTracker", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadSetAngularVelocityBiasCorrectionState(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadSetAngularVelocityBiasCorrectionState",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadSetAngularVelocityDeadbandState(s32 handle, bool bEnable) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadSetAngularVelocityDeadbandState",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadSetAutoPowerOffCount(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadSetAutoPowerOffCount", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadSetButtonRemappingInfo(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadSetButtonRemappingInfo", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadSetConnection(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadSetConnection", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadSetExtensionReport(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadSetExtensionReport", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadSetFeatureReport(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadSetFeatureReport", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadSetForceIntercepted(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadSetForceIntercepted", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadSetLightBar(s32 handle, const OrbisPadLightBarParam* pParam) {
    const pad_handle_slot* slot = FindHandle(handle);
    if (slot == NULL) {
        return ORBIS_PAD_ERROR_INVALID_HANDLE;
    }
    if (pParam != NULL) {
        input_colour_t colour;
        c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_DEBUG, "scePadSetLightBar",
              "called handle = %d rgb = %u %u %u", (int)handle, (unsigned)pParam->r,
              (unsigned)pParam->g, (unsigned)pParam->b);

        if (pParam->r < 0xD && pParam->g < 0xD && pParam->b < 0xD) {
            c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_INFO, "scePadSetLightBar", "Invalid lightbar setting");
            return ORBIS_PAD_ERROR_INVALID_LIGHTBAR_SETTING;
        }

        colour.r = pParam->r;
        colour.g = pParam->g;
        colour.b = pParam->b;
        input_controller_set_light_bar(slot->controller, colour);
        return ORBIS_OK;
    }
    return ORBIS_PAD_ERROR_INVALID_ARG;
}

int PS4_SYSV_ABI scePadSetLightBarBaseBrightness(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadSetLightBarBaseBrightness", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadSetLightBarBlinking(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadSetLightBarBlinking", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadSetLightBarForTracker(s32 handle, const OrbisPadLightBarParam* pParam) {
    const pad_handle_slot* slot;
    input_colour_t colour;
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_INFO, "scePadSetLightBarForTracker",
          "called, r: %u g: %u b: %u", (unsigned)pParam->r, (unsigned)pParam->g,
          (unsigned)pParam->b);
    slot = FindHandle(handle);
    if (slot == NULL) {
        return ORBIS_PAD_ERROR_INVALID_HANDLE;
    }
    colour.r = pParam->r;
    colour.g = pParam->g;
    colour.b = pParam->b;
    input_controller_set_light_bar(slot->controller, colour);
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadSetLoginUserNumber(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadSetLoginUserNumber", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadSetMotionSensorState(s32 handle, bool bEnable) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadSetMotionSensorState", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadSetProcessFocus(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadSetProcessFocus", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadSetProcessPrivilege(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadSetProcessPrivilege", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadSetProcessPrivilegeOfButtonRemapping(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadSetProcessPrivilegeOfButtonRemapping",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadSetShareButtonMaskForRemotePlay(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadSetShareButtonMaskForRemotePlay",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadSetTiltCorrectionState(s32 handle, bool bEnable) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadSetTiltCorrectionState", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadSetUserColor(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadSetUserColor", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadSetVibration(s32 handle, const OrbisPadVibrationParam* pParam) {
    const pad_handle_slot* slot = FindHandle(handle);
    if (slot == NULL) {
        return ORBIS_PAD_ERROR_INVALID_HANDLE;
    }
    if (pParam != NULL) {
        c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_DEBUG, "scePadSetVibration",
              "scePadSetVibration called handle = %d data = %u , %u", (int)handle,
              (unsigned)pParam->smallMotor, (unsigned)pParam->largeMotor);
        input_controller_set_vibration(slot->controller, pParam->smallMotor, pParam->largeMotor);
        return ORBIS_OK;
    }
    return ORBIS_PAD_ERROR_INVALID_ARG;
}

int PS4_SYSV_ABI scePadSetVibrationForce(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadSetVibrationForce", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadSetVrTrackingMode(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadSetVrTrackingMode", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadShareOutputData(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadShareOutputData", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadStartRecording(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadStartRecording", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadStopRecording(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadStopRecording", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadSwitchConnection(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadSwitchConnection", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadVertualDeviceAddDevice(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadVertualDeviceAddDevice", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadVirtualDeviceAddDevice(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadVirtualDeviceAddDevice", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadVirtualDeviceDeleteDevice(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadVirtualDeviceDeleteDevice", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadVirtualDeviceDisableButtonRemapping(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadVirtualDeviceDisableButtonRemapping",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadVirtualDeviceGetRemoteSetting(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadVirtualDeviceGetRemoteSetting",
          "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePadVirtualDeviceInsertData(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "scePadVirtualDeviceInsertData", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI Pad_Func_298D21481F94C9FA(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "Pad_Func_298D21481F94C9FA", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI Pad_Func_51E514BCD3A05CA5(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "Pad_Func_51E514BCD3A05CA5", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI Pad_Func_89C9237E393DA243(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "Pad_Func_89C9237E393DA243", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI Pad_Func_EF103E845B6F0420(void) {
    c_log(C_LOG_LIB_PAD, C_LOG_LEVEL_ERROR, "Pad_Func_EF103E845B6F0420", "(STUBBED) called");
    return ORBIS_OK;
}

void pad_library_init(void) {
    input_connect_first_controller();
}
