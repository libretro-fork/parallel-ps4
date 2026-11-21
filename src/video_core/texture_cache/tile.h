// SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/alignment.h"
#include "common/assert.h"
#include "common/types.h"
#include "video_core/amdgpu/tiling.h"

namespace AmdGpu {
enum class TileMode : u32;
}

namespace VideoCore {

/// A macro tile's width and height in pixels
struct TileExtent {
    u32 width;
    u32 height;
};

// clang-format off
// The table of macro tiles parameters for given tiling index (row) and bpp (column)
/* Calculation:
 * - Inputs:
 *     TileMode, BytesPerPixel, NumFragments
 * - Constants:
 *     MicroTileWidth = 8, MicroTileHeight = 8,
 *     Tile Mode LUTs: IsDepth(), IsPrt(), TileThickness(), TileSplit(), SampleSplit(), NumPipes()
 *     Macro Tile Mode LUTs: BankWidth(), BankHeight(), NumBanks(), MacroTileAspect()
 * - Determine the macro tile mode:
 *     TileBytes = MicroTileWidth * MicroTileHeight * TileThickness(TileMode) * BytesPerPixel
 *     TileSplit = min(IsDepth(TileMode) ? TileSplit(TileMode) : max(TileBytes * SampleSplit(TileMode), 256), NumFragments * TileBytes, 1024)
 *     MacroTileModeIndex = log2(TileSplit / 64)
 *     MacroTileMode = IsPrt(TileMode) ? MacroTileModeIndex + 8 : MacroTileModeIndex
 * - Calculate macro tile width and height:
 *     Width = NumPipes(TileMode) * BankWidth(MacroTileMode) * MicroTileWidth * MacroTileAspect(MacroTileMode, AltTileMode)
 *     Height = NumBanks(MacroTileMode, AltTileMode) * BankHeight(MacroTileMode, AltTileMode) * MicroTileHeight / MacroTileAspect(MacroTileMode, AltTileMode)
 */

constexpr TileExtent macro_tile_extents_x1[] = {
    TileExtent{256u, 128u}, TileExtent{256u, 128u}, TileExtent{256u, 128u}, TileExtent{256u, 128u}, TileExtent{256u, 128u}, // 00
    TileExtent{256u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, // 01
    TileExtent{256u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{128u, 64u},  // 02
    TileExtent{256u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{128u, 64u},  // 03
    TileExtent{256u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{64u, 64u},   // 04
    TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     // 05
    TileExtent{256u, 256u}, TileExtent{256u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, // 06
    TileExtent{256u, 256u}, TileExtent{256u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{64u, 64u},   // 07
    TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     // 08
    TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     // 09
    TileExtent{256u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{64u, 64u},   // 0A
    TileExtent{256u, 256u}, TileExtent{256u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{64u, 64u},   // 0B
    TileExtent{256u, 256u}, TileExtent{256u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{64u, 64u},   // 0C
    TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     // 0D
    TileExtent{256u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{64u, 64u},   // 0E
    TileExtent{256u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{64u, 64u},   // 0F
    TileExtent{256u, 256u}, TileExtent{256u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{64u, 64u},   // 10
    TileExtent{256u, 256u}, TileExtent{256u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{64u, 64u},   // 11
    TileExtent{256u, 256u}, TileExtent{256u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{64u, 64u},   // 12
    TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     // 13
    TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 14
    TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 15
    TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 16
    TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 17
    TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 18
    TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 19
    TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 1A
};

constexpr TileExtent macro_tile_extents_x2[] = {
    TileExtent{256u, 128u}, TileExtent{256u, 128u}, TileExtent{256u, 128u}, TileExtent{256u, 128u}, TileExtent{256u, 128u}, // 00
    TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, // 01
    TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{128u, 64u},  // 02
    TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{128u, 64u},  // 03
    TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 04
    TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     // 05
    TileExtent{256u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, // 06
    TileExtent{256u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 07
    TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     // 08
    TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     // 09
    TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 0A
    TileExtent{256u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 0B
    TileExtent{256u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 0C
    TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     // 0D
    TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 0E
    TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 0F
    TileExtent{256u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 10
    TileExtent{256u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 11
    TileExtent{256u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 12
    TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     // 13
    TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 14
    TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 15
    TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 16
    TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 17
    TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 18
    TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 19
    TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 1A
};

constexpr TileExtent macro_tile_extents_x4[] = {
    TileExtent{256u, 128u}, TileExtent{256u, 128u}, TileExtent{256u, 128u}, TileExtent{256u, 128u}, TileExtent{256u, 128u}, // 00
    TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, // 01
    TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{128u, 64u},  // 02
    TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{128u, 64u},  // 03
    TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 04
    TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     // 05
    TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, // 06
    TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 07
    TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     // 08
    TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     // 09
    TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 0A
    TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 0B
    TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 0C
    TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     // 0D
    TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 0E
    TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 0F
    TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 10
    TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 11
    TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 12
    TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     // 13
    TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 14
    TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 15
    TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 16
    TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 17
    TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 18
    TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 19
    TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 1A
};

constexpr TileExtent macro_tile_extents_x8[] = {
    TileExtent{256u, 128u}, TileExtent{256u, 128u}, TileExtent{256u, 128u}, TileExtent{256u, 128u}, TileExtent{256u, 128u}, // 00
    TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, // 01
    TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{128u, 64u},  // 02
    TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{128u, 64u},  // 03
    TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 04
    TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     // 05
    TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, // 06
    TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 07
    TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     // 08
    TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     // 09
    TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 0A
    TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 0B
    TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 0C
    TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     // 0D
    TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 0E
    TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 0F
    TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 10
    TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 11
    TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 12
    TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     // 13
    TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 14
    TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 15
    TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 16
    TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 17
    TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 18
    TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 19
    TileExtent{128u, 64u},  TileExtent{64u, 64u},   TileExtent{64u, 64u},   TileExtent{64u, 64u},   TileExtent{64u, 64u},   // 1A
};

constexpr TileExtent macro_tile_extents_alt_x1[] = {
    TileExtent{256u, 128u}, TileExtent{256u, 128u}, TileExtent{256u, 128u}, TileExtent{256u, 128u}, TileExtent{256u, 128u}, // 00
    TileExtent{256u, 128u}, TileExtent{256u, 128u}, TileExtent{256u, 128u}, TileExtent{256u, 128u}, TileExtent{256u, 128u}, // 01
    TileExtent{256u, 128u}, TileExtent{256u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, // 02
    TileExtent{256u, 128u}, TileExtent{256u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, // 03
    TileExtent{256u, 128u}, TileExtent{256u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  // 04
    TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     // 05
    TileExtent{256u, 256u}, TileExtent{256u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, // 06
    TileExtent{256u, 256u}, TileExtent{256u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 32u},  // 07
    TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     // 08
    TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     // 09
    TileExtent{256u, 128u}, TileExtent{256u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  // 0A
    TileExtent{256u, 256u}, TileExtent{256u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 32u},  // 0B
    TileExtent{256u, 256u}, TileExtent{256u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 32u},  // 0C
    TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     // 0D
    TileExtent{256u, 128u}, TileExtent{256u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  // 0E
    TileExtent{256u, 128u}, TileExtent{256u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  // 0F
    TileExtent{256u, 256u}, TileExtent{256u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 32u},  // 10
    TileExtent{256u, 256u}, TileExtent{256u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 32u},  // 11
    TileExtent{256u, 256u}, TileExtent{256u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 32u},  // 12
    TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     // 13
    TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{128u, 64u},  // 14
    TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{128u, 64u},  // 15
    TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 32u},  TileExtent{128u, 32u},  TileExtent{128u, 32u},  // 16
    TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 32u},  TileExtent{128u, 32u},  TileExtent{128u, 32u},  // 17
    TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 32u},  TileExtent{128u, 32u},  TileExtent{128u, 32u},  // 18
    TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{128u, 64u},  // 19
    TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{128u, 64u},  // 1A
};

constexpr TileExtent macro_tile_extents_alt_x2[] = {
    TileExtent{256u, 128u}, TileExtent{256u, 128u}, TileExtent{256u, 128u}, TileExtent{256u, 128u}, TileExtent{256u, 128u}, // 00
    TileExtent{256u, 128u}, TileExtent{256u, 128u}, TileExtent{256u, 128u}, TileExtent{256u, 128u}, TileExtent{256u, 128u}, // 01
    TileExtent{256u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, // 02
    TileExtent{256u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, // 03
    TileExtent{256u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 64u},  // 04
    TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     // 05
    TileExtent{256u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, // 06
    TileExtent{256u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 32u},  TileExtent{128u, 32u},  // 07
    TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     // 08
    TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     // 09
    TileExtent{256u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 64u},  // 0A
    TileExtent{256u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 32u},  TileExtent{128u, 32u},  // 0B
    TileExtent{256u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 32u},  TileExtent{128u, 32u},  // 0C
    TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     // 0D
    TileExtent{256u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 64u},  // 0E
    TileExtent{256u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 64u},  // 0F
    TileExtent{256u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 32u},  TileExtent{128u, 32u},  // 10
    TileExtent{256u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 32u},  TileExtent{128u, 32u},  // 11
    TileExtent{256u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 32u},  TileExtent{128u, 32u},  // 12
    TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     // 13
    TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{128u, 64u},  // 14
    TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{128u, 64u},  // 15
    TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 32u},  TileExtent{128u, 32u},  TileExtent{128u, 32u},  // 16
    TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 32u},  TileExtent{128u, 32u},  TileExtent{128u, 32u},  // 17
    TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 32u},  TileExtent{128u, 32u},  TileExtent{128u, 32u},  // 18
    TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{128u, 64u},  // 19
    TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{128u, 64u},  // 1A
};

constexpr TileExtent macro_tile_extents_alt_x4[] = {
    TileExtent{256u, 128u}, TileExtent{256u, 128u}, TileExtent{256u, 128u}, TileExtent{256u, 128u}, TileExtent{256u, 128u}, // 00
    TileExtent{256u, 128u}, TileExtent{256u, 128u}, TileExtent{256u, 128u}, TileExtent{256u, 128u}, TileExtent{256u, 128u}, // 01
    TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, // 02
    TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, // 03
    TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{128u, 64u},  // 04
    TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     // 05
    TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, // 06
    TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 32u},  TileExtent{128u, 32u},  TileExtent{128u, 32u},  // 07
    TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     // 08
    TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     // 09
    TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 64u},  // 0A
    TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 32u},  TileExtent{128u, 32u},  // 0B
    TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 32u},  TileExtent{128u, 32u},  // 0C
    TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     // 0D
    TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 64u},  // 0E
    TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 64u},  // 0F
    TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 32u},  TileExtent{128u, 32u},  // 10
    TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 32u},  TileExtent{128u, 32u},  // 11
    TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 32u},  TileExtent{128u, 32u},  // 12
    TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     // 13
    TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{128u, 64u},  // 14
    TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{128u, 64u},  // 15
    TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 32u},  TileExtent{128u, 32u},  TileExtent{128u, 32u},  // 16
    TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 32u},  TileExtent{128u, 32u},  TileExtent{128u, 32u},  // 17
    TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 32u},  TileExtent{128u, 32u},  TileExtent{128u, 32u},  // 18
    TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{128u, 64u},  // 19
    TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{128u, 64u},  // 1A
};

constexpr TileExtent macro_tile_extents_alt_x8[] = {
    TileExtent{256u, 128u}, TileExtent{256u, 128u}, TileExtent{256u, 128u}, TileExtent{256u, 128u}, TileExtent{256u, 128u}, // 00
    TileExtent{256u, 128u}, TileExtent{256u, 128u}, TileExtent{256u, 128u}, TileExtent{256u, 128u}, TileExtent{256u, 128u}, // 01
    TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, // 02
    TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, // 03
    TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{128u, 64u},  // 04
    TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     // 05
    TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, // 06
    TileExtent{128u, 64u},  TileExtent{128u, 32u},  TileExtent{128u, 32u},  TileExtent{128u, 32u},  TileExtent{128u, 32u},  // 07
    TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     // 08
    TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     // 09
    TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 64u},  // 0A
    TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 32u},  TileExtent{128u, 32u},  // 0B
    TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 32u},  TileExtent{128u, 32u},  // 0C
    TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     // 0D
    TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 64u},  // 0E
    TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 64u},  // 0F
    TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 32u},  TileExtent{128u, 32u},  // 10
    TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 32u},  TileExtent{128u, 32u},  // 11
    TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 32u},  TileExtent{128u, 32u},  // 12
    TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     TileExtent{0u, 0u},     // 13
    TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{128u, 64u},  // 14
    TileExtent{128u, 128u}, TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{128u, 64u},  // 15
    TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 32u},  TileExtent{128u, 32u},  TileExtent{128u, 32u},  // 16
    TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 32u},  TileExtent{128u, 32u},  TileExtent{128u, 32u},  // 17
    TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 32u},  TileExtent{128u, 32u},  TileExtent{128u, 32u},  // 18
    TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{128u, 64u},  // 19
    TileExtent{128u, 128u}, TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{128u, 64u},  TileExtent{128u, 64u},  // 1A
};

constexpr const TileExtent* macro_tile_extents[] = {
    macro_tile_extents_x1,
    macro_tile_extents_x2,
    macro_tile_extents_x4,
    macro_tile_extents_x8,
};

constexpr const TileExtent* macro_tile_extents_alt[] = {
    macro_tile_extents_alt_x1,
    macro_tile_extents_alt_x2,
    macro_tile_extents_alt_x4,
    macro_tile_extents_alt_x8,
};
// clang-format on

constexpr TileExtent micro_tile_extent{8u, 8u};
constexpr u32 hw_pipe_interleave = 256u;

constexpr TileExtent GetMacroTileExtents(AmdGpu::TileMode tile_mode, u32 bpp, u32 num_samples,
                                         bool alt) {
    ASSERT(num_samples > 0 && num_samples <= 8);
    const u32 samples_log = Common::BitWidth(num_samples) - 1;
    const u32 row = u32(tile_mode) * 5;
    const u32 column = Common::BitWidth(bpp) - 4; // bpps are 8, 16, 32, 64, 128
    return (alt ? macro_tile_extents_alt : macro_tile_extents)[samples_log][row + column];
}

/// A mip's pitch and height as the hardware lays them out (16 bits each, as kept), and its size
struct ImageSize {
    u16 pitch;
    u16 height;
    size_t size;
};

constexpr ImageSize ImageSizeLinearAligned(u32 pitch, u32 height, u32 bpp, u32 num_samples) {
    const u32 pitch_align = Common::Max(8u, 64u / ((bpp + 7) / 8));
    u32 pitch_aligned = (pitch + pitch_align - 1) & ~(pitch_align - 1);
    const u32 height_aligned = height;
    size_t log_sz = pitch_aligned * height_aligned * num_samples;
    const u32 slice_align = Common::Max(64u, 256u / ((bpp + 7) / 8));
    while (log_sz % slice_align) {
        pitch_aligned += pitch_align;
        log_sz = pitch_aligned * height_aligned * num_samples;
    }
    return {static_cast<u16>(pitch_aligned), static_cast<u16>(height_aligned),
            (log_sz * bpp + 7) / 8};
}

constexpr ImageSize ImageSizeMicroTiled(u32 pitch, u32 height, u32 thickness, u32 bpp,
                                        u32 num_samples) {
    constexpr u32 pitch_align = micro_tile_extent.width;
    constexpr u32 height_align = micro_tile_extent.height;
    u32 pitch_aligned = (pitch + pitch_align - 1) & ~(pitch_align - 1);
    const u32 height_aligned = (height + height_align - 1) & ~(height_align - 1);
    size_t log_sz = (pitch_aligned * height_aligned * bpp * num_samples + 7) / 8;
    while ((log_sz * thickness) % 256) {
        pitch_aligned += pitch_align;
        log_sz = (pitch_aligned * height_aligned * bpp * num_samples + 7) / 8;
    }
    return {static_cast<u16>(pitch_aligned), static_cast<u16>(height_aligned), log_sz};
}

constexpr bool IsMacroTiledMip(u32 pitch, u32 height, u32 bpp, u32 num_samples,
                               AmdGpu::TileMode tile_mode, u32 mip_n, bool alt) {
    if (mip_n == 0) {
        return true;
    }
    const TileExtent align = GetMacroTileExtents(tile_mode, bpp, num_samples, alt);
    return pitch >= align.width && height >= align.height;
}

constexpr ImageSize ImageSizeMacroTiled(u32 pitch, u32 height, u32 bpp, u32 num_samples,
                                        AmdGpu::TileMode tile_mode, bool alt) {
    const TileExtent align = GetMacroTileExtents(tile_mode, bpp, num_samples, alt);
    const u32 pitch_align = align.width;
    const u32 height_align = align.height;
    ASSERT(pitch_align != 0 && height_align != 0);
    const u32 pitch_aligned = (pitch + pitch_align - 1) & ~(pitch_align - 1);
    const u32 height_aligned = (height + height_align - 1) & ~(height_align - 1);
    const u32 log_sz = pitch_aligned * height_aligned * num_samples;
    return {static_cast<u16>(pitch_aligned), static_cast<u16>(height_aligned),
            (log_sz * bpp + 7) / 8};
}

struct MipInfo {
    u32 size;
    u16 pitch;
    u16 height;
    u32 offset;
};

static constexpr size_t MAX_MIPS = 16;

constexpr void ComputeImageSize(u32 pitch, u32 height, u32 depth, u32 levels, u32 layers,
                                u32 num_bits, u32 num_samples, bool is_block, bool is_pow2,
                                bool alt_tile, AmdGpu::TileMode tile_mode,
                                AmdGpu::ArrayMode array_mode, u32* out_size,
                                u32* out_micro_tiled_mips, MipInfo* out_mips_layout) {
    const u32 thickness = AmdGpu::GetMicroTileThickness(array_mode);
    const bool macro = AmdGpu::IsMacroTiled(array_mode);
    *out_size = 0;
    *out_micro_tiled_mips = 0;
    for (s32 mip = 0; mip < levels; ++mip) {
        u32 mip_w = pitch >> mip;
        u32 mip_h = height >> mip;
        if (is_block) {
            mip_w = (mip_w + 3) / 4;
            mip_h = (mip_h + 3) / 4;
        }
        mip_w = Common::Max(mip_w, 1u);
        mip_h = Common::Max(mip_h, 1u);
        u32 mip_d = Common::Max(depth >> mip, 1u);

        if (is_pow2) {
            mip_w = Common::BitCeil(mip_w);
            mip_h = Common::BitCeil(mip_h);
            mip_d = Common::BitCeil(mip_d);
        }

        MipInfo& mip_info = out_mips_layout[mip];
        u32 mip_thickness = 1;
        ImageSize mip_size{};
        if (array_mode == AmdGpu::ArrayMode::ArrayLinearAligned) {
            mip_size = ImageSizeLinearAligned(mip_w, mip_h, num_bits, num_samples);
        } else if (macro &&
                   IsMacroTiledMip(mip_w, mip_h, num_bits, num_samples, tile_mode, mip, alt_tile)) {
            mip_thickness = thickness;
            mip_size =
                ImageSizeMacroTiled(mip_w, mip_h, num_bits, num_samples, tile_mode, alt_tile);
        } else {
            mip_thickness = Common::Min(thickness, 4u);
            mip_size = ImageSizeMicroTiled(mip_w, mip_h, mip_thickness, num_bits, num_samples);
            if (macro) {
                *out_micro_tiled_mips |= 1u << mip;
            }
        }
        mip_info.pitch = mip_size.pitch;
        mip_info.height = mip_size.height;
        mip_info.size = static_cast<u32>(mip_size.size);
        if (is_block) {
            mip_info.pitch = static_cast<u16>(Common::Max<u32>(u16(mip_info.pitch * 4), 32u));
            mip_info.height = static_cast<u16>(Common::Max<u32>(u16(mip_info.height * 4), 32u));
        }
        u32 num_slices = mip_d * layers;
        num_slices += (-num_slices) & (mip_thickness - 1);
        mip_info.size *= num_slices;
        mip_info.offset = *out_size;
        *out_size += mip_info.size;
    }
}

struct TableEntry {
    u32 guest_size;
    u32 micro_tiled_mips;
    MipInfo mips_layout[MAX_MIPS];
};

static constexpr u32 MAX_DIM_LOG2 = 12;

/// The layouts of the square power-of-two block-compressed images, by log2 of their size
struct Pow2ImageTable {
    TableEntry entries[MAX_DIM_LOG2];

    constexpr const TableEntry& operator[](u32 index) const {
        return entries[index];
    }
};

constexpr Pow2ImageTable MakePow2BcnImageTable(u32 num_bits) {
    Pow2ImageTable table{};
    for (u32 i = 0; i < MAX_DIM_LOG2; ++i) {
        const u32 pitch = 1 << i;
        ComputeImageSize(pitch, pitch, 1, Common::BitWidth(pitch), 1, num_bits, 1, true, true,
                         false, AmdGpu::TileMode::Thin1DThin, AmdGpu::ArrayMode::Array1DTiledThin1,
                         &table.entries[i].guest_size, &table.entries[i].micro_tiled_mips,
                         table.entries[i].mips_layout);
    }
    return table;
}

constexpr Pow2ImageTable Pow2Bcn64ImageTable = MakePow2BcnImageTable(64);
constexpr Pow2ImageTable Pow2Bcn128ImageTable = MakePow2BcnImageTable(128);

} // namespace VideoCore
