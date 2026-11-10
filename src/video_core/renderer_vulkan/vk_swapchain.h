// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <vector>
#include "common/types.h"
#include "video_core/renderer_vulkan/vk_common.h"

namespace Frontend {
class Window;
}

namespace Vulkan {

class Instance;
class Scheduler;

class Swapchain {
public:
    explicit Swapchain(const Instance& instance, const Frontend::Window& window);
    ~Swapchain();

    /// Creates (or recreates) the swapchain with a given size.
    void Create(u32 width, u32 height);

    /// Recreates the images with a given size.
    void Recreate(u32 width, u32 height);

    /// Takes the next image to draw the frame into.
    void AcquireNextImage();

    /// Moves on to the next frame once the current one is submitted.
    void Present();

    vk::Image Image() const {
        return images[image_index];
    }

    vk::ImageView ImageView() const {
        return images_view[image_index];
    }

    vk::SurfaceFormatKHR GetSurfaceFormat() const {
        return surface_format;
    }

    u32 GetWidth() const {
        return width;
    }

    u32 GetHeight() const {
        return height;
    }

    u32 GetImageCount() const {
        return image_count;
    }

    u32 GetFrameIndex() const {
        return frame_index;
    }

    /// Which of the images that stand in for a swapchain's the frame is drawn into
    u32 GetImageIndex() const {
        return image_index;
    }

    vk::Extent2D GetExtent() const {
        return extent;
    }

private:
    /// Destroys current swapchain resources
    void Destroy();

    /// Performs creation of image views and framebuffers from the swapchain images
    void SetupImages();

    /// Creates the images that stand in for a swapchain's
    void CreateOffscreenImages();

    /// Makes them of exportable memory and sends them to the frontend, when the picture is to
    /// be handed over on the GPU. False when that cannot be done: the images are then to be
    /// made the plain way
    bool CreateSharedImages();

private:
    const Instance& instance;
    const Frontend::Window& window;
    vk::SurfaceFormatKHR surface_format;
    vk::Extent2D extent;
    std::vector<vk::Image> images;
    std::vector<vk::DeviceMemory> offscreen_memory;
    std::vector<vk::ImageView> images_view;
    u32 width = 0;
    u32 height = 0;
    u32 image_count = 0;
    u32 image_index = 0;
    u32 frame_index = 0;
};

} // namespace Vulkan
