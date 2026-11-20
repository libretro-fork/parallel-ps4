// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <memory>
#include "common/assert.h"
#include "video_core/renderer_vulkan/liverpool_to_vk.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_memory.h"
#include "video_core/renderer_vulkan/vk_runtime.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/texture_cache/blit_helper.h"
#include "video_core/texture_cache/image.h"

namespace VideoCore {

using namespace Vulkan;

Common::IncrementalIdProvider<u64> Image::global_image_uid{};

static vk::ImageUsageFlags ImageUsageFlags(const Vulkan::Instance& instance,
                                           const ImageInfo& info) {
    vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eTransferSrc |
                                vk::ImageUsageFlagBits::eTransferDst |
                                vk::ImageUsageFlagBits::eSampled;
    if (!info.props.is_block) {
        if (info.props.is_depth) {
            usage |= vk::ImageUsageFlagBits::eDepthStencilAttachment;
        } else {
            usage |= vk::ImageUsageFlagBits::eColorAttachment;
            if (instance.IsAttachmentFeedbackLoopLayoutSupported()) {
                usage |= vk::ImageUsageFlagBits::eAttachmentFeedbackLoopEXT;
            }
            // Always create images with storage flag to avoid needing re-creation in case of e.g
            // compute clears This sacrifices a bit of performance but is less work. ExtendedUsage
            // flag is also used. The exception here is for multisample images when storage is not
            // supported, where even with ExtendedUsage we may get only one supported sample back.
            if (info.num_samples == 1 || instance.IsMultisampleStorageImageSupported())
                usage |= vk::ImageUsageFlagBits::eStorage;
        }
    } else {
        // Similarly to above, we specify storage usage. This is typically not supported by
        // compressed formats, but may be used for uncompressed views. In order to satisfy this,
        // we will also specify the extended usage bit.
        usage |= vk::ImageUsageFlagBits::eStorage;
    }

    return usage;
}

static vk::ImageType ConvertImageType(AmdGpu::ImageType type) noexcept {
    switch (type) {
    case AmdGpu::ImageType::Color1D:
    case AmdGpu::ImageType::Color1DArray:
    case AmdGpu::ImageType::Color2D:
    case AmdGpu::ImageType::Color2DMsaa:
    case AmdGpu::ImageType::Color2DArray:
    case AmdGpu::ImageType::Cube:
        return vk::ImageType::e2D;
    case AmdGpu::ImageType::Color3D:
        return vk::ImageType::e3D;
    default:
        UNREACHABLE();
    }
}

static vk::FormatFeatureFlags2 FormatFeatureFlags(const vk::ImageUsageFlags usage_flags) {
    vk::FormatFeatureFlags2 feature_flags{};
    if (usage_flags & vk::ImageUsageFlagBits::eTransferSrc) {
        feature_flags |= vk::FormatFeatureFlagBits2::eTransferSrc;
    }
    if (usage_flags & vk::ImageUsageFlagBits::eTransferDst) {
        feature_flags |= vk::FormatFeatureFlagBits2::eTransferDst;
    }
    if (usage_flags & vk::ImageUsageFlagBits::eSampled) {
        feature_flags |= vk::FormatFeatureFlagBits2::eSampledImage;
    }
    if (usage_flags & vk::ImageUsageFlagBits::eColorAttachment) {
        feature_flags |= vk::FormatFeatureFlagBits2::eColorAttachment;
    }
    if (usage_flags & vk::ImageUsageFlagBits::eDepthStencilAttachment) {
        feature_flags |= vk::FormatFeatureFlagBits2::eDepthStencilAttachment;
    }
    // Note: StorageImage is intentionally ignored for now since it is always set, and can mess up
    // compatibility checks.
    return feature_flags;
}

UniqueImage::~UniqueImage() {
    if (image) {
        allocator->DestroyImage(image, allocation);
    }
}

void UniqueImage::Destroy() {
    if (image) {
        allocator->DestroyImage(image, allocation);
        image = vk::Image{};
        allocation = {};
    }
}

void UniqueImage::Create(const vk::ImageCreateInfo& image_ci) {
    this->image_ci = image_ci;
    ASSERT(!image);
    vk::Result result = allocator->CreateImage(image_ci, true, image, allocation);
    if (result == vk::Result::eErrorOutOfDeviceMemory) {
        LOG_WARNING(Render_Vulkan, "Allocating image of %ux%ux%u over budget.",
                    image_ci.extent.width, image_ci.extent.height, image_ci.extent.depth);
        result = allocator->CreateImage(image_ci, false, image, allocation);
    }
    ASSERT_MSG(result == vk::Result::eSuccess, "Failed allocating image with error %s",
               (vk::to_string(result)).c_str());
    size_bytes = allocation->size;
}

Image::Image(const Vulkan::Instance& instance, Vulkan::Runtime& runtime_,
             Common::SlotVector<ImageView>& slot_image_views_, const ImageInfo& info_,
             u32 scale_num_, u32 scale_den_)
    : runtime{&runtime_}, slot_image_views{&slot_image_views_}, info{info_},
      scale_num{scale_num_}, scale_den{scale_den_} {
    if (info.pixel_format == vk::Format::eUndefined) {
        return;
    }

    image_uid = global_image_uid.Next();
    vk::ImageCreateFlags flags{vk::ImageCreateFlagBits::eMutableFormat |
                               vk::ImageCreateFlagBits::eExtendedUsage};
    if (info.props.is_volume) {
        flags |= vk::ImageCreateFlagBits::e2DArrayCompatible;
        if (instance.Is2dViewOf3dSupported()) {
            flags |= vk::ImageCreateFlagBits::e2DViewCompatibleEXT;
        }
    }
    if (info.props.is_block && instance.IsBlockTexelViewSupported()) {
        flags |= vk::ImageCreateFlagBits::eBlockTexelViewCompatible;
    }

    usage_flags = ImageUsageFlags(instance, info);
    format_features = FormatFeatureFlags(usage_flags);
    if (info.props.is_depth && info.num_samples > 1 &&
        instance.SampleLocationGrid(info.num_samples)) {
        // A pass that moves its samples needs this of its depth buffer
        flags |= vk::ImageCreateFlagBits::eSampleLocationsCompatibleDepthEXT;
    }
    if (info.props.is_depth) {
        aspect_mask = vk::ImageAspectFlagBits::eDepth;
        if (info.props.has_stencil) {
            aspect_mask |= vk::ImageAspectFlagBits::eStencil;
        }
    }

    constexpr vk::ImageTiling tiling = vk::ImageTiling::eOptimal;
    const vk::Format supported_format =
        instance.GetSupportedFormat(info.pixel_format, format_features);
    VkPhysicalDeviceImageFormatInfo2 format_info{};
    format_info.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2;
    format_info.format = static_cast<VkFormat>(supported_format);
    format_info.type = static_cast<VkImageType>(ConvertImageType(info.type));
    format_info.tiling = static_cast<VkImageTiling>(tiling);
    format_info.usage = static_cast<VkImageUsageFlags>(usage_flags);
    format_info.flags = static_cast<VkImageCreateFlags>(flags);
    VkImageFormatProperties2 image_format_properties{};
    image_format_properties.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2;
    const VkResult format_result = vkGetPhysicalDeviceImageFormatProperties2(
        static_cast<VkPhysicalDevice>(instance.GetPhysicalDevice()), &format_info,
        &image_format_properties);
    if (format_result == VK_ERROR_FORMAT_NOT_SUPPORTED) {
        LOG_ERROR(Render_Vulkan, "image format %s type %s is not supported (flags %s, usage %s)",
                  vk::to_string(supported_format).c_str(),
                  vk::to_string(ConvertImageType(info.type)).c_str(), vk::to_string(flags).c_str(),
                  vk::to_string(usage_flags).c_str());
    }
    supported_samples =
        format_result == VK_SUCCESS
            ? vk::SampleCountFlags{image_format_properties.imageFormatProperties.sampleCounts}
            : vk::SampleCountFlagBits::e1;

    const vk::ImageCreateInfo image_ci = {
        .flags = flags,
        .imageType = ConvertImageType(info.type),
        .format = supported_format,
        .extent{
            .width = HostSize().width,
            .height = HostSize().height,
            .depth = HostSize().depth,
        },
        .mipLevels = static_cast<u32>(info.resources.levels),
        .arrayLayers = static_cast<u32>(info.resources.layers),
        .samples = LiverpoolToVK::NumSamples(info.num_samples, supported_samples),
        .tiling = tiling,
        .usage = usage_flags,
        .initialLayout = vk::ImageLayout::eUndefined,
    };

    backing = &backing_images.emplace_back();
    backing->num_samples = info.num_samples;
    backing->image = UniqueImage{instance.GetDevice(), instance.GetAllocator()};
    backing->image.Create(image_ci);

    Vulkan::SetObjectName(
        instance.GetDevice(), GetImage(),
        "Image %ux%ux%u %.*s %s 0x%" PRIx64 ":0x%x L:%u M:%u S:%u", info.size.width,
        info.size.height, info.size.depth, static_cast<int>(AmdGpu::NameOf(info.tile_mode).size()),
        AmdGpu::NameOf(info.tile_mode).data(), vk::to_string(info.pixel_format).c_str(),
        info.guest_address, info.guest_size, static_cast<unsigned>(info.resources.layers),
        static_cast<unsigned>(info.resources.levels), static_cast<unsigned>(info.num_samples));
}

Image::~Image() = default;

ImageView& Image::FindView(const ImageViewInfo& view_info, bool ensure_guest_samples) {
    if (ensure_guest_samples && backing->num_samples > 1 != info.num_samples > 1) {
        runtime->SetBackingSamples(this, info.num_samples);
    }
    const Common::SmallVector<ImageViewInfo, 2>& view_infos = backing->image_view_infos;
    for (size_t i = 0; i < view_infos.size(); ++i) {
        if (view_infos[i] == view_info) {
            return (*slot_image_views)[backing->image_view_ids[i]];
        }
    }
    const ImageViewId view_id = slot_image_views->Insert(runtime->GetInstance(), view_info, *this);
    backing->image_view_infos.emplace_back(view_info);
    backing->image_view_ids.emplace_back(view_id);
    return (*slot_image_views)[view_id];
}

void Image::GetBarriers(Barriers& barriers, vk::ImageLayout dst_layout, vk::AccessFlags2 dst_mask,
                        vk::PipelineStageFlags2 dst_stage, const SubresourceRange* subres_range) {
    State& last_state = backing->state;

    const bool needs_partial_transition =
        subres_range &&
        (subres_range->base != SubresourceBase{} || subres_range->extent != info.resources);
    const bool partially_transited = backing->num_subresource_states != 0;

    if (needs_partial_transition || partially_transited) {
        if (!partially_transited) {
            const u32 count = u32(info.resources.levels) * info.resources.layers;
            if (backing->subresource_capacity < count) {
                delete[] backing->subresource_states;
                backing->subresource_states = new State[count];
                backing->subresource_capacity = count;
            }
            for (u32 i = 0; i < count; ++i) {
                backing->subresource_states[i] = last_state;
            }
            backing->num_subresource_states = count;
        }
        State* const subresource_states = backing->subresource_states;

        // In case of partial transition, we need to change the specified subresources only.
        // Otherwise all subresources need to be set to the same state so we can use a full
        // resource transition for the next time.
        const u32 start_mip = needs_partial_transition ? subres_range->base.level : u16{0};
        const u32 end_mip = needs_partial_transition
                                ? subres_range->base.level + subres_range->extent.levels
                                : info.resources.levels;
        const u32 start_layer = needs_partial_transition ? subres_range->base.layer : u16{0};
        const u32 end_layer = needs_partial_transition
                                  ? subres_range->base.layer + subres_range->extent.layers
                                  : info.resources.layers;
        for (u32 mip = start_mip; mip < end_mip; ++mip) {
            for (u32 layer = start_layer; layer < end_layer; ++layer) {
                // NOTE: these loops may produce a lot of small barriers.
                // If this becomes a problem, we can optimize it by merging adjacent barriers.
                const u32 subres_idx = mip * info.resources.layers + layer;
                ASSERT(subres_idx < backing->num_subresource_states);
                State& state = subresource_states[subres_idx];

                constexpr vk::AccessFlags2 write_flags = vk::AccessFlagBits2::eTransferWrite |
                                                         vk::AccessFlagBits2::eShaderWrite |
                                                         vk::AccessFlagBits2::eMemoryWrite;
                const bool is_write = static_cast<bool>(state.access_mask & write_flags);
                if (state.layout != dst_layout || state.access_mask != dst_mask || is_write) {
                    barriers.emplace_back(vk::ImageMemoryBarrier2{
                        .srcStageMask = state.pl_stage,
                        .srcAccessMask = state.access_mask,
                        .dstStageMask = dst_stage,
                        .dstAccessMask = dst_mask,
                        .oldLayout = state.layout,
                        .newLayout = dst_layout,
                        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .image = GetImage(),
                        .subresourceRange{
                            .aspectMask = aspect_mask,
                            .baseMipLevel = mip,
                            .levelCount = 1,
                            .baseArrayLayer = layer,
                            .layerCount = 1,
                        },
                    });
                    state.layout = dst_layout;
                    state.access_mask = dst_mask;
                    state.pl_stage = dst_stage;
                }
            }
        }

        if (!needs_partial_transition) {
            backing->num_subresource_states = 0;
        }
    } else { // Full resource transition
        constexpr vk::AccessFlags2 write_flags = vk::AccessFlagBits2::eTransferWrite |
                                                 vk::AccessFlagBits2::eShaderWrite |
                                                 vk::AccessFlagBits2::eMemoryWrite;
        const bool is_write = static_cast<bool>(last_state.access_mask & write_flags);
        if (last_state.layout == dst_layout && last_state.access_mask == dst_mask && !is_write) {
            return;
        }
        barriers.emplace_back(vk::ImageMemoryBarrier2{
            .srcStageMask = last_state.pl_stage,
            .srcAccessMask = last_state.access_mask,
            .dstStageMask = dst_stage,
            .dstAccessMask = dst_mask,
            .oldLayout = last_state.layout,
            .newLayout = dst_layout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = GetImage(),
            .subresourceRange{
                .aspectMask = aspect_mask,
                .baseMipLevel = 0,
                .levelCount = VK_REMAINING_MIP_LEVELS,
                .baseArrayLayer = 0,
                .layerCount = VK_REMAINING_ARRAY_LAYERS,
            },
        });
    }

    last_state.layout = dst_layout;
    last_state.access_mask = dst_mask;
    last_state.pl_stage = dst_stage;
}

} // namespace VideoCore
