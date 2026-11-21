// SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <string.h>

#include "common/alignment.h"
#include "video_core/buffer_cache/buffer.h"
#include "video_core/renderer_vulkan/liverpool_to_vk.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_runtime.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/texture_cache/blit_helper.h"
#include "video_core/texture_cache/image.h"

#include <vulkan/vulkan_format_traits.hpp>

namespace Vulkan {

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
        UNREACHABLE_MSG("Unexpected image type %u", u32(type));
    }
}

/// The layers a copy between @src_info and @dst_info can take of each, into *@out_src_layers
/// and *@out_dst_layers
static void SanitizeCopyLayers(const VideoCore::ImageInfo& src_info,
                               const VideoCore::ImageInfo& dst_info, const u32 depth,
                               u32* out_src_layers, u32* out_dst_layers) {
    const VkImageType vk_src_type = ConvertImageType(src_info.type);
    const VkImageType vk_dst_type = ConvertImageType(dst_info.type);

    u32 src_layers = src_info.resources.layers;
    u32 dst_layers = dst_info.resources.layers;

    // 3D images can only use 1 layer.
    if (vk_src_type == VK_IMAGE_TYPE_3D && src_layers != 1) {
        LOG_WARNING(Render_Vulkan, "Coercing copy 3D source layers %u to 1.", src_layers);
        src_layers = 1;
    }
    if (vk_dst_type == VK_IMAGE_TYPE_3D && dst_layers != 1) {
        LOG_WARNING(Render_Vulkan, "Coercing copy 3D destination layers %u to 1.", dst_layers);
        dst_layers = 1;
    }

    // If the image type is equal, layer count must match. Take the minimum of both.
    if (vk_src_type == vk_dst_type) {
        if (src_layers != dst_layers) {
            LOG_WARNING(Render_Vulkan,
                        "Coercing copy source layers %u and destination layers %u to minimum.",
                        src_layers, dst_layers);
            src_layers = dst_layers = Common::Min(src_layers, dst_layers);
        }
    } else {
        // For 2D <-> 3D copies, 2D layer count must equal 3D depth.
        if (vk_src_type == VK_IMAGE_TYPE_2D && vk_dst_type == VK_IMAGE_TYPE_3D &&
            src_layers != depth) {
            LOG_WARNING(Render_Vulkan,
                        "Coercing copy 2D source layers %u to 3D destination depth %u", src_layers,
                        depth);
            src_layers = depth;
        }
        if (vk_src_type == VK_IMAGE_TYPE_3D && vk_dst_type == VK_IMAGE_TYPE_2D &&
            dst_layers != depth) {
            LOG_WARNING(Render_Vulkan,
                        "Coercing copy 2D destination layers %u to 3D source depth %u", dst_layers,
                        depth);
            dst_layers = depth;
        }
    }

    *out_src_layers = src_layers;
    *out_dst_layers = dst_layers;
}

static u32 BufferImageCopySize(const VkBufferImageCopy& copy, const vk::Format pixel_format) {
    const u32 row_length = copy.bufferRowLength ? copy.bufferRowLength : copy.imageExtent.width;
    const u32 height = copy.bufferImageHeight ? copy.bufferImageHeight : copy.imageExtent.height;

    // vulkan.hpp's format traits, the one thing of it here, until a table of the core's own
    const std::array<u8, 3> block = vk::blockExtent(pixel_format);
    const u32 block_size = vk::blockSize(pixel_format);
    const u32 row_pitch = (row_length / block[0]) * block_size;
    const u32 slice_pitch = (height / block[1]) * row_pitch;

    const u32 width_in_blocks = (copy.imageExtent.width + block[0] - 1) / block[0];
    const u32 height_in_blocks = (copy.imageExtent.height + block[1] - 1) / block[1];
    const u32 num_slices = copy.imageExtent.depth * copy.imageSubresource.layerCount;

    return (num_slices - 1) * slice_pitch + (height_in_blocks - 1) * row_pitch +
           width_in_blocks * block_size;
}

Runtime::Runtime(const Instance& instance_, Scheduler& scheduler_)
    : instance{instance_}, scheduler{scheduler_}, staging_pool{instance_, scheduler_} {
    blit_helper = new VideoCore::BlitHelper(instance, scheduler);

    memory_barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    memory_barrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
}

Runtime::~Runtime() {
    delete blit_helper;
}

void Runtime::TickFrame() {
    staging_pool.TickFrame();
}

void Runtime::CopyBuffer(const VideoCore::Buffer* src, const VideoCore::Buffer* dst,
                         const VkBufferCopy* copies, u32 num_copies) {
    scheduler.EndRendering();

    bool needs_flush{};
    for (u32 i = 0; i < num_copies; ++i) {
        needs_flush |= IsBufferAccessed(src, copies[i].srcOffset, copies[i].size);
        needs_flush |= IsBufferAccessed(dst, copies[i].dstOffset, copies[i].size, true);
    }
    if (needs_flush) {
        FlushBarriers();
    }

    const VkCommandBuffer cmdbuf = scheduler.CommandBuffer();
    instance.Mark(cmdbuf, "buffer copy: to buffer of size, first offset, first size, copies",
                  dst->SizeBytes(), num_copies == 0 ? 0 : copies[0].dstOffset,
                  num_copies == 0 ? 0 : copies[0].size, static_cast<size_t>(num_copies));
    if (num_copies != 0) {
        vkCmdCopyBuffer(cmdbuf, static_cast<VkBuffer>(src->Handle()),
                        static_cast<VkBuffer>(dst->Handle()), num_copies, copies);
    }

    for (u32 i = 0; i < num_copies; ++i) {
        const VkBufferCopy& copy = copies[i];
        AccessBuffer(src, copy.srcOffset, copy.size, VK_PIPELINE_STAGE_2_COPY_BIT,
                     VK_ACCESS_2_TRANSFER_READ_BIT);
        AccessBuffer(dst, copy.dstOffset, copy.size, VK_PIPELINE_STAGE_2_COPY_BIT,
                     VK_ACCESS_2_TRANSFER_WRITE_BIT);
    }
}

void Runtime::FillBuffer(const VideoCore::Buffer* dst, u64 offset, u64 size, u32 value) {
    scheduler.EndRendering();

    if (IsBufferAccessed(dst, offset, size, true)) {
        FlushBarriers();
    }

    const VkCommandBuffer cmdbuf = scheduler.CommandBuffer();
    instance.Mark(cmdbuf, "buffer fill: buffer of size, offset, size, value", dst->SizeBytes(),
                  offset, size, value);
    vkCmdFillBuffer(cmdbuf, dst->Handle(), offset, size, value);

    AccessBuffer(dst, offset, size, VK_PIPELINE_STAGE_2_CLEAR_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
}

void Runtime::InlineData(VideoCore::Buffer* dst, u64 offset, u32 value) {
    scheduler.EndRendering();

    if (IsBufferAccessed(dst, offset, sizeof(value), true)) {
        FlushBarriers();
    }

    const VkCommandBuffer cmdbuf = scheduler.CommandBuffer();
    instance.Mark(cmdbuf, "buffer word: buffer of size, offset, value", dst->SizeBytes(), offset,
                  value);
    vkCmdUpdateBuffer(cmdbuf, dst->Handle(), offset, sizeof(value), &value);

    AccessBuffer(dst, offset, sizeof(value), VK_PIPELINE_STAGE_2_COPY_BIT,
                 VK_ACCESS_2_TRANSFER_WRITE_BIT);
}

bool Runtime::Transit(VideoCore::Image* image, VkImageLayout dst_layout,
                      VkPipelineStageFlags2 dst_stage, VkAccessFlags2 dst_access,
                      const VideoCore::SubresourceRange* subres_range) {
    const size_t prev_num_barriers = static_cast<size_t>(image_barriers.size());
    image->GetBarriers(image_barriers, dst_layout, dst_access, dst_stage, subres_range);
    if (image->sample_locs_grid && image_barriers.size() != prev_num_barriers &&
        num_barrier_sample_locs < MaxBarrierSampleLocs) {
        BarrierSampleLocs& entry = barrier_sample_locs[num_barrier_sample_locs];
        const u32 grid = image->sample_locs_grid;
        const u32 count = grid * grid * image->sample_locs_samples;
        for (u32 i = 0; i < count; ++i) {
            entry.locations[i] = VkSampleLocationEXT{image->sample_locs[i * 2] / 16.0f,
                                                     image->sample_locs[i * 2 + 1] / 16.0f};
        }
        entry.info = VkSampleLocationsInfoEXT{
            .sType = VK_STRUCTURE_TYPE_SAMPLE_LOCATIONS_INFO_EXT,
            .sampleLocationsPerPixel =
                static_cast<VkSampleCountFlagBits>(image->sample_locs_samples),
            .sampleLocationGridSize = {grid, grid},
            .sampleLocationsCount = count,
            .pSampleLocations = entry.locations,
        };
        bool used = false;
        for (size_t i = prev_num_barriers; i < image_barriers.size(); ++i) {
            if (image_barriers[i].oldLayout != image_barriers[i].newLayout) {
                image_barriers[i].pNext = &entry.info;
                used = true;
            }
        }
        num_barrier_sample_locs += used;
    }
    return image_barriers.size() != prev_num_barriers;
}

void Runtime::UploadImage(VideoCore::Image* dst, const VideoCore::Buffer* src,
                          const VkBufferImageCopy* copies, u32 num_copies) {
    SetBackingSamples(dst, dst->info.num_samples, false);
    scheduler.EndRendering();

    bool needs_flush = Transit(dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
    for (u32 i = 0; i < num_copies; ++i) {
        const u32 copy_size = BufferImageCopySize(
            reinterpret_cast<const VkBufferImageCopy&>(copies[i]), dst->info.pixel_format);
        needs_flush |= IsBufferAccessed(src, copies[i].bufferOffset, copy_size);
    }
    if (needs_flush) {
        FlushBarriers();
    }

    const VkCommandBuffer cmdbuf = scheduler.CommandBuffer();
    instance.Mark(cmdbuf, "buffer to image: image at, size, w|h<<16|d<<32, copies|buffer size<<8",
                  dst->info.guest_address, dst->info.guest_size,
                  u64(dst->info.size.width) | u64(dst->info.size.height) << 16 |
                      u64(dst->info.size.depth) << 32,
                  u64(num_copies) | u64(src->SizeBytes()) << 8);
    if (num_copies != 0) {
        vkCmdCopyBufferToImage(cmdbuf, src->Handle(), static_cast<VkImage>(dst->GetImage()),
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, num_copies, copies);
    }

    for (u32 i = 0; i < num_copies; ++i) {
        const u32 copy_size = BufferImageCopySize(
            reinterpret_cast<const VkBufferImageCopy&>(copies[i]), dst->info.pixel_format);
        AccessBuffer(src, copies[i].bufferOffset, copy_size, VK_PIPELINE_STAGE_2_COPY_BIT,
                     VK_ACCESS_2_TRANSFER_READ_BIT);
    }

    dst->flags &= ~VideoCore::ImageFlagBits::Dirty;
}

void Runtime::DownloadImage(VideoCore::Image* src, const VideoCore::Buffer* dst,
                            const VkBufferImageCopy* copies, u32 num_copies) {
    SetBackingSamples(src, src->info.num_samples);
    scheduler.EndRendering();

    bool needs_flush = Transit(src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
    for (u32 i = 0; i < num_copies; ++i) {
        const u32 copy_size = BufferImageCopySize(
            reinterpret_cast<const VkBufferImageCopy&>(copies[i]), src->info.pixel_format);
        needs_flush |= IsBufferAccessed(dst, copies[i].bufferOffset, copy_size, true);
    }
    if (needs_flush) {
        FlushBarriers();
    }

    const VkCommandBuffer cmdbuf = scheduler.CommandBuffer();
    instance.Mark(cmdbuf, "image to buffer: image at, size, w|h<<16|d<<32, buffer of size",
                  src->info.guest_address, src->info.guest_size,
                  u64(src->info.size.width) | u64(src->info.size.height) << 16 |
                      u64(src->info.size.depth) << 32,
                  dst->SizeBytes());
    if (num_copies != 0) {
        vkCmdCopyImageToBuffer(cmdbuf, static_cast<VkImage>(src->GetImage()),
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst->Handle(), num_copies,
                               copies);
    }

    for (u32 i = 0; i < num_copies; ++i) {
        const u32 copy_size = BufferImageCopySize(
            reinterpret_cast<const VkBufferImageCopy&>(copies[i]), src->info.pixel_format);
        AccessBuffer(dst, copies[i].bufferOffset, copy_size, VK_PIPELINE_STAGE_2_COPY_BIT,
                     VK_ACCESS_2_TRANSFER_WRITE_BIT);
    }
}

bool Runtime::BlitScaled(VideoCore::Image* src, VideoCore::Image* dst, u32 dst_mip,
                         u32 dst_layer) {
    const VkImageAspectFlags src_aspect = src->aspect_mask;
    const VkImageAspectFlags dst_aspect = dst->aspect_mask;
    const bool depth = src->info.props.is_depth || dst->info.props.is_depth;
    if (src_aspect != dst_aspect || (depth && src->info.pixel_format != dst->info.pixel_format) ||
        src->info.num_samples > 1 || dst->info.num_samples > 1 || src->info.props.is_volume ||
        dst->info.props.is_volume) {
        LOG_WARNING(Render_Vulkan, "Render Resolution: no copy from 0x%" PRIxPTR " to 0x%" PRIxPTR,
                    src->info.guest_address, dst->info.guest_address);
        return false;
    }
    SetBackingSamples(dst, 1, false);
    SetBackingSamples(src, 1);
    const VideoCore::Extent3D src_size = src->HostSize();
    const VideoCore::Extent3D dst_size = dst->HostSize();
    const u32 layers =
        Common::Min<u32>(src->info.resources.layers, dst->info.resources.layers - dst_layer);
    const u32 levels =
        Common::Min<u32>(src->info.resources.levels, dst->info.resources.levels - dst_mip);
    Common::SmallVector<VkImageBlit, 8> regions;
    for (u32 mip = 0; mip < levels; ++mip) {
        VkImageBlit region{
            .srcSubresource{src_aspect, mip, 0, layers},
            .dstSubresource{dst_aspect, dst_mip + mip, dst_layer, layers},
        };
        region.srcOffsets[1] =
            VkOffset3D{static_cast<s32>(Common::Max(src_size.width >> mip, 1u)),
                       static_cast<s32>(Common::Max(src_size.height >> mip, 1u)), 1};
        region.dstOffsets[1] =
            VkOffset3D{static_cast<s32>(Common::Max(dst_size.width >> (dst_mip + mip), 1u)),
                       static_cast<s32>(Common::Max(dst_size.height >> (dst_mip + mip), 1u)), 1};
        regions.push_back(region);
    }

    scheduler.EndRendering();
    bool needs_flush = Transit(src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
    needs_flush |= Transit(dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_BLIT_BIT,
                           VK_ACCESS_2_TRANSFER_WRITE_BIT);
    if (needs_flush) {
        FlushBarriers();
    }
    instance.Mark(scheduler.CommandBuffer(), "blit: from image at, to image at, from w|h, to w|h",
                  src->info.guest_address, dst->info.guest_address,
                  u64(src->info.size.width) | u64(src->info.size.height) << 16 |
                      u64(src->info.size.depth) << 32,
                  u64(dst->info.size.width) | u64(dst->info.size.height) << 16 |
                      u64(dst->info.size.depth) << 32);
    vkCmdBlitImage(scheduler.CommandBuffer(), src->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   dst->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   static_cast<u32>(regions.size()), regions.data(),
                   depth ? VK_FILTER_NEAREST : VK_FILTER_LINEAR);

    dst->flags |= (src->flags & VideoCore::ImageFlagBits::GpuModified);
    dst->flags &= ~VideoCore::ImageFlagBits::Dirty;
    return true;
}

void Runtime::CopyImage(VideoCore::Image* src, VideoCore::Image* dst) {
    if (src->IsScaled() || dst->IsScaled()) {
        BlitScaled(src, dst);
        return;
    }
    const u32 num_mips = Common::Min(src->info.resources.levels, dst->info.resources.levels);

    // Format mismatch warning (safe but useful)
    if (src->info.pixel_format != dst->info.pixel_format) {
        LOG_DEBUG(Render_Vulkan,
                  "Copy between different formats: src=%d, dst=%d. "
                  "Result may be undefined.",
                  static_cast<int>(src->info.pixel_format),
                  static_cast<int>(dst->info.pixel_format));
    }

    const u32 base_width = src->info.size.width;
    const u32 base_height = src->info.size.height;
    const u32 base_depth =
        dst->info.type == AmdGpu::ImageType::Color3D ? dst->info.size.depth : src->info.size.depth;

    // Match sample count before copying
    SetBackingSamples(dst, dst->info.num_samples, false);
    SetBackingSamples(src, src->info.num_samples);

    Common::SmallVector<VkImageCopy, 8> regions;

    const VkImageAspectFlags src_aspect = src->aspect_mask & ~VK_IMAGE_ASPECT_STENCIL_BIT;
    const VkImageAspectFlags dst_aspect = dst->aspect_mask & ~VK_IMAGE_ASPECT_STENCIL_BIT;

    const bool src_is_2d = ConvertImageType(src->info.type) == VK_IMAGE_TYPE_2D;
    const bool src_is_3d = ConvertImageType(src->info.type) == VK_IMAGE_TYPE_3D;

    const bool dst_is_2d = ConvertImageType(dst->info.type) == VK_IMAGE_TYPE_2D;
    const bool dst_is_3d = ConvertImageType(dst->info.type) == VK_IMAGE_TYPE_3D;

    const bool is_2d_to_3d = src_is_2d && dst_is_3d;
    const bool is_3d_to_2d = src_is_3d && dst_is_2d;
    const bool is_same_type = !is_2d_to_3d && !is_3d_to_2d;

    for (u32 mip = 0; mip < num_mips; ++mip) {
        const u32 mip_w = Common::Max(base_width >> mip, 1u);
        const u32 mip_h = Common::Max(base_height >> mip, 1u);
        const u32 mip_d = Common::Max(base_depth >> mip, 1u);

        u32 src_layers;
        u32 dst_layers;
        SanitizeCopyLayers(src->info, dst->info, mip_d, &src_layers, &dst_layers);

        VkImageCopy region{};
        region.srcSubresource.aspectMask = src_aspect;
        region.srcSubresource.mipLevel = mip;
        region.srcSubresource.baseArrayLayer = 0;
        region.dstSubresource.aspectMask = dst_aspect;
        region.dstSubresource.mipLevel = mip;
        region.dstSubresource.baseArrayLayer = 0;

        if (is_same_type) {
            // 2D->2D OR 3D->3D
            if (src_is_3d) {
                // 3D images must use layerCount=1
                region.srcSubresource.layerCount = 1;
                region.dstSubresource.layerCount = 1;
                region.extent = VkExtent3D{mip_w, mip_h, mip_d};
            } else {
                // Array images
                const u32 copy_layers = Common::Min(src_layers, dst_layers);
                region.srcSubresource.layerCount = copy_layers;
                region.dstSubresource.layerCount = copy_layers;
                region.extent = VkExtent3D{mip_w, mip_h, 1};
            }
        } else if (is_2d_to_3d) {
            // 2D array -> 3D volume
            region.srcSubresource.layerCount = src_layers;
            region.dstSubresource.layerCount = 1;
            region.extent = VkExtent3D{mip_w, mip_h, src_layers};
        } else if (is_3d_to_2d) {
            // 3D volume -> 2D array
            region.srcSubresource.layerCount = 1;
            region.dstSubresource.layerCount = dst_layers;
            region.extent = VkExtent3D{mip_w, mip_h, dst_layers};
        }

        regions.push_back(region);
    }

    scheduler.EndRendering();

    bool needs_flush = Transit(src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
    needs_flush |= Transit(dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_COPY_BIT,
                           VK_ACCESS_2_TRANSFER_WRITE_BIT);
    if (needs_flush) {
        FlushBarriers();
    }

    const VkCommandBuffer cmdbuf = scheduler.CommandBuffer();
    instance.Mark(cmdbuf, "image copy: from image at, to image at, from w|h, to w|h",
                  src->info.guest_address, dst->info.guest_address,
                  u64(src->info.size.width) | u64(src->info.size.height) << 16 |
                      u64(src->info.size.depth) << 32,
                  u64(dst->info.size.width) | u64(dst->info.size.height) << 16 |
                      u64(dst->info.size.depth) << 32);
    vkCmdCopyImage(cmdbuf, src->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst->GetImage(),
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<u32>(regions.size()),
                   regions.data());

    dst->flags |= (src->flags & VideoCore::ImageFlagBits::GpuModified);
    dst->flags &= ~VideoCore::ImageFlagBits::Dirty;
}

void Runtime::CopyImageWithBuffer(VideoCore::Image* src, VideoCore::Image* dst,
                                  const VideoCore::Buffer* buffer, u64 offset) {
    const u32 num_mips = Common::Min(src->info.resources.levels, dst->info.resources.levels);
    const u32 num_layers = Common::Min(src->info.resources.layers, dst->info.resources.layers);

    SetBackingSamples(dst, dst->info.num_samples, false);
    SetBackingSamples(src, src->info.num_samples);

    VkBufferImageCopy buffer_copy = {
        .bufferOffset = offset,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource{
            .aspectMask = src->aspect_mask & ~VK_IMAGE_ASPECT_STENCIL_BIT,
            .mipLevel = 0u,
            .baseArrayLayer = 0,
            .layerCount = num_layers,
        },
        .imageOffset = {0, 0, 0},
        .imageExtent = {src->HostSize().width, src->HostSize().height, src->HostSize().depth},
    };
    u64 copy_size = BufferImageCopySize(buffer_copy, src->info.pixel_format);
    Common::SmallVector<VkBufferImageCopy, 8> buffer_copies;
    buffer_copies.push_back(buffer_copy);
    for (u32 mip = 1; mip < num_mips; ++mip) {
        VkBufferImageCopy mip_copy = buffer_copy;
        mip_copy.bufferOffset = Common::AlignUp(offset + copy_size, 16);
        mip_copy.imageSubresource.mipLevel = mip;
        mip_copy.imageExtent = VkExtent3D{Common::Max(src->HostSize().width >> mip, 1u),
                                          Common::Max(src->HostSize().height >> mip, 1u),
                                          Common::Max(src->HostSize().depth >> mip, 1u)};
        copy_size =
            mip_copy.bufferOffset - offset + BufferImageCopySize(mip_copy, src->info.pixel_format);
        buffer_copies.push_back(mip_copy);
    }

    scheduler.EndRendering();

    bool needs_flush = Transit(src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
    needs_flush |= Transit(dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_COPY_BIT,
                           VK_ACCESS_2_TRANSFER_WRITE_BIT);
    needs_flush |= IsBufferAccessed(buffer, offset, copy_size);
    if (needs_flush) {
        FlushBarriers();
    }

    const VkCommandBuffer cmdbuf = scheduler.CommandBuffer();
    instance.Mark(cmdbuf, "image copy through buffer: from image at, to image at, from w|h, to w|h",
                  src->info.guest_address, dst->info.guest_address,
                  u64(src->info.size.width) | u64(src->info.size.height) << 16 |
                      u64(src->info.size.depth) << 32,
                  u64(dst->info.size.width) | u64(dst->info.size.height) << 16 |
                      u64(dst->info.size.depth) << 32);
    vkCmdCopyImageToBuffer(cmdbuf, src->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           buffer->Handle(), static_cast<u32>(buffer_copies.size()),
                           buffer_copies.data());

    const VkMemoryBarrier2 post_copy_barrier = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
        .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
        .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
    };
    const VkDependencyInfo post_copy_dep{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
        .memoryBarrierCount = 1u,
        .pMemoryBarriers = &post_copy_barrier,
    };
    vkCmdPipelineBarrier2(cmdbuf, &post_copy_dep);

    for (size_t i = 0; i < buffer_copies.size(); ++i) {
        buffer_copies[i].imageSubresource.aspectMask =
            dst->aspect_mask & ~VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    vkCmdCopyBufferToImage(cmdbuf, buffer->Handle(), dst->GetImage(),
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           static_cast<u32>(buffer_copies.size()), buffer_copies.data());

    dst->flags |= (src->flags & VideoCore::ImageFlagBits::GpuModified);
    dst->flags &= ~VideoCore::ImageFlagBits::Dirty;
}

void Runtime::CopyMip(VideoCore::Image* src, VideoCore::Image* dst, u32 mip, u32 slice) {
    if (src->IsScaled() || dst->IsScaled()) {
        BlitScaled(src, dst, mip, slice);
        return;
    }
    const u32 dst_dim = dst->info.props.is_block ? 2 : 0;
    const u32 mip_block_w = Common::Max(dst->info.size.width >> (mip + dst_dim), 1u);
    const u32 mip_block_h = Common::Max(dst->info.size.height >> (mip + dst_dim), 1u);
    const u32 mip_block_p = Common::Max<u32>(dst->info.mips_layout[mip].pitch >> dst_dim, 1u);

    const u32 src_dim = src->info.props.is_block ? 2 : 0;
    ASSERT(mip_block_w == (src->info.size.width >> src_dim));
    ASSERT(mip_block_h == (src->info.size.height >> src_dim));
    ASSERT(mip_block_p == (src->info.pitch >> src_dim));

    u32 src_layers;
    u32 dst_layers;
    SanitizeCopyLayers(src->info, dst->info, src->info.size.depth, &src_layers, &dst_layers);

    const VkImageCopy image_copy{
        .srcSubresource{
            .aspectMask = src->aspect_mask,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = src_layers,
        },
        .dstSubresource{
            .aspectMask = src->aspect_mask,
            .mipLevel = mip,
            .baseArrayLayer = slice,
            .layerCount = dst_layers,
        },
        .extent = {src->info.size.width, src->info.size.height, src->info.size.depth},
    };

    SetBackingSamples(dst, dst->info.num_samples);
    SetBackingSamples(src, src->info.num_samples);

    scheduler.EndRendering();

    bool needs_flush = Transit(src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
    needs_flush |= Transit(dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_COPY_BIT,
                           VK_ACCESS_2_TRANSFER_WRITE_BIT);
    if (needs_flush) {
        FlushBarriers();
    }

    const VkCommandBuffer cmdbuf = scheduler.CommandBuffer();
    instance.Mark(cmdbuf, "mip copy: from image at, to image at, from w|h, to w|h",
                  src->info.guest_address, dst->info.guest_address,
                  u64(src->info.size.width) | u64(src->info.size.height) << 16 |
                      u64(src->info.size.depth) << 32,
                  u64(dst->info.size.width) | u64(dst->info.size.height) << 16 |
                      u64(dst->info.size.depth) << 32);
    vkCmdCopyImage(cmdbuf, src->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst->GetImage(),
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &image_copy);

    dst->flags |= (src->flags & VideoCore::ImageFlagBits::GpuModified);
    dst->flags &= ~VideoCore::ImageFlagBits::Dirty;
}

void Runtime::CopyColorAndDepth(VideoCore::Image* src, VideoCore::Image* dst) {
    if (src->HostSize() != dst->HostSize()) {
        BlitScaled(src, dst);
        return;
    }
    if (src->info.num_samples == 1 && dst->info.num_samples == 1) {
        if ((instance.IsMaintenance8Supported() ||
             src->info.props.is_depth == dst->info.props.is_depth) &&
            src->aspect_mask == dst->aspect_mask) {
            CopyImage(src, dst);
        } else {
            // Perform depth from/to color copy using the intermediate copy buffer.
            static constexpr size_t COPY_BUFFER_SIZE = 128_MB;
            const StagingBufferRef copy_ref =
                staging_pool.Request(COPY_BUFFER_SIZE, VideoCore::MemoryType::DeviceLocal);
            CopyImageWithBuffer(src, dst, copy_ref.buffer, copy_ref.offset);
        }
    } else if (src->info.num_samples == 1 && dst->info.num_samples > 1 &&
               dst->info.props.is_depth) {
        // Perform a rendering pass to transfer the channels of source as samples in dest.
        bool needs_flush =
            Transit(src, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);
        needs_flush |= Transit(dst, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                               VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                                   VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                               VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
        if (needs_flush) {
            FlushBarriers();
        }

        blit_helper->ReinterpretColorAsMsDepth(
            dst->HostSize().width, dst->HostSize().height, dst->info.num_samples,
            static_cast<VkFormat>(src->info.pixel_format),
            static_cast<VkFormat>(dst->info.pixel_format), static_cast<VkImage>(src->GetImage()),
            static_cast<VkImage>(dst->GetImage()));
    } else {
        LOG_WARNING(Render_Vulkan, "Unimplemented depth overlap copy");
    }
}

void Runtime::CopyDepthStencil(VideoCore::Image* src, VideoCore::Image* dst,
                               const VideoCore::SubresourceRange& sub_range) {
    if (src->HostSize() != dst->HostSize()) {
        BlitScaled(src, dst);
        return;
    }
    scheduler.EndRendering();

    bool needs_flush = Transit(src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
    needs_flush |= Transit(dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_COPY_BIT,
                           VK_ACCESS_2_TRANSFER_WRITE_BIT);
    if (needs_flush) {
        FlushBarriers();
    }

    const VkImageAspectFlags aspect_mask = src->aspect_mask & dst->aspect_mask;

    const VkImageCopy region = {
        .srcSubresource{
            .aspectMask = aspect_mask,
            .mipLevel = 0,
            .baseArrayLayer = sub_range.base.layer,
            .layerCount = sub_range.extent.layers,
        },
        .srcOffset = {0, 0, 0},
        .dstSubresource{
            .aspectMask = aspect_mask,
            .mipLevel = 0,
            .baseArrayLayer = sub_range.base.layer,
            .layerCount = sub_range.extent.layers,
        },
        .dstOffset = {0, 0, 0},
        .extent = {dst->HostSize().width, dst->HostSize().height, 1},
    };

    const VkCommandBuffer cmdbuf = scheduler.CommandBuffer();
    instance.Mark(cmdbuf, "depth copy: from image at, to image at, from w|h, to w|h",
                  src->info.guest_address, dst->info.guest_address,
                  u64(src->info.size.width) | u64(src->info.size.height) << 16 |
                      u64(src->info.size.depth) << 32,
                  u64(dst->info.size.width) | u64(dst->info.size.height) << 16 |
                      u64(dst->info.size.depth) << 32);
    vkCmdCopyImage(cmdbuf, src->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst->GetImage(),
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    dst->flags |= VideoCore::ImageFlagBits::GpuModified;
    dst->flags &= ~VideoCore::ImageFlagBits::Dirty;
}

void Runtime::CopyDepthToColor(VideoCore::Image* src, VideoCore::Image* dst, bool stencil,
                               bool uint_out, VkColorComponentFlags write_mask, u32 sample,
                               u32 src_layer, u32 dst_layer) {
    const VkImageAspectFlags plane =
        stencil ? VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_DEPTH_BIT;
    if (!(src->aspect_mask & plane) || !(dst->aspect_mask & VK_IMAGE_ASPECT_COLOR_BIT)) {
        return;
    }
    // Channels outside the mask keep what they hold
    SetBackingSamples(dst, dst->info.num_samples);
    scheduler.EndRendering();

    bool needs_flush =
        Transit(src, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);
    needs_flush |=
        Transit(dst, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
    if (needs_flush) {
        FlushBarriers();
    }

    const u32 src_samples = src->backing->num_samples;
    const u32 width = Common::Min(src->HostSize().width, dst->HostSize().width);
    const u32 height = Common::Min(src->HostSize().height, dst->HostSize().height);
    instance.Mark(scheduler.CommandBuffer(),
                  "depth to colour: from image at, to image at, stencil|sample<<8|samples<<16, "
                  "w|h<<16",
                  src->info.guest_address, dst->info.guest_address,
                  u64(stencil) | u64(sample) << 8 | u64(src_samples) << 16,
                  u64(width) | u64(height) << 16);
    blit_helper->CopyDepthToColor(
        width, height, dst->backing->num_samples, static_cast<VkFormat>(src->info.pixel_format),
        static_cast<VkFormat>(dst->info.pixel_format), src_samples > 1, stencil, uint_out,
        write_mask, Common::Min(sample, src_samples - 1), static_cast<VkImage>(src->GetImage()),
        src_layer, static_cast<VkImage>(dst->GetImage()), dst_layer);

    dst->flags |= VideoCore::ImageFlagBits::GpuModified;
    dst->flags &= ~VideoCore::ImageFlagBits::Dirty;
}

void Runtime::ResolveImage(VideoCore::Image* src, VideoCore::Image* dst,
                           const VideoCore::SubresourceRange& src_range,
                           const VideoCore::SubresourceRange& dst_range) {
    if (src->HostSize() != dst->HostSize() && src->backing->num_samples == 1) {
        BlitScaled(src, dst, dst_range.base.level, dst_range.base.layer);
        return;
    }
    SetBackingSamples(dst, 1, false);
    scheduler.EndRendering();

    const bool needs_resolve = src->backing->num_samples != 1;
    const VkPipelineStageFlags2 dst_stage =
        needs_resolve ? VK_PIPELINE_STAGE_2_RESOLVE_BIT : VK_PIPELINE_STAGE_2_COPY_BIT;

    bool needs_flush = Transit(src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst_stage,
                               VK_ACCESS_2_TRANSFER_READ_BIT);
    needs_flush |= Transit(dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, dst_stage,
                           VK_ACCESS_2_TRANSFER_WRITE_BIT);
    if (needs_flush) {
        FlushBarriers();
    }

    const VkCommandBuffer cmdbuf = scheduler.CommandBuffer();
    u32 src_layers;
    u32 dst_layers;
    SanitizeCopyLayers(src->info, dst->info, 1, &src_layers, &dst_layers);
    if (!needs_resolve) {
        const VkImageCopy region = {
            .srcSubresource{
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = 0,
                .baseArrayLayer = src_range.base.layer,
                .layerCount = src_layers,
            },
            .srcOffset = {0, 0, 0},
            .dstSubresource{
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = 0,
                .baseArrayLayer = dst_range.base.layer,
                .layerCount = dst_layers,
            },
            .dstOffset = {0, 0, 0},
            .extent = {src->HostSize().width, src->HostSize().height, 1},
        };
        instance.Mark(cmdbuf, "resolve by copy: from image at, to image at, from w|h, to w|h",
                      src->info.guest_address, dst->info.guest_address,
                      u64(src->info.size.width) | u64(src->info.size.height) << 16 |
                          u64(src->info.size.depth) << 32,
                      u64(dst->info.size.width) | u64(dst->info.size.height) << 16 |
                          u64(dst->info.size.depth) << 32);
        vkCmdCopyImage(cmdbuf, src->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       dst->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    } else {
        const VkImageResolve region = {
            .srcSubresource{
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = 0,
                .baseArrayLayer = src_range.base.layer,
                .layerCount = src_layers,
            },
            .srcOffset = {0, 0, 0},
            .dstSubresource{
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = 0,
                .baseArrayLayer = dst_range.base.layer,
                .layerCount = dst_layers,
            },
            .dstOffset = {0, 0, 0},
            .extent = {src->HostSize().width, src->HostSize().height, 1},
        };
        instance.Mark(cmdbuf, "resolve: from image at, to image at, from w|h, to w|h",
                      src->info.guest_address, dst->info.guest_address,
                      u64(src->info.size.width) | u64(src->info.size.height) << 16 |
                          u64(src->info.size.depth) << 32,
                      u64(dst->info.size.width) | u64(dst->info.size.height) << 16 |
                          u64(dst->info.size.depth) << 32);
        vkCmdResolveImage(cmdbuf, src->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                          dst->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    }

    dst->flags |= VideoCore::ImageFlagBits::GpuModified;
    dst->flags &= ~VideoCore::ImageFlagBits::Dirty;
}

void Runtime::ClearImage(VideoCore::Image* dst, const VideoCore::SubresourceRange& range,
                         const VkClearValue& clear_value) {
    scheduler.EndRendering();

    const bool needs_flush =
        Transit(dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_CLEAR_BIT,
                VK_ACCESS_2_TRANSFER_WRITE_BIT, &range);
    if (needs_flush) {
        FlushBarriers();
    }

    const VkImageSubresourceRange vk_range = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = range.base.level,
        .levelCount = range.extent.levels,
        .baseArrayLayer = range.base.layer,
        .layerCount = range.extent.layers,
    };
    const VkCommandBuffer cmdbuf = scheduler.CommandBuffer();
    instance.Mark(cmdbuf, "clear: image at, size, w|h<<16|d<<32", dst->info.guest_address,
                  dst->info.guest_size,
                  u64(dst->info.size.width) | u64(dst->info.size.height) << 16 |
                      u64(dst->info.size.depth) << 32);
    vkCmdClearColorImage(cmdbuf, dst->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         &clear_value.color, 1, &vk_range);

    dst->flags |= VideoCore::ImageFlagBits::GpuModified;
    dst->flags &= ~VideoCore::ImageFlagBits::Dirty;
}

void Runtime::SetBackingSamples(VideoCore::Image* image, u32 num_samples, bool copy_backing) {
    VideoCore::Image::BackingImage*& backing = image->backing;
    Common::SmallVector<VideoCore::Image::BackingImage, 2>& backing_images = image->backing_images;
    const VideoCore::ImageInfo& info = image->info;
    if (!backing || backing->num_samples == num_samples) {
        return;
    }
    ASSERT_MSG(!image->info.props.is_depth, "Swapping samples is only valid for color images");
    VideoCore::Image::BackingImage* new_backing = nullptr;
    for (size_t i = 0; i < backing_images.size(); ++i) {
        if (backing_images[i].num_samples == num_samples) {
            new_backing = &backing_images[i];
            break;
        }
    }
    if (!new_backing) {
        const u64 previous_size = Common::AlignUp(image->SizeBytes(), 1024);
        VkImageCreateInfo new_image_ci = backing->image.image_ci;
        new_image_ci.samples = static_cast<VkSampleCountFlagBits>(
            LiverpoolToVK::NumSamples(num_samples, vk::SampleCountFlags{image->supported_samples}));

        new_backing = &backing_images.emplace_back();
        new_backing->num_samples = num_samples;
        new_backing->image = VideoCore::UniqueImage{static_cast<VkDevice>(instance.GetDevice()),
                                                    instance.GetAllocator()};
        new_backing->image.Create(new_image_ci);
        if (image->memory_usage_counter) {
            *image->memory_usage_counter +=
                Common::AlignUp(image->SizeBytes(), 1024) - previous_size;
        }

        Vulkan::SetObjectName(
            instance.GetDevice(), new_backing->image.image,
            "Image %ux%ux%u %.*s %d 0x%" PRIx64 ":0x%x L:%u M:%u S:%u (backing)", info.size.width,
            info.size.height, info.size.depth,
            static_cast<int>(AmdGpu::NameOf(info.tile_mode).size()),
            AmdGpu::NameOf(info.tile_mode).data(), static_cast<int>(info.pixel_format),
            info.guest_address, info.guest_size, static_cast<unsigned>(info.resources.layers),
            static_cast<unsigned>(info.resources.levels), static_cast<unsigned>(num_samples));
    }

    if (copy_backing) {
        scheduler.EndRendering();
        ASSERT(image->info.resources.levels == 1 && image->info.resources.layers == 1);

        // Transition current backing to shader read layout
        Transit(image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);

        // Transition dest backing to color attachment layout, not caring of previous contents
        constexpr VkPipelineStageFlags2 dst_stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        constexpr VkAccessFlags2 dst_access =
            VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        constexpr VkImageLayout dst_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        image_barriers.push_back(VkImageMemoryBarrier2{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            .srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
            .dstStageMask = dst_stage,
            .dstAccessMask = dst_access,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = dst_layout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = new_backing->image,
            .subresourceRange{
                .aspectMask = image->aspect_mask,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = image->info.resources.layers,
            },
        });
        FlushBarriers();

        instance.Mark(scheduler.CommandBuffer(),
                      "samples copy: image at, from samples, to samples, w|h<<16",
                      info.guest_address, backing->num_samples, new_backing->num_samples,
                      u64(image->HostSize().width) | u64(image->HostSize().height) << 16);
        // Copy between ms and non ms backing images
        blit_helper->CopyBetweenMsImages(
            image->HostSize().width, image->HostSize().height, new_backing->num_samples,
            static_cast<VkFormat>(info.pixel_format), backing->num_samples > 1,
            static_cast<VkImage>(backing->image), static_cast<VkImage>(new_backing->image));

        // Update current layout in tracker to new backings layout
        new_backing->state.layout = dst_layout;
        new_backing->state.access_mask = dst_access;
        new_backing->state.pl_stage = dst_stage;
    }

    backing = new_backing;
}

bool Runtime::IsBufferAccessed(const VideoCore::Buffer* handle, u64 offset, u64 size,
                               bool check_read_access) {
    MakeCurrent(handle);
    bool has_access = resource->write_ranges.Overlaps(offset, offset + size);
    if (check_read_access && !has_access) {
        has_access |= resource->read_ranges.Overlaps(offset, offset + size);
    }
    return has_access;
}

void Runtime::AccessBuffer(const VideoCore::Buffer* handle, u64 offset, u64 size,
                           VkPipelineStageFlags2 src_stage, VkAccessFlags2 src_access) {
    MakeCurrent(handle);

    const Interval range = {
        .start = offset,
        .end = offset + size,
    };

    constexpr static VkAccessFlags2 READ_MASK =
        VK_ACCESS_2_INDEX_READ_BIT | VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT |
        VK_ACCESS_2_UNIFORM_READ_BIT | VK_ACCESS_2_SHADER_READ_BIT |
        VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
        VK_ACCESS_2_TRANSFER_READ_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

    constexpr static VkAccessFlags2 WRITE_MASK =
        VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT |
        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT |
        VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_TRANSFORM_FEEDBACK_WRITE_BIT_EXT;

    if (src_access & WRITE_MASK) {
        resource->write_ranges.Add(range);
    }
    if (src_access & READ_MASK) {
        resource->read_ranges.Add(range);
    }

    memory_barrier.srcStageMask |= src_stage;
    memory_barrier.srcAccessMask |= src_access & WRITE_MASK;
}

void Runtime::FlushBarriers() {
    VkDependencyInfo dep_info{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO};

    if (memory_barrier.srcStageMask) {
        dep_info.pMemoryBarriers = &memory_barrier;
        dep_info.memoryBarrierCount = 1U;
    }
    if (!image_barriers.empty()) {
        dep_info.pImageMemoryBarriers = image_barriers.data();
        dep_info.imageMemoryBarrierCount = static_cast<u32>(image_barriers.size());
    }

    if (!dep_info.memoryBarrierCount && !dep_info.imageMemoryBarrierCount) {
        return;
    }

    scheduler.EndRendering();
    const VkCommandBuffer cmdbuf = scheduler.CommandBuffer();
    vkCmdPipelineBarrier2(cmdbuf, &dep_info);

    memory_barrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
    memory_barrier.srcAccessMask = VK_ACCESS_2_NONE;

    image_barriers.clear();
    num_barrier_sample_locs = 0;
    for (size_t i = 0; i < resources.size(); ++i) {
        resources[i].read_ranges.Clear();
        resources[i].write_ranges.Clear();
    }
    resources.clear();
    resource = nullptr;
}

void Runtime::MakeCurrent(const VideoCore::Buffer* handle) {
    if (resource && resource->handle == handle) {
        return;
    }
    for (size_t i = 0; i < resources.size(); ++i) {
        if (resources[i].handle == handle) {
            resource = &resources[i];
            return;
        }
    }
    resource = &resources.emplace_back();
    resource->handle = handle;
}

} // namespace Vulkan
