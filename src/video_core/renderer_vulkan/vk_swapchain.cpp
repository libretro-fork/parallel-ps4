// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <limits>
#include "common/assert.h"
#include "common/logging/log.h"
#include "core/emulator_settings.h"
#include "imgui/renderer/imgui_core.h"
#include "libretro/libretro_bridge.h"
#include "sdl_window.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_swapchain.h"

namespace Vulkan {

static constexpr vk::SurfaceFormatKHR SURFACE_FORMAT_HDR = {
    .format = vk::Format::eA2B10G10R10UnormPack32,
    .colorSpace = vk::ColorSpaceKHR::eHdr10St2084EXT,
};

// The libretro core has no surface to present to: the "swapchain" is a few images of its own,
// taken in turn, and the presenter reads each finished one back for the frontend. B8G8R8A8 is
// the frontend's XRGB8888 byte for byte.
static constexpr u32 OFFSCREEN_IMAGE_COUNT = 3;

Swapchain::Swapchain(const Instance& instance_, const Frontend::WindowSDL& window_)
    : instance{instance_}, window{window_} {
    surface_format.format = vk::Format::eB8G8R8A8Unorm;
    surface_format.colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
    present_mode = vk::PresentModeKHR::eFifo;

    Create(window.GetWidth(), window.GetHeight());
    ImGui::Core::Initialize(instance, window, image_count, surface_format.format);
}

Swapchain::~Swapchain() {
    Destroy();
}

void Swapchain::Create(u32 width_, u32 height_) {
    width = width_;
    height = height_;
    needs_recreation = false;

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
    RefreshSemaphores();
    return;

    SetSurfaceProperties();

    const std::array queue_family_indices = {
        instance.GetGraphicsQueueFamilyIndex(),
        instance.GetPresentQueueFamilyIndex(),
    };

    const bool exclusive = queue_family_indices[0] == queue_family_indices[1];
    const u32 queue_family_indices_count = exclusive ? 1u : 2u;
    const vk::SharingMode sharing_mode =
        exclusive ? vk::SharingMode::eExclusive : vk::SharingMode::eConcurrent;
    const auto format = needs_hdr ? SURFACE_FORMAT_HDR : surface_format;
    const vk::SwapchainCreateInfoKHR swapchain_info = {
        .surface = surface,
        .minImageCount = image_count,
        .imageFormat = format.format,
        .imageColorSpace = format.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = vk::ImageUsageFlagBits::eColorAttachment |
                      vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst,
        .imageSharingMode = sharing_mode,
        .queueFamilyIndexCount = queue_family_indices_count,
        .pQueueFamilyIndices = queue_family_indices.data(),
        .preTransform = transform,
        .compositeAlpha = composite_alpha,
        .presentMode = present_mode,
        .clipped = true,
        .oldSwapchain = nullptr,
    };

    auto [swapchain_result, chain] = instance.GetDevice().createSwapchainKHR(swapchain_info);
    ASSERT_MSG(swapchain_result == vk::Result::eSuccess, "Failed to create swapchain: {}",
               vk::to_string(swapchain_result));
    swapchain = chain;

    SetupImages();
    RefreshSemaphores();
}

void Swapchain::Recreate(u32 width_, u32 height_) {
    LOG_DEBUG(Render_Vulkan, "Recreate the swapchain: width={} height={} HDR={}", width_, height_,
              needs_hdr);
    Create(width_, height_);
}

void Swapchain::SetHDR(bool hdr) {
    if (needs_hdr == hdr) {
        return;
    }

    auto result = instance.GetDevice().waitIdle();
    if (result != vk::Result::eSuccess) {
        LOG_WARNING(ImGui, "Failed to wait for Vulkan device idle on mode change: {}",
                    vk::to_string(result));
    }

    needs_hdr = hdr;
    Recreate(width, height);
    ImGui::Core::OnSurfaceFormatChange(needs_hdr ? SURFACE_FORMAT_HDR.format
                                                 : surface_format.format);
}

bool Swapchain::AcquireNextImage() {
    // An image the frontend is not showing, when it shows them itself.
    image_index = Libretro::GpuFramesActive() ? Libretro::AcquireImage()
                                              : (image_index + 1) % image_count;
    return true;
    vk::Device device = instance.GetDevice();
    vk::Result result =
        device.acquireNextImageKHR(swapchain, std::numeric_limits<u64>::max(),
                                   image_acquired[frame_index], VK_NULL_HANDLE, &image_index);

    switch (result) {
    case vk::Result::eSuccess:
        break;
    case vk::Result::eSuboptimalKHR:
    case vk::Result::eErrorSurfaceLostKHR:
    case vk::Result::eErrorOutOfDateKHR:
    case vk::Result::eErrorUnknown:
        needs_recreation = true;
        break;
    default:
        LOG_CRITICAL(Render_Vulkan, "Swapchain acquire returned unknown result {}",
                     vk::to_string(result));
        UNREACHABLE();
        break;
    }

    return !needs_recreation;
}

bool Swapchain::Present() {
    frame_index = (frame_index + 1) % image_count;
    return true;
    const vk::PresentInfoKHR present_info = {
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &present_ready[image_index],
        .swapchainCount = 1,
        .pSwapchains = &swapchain,
        .pImageIndices = &image_index,
    };

    auto result = instance.GetPresentQueue().presentKHR(present_info);
    if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR) {
        needs_recreation = true;
    } else {
        ASSERT_MSG(result == vk::Result::eSuccess, "Swapchain presentation failed: {}",
                   vk::to_string(result));
    }

    frame_index = (frame_index + 1) % image_count;

    return !needs_recreation;
}

void Swapchain::FindPresentFormat() {
    const auto [formats_result, formats] =
        instance.GetPhysicalDevice().getSurfaceFormatsKHR(surface);
    ASSERT_MSG(formats_result == vk::Result::eSuccess, "Failed to query surface formats: {}",
               vk::to_string(formats_result));

    // Check if the device supports HDR formats. Here we care of Rec.2020 PQ only as it is expected
    // game output. Other variants as e.g. linear Rec.2020 will require additional color space
    // rotation
    supports_hdr =
        std::find_if(formats.begin(), formats.end(), [](const vk::SurfaceFormatKHR& format) {
            return format == SURFACE_FORMAT_HDR;
        }) != formats.end();
    // Also make sure that user allowed us to use HDR
    supports_hdr &= EmulatorSettings.IsHdrAllowed();

    // If there is a single undefined surface format, the device doesn't care, so we'll just use
    // RGBA sRGB.
    if (formats[0].format == vk::Format::eUndefined) {
        surface_format.format = vk::Format::eR8G8B8A8Unorm;
        surface_format.colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
        return;
    }

    // Try to find a suitable format.
    for (const vk::SurfaceFormatKHR& sformat : formats) {
        vk::Format format = sformat.format;
        if (format != vk::Format::eR8G8B8A8Unorm && format != vk::Format::eB8G8R8A8Unorm) {
            continue;
        }

        surface_format.format = format;
        surface_format.colorSpace = sformat.colorSpace;
        return;
    }

    UNREACHABLE_MSG("Unable to find required swapchain format!");
}

void Swapchain::FindPresentMode() {
    const auto [modes_result, modes] =
        instance.GetPhysicalDevice().getSurfacePresentModesKHR(surface);
    if (modes_result != vk::Result::eSuccess) {
        LOG_ERROR(Render, "Failed to query available present modes, falling back to Fifo as "
                          "guaranteed supported option.");
        present_mode = vk::PresentModeKHR::eFifo;
        return;
    }

    const auto requested_mode = EmulatorSettings.GetPresentMode();
    if (requested_mode == "Mailbox") {
        present_mode = vk::PresentModeKHR::eMailbox;
    } else if (requested_mode == "Fifo") {
        present_mode = vk::PresentModeKHR::eFifo;
    } else if (requested_mode == "Immediate") {
        present_mode = vk::PresentModeKHR::eImmediate;
    } else {
        LOG_ERROR(Render_Vulkan, "Unknown present mode {}, defaulting to Mailbox.",
                  EmulatorSettings.GetPresentMode());
        present_mode = vk::PresentModeKHR::eMailbox;
    }

    if (std::ranges::find(modes, present_mode) == modes.cend()) {
        // FIFO is guaranteed to be supported by the Vulkan spec.
        constexpr auto fallback = vk::PresentModeKHR::eFifo;
        LOG_WARNING(Render, "Requested present mode {} is not supported, falling back to {}.",
                    vk::to_string(present_mode), vk::to_string(fallback));
        present_mode = fallback;
    }
}

void Swapchain::SetSurfaceProperties() {
    const auto [capabilities_result, capabilities] =
        instance.GetPhysicalDevice().getSurfaceCapabilitiesKHR(surface);
    ASSERT_MSG(capabilities_result == vk::Result::eSuccess,
               "Failed to query surface capabilities: {}", vk::to_string(capabilities_result));

    extent = capabilities.currentExtent;
    if (capabilities.currentExtent.width == std::numeric_limits<u32>::max()) {
        extent.width = std::max(capabilities.minImageExtent.width,
                                std::min(capabilities.maxImageExtent.width, width));
        extent.height = std::max(capabilities.minImageExtent.height,
                                 std::min(capabilities.maxImageExtent.height, height));
    }

    // Select number of images in swap chain, we prefer one buffer in the background to work on
    image_count = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0) {
        image_count = std::min(image_count, capabilities.maxImageCount);
    }

    // Prefer identity transform if possible
    transform = vk::SurfaceTransformFlagBitsKHR::eIdentity;
    if (!(capabilities.supportedTransforms & transform)) {
        transform = capabilities.currentTransform;
    }

    // Opaque is not supported everywhere.
    composite_alpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    if (!(capabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::eOpaque)) {
        composite_alpha = vk::CompositeAlphaFlagBitsKHR::eInherit;
    }
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

    if (swapchain) {
        device.destroySwapchainKHR(swapchain);
    }
    for (const auto& image : images) {
        device.destroyImage(image);
    }
    images.clear();
    for (const auto& memory : offscreen_memory) {
        device.freeMemory(memory);
    }
    offscreen_memory.clear();

    for (const auto& sem : image_acquired) {
        device.destroySemaphore(sem);
    }
    for (const auto& sem : present_ready) {
        device.destroySemaphore(sem);
    }

    image_acquired.clear();
    present_ready.clear();
}

void Swapchain::RefreshSemaphores() {
    const vk::Device device = instance.GetDevice();
    image_acquired.resize(image_count);
    present_ready.resize(image_count);

    for (vk::Semaphore& semaphore : image_acquired) {
        auto [semaphore_result, sem] = device.createSemaphore({});
        ASSERT_MSG(semaphore_result == vk::Result::eSuccess,
                   "Failed to create image acquired semaphore: {}",
                   vk::to_string(semaphore_result));
        semaphore = sem;
    }
    for (vk::Semaphore& semaphore : present_ready) {
        auto [semaphore_result, sem] = device.createSemaphore({});
        ASSERT_MSG(semaphore_result == vk::Result::eSuccess,
                   "Failed to create present ready semaphore: {}", vk::to_string(semaphore_result));
        semaphore = sem;
    }

    for (u32 i = 0; i < image_count; ++i) {
        SetObjectName(device, image_acquired[i], "Swapchain Semaphore: image_acquired {}", i);
        SetObjectName(device, present_ready[i], "Swapchain Semaphore: present_ready {}", i);
    }
}

bool Swapchain::CreateSharedImages() {
#if defined(__linux__) || defined(_WIN32)
    // Once only: the images' descriptors are sent to the frontend once, and the picture's size
    // does not change while a game runs.
    if (!Libretro::GpuFramesWanted() || Libretro::GpuFramesActive() ||
        !instance.IsExternalMemoryExportSupported()) {
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
            .format = needs_hdr ? SURFACE_FORMAT_HDR.format : surface_format.format,
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
