// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <stdint.h>

using s8 = int8_t;
using s16 = int16_t;
using s32 = int32_t;
using s64 = int64_t;

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using f32 = float;
using f64 = double;

using VAddr = uintptr_t;
using PAddr = uintptr_t;

#define PS4_SYSV_ABI __attribute__((sysv_abi))

// UDLs for memory size values
constexpr unsigned long long operator""_KB(unsigned long long x) {
    return 1024ULL * x;
}
constexpr unsigned long long operator""_MB(unsigned long long x) {
    return 1024_KB * x;
}
constexpr unsigned long long operator""_GB(unsigned long long x) {
    return 1024_MB * x;
}
