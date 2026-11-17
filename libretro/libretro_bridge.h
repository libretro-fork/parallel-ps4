// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <stop_token>

// What the emulator calls in the libretro core's side of its own process (host.cpp), which
// passes it on to the core in the frontend's process.
namespace Libretro {

/// The slots a frame can be in, in the memory shared with the core
/// (shared_state.h): the newest one published, the one the core is showing, and
/// three for the emulator to write the next into. The GPU writes a frame
/// straight into its slot (AcquireFrameSlot), so each frame it has not finished
/// yet holds one.
inline constexpr std::uint32_t FRAME_SLOTS = 5;

/// A frame's size at most, and the bytes of a slot
inline constexpr std::uint32_t MAX_WIDTH = 3840;
inline constexpr std::uint32_t MAX_HEIGHT = 2160;
inline constexpr std::size_t FRAME_BYTES =
    static_cast<std::size_t>(MAX_WIDTH) * MAX_HEIGHT * 4;

/// A finished frame, read back from the GPU: 32 bits a pixel, bytes B, G, R, X,
/// rows top to bottom, no padding between them, copied into the shared memory.
/// With HDR Output the frame is A2B10G10R10, and put in libretro's order on the
/// way unless @in_order says it is already (A2R10G10B10). Called from the
/// renderer's thread.
void PushFrame(const void *pixels, std::uint32_t width, std::uint32_t height,
               bool in_order);

/// A slot of the shared memory for the GPU to read the next frame back into
/// itself, with no copy after (PublishFrameSlot, or ReleaseFrameSlot if it is
/// not written after all): not the newest frame's, not the one the core is
/// showing, and not one another frame is being written into. Null when all are
/// taken; nothing waits. A frame is then read back as before.
std::uint8_t *AcquireFrameSlot(std::uint32_t &slot);
/// The frame in @slot is finished, the GPU's work on it done: as PushFrame has
/// it.
void PublishFrameSlot(std::uint32_t slot, std::uint32_t width,
                      std::uint32_t height);
void ReleaseFrameSlot(std::uint32_t slot);

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

/// The image to draw the next frame into: not the newest one published, not the
/// one the core is showing, and not one the frontend may still draw from. One
/// free if there is one, else one whose frame the GPU is still drawing, taken
/// over for the next (that frame is then not published). Never waits.
/// @generation is the frame's, for PushImage.
std::uint32_t AcquireImage(int &generation);

/// A frame is finished in an image, the GPU's work on it done: published unless
/// a newer frame took the image over. Called from the renderer.
void PushImage(std::uint32_t image, int generation, std::uint32_t width,
               std::uint32_t height);

/// A pad's motors, 0 to 255 each, as the game set them. The pad is the controller's index.
void SetRumble(std::uint32_t pad, std::uint8_t small_motor, std::uint8_t large_motor);

/// The frontend's frames so far, one a retro_run (shared_state.h, vblank_ticks); it wraps.
std::uint32_t VblankTicks();
/// Until the frontend's frames are past @seen, the emulator quits, or @stop is requested
/// (which wakes it). Nothing is timed: it parks on the core's notify. @return false when
/// stopped or quitting.
bool WaitVblankTicks(std::uint32_t seen, const std::stop_token& stop);

} // namespace Libretro
