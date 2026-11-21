// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/logging/log.h"
#include "shader_recompiler/resource.h"
#include "video_core/amdgpu/pixel_format.h"
#include "video_core/renderer_vulkan/liverpool_to_vk.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/texture_cache/image.h"
#include "video_core/texture_cache/image_view.h"
#include "vulkan/vulkan.hpp"

#include "common/enum_names.h"

namespace VideoCore {

VkImageViewType ConvertImageViewType(AmdGpu::ImageType type) {
    switch (type) {
    case AmdGpu::ImageType::Color1D:
    case AmdGpu::ImageType::Color2D:
    case AmdGpu::ImageType::Color2DMsaa:
        return VK_IMAGE_VIEW_TYPE_2D;
    case AmdGpu::ImageType::Color1DArray:
    case AmdGpu::ImageType::Color2DArray:
    case AmdGpu::ImageType::Cube:
        return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    case AmdGpu::ImageType::Color3D:
        return VK_IMAGE_VIEW_TYPE_3D;
    default:
        UNREACHABLE();
    }
}

bool IsViewTypeCompatible(AmdGpu::ImageType view_type, AmdGpu::ImageType image_type) {
    switch (view_type) {
    case AmdGpu::ImageType::Color1D:
    case AmdGpu::ImageType::Color1DArray:
    case AmdGpu::ImageType::Color2D:
    case AmdGpu::ImageType::Color2DArray:
    case AmdGpu::ImageType::Color2DMsaa:
    case AmdGpu::ImageType::Color2DMsaaArray:
    case AmdGpu::ImageType::Cube:
        return image_type == AmdGpu::ImageType::Color1D ||
               image_type == AmdGpu::ImageType::Color2D || image_type == AmdGpu::ImageType::Color3D;
    case AmdGpu::ImageType::Color3D:
        return image_type == AmdGpu::ImageType::Color3D;
    default:
        UNREACHABLE();
    }
}

ImageViewInfo::ImageViewInfo(const AmdGpu::Image& image, const Shader::ImageResource& desc) noexcept
    : is_storage{desc.is_written} {
    const AmdGpu::DataFormat dfmt = image.GetDataFmt();
    AmdGpu::NumberFormat nfmt = image.GetNumberFmt();
    if (is_storage && nfmt == AmdGpu::NumberFormat::Srgb) {
        nfmt = AmdGpu::NumberFormat::Unorm;
    }
    format = Vulkan::LiverpoolToVK::SurfaceFormat(dfmt, nfmt);
    if (desc.is_depth) {
        format = Vulkan::LiverpoolToVK::PromoteFormatToDepth(format);
    }

    range.base.level = image.base_level;
    const u32 base_array = static_cast<u32>(image.base_array);
    const u32 last_layer = static_cast<u32>(image.NumLayers() - 1);
    range.base.layer = base_array < last_layer ? base_array : last_layer;
    range.extent.levels = image.NumViewLevels(desc.is_array);
    range.extent.layers = image.NumViewLayers(desc.is_array);
    type = image.GetViewType(desc.is_array);
    min_lod = static_cast<u32>(image.min_lod);
    if (!is_storage) {
        mapping = image.DstSelect();
    }
}

ImageViewInfo::ImageViewInfo(const AmdGpu::ColorBuffer& col_buffer) noexcept {
    range.base.layer = col_buffer.BaseSlice();
    range.extent.layers = col_buffer.NumSlices() - range.base.layer;
    type = range.extent.layers > 1 ? AmdGpu::ImageType::Color2DArray : AmdGpu::ImageType::Color2D;
    format =
        Vulkan::LiverpoolToVK::SurfaceFormat(col_buffer.GetDataFmt(), col_buffer.GetNumberFmt());
}

ImageViewInfo::ImageViewInfo(const AmdGpu::DepthBuffer& depth_buffer, AmdGpu::DepthView view,
                             AmdGpu::DepthControl ctl) {
    format = Vulkan::LiverpoolToVK::DepthFormat(depth_buffer.z_info.format,
                                                depth_buffer.stencil_info.format);
    is_storage = ctl.depth_write_enable;
    range.base.layer = view.slice_start;
    range.extent.layers = view.NumSlices() - range.base.layer;
    type = range.extent.layers > 1 ? AmdGpu::ImageType::Color2DArray : AmdGpu::ImageType::Color2D;
}

ImageView::ImageView(const Vulkan::Instance& instance, const ImageViewInfo& info_,
                     const Image& image)
    : info{info_} {
    VkImageViewUsageCreateInfo usage_ci{};
    usage_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO;
    usage_ci.usage = static_cast<VkImageUsageFlags>(image.usage_flags);
    if (!info.is_storage) {
        usage_ci.usage &= ~VK_IMAGE_USAGE_STORAGE_BIT;
    }
    VkImageViewMinLodCreateInfoEXT min_lod_ci{};
    min_lod_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_MIN_LOD_CREATE_INFO_EXT;
    if (info.min_lod != 0 && instance.IsImageViewMinLodSupported()) {
        const float last_level =
            static_cast<float>(info.range.base.level + info.range.extent.levels - 1);
        const float min_lod = static_cast<float>(info.min_lod) / 256.f;
        min_lod_ci.minLod = min_lod < last_level ? min_lod : last_level;
        usage_ci.pNext = &min_lod_ci;
    }
    // When sampling D32/D16 texture from shader, the T# specifies R32/R16 format so adjust it.
    vk::Format format = info.format;
    const VkImageAspectFlags image_aspect = static_cast<VkImageAspectFlags>(image.aspect_mask);
    VkImageAspectFlags aspect = image_aspect;
    if ((image_aspect & VK_IMAGE_ASPECT_DEPTH_BIT) &&
        Vulkan::LiverpoolToVK::IsFormatDepthCompatible(format)) {
        format = image.info.pixel_format;
        aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    }
    if ((image_aspect & VK_IMAGE_ASPECT_STENCIL_BIT) &&
        Vulkan::LiverpoolToVK::IsFormatStencilCompatible(format)) {
        format = image.info.pixel_format;
        aspect = VK_IMAGE_ASPECT_STENCIL_BIT;
    }

    const vk::ComponentMapping components =
        info.mapping == AmdGpu::IdentityMapping
            ? vk::ComponentMapping{}
            : Vulkan::LiverpoolToVK::ComponentMapping(info.mapping);

    VkImageViewCreateInfo image_view_ci{};
    image_view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_ci.pNext = &usage_ci;
    image_view_ci.image = static_cast<VkImage>(image.GetImage());
    image_view_ci.viewType = ConvertImageViewType(info.type);
    image_view_ci.format = static_cast<VkFormat>(
        instance.GetSupportedFormat(format, vk::FormatFeatureFlags2{image.format_features}));
    image_view_ci.components = static_cast<VkComponentMapping>(components);
    image_view_ci.subresourceRange.aspectMask = aspect;
    image_view_ci.subresourceRange.baseMipLevel = info.range.base.level;
    image_view_ci.subresourceRange.levelCount = info.range.extent.levels;
    image_view_ci.subresourceRange.baseArrayLayer = info.range.base.layer;
    image_view_ci.subresourceRange.layerCount = info.range.extent.layers;
    if (!IsViewTypeCompatible(info.type, image.info.type)) {
        LOG_ERROR(Render_Vulkan, "image view type %s is incompatible with image type %s",
                  enum_name(&enum_names_AmdGpu_ImageType, static_cast<s64>(info.type)),
                  enum_name(&enum_names_AmdGpu_ImageType, static_cast<s64>(image.info.type)));
    }

    device = static_cast<VkDevice>(instance.GetDevice());
    const VkResult view_result = vkCreateImageView(device, &image_view_ci, nullptr, &image_view);
    ASSERT_MSG(view_result == VK_SUCCESS, "Failed to create image view: %s",
               Vulkan::ResultName(view_result));

    const char* const view_aspect = (aspect & VK_IMAGE_ASPECT_DEPTH_BIT)     ? "Depth"
                                    : (aspect & VK_IMAGE_ASPECT_STENCIL_BIT) ? "Stencil"
                                                                             : "Color";
    Vulkan::SetObjectName(
        instance.GetDevice(), Handle(), "ImageView %ux%ux%u 0x%" PRIxPTR ":0x%x %d:%d %d:%d (%s)",
        image.info.size.width, image.info.size.height, image.info.size.depth,
        image.info.guest_address, image.info.guest_size, info.range.base.level,
        info.range.base.level + info.range.extent.levels - 1, info.range.base.layer,
        info.range.base.layer + info.range.extent.layers - 1, view_aspect);
}

ImageView::~ImageView() {
    if (image_view != VK_NULL_HANDLE) {
        vkDestroyImageView(device, image_view, nullptr);
    }
}

ImageView::ImageView(ImageView&& other) noexcept
    : info{other.info}, device{other.device}, image_view{other.image_view} {
    other.image_view = VK_NULL_HANDLE;
}

ImageView& ImageView::operator=(ImageView&& other) noexcept {
    if (this != &other) {
        if (image_view != VK_NULL_HANDLE) {
            vkDestroyImageView(device, image_view, nullptr);
        }
        info = other.info;
        device = other.device;
        image_view = other.image_view;
        other.image_view = VK_NULL_HANDLE;
    }
    return *this;
}

} // namespace VideoCore
