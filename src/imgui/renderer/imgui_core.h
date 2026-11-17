// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <imgui.h>

#include "video_core/renderer_vulkan/vk_instance.h"
#include "vulkan/vulkan_handles.hpp"

namespace Frontend {
class Window;
}

namespace Vulkan {
struct Frame;
}

namespace ImGui::Core {

void Initialize(const Vulkan::Instance& instance, const Frontend::Window& window, u32 image_count,
                vk::Format surface_format, const vk::AllocationCallbacks* allocator = nullptr);

void OnSurfaceFormatChange(vk::Format surface_format);

void Shutdown(const vk::Device& device);

void AcquireGamepadInputCapture();
void ReleaseGamepadInputCapture();
bool IsGamepadInputCaptured();

void NewFrame();

/// Draws the overlays into @image_view, over what is there; with nothing to draw, the image is
/// left as it is, with no pass.
void Render(const vk::CommandBuffer& cmdbuf, const vk::ImageView& image_view,
            const vk::Extent2D& extent);

bool MustKeepDrawing(); // Force the emulator redraw

} // namespace ImGui::Core
