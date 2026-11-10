// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/types.h"

namespace Libraries::Camera {

// The picture the camera library hands the game: what a backend lends it, in the console
// camera's own format and size - YUY2 (Y0 U Y1 V), CAMERA_WIDTH x CAMERA_HEIGHT, rows
// packed. The library converts it once, straight into the buffers the game reads.
inline constexpr u32 CAMERA_WIDTH = 1280;
inline constexpr u32 CAMERA_HEIGHT = 800;

struct CameraFrame {
    const u8* yuy2;   // borrowed: valid until the next AcquireFrame or Stop
    u64 timestamp_us; // when it was captured
};

// Where the camera's pictures come from. The core has no camera yet (NoCamera, camera.cpp):
// the frontend's camera (libretro's RETRO_ENVIRONMENT_GET_CAMERA_INTERFACE) is the one meant
// to plug in here, its frames passed over from the core's process. Called from the game's
// threads; an implementation must not block them beyond handing over a frame it has.
class CameraBackend {
public:
    virtual ~CameraBackend() = default;

    // A camera is there (sceCameraIsAttached).
    virtual bool IsAttached() = 0;

    // The game starts the camera: frames at the size above, up to @fps a second.
    virtual bool Start(u32 fps) = 0;
    virtual void Stop() = 0;

    // The newest frame not handed over yet, or false when there is none since the last.
    virtual bool AcquireFrame(CameraFrame& frame) = 0;
};

// The backend in use, made once when the library is first opened.
CameraBackend& GetCameraBackend();

} // namespace Libraries::Camera
