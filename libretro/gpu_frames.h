// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstdint>

#include "libretro.h"

namespace Libretro {
struct SharedState;
class SharedWake;
}

// The picture handed to the frontend on the GPU, in the frontend's process (gpu_frames.cpp):
// the emulator's finished images are imported into the frontend's Vulkan device and shown from
// there, in place of being read back into system memory and uploaded again. Linux and Windows,
// and a frontend whose video driver is Vulkan.
namespace Libretro::GpuFrames {

/// Asks the frontend for a Vulkan context, in retro_load_game, whatever its video driver: a
/// frontend that can switches to its Vulkan driver. False when it gives none: the picture then
/// goes through system memory as software frames.
bool Request(retro_environment_t environment, retro_log_printf_t log);

/// The memory shared with the emulator, in which it is told which GPU the frontend is on, and
/// the wake that tells it. nullptr once that memory is gone.
void SetShared(SharedState* shared, SharedWake* wake);

/// Takes what the emulator has handed over about its images: on Linux over the socket, on
/// Windows in the shared memory, with handles to duplicate from the emulator's process.
void Receive(int socket, void* emulator_process);

/// Shows the image in a slot, or the last one again when there is none to show yet.
void Show(retro_video_refresh_t video, std::uint32_t slot, bool have_frame, unsigned width,
          unsigned height);

/// Where the images cannot be shared: a frame read back into the shared memory (XRGB8888,
/// width * 4 bytes a row), uploaded into an image of the frontend's device and shown from there.
void ShowPixels(retro_video_refresh_t video, const void* pixels, unsigned width, unsigned height);

/// The emulator's images are imported, and frames are shown from them.
bool Shared();

/// Why the picture cannot be handed over on the GPU, once that is known; nullptr otherwise.
const char* Failure();

/// Content closed: everything imported is let go.
void Close();

} // namespace Libretro::GpuFrames
