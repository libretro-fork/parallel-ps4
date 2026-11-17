// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/types.h"
#include "video_core/renderer_vulkan/vk_common.h"

namespace Vulkan {
class Instance;
}

/// The overlays (overlay/ovl.h) on the presenter's images: built once a frame, drawn over the
/// game's picture in one pass.
namespace Overlay {

/// Made with the swapchain: the fonts, the atlas, the pipeline and a frame's buffers for each of
/// its @image_count images
void Initialize(const Vulkan::Instance& instance, u32 image_count, vk::Format surface_format);

/// After the device has gone idle
void Shutdown(vk::Device device);

/// Builds the frame's overlays over a @width x @height picture. A frame of the game's own
/// (not @is_reusing_frame) also measures its frame rate, which the FPS counter shows.
void NewFrame(u32 width, u32 height, bool is_reusing_frame);

/// Draws what NewFrame built over @image_view, in the colour attachment layout, keeping what
/// is there; with nothing to draw, records nothing.
void Render(vk::CommandBuffer cmdbuf, vk::ImageView image_view, vk::Extent2D extent);

} // namespace Overlay
