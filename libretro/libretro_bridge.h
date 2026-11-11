// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstdint>

// What the emulator calls in the libretro core's side of its own process (host.cpp), which
// passes it on to the core in the frontend's process.
namespace Libretro {

/// A finished frame, read back from the GPU: 32 bits a pixel, bytes B, G, R, X, rows top to
/// bottom, no padding between them. Called from the renderer's thread.
void PushFrame(const void* pixels, std::uint32_t width, std::uint32_t height);

// Resolution: Default - the size of the game's video out buffers, as it registers them. The
// picture follows it: TakeGameSize hands a new one to the presenter once, and until the
// first, GameSizePending holds back the images shared with the frontend.
void SetGameSize(std::uint32_t width, std::uint32_t height);
bool TakeGameSize(std::uint32_t& width, std::uint32_t& height);
bool GameSizePending();

/// The rate the frontend is given the sound at.
inline constexpr int AUDIO_SAMPLE_RATE = 48000;

/// Sets the emulator's settings from the frontend's core options, as they were when the game was
/// loaded. Called by the emulator once it has loaded its own configuration.
void ApplyCoreOptions();

/// Whether the picture is to be handed to the frontend on the GPU (libretro/gpu_frames.h): the
/// core option says so and the frontend gave the core a Vulkan context.
bool GpuFramesWanted();

/// The GPU the frontend's Vulkan device is on, by its device UUID, for the emulator to make
/// its own on. Waits a few seconds for the core to learn it. False when the picture is not
/// handed over on the GPU or the frontend's GPU is not known: the emulator then picks as usual.
bool FrontendGpu(std::uint8_t* device_uuid);

/// How many images the emulator draws its frames into then.
std::uint32_t GpuFrameImages();

/// The handle to an image's memory that the core is sent: a descriptor on Linux, an NT handle
/// on Windows. The device and memory are Vulkan's handles, and the function the device's
/// vkGetDeviceProcAddr. False when the memory could not be exported.
bool ExportImageMemory(void* device, std::uint64_t memory, void* get_device_proc_addr,
                       std::uint64_t& handle);

/// Lets go of such a handle.
void CloseImageMemory(std::uint64_t handle);

/// The images that stand in for a swapchain, made of exportable memory: a handle to each one's
/// memory (ExportImageMemory). On Linux they are sent and can be closed afterwards; on Windows
/// the core duplicates them later, and they stay open. The format and usage are Vulkan's
/// values. False when they could not be sent; the picture then goes through system memory, as
/// it does without this.
bool ShareImages(const std::uint64_t* memory_handles, std::uint32_t count, std::uint32_t width,
                 std::uint32_t height, std::uint32_t format, std::uint32_t usage,
                 std::uint32_t memory_type, const std::uint64_t* allocation_sizes,
                 const std::uint8_t* device_uuid, const std::uint8_t* driver_uuid);

/// Whether the images were shared: finished frames are published with PushImage, not PushFrame.
bool GpuFramesActive();
/// The emulator's images cannot be shared with the frontend: frames are read back into the
/// shared memory from now on, and the core is told why.
void GpuFramesFailed(const char* why);

/// The image to draw the next frame into: not the newest one published, not the one the core
/// is showing, and not one still being drawn. Waits a moment when all are taken.
std::uint32_t AcquireImage();

/// A frame is finished in an image, the GPU's work on it done. Called from the renderer.
void PushImage(std::uint32_t image, std::uint32_t width, std::uint32_t height);

/// A pad's motors, 0 to 255 each, as the game set them. The pad is the controller's index.
void SetRumble(std::uint32_t pad, std::uint8_t small_motor, std::uint8_t large_motor);

} // namespace Libretro
