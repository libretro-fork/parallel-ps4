// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/enum.h"
#include "core/libraries/system/userservice.h"

namespace Core::Loader {
class SymbolsResolver;
}

namespace Libraries::Mouse {

struct OrbisMouseData {
    u64 timestamp;
    bool connected;
    u32 buttons;
    s32 x_axis;
    s32 y_axis;
    s32 wheel;
    s32 tilt;
    u8 reserve[8];
};

enum class MouseOpenBehaviour : u8 {
    Normal = 0,
    Merged = 1,
};
DECLARE_ENUM_FLAG_OPERATORS(MouseOpenBehaviour);

struct OrbisMouseOpenParam {
    MouseOpenBehaviour flag;
    u8 reserve[7];
};

s32 PS4_SYSV_ABI sceMouseClose(s32 handle);
s32 PS4_SYSV_ABI sceMouseConnectPort();
s32 PS4_SYSV_ABI sceMouseDebugGetDeviceId();
s32 PS4_SYSV_ABI sceMouseDeviceOpen();
s32 PS4_SYSV_ABI sceMouseDisconnectDevice();
s32 PS4_SYSV_ABI sceMouseDisconnectPort();
s32 PS4_SYSV_ABI sceMouseGetDeviceInfo();
s32 PS4_SYSV_ABI sceMouseInit();
s32 PS4_SYSV_ABI sceMouseMbusInit();
s32 PS4_SYSV_ABI sceMouseOpen(Libraries::UserService::OrbisUserServiceUserId userId, s32 type,
                              s32 index, OrbisMouseOpenParam* pParam);
s32 PS4_SYSV_ABI sceMouseRead(s32 handle, OrbisMouseData* pData, s32 num);
s32 PS4_SYSV_ABI sceMouseSetHandType();
s32 PS4_SYSV_ABI sceMouseSetPointerSpeed();
s32 PS4_SYSV_ABI sceMouseSetProcessPrivilege();

void RegisterLib(Core::Loader::SymbolsResolver* sym);
} // namespace Libraries::Mouse