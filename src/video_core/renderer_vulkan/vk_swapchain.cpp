// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <limits>
#include "common/assert.h"
#include "common/logging/log.h"
#include "frontend_window.h"
#include "imgui/renderer/imgui_core.h"
#include "libretro/libretro_bridge.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_swapchain.h"

namespace Vulkan {

// The libretro core has no surface to present to: the "swapchain" is a few images of its own,
// taken in turn, and the presenter reads each finished one back for the frontend. B8G8R8A8 is
// the frontend's XRGB8888 byte for byte.
static constexpr u32 OFFSCREEN_IMAGE_COUNT = 3;

Swapchain::Swapchain(const Instance& instance_, const Frontend::Window& window_)
    : instance{instance_}, window{window_} {
    surface_format.format = vk::Format::eB8G8R8A8Unorm;
    surface_format.colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;

    Create(window.GetWidth(), window.GetHeight());
    ImGui::Core::Initialize(instance, window, image_count, surface_format.format);
}

Swapchain::~Swapchain() {
    Destroy();
}

void Swapchain::Create(u32 width_, u32 height_) {
    width = width_;
    height = height_;

    // Images the frontend has are kept: it was sent them once, and draws from them.
    if (Libretro::GpuFramesActive() && !images.empty()) {
        return;
    }

    Destroy();

    extent = vk::Extent2D{std::max(width, 1u), std::max(height, 1u)};
    image_index = 0;
    frame_index = 0;
    // Handed to the frontend on the GPU where that can be done; otherwise each finished frame
    // is read back, as before.
    image_count = Libretro::GpuFrameImages();
    // With Resolution: Default they are shared once the game's size is known
    if (Libretro::GameSizePending() || !CreateSharedImages()) {
        image_count = OFFSCREEN_IMAGE_COUNT;
        CreateOffscreenImages();
    }
    SetupImages();
}

void Swapchain::Recreate(u32 width_, u32 height_) {
    LOG_DEBUG(Render_Vulkan, "Recreate the swapchain: width={} height={}", width_, height_);
    Create(width_, height_);
}

void Swapchain::AcquireNextImage() {
    // An image the frontend is not showing, when it shows them itself.
    image_index =
        Libretro::GpuFramesActive() ? Libretro::AcquireImage() : (image_index + 1) % image_count;
}

void Swapchain::Present() {
    frame_index = (frame_index + 1) % image_count;
}

void Swapchain::Destroy() {
    vk::Device device = instance.GetDevice();
    const auto wait_result = device.waitIdle();
    if (wait_result != vk::Result::eSuccess) {
        LOG_WARNING(Render_Vulkan, "Failed to wait for device to become idle: {}",
                    vk::to_string(wait_result));
    }

    for (auto& image_view : images_view) {
        device.destroyImageView(image_view);
    }
    images_view.clear();

    for (const auto& image : images) {
        device.destroyImage(image);
    }
    images.clear();
    for (const auto& memory : offscreen_memory) {
        device.freeMemory(memory);
    }
    offscreen_memory.clear();
}

bool Swapchain::CreateSharedImages() {
#if defined(__linux__) || defined(_WIN32)
    // Once only: the images' descriptors are sent to the frontend once, and the picture's size
    // does not change while a game runs.
    if (!Libretro::GpuFramesWanted() || Libretro::GpuFramesActive()) {
        return false;
    }
    if (!instance.IsExternalMemoryExportSupported()) {
        Libretro::GpuFramesFailed("the emulator's Vulkan device cannot export memory");
        return false;
    }
    const vk::Device device = instance.GetDevice();
    const auto memory_properties = instance.GetPhysicalDevice().getMemoryProperties();
#if defined(_WIN32)
    const auto handle_type = vk::ExternalMemoryHandleTypeFlagBits::eOpaqueWin32;
#else
    const auto handle_type = vk::ExternalMemoryHandleTypeFlagBits::eOpaqueFd;
#endif
    // The frontend makes the same images on this memory (libretro/gpu_frames.cpp, Import):
    // what is set here is what it is told.
    const vk::ImageUsageFlags usage =
        vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc |
        vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;

    std::vector<u64> handles;
    std::vector<u64> sizes;
    u32 memory_type = std::numeric_limits<u32>::max();
    const auto give_up = [&] {
        Libretro::GpuFramesFailed("the images for the frontend could not be made or sent");
        for (const u64 handle : handles) {
            Libretro::CloseImageMemory(handle);
        }
        for (const auto& image : images) {
            device.destroyImage(image);
        }
        images.clear();
        for (const auto& memory : offscreen_memory) {
            device.freeMemory(memory);
        }
        offscreen_memory.clear();
        return false;
    };

    for (u32 i = 0; i < image_count; ++i) {
        const vk::ExternalMemoryImageCreateInfo external_info = {
            .handleTypes = handle_type,
        };
        const vk::ImageCreateInfo image_info = {
            .pNext = &external_info,
            .imageType = vk::ImageType::e2D,
            .format = surface_format.format,
            .extent = {extent.width, extent.height, 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = vk::SampleCountFlagBits::e1,
            .tiling = vk::ImageTiling::eOptimal,
            .usage = usage,
            .sharingMode = vk::SharingMode::eExclusive,
            .initialLayout = vk::ImageLayout::eUndefined,
        };
        auto [image_result, image] = device.createImage(image_info);
        if (image_result != vk::Result::eSuccess) {
            LOG_WARNING(Render_Vulkan, "No exportable image for the frontend: {}",
                        vk::to_string(image_result));
            return give_up();
        }
        images.push_back(image);

        const auto requirements = device.getImageMemoryRequirements(image);
        if (memory_type == std::numeric_limits<u32>::max()) {
            for (u32 type = 0; type < memory_properties.memoryTypeCount; ++type) {
                const bool allowed = (requirements.memoryTypeBits & (1u << type)) != 0;
                const bool device_local =
                    static_cast<bool>(memory_properties.memoryTypes[type].propertyFlags &
                                      vk::MemoryPropertyFlagBits::eDeviceLocal);
                if (allowed && device_local) {
                    memory_type = type;
                    break;
                }
            }
        }
        if (memory_type == std::numeric_limits<u32>::max() ||
            (requirements.memoryTypeBits & (1u << memory_type)) == 0) {
            LOG_WARNING(Render_Vulkan, "No device memory type for the frontend's images");
            return give_up();
        }

        const vk::ExportMemoryAllocateInfo export_info = {
            .handleTypes = handle_type,
        };
        const vk::MemoryAllocateInfo allocate_info = {
            .pNext = &export_info,
            .allocationSize = requirements.size,
            .memoryTypeIndex = memory_type,
        };
        auto [memory_result, memory] = device.allocateMemory(allocate_info);
        if (memory_result != vk::Result::eSuccess) {
            LOG_WARNING(Render_Vulkan, "No exportable memory for the frontend's images: {}",
                        vk::to_string(memory_result));
            return give_up();
        }
        offscreen_memory.push_back(memory);
        if (device.bindImageMemory(image, memory, 0) != vk::Result::eSuccess) {
            return give_up();
        }

        u64 handle = 0;
        if (!Libretro::ExportImageMemory(
                static_cast<VkDevice>(device),
                reinterpret_cast<u64>(static_cast<VkDeviceMemory>(memory)),
                reinterpret_cast<void*>(VULKAN_HPP_DEFAULT_DISPATCHER.vkGetDeviceProcAddr),
                handle)) {
            LOG_WARNING(Render_Vulkan, "The frontend's images could not be exported");
            return give_up();
        }
        handles.push_back(handle);
        sizes.push_back(requirements.size);
    }

    const auto ids = instance.GetPhysicalDevice()
                         .getProperties2<vk::PhysicalDeviceProperties2,
                                         vk::PhysicalDeviceIDProperties>()
                         .get<vk::PhysicalDeviceIDProperties>();
    if (!Libretro::ShareImages(handles.data(), image_count, extent.width, extent.height,
                               static_cast<u32>(surface_format.format),
                               static_cast<u32>(static_cast<VkImageUsageFlags>(usage)),
                               memory_type, sizes.data(), ids.deviceUUID.data(),
                               ids.driverUUID.data())) {
        return give_up();
    }
#if !defined(_WIN32)
    // Sent: the frontend's process has its own copies of them now. On Windows the core
    // duplicates them from this process later, so they stay open.
    for (const u64 handle : handles) {
        Libretro::CloseImageMemory(handle);
    }
#endif
    return true;
#else
    return false;
#endif
}

void Swapchain::CreateOffscreenImages() {
    const vk::Device device = instance.GetDevice();
    const auto memory_properties = instance.GetPhysicalDevice().getMemoryProperties();

    images.resize(image_count);
    offscreen_memory.resize(image_count);
    for (u32 i = 0; i < image_count; ++i) {
        const vk::ImageCreateInfo image_info = {
            .imageType = vk::ImageType::e2D,
            .format = surface_format.format,
            .extent = {extent.width, extent.height, 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = vk::SampleCountFlagBits::e1,
            .tiling = vk::ImageTiling::eOptimal,
            .usage = vk::ImageUsageFlagBits::eColorAttachment |
                     vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst,
            .sharingMode = vk::SharingMode::eExclusive,
            .initialLayout = vk::ImageLayout::eUndefined,
        };
        auto [image_result, image] = device.createImage(image_info);
        ASSERT_MSG(image_result == vk::Result::eSuccess, "Failed to create offscreen image: {}",
                   vk::to_string(image_result));
        images[i] = image;

        const auto requirements = device.getImageMemoryRequirements(image);
        u32 memory_type = std::numeric_limits<u32>::max();
        for (u32 type = 0; type < memory_properties.memoryTypeCount; ++type) {
            const bool allowed = (requirements.memoryTypeBits & (1u << type)) != 0;
            const bool device_local =
                static_cast<bool>(memory_properties.memoryTypes[type].propertyFlags &
                                  vk::MemoryPropertyFlagBits::eDeviceLocal);
            if (allowed && device_local) {
                memory_type = type;
                break;
            }
        }
        ASSERT_MSG(memory_type != std::numeric_limits<u32>::max(),
                   "No device memory type for the offscreen image");

        const vk::MemoryAllocateInfo allocate_info = {
            .allocationSize = requirements.size,
            .memoryTypeIndex = memory_type,
        };
        auto [memory_result, memory] = device.allocateMemory(allocate_info);
        ASSERT_MSG(memory_result == vk::Result::eSuccess,
                   "Failed to allocate offscreen image memory: {}", vk::to_string(memory_result));
        offscreen_memory[i] = memory;

        const auto bind_result = device.bindImageMemory(image, memory, 0);
        ASSERT_MSG(bind_result == vk::Result::eSuccess,
                   "Failed to bind offscreen image memory: {}", vk::to_string(bind_result));
    }
}

void Swapchain::SetupImages() {
    vk::Device device = instance.GetDevice();
    images_view.resize(image_count);
    for (u32 i = 0; i < image_count; ++i) {
        if (images_view[i]) {
            device.destroyImageView(images_view[i]);
        }
        auto [im_view_result, im_view] = device.createImageView(vk::ImageViewCreateInfo{
            .image = images[i],
            .viewType = vk::ImageViewType::e2D,
            .format = surface_format.format,
            .subresourceRange =
                {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .levelCount = 1,
                    .layerCount = 1,
                },
        });
        ASSERT_MSG(im_view_result == vk::Result::eSuccess, "Failed to create image view: {}",
                   vk::to_string(im_view_result));
        images_view[i] = im_view;
    }

    for (u32 i = 0; i < image_count; ++i) {
        SetObjectName(device, images[i], "Swapchain Image {}", i);
        SetObjectName(device, images_view[i], "Swapchain ImageView {}", i);
    }
}

} // namespace Vulkan
