// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <utility>

#include "common/types.h"

namespace DebugStateType {

/// What the presenter measures for the overlays: the frame rate the FPS counter shows, and the
/// sizes the on-screen keyboard places itself by. Written by the presenter's thread.
struct DebugStateImpl {
    float Framerate = 1.0f / 60.0f;
    float FrameDeltaTime{};
    std::pair<u32, u32> game_resolution{};
    std::pair<u32, u32> output_resolution{};
};

} // namespace DebugStateType

inline DebugStateType::DebugStateImpl DebugState{};
