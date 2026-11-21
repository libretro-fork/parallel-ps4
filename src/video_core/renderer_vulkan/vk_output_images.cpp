// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/assert.h"
#include "common/logging/log.h"
#include "core/emulator_settings.h"
#include "libretro/libretro_bridge.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_output_images.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"

namespace Vulkan {

OutputImages::OutputImages(const Instance& instance_)
    : instance{instance_}, device{static_cast<VkDevice>(instance_.GetDevice())} {
    // Ten bits a channel, so a game that outputs ten (A2R10G10B10) reaches the frontend with
    // all of them: RetroArch draws from an image of any format. A frame read back for a
    // frontend that cannot share the images is narrowed to its XRGB8888 there, once
    // (vk_presenter.cpp). Eight bits where the device cannot draw to, blit from and sample
    // the ten-bit format, which Vulkan requires of every device.
    static constexpr vk::FormatFeatureFlags2 Needed =
        vk::FormatFeatureFlagBits2::eColorAttachment |
        vk::FormatFeatureFlagBits2::eColorAttachmentBlend |
        vk::FormatFeatureFlagBits2::eSampledImage | vk::FormatFeatureFlagBits2::eBlitSrc |
        vk::FormatFeatureFlagBits2::eTransferSrc | vk::FormatFeatureFlagBits2::eTransferDst;
    format = instance.IsFormatSupported(vk::Format::eA2B10G10R10UnormPack32, Needed)
                 ? VK_FORMAT_A2B10G10R10_UNORM_PACK32
                 : VK_FORMAT_B8G8R8A8_UNORM;
    // HDR10 is ten bits a channel, which Vulkan requires of every device; without them PQ
    // would not fit
    if (emu_settings_get_hdr_allowed() && format != VK_FORMAT_A2B10G10R10_UNORM_PACK32) {
        LOG_ERROR(Render_Vulkan, "HDR Output without ten-bit frames: the picture will be wrong");
    }
}

OutputImages::~OutputImages() {
    // The presenter has finished its schedulers before
    DestroySet(device, Take());
}

void OutputImages::DestroySet(VkDevice device, const Set& set) {
    for (u32 i = 0; i < set.count; ++i) {
        vkDestroyImageView(device, set.views[i], nullptr);
        vkDestroyImage(device, set.images[i], nullptr);
        vkFreeMemory(device, set.memory[i], nullptr);
    }
}

OutputImages::Set OutputImages::Take() {
    Set set{};
    set.count = count;
    for (u32 i = 0; i < count; ++i) {
        set.images[i] = images[i];
        set.views[i] = views[i];
        set.memory[i] = memory[i];
        images[i] = VK_NULL_HANDLE;
        views[i] = VK_NULL_HANDLE;
        memory[i] = VK_NULL_HANDLE;
    }
    count = 0;
    return set;
}

void OutputImages::Create(u32 width, u32 height, Scheduler& scheduler) {
    if (Libretro::GpuFramesActive() && count != 0) {
        return;
    }

    // The images so far, destroyed once the GPU has drawn the last frame into them: the
    // frames already submitted go on without waiting
    if (count != 0) {
        scheduler.DeferOperation(
            [device = device, set = Take()] { OutputImages::DestroySet(device, set); });
    }

    extent = VkExtent2D{width > 1 ? width : 1u, height > 1 ? height : 1u};
    index = 0;
    // Handed to the frontend on the GPU where that can be done; otherwise each finished frame
    // is read back, as before.
    count = Libretro::GpuFrameImages();
    ASSERT_MSG(count <= MaxCount, "The frontend shares more images (%u) than are kept", count);
    // With Resolution: Default they are shared once the game's size is known
    if (Libretro::GameSizePending() || !CreateSharedImages()) {
        count = OffscreenCount;
        CreateOffscreenImages();
    }
    CreateViews();
}

void OutputImages::Acquire() {
    // An image the frontend is not showing, when it shows them itself.
    index = Libretro::GpuFramesActive() ? Libretro::AcquireImage(generation) : (index + 1) % count;
}

bool OutputImages::CreateImage(u32 i, const VkImageCreateInfo& image_ci, const void* allocate_next,
                               VkDeviceSize* out_size) {
    VkResult result = vkCreateImage(device, &image_ci, nullptr, &images[i]);
    if (result != VK_SUCCESS) {
        LOG_WARNING(Render_Vulkan, "No image for the frames: %s", ResultName(result));
        return false;
    }
    VkMemoryRequirements requirements;
    vkGetImageMemoryRequirements(device, images[i], &requirements);
    if (memory_type == ~0u) {
        VkPhysicalDeviceMemoryProperties memory_properties;
        vkGetPhysicalDeviceMemoryProperties(
            static_cast<VkPhysicalDevice>(instance.GetPhysicalDevice()), &memory_properties);
        for (u32 type = 0; type < memory_properties.memoryTypeCount; ++type) {
            if ((requirements.memoryTypeBits & (1u << type)) != 0 &&
                (memory_properties.memoryTypes[type].propertyFlags &
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0) {
                memory_type = type;
                break;
            }
        }
    }
    if (memory_type == ~0u || (requirements.memoryTypeBits & (1u << memory_type)) == 0) {
        LOG_WARNING(Render_Vulkan, "No device memory type for the frames' images");
        return false;
    }
    const VkMemoryAllocateInfo allocate_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = allocate_next,
        .allocationSize = requirements.size,
        .memoryTypeIndex = memory_type,
    };
    result = vkAllocateMemory(device, &allocate_info, nullptr, &memory[i]);
    if (result != VK_SUCCESS) {
        LOG_WARNING(Render_Vulkan, "No memory for the frames' images: %s", ResultName(result));
        return false;
    }
    result = vkBindImageMemory(device, images[i], memory[i], 0);
    if (result != VK_SUCCESS) {
        LOG_WARNING(Render_Vulkan, "The frames' image memory could not be bound: %s",
                    ResultName(result));
        return false;
    }
    *out_size = requirements.size;
    return true;
}

bool OutputImages::CreateSharedImages() {
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
    const VkPhysicalDevice physical_device =
        static_cast<VkPhysicalDevice>(instance.GetPhysicalDevice());
#if defined(_WIN32)
    const VkExternalMemoryHandleTypeFlagBits handle_type =
        VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
#else
    const VkExternalMemoryHandleTypeFlagBits handle_type =
        VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
#endif
    // The frontend makes the same images on this memory (libretro/gpu_frames.cpp, Import):
    // what is set here is what it is told.
    const VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                    VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    {
        // Images of this format can be exported: else they are read back, as when sharing fails
        const VkPhysicalDeviceExternalImageFormatInfo external_format_info = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO,
            .handleType = handle_type,
        };
        const VkPhysicalDeviceImageFormatInfo2 format_info = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
            .pNext = &external_format_info,
            .format = format,
            .type = VK_IMAGE_TYPE_2D,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = usage,
        };
        VkExternalImageFormatProperties external_props = {
            .sType = VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES,
        };
        VkImageFormatProperties2 props = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2,
            .pNext = &external_props,
        };
        const VkResult result =
            vkGetPhysicalDeviceImageFormatProperties2(physical_device, &format_info, &props);
        if (result != VK_SUCCESS ||
            !(external_props.externalMemoryProperties.externalMemoryFeatures &
              VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT)) {
            Libretro::GpuFramesFailed("the emulator's images cannot be exported in their format");
            return false;
        }
    }

    u64 handles[MaxCount]{};
    u64 sizes[MaxCount]{};
    u32 num_handles = 0;
    bool made = true;
    for (u32 i = 0; i < count && made; ++i) {
        const VkExternalMemoryImageCreateInfo external_info = {
            .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
            .handleTypes = static_cast<VkExternalMemoryHandleTypeFlags>(handle_type),
        };
        const VkImageCreateInfo image_ci = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = &external_info,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = format,
            .extent = {extent.width, extent.height, 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = usage,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        };
        const VkExportMemoryAllocateInfo export_info = {
            .sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO,
            .handleTypes = static_cast<VkExternalMemoryHandleTypeFlags>(handle_type),
        };
        VkDeviceSize size = 0;
        made = CreateImage(i, image_ci, &export_info, &size);
        if (made) {
            made = Libretro::ExportImageMemory(device, reinterpret_cast<u64>(memory[i]),
                                               reinterpret_cast<void*>(vkGetDeviceProcAddr),
                                               handles[num_handles]);
            if (made) {
                sizes[num_handles++] = size;
            } else {
                LOG_WARNING(Render_Vulkan, "The frontend's images could not be exported");
            }
        }
    }

    if (made) {
        VkPhysicalDeviceIDProperties ids = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES,
        };
        VkPhysicalDeviceProperties2 properties = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
            .pNext = &ids,
        };
        vkGetPhysicalDeviceProperties2(physical_device, &properties);
        made = Libretro::ShareImages(handles, count, extent.width, extent.height,
                                     static_cast<u32>(format), static_cast<u32>(usage), memory_type,
                                     sizes, ids.deviceUUID, ids.driverUUID);
    }
    if (!made) {
        Libretro::GpuFramesFailed("the images for the frontend could not be made or sent");
        for (u32 i = 0; i < num_handles; ++i) {
            Libretro::CloseImageMemory(handles[i]);
        }
        // Made just now: the GPU has never seen them
        DestroySet(device, Take());
        memory_type = ~0u;
        return false;
    }
#if !defined(_WIN32)
    // Sent: the frontend's process has its own copies of them now. On Windows the core
    // duplicates them from this process later, so they stay open.
    for (u32 i = 0; i < num_handles; ++i) {
        Libretro::CloseImageMemory(handles[i]);
    }
#endif
    return true;
#else
    return false;
#endif
}

void OutputImages::CreateOffscreenImages() {
    const VkImageCreateInfo image_ci = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = {extent.width, extent.height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                 VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    for (u32 i = 0; i < count; ++i) {
        VkDeviceSize size = 0;
        const bool made = CreateImage(i, image_ci, nullptr, &size);
        ASSERT_MSG(made, "No offscreen image for the frames");
    }
}

void OutputImages::CreateViews() {
    const vk::Device vk_device{device};
    for (u32 i = 0; i < count; ++i) {
        const VkImageViewCreateInfo view_ci = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = format,
            .subresourceRange =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .levelCount = 1,
                    .layerCount = 1,
                },
        };
        const VkResult result = vkCreateImageView(device, &view_ci, nullptr, &views[i]);
        ASSERT_MSG(result == VK_SUCCESS, "Failed to create image view: %s", ResultName(result));
        SetObjectName(vk_device, vk::Image{images[i]}, "Output Image %u", i);
        SetObjectName(vk_device, vk::ImageView{views[i]}, "Output ImageView %u", i);
    }
}

} // namespace Vulkan
