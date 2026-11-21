// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

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

static VkImageUsageFlags ImageUsageFlags(const Vulkan::Instance& instance, const ImageInfo& info) {
    VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                              VK_IMAGE_USAGE_SAMPLED_BIT;
    if (!info.props.is_block) {
        if (info.props.is_depth) {
            usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        } else {
            usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            if (instance.IsAttachmentFeedbackLoopLayoutSupported()) {
                usage |= VK_IMAGE_USAGE_ATTACHMENT_FEEDBACK_LOOP_BIT_EXT;
            }
            // Always create images with storage flag to avoid needing re-creation in case of e.g
            // compute clears This sacrifices a bit of performance but is less work. ExtendedUsage
            // flag is also used. The exception here is for multisample images when storage is not
            // supported, where even with ExtendedUsage we may get only one supported sample back.
            if (info.num_samples == 1 || instance.IsMultisampleStorageImageSupported())
                usage |= VK_IMAGE_USAGE_STORAGE_BIT;
        }
    } else {
        // Similarly to above, we specify storage usage. This is typically not supported by
        // compressed formats, but may be used for uncompressed views. In order to satisfy this,
        // we will also specify the extended usage bit.
        usage |= VK_IMAGE_USAGE_STORAGE_BIT;
    }

    return usage;
}

static VkImageType ConvertImageType(AmdGpu::ImageType type) noexcept {
    switch (type) {
    case AmdGpu::ImageType::Color1D:
    case AmdGpu::ImageType::Color1DArray:
    case AmdGpu::ImageType::Color2D:
    case AmdGpu::ImageType::Color2DMsaa:
    case AmdGpu::ImageType::Color2DArray:
    case AmdGpu::ImageType::Cube:
        return VK_IMAGE_TYPE_2D;
    case AmdGpu::ImageType::Color3D:
        return VK_IMAGE_TYPE_3D;
    default:
        UNREACHABLE();
    }
}

static VkFormatFeatureFlags2 FormatFeatureFlags(const VkImageUsageFlags usage_flags) {
    VkFormatFeatureFlags2 feature_flags{};
    if (usage_flags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) {
        feature_flags |= VK_FORMAT_FEATURE_2_TRANSFER_SRC_BIT;
    }
    if (usage_flags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) {
        feature_flags |= VK_FORMAT_FEATURE_2_TRANSFER_DST_BIT;
    }
    if (usage_flags & VK_IMAGE_USAGE_SAMPLED_BIT) {
        feature_flags |= VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_BIT;
    }
    if (usage_flags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) {
        feature_flags |= VK_FORMAT_FEATURE_2_COLOR_ATTACHMENT_BIT;
    }
    if (usage_flags & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
        feature_flags |= VK_FORMAT_FEATURE_2_DEPTH_STENCIL_ATTACHMENT_BIT;
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
        image = VkImage{};
        allocation = {};
    }
}

void UniqueImage::Create(const VkImageCreateInfo& image_ci) {
    this->image_ci = image_ci;
    ASSERT(!image);
    VkResult result = allocator->CreateImage(image_ci, true, image, allocation);
    if (result == VK_ERROR_OUT_OF_DEVICE_MEMORY) {
        LOG_WARNING(Render_Vulkan, "Allocating image of %ux%ux%u over budget.",
                    image_ci.extent.width, image_ci.extent.height, image_ci.extent.depth);
        result = allocator->CreateImage(image_ci, false, image, allocation);
    }
    ASSERT_MSG(result == VK_SUCCESS, "Failed allocating image with error %s",
               Vulkan::ResultName(result));
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
    VkImageCreateFlags flags{VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT |
                             VK_IMAGE_CREATE_EXTENDED_USAGE_BIT};
    if (info.props.is_volume) {
        flags |= VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT;
        if (instance.Is2dViewOf3dSupported()) {
            flags |= VK_IMAGE_CREATE_2D_VIEW_COMPATIBLE_BIT_EXT;
        }
    }
    if (info.props.is_block && instance.IsBlockTexelViewSupported()) {
        flags |= VK_IMAGE_CREATE_BLOCK_TEXEL_VIEW_COMPATIBLE_BIT;
    }

    usage_flags = ImageUsageFlags(instance, info);
    format_features = FormatFeatureFlags(usage_flags);
    if (info.props.is_depth && info.num_samples > 1 &&
        instance.SampleLocationGrid(info.num_samples)) {
        // A pass that moves its samples needs this of its depth buffer
        flags |= VK_IMAGE_CREATE_SAMPLE_LOCATIONS_COMPATIBLE_DEPTH_BIT_EXT;
    }
    if (info.props.is_depth) {
        aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (info.props.has_stencil) {
            aspect_mask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
    }

    constexpr VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
    const VkFormat supported_format = static_cast<VkFormat>(
        instance.GetSupportedFormat(info.pixel_format, vk::FormatFeatureFlags2{format_features}));
    VkPhysicalDeviceImageFormatInfo2 format_info{};
    format_info.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2;
    format_info.format = supported_format;
    format_info.type = ConvertImageType(info.type);
    format_info.tiling = tiling;
    format_info.usage = usage_flags;
    format_info.flags = flags;
    VkImageFormatProperties2 image_format_properties{};
    image_format_properties.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2;
    const VkResult format_result = vkGetPhysicalDeviceImageFormatProperties2(
        static_cast<VkPhysicalDevice>(instance.GetPhysicalDevice()), &format_info,
        &image_format_properties);
    if (format_result == VK_ERROR_FORMAT_NOT_SUPPORTED) {
        LOG_ERROR(Render_Vulkan,
                  "image format %d type %d is not supported (flags 0x%x, usage 0x%x)",
                  static_cast<int>(supported_format), static_cast<int>(ConvertImageType(info.type)),
                  static_cast<unsigned>(flags), static_cast<unsigned>(usage_flags));
    }
    supported_samples = format_result == VK_SUCCESS
                            ? image_format_properties.imageFormatProperties.sampleCounts
                            : VK_SAMPLE_COUNT_1_BIT;

    const VkImageCreateInfo image_ci = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
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
        .samples = static_cast<VkSampleCountFlagBits>(
            LiverpoolToVK::NumSamples(info.num_samples, vk::SampleCountFlags{supported_samples})),
        .tiling = tiling,
        .usage = usage_flags,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    backing = &backing_images.emplace_back();
    backing->num_samples = info.num_samples;
    backing->image =
        UniqueImage{static_cast<VkDevice>(instance.GetDevice()), instance.GetAllocator()};
    backing->image.Create(image_ci);

    Vulkan::SetObjectName(
        instance.GetDevice(), GetImage(),
        "Image %ux%ux%u %.*s format %d 0x%" PRIx64 ":0x%x L:%u M:%u S:%u", info.size.width,
        info.size.height, info.size.depth, static_cast<int>(AmdGpu::NameOf(info.tile_mode).size()),
        AmdGpu::NameOf(info.tile_mode).data(), static_cast<int>(info.pixel_format),
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

void Image::GetBarriers(Barriers& barriers, VkImageLayout dst_layout, VkAccessFlags2 dst_mask,
                        VkPipelineStageFlags2 dst_stage, const SubresourceRange* subres_range) {
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

                constexpr VkAccessFlags2 write_flags = VK_ACCESS_2_TRANSFER_WRITE_BIT |
                                                       VK_ACCESS_2_SHADER_WRITE_BIT |
                                                       VK_ACCESS_2_MEMORY_WRITE_BIT;
                const bool is_write = static_cast<bool>(state.access_mask & write_flags);
                if (state.layout != dst_layout || state.access_mask != dst_mask || is_write) {
                    barriers.emplace_back(VkImageMemoryBarrier2{
                        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
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
        constexpr VkAccessFlags2 write_flags = VK_ACCESS_2_TRANSFER_WRITE_BIT |
                                               VK_ACCESS_2_SHADER_WRITE_BIT |
                                               VK_ACCESS_2_MEMORY_WRITE_BIT;
        const bool is_write = static_cast<bool>(last_state.access_mask & write_flags);
        if (last_state.layout == dst_layout && last_state.access_mask == dst_mask && !is_write) {
            return;
        }
        barriers.emplace_back(VkImageMemoryBarrier2{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
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
