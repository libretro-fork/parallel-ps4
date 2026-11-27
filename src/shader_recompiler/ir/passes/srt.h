// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/small_vector.h"
#include "common/types.h"

namespace Serialization {
struct Archive;
}

namespace Shader {

// zeroes: SrtZeroes(), what a walker reads from a null table in its place
using PFN_SrtWalker = void PS4_SYSV_ABI (*)(const u32* /*user_data*/, u32* /*flat_dst*/,
                                            const u32* /*zeroes*/);
// Bytes of zeroes a walker can read a null table's values from: a level of tables whose values
// all sit within them (and are read at fixed offsets) reads them there, with no fault
constexpr u32 SrtZeroBytes = 64 * 1024;
const u32* SrtZeroes();
PFN_SrtWalker RegisterWalkerCode(const u8* ptr, size_t size);
#if defined(__aarch64__) && defined(__linux__)
// Runs an x86-64 walker on aarch64 (srt_walker_interpreter.cpp)
void RunSrtWalker(PFN_SrtWalker walker, const u32* user_data, u32* flat_dst, const u32* zeroes);
#endif

struct PersistentSrtInfo {
    // Special case when fetch shader uses step rates.
    struct SrtSharpReservation {
        u32 sgpr_base;
        u32 dword_offset;
        u32 num_dwords;
    };

    PFN_SrtWalker walker_func{};
    size_t walker_func_size{};
    u32 flattened_bufsize_dw = 16; // NumUserDataRegs
};

} // namespace Shader
