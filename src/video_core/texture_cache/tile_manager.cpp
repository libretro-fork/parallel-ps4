// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdio.h>
#include <string.h>

#include "video_core/renderer_vulkan/vk_format_traits.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_runtime.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_shader_util.h"
#include "video_core/texture_cache/image.h"
#include "video_core/texture_cache/image_info.h"
#include "video_core/texture_cache/image_view.h"
#include "video_core/texture_cache/tile_manager.h"

#include "common/enum_names.h"

namespace VideoCore {

namespace {

/// A tiling pipeline's key, packed: tile mode, pixel width, samples, and tiling or not
u64 TilingKey(const ImageInfo& info, bool is_tiler) {
    return (u64(info.tile_mode) << 32) | (u64(info.num_bits) << 16) | (u64(info.num_samples) << 1) |
           (is_tiler ? 1 : 0);
}

/// The same for a pipeline that detiles straight into an image (a 3D one for @volume)
u64 DetileImageKey(const ImageInfo& info, bool volume) {
    return TilingKey(info, false) | (u64(1) << 63) | (u64(volume) << 62);
}

/// The storage view format DetileIntoImage writes texels of @num_bits as, none for a width it
/// has none for
VkFormat DetileViewFormat(u32 num_bits) {
    switch (num_bits) {
    case 8:
        return VK_FORMAT_R8_UINT;
    case 16:
        return VK_FORMAT_R16_UINT;
    case 32:
        return VK_FORMAT_R32_UINT;
    case 64:
        return VK_FORMAT_R32G32_UINT;
    case 128:
        return VK_FORMAT_R32G32B32A32_UINT;
    default:
        return VK_FORMAT_UNDEFINED;
    }
}

/// DetileIntoImage's push constants: the mip written, and its size in the image
struct DetileImagePush {
    u32 mip;
    u32 width;
    u32 height;
    u32 depth;
};

} // Anonymous namespace

struct TilingInfo {
    u32 bank_swizzle;
    u32 micro_tiled_mips;
    u32 num_mips;
    MipInfo mips[MAX_MIPS];
};

TileManager::TileManager(const Vulkan::Instance& instance_, Vulkan::Scheduler& scheduler_,
                         Vulkan::Runtime& runtime_, StreamBuffer& stream_buffer_)
    : instance{instance_}, scheduler{scheduler_}, runtime{runtime_}, stream_buffer{stream_buffer_} {
    // Made once: a game uses a few dozen tiling pipelines
    u64_map_init(&tiling_pipelines, sizeof(VkPipeline), 64);
    const VkDevice device = instance.GetDevice();
    VkDescriptorSetLayoutBinding bindings[3]{};
    for (u32 i = 0; i < 3; ++i) {
        bindings[i].binding = i;
        bindings[i].descriptorType =
            i == 2 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }

    VkDescriptorSetLayoutCreateInfo desc_layout_ci{};
    desc_layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    desc_layout_ci.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
    desc_layout_ci.bindingCount = 3;
    desc_layout_ci.pBindings = bindings;
    const VkResult desc_layout_result =
        vkCreateDescriptorSetLayout(device, &desc_layout_ci, nullptr, &desc_layout);
    ASSERT_MSG(desc_layout_result == VK_SUCCESS, "Failed to create descriptor set layout: %s",
               Vulkan::ResultName(desc_layout_result));

    VkPipelineLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.setLayoutCount = 1U;
    layout_info.pSetLayouts = &desc_layout;
    const VkResult layout_result =
        vkCreatePipelineLayout(device, &layout_info, nullptr, &pl_layout);
    ASSERT_MSG(layout_result == VK_SUCCESS, "Failed to create pipeline layout: %s",
               Vulkan::ResultName(layout_result));

    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    const VkResult image_desc_result =
        vkCreateDescriptorSetLayout(device, &desc_layout_ci, nullptr, &image_desc_layout);
    ASSERT_MSG(image_desc_result == VK_SUCCESS, "Failed to create descriptor set layout: %s",
               Vulkan::ResultName(image_desc_result));
    VkPushConstantRange push_range{};
    push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    push_range.size = sizeof(DetileImagePush);
    layout_info.pSetLayouts = &image_desc_layout;
    layout_info.pushConstantRangeCount = 1;
    layout_info.pPushConstantRanges = &push_range;
    const VkResult image_layout_result =
        vkCreatePipelineLayout(device, &layout_info, nullptr, &image_pl_layout);
    ASSERT_MSG(image_layout_result == VK_SUCCESS, "Failed to create pipeline layout: %s",
               Vulkan::ResultName(image_layout_result));
}

TileManager::~TileManager() {
    const VkDevice device = instance.GetDevice();
    size_t pos = 0;
    while (const VkPipeline* pipeline =
               static_cast<const VkPipeline*>(u64_map_next(&tiling_pipelines, &pos, nullptr))) {
        vkDestroyPipeline(device, *pipeline, nullptr);
    }
    u64_map_free(&tiling_pipelines);
    vkDestroyPipelineLayout(device, pl_layout, nullptr);
    vkDestroyDescriptorSetLayout(device, desc_layout, nullptr);
    vkDestroyPipelineLayout(device, image_pl_layout, nullptr);
    vkDestroyDescriptorSetLayout(device, image_desc_layout, nullptr);
}

namespace {

/// Writes the tiling parameters straight into the stream buffer, with no copy on the stack.
/// The bytes are the same as before: the mips past num_mips stay zero.
u64 WriteTilingInfo(StreamBuffer& stream_buffer, u64 alignment, const ImageInfo& info,
                    u32 num_mips) {
    u64 offset = 0;
    u8* const data = stream_buffer.Map(sizeof(TilingInfo), &offset, alignment);
    ASSERT_MSG(data, "Failed to map the tiling parameters");
    TilingInfo* params = reinterpret_cast<TilingInfo*>(data);
    params->bank_swizzle = info.bank_swizzle;
    params->micro_tiled_mips = info.micro_tiled_mips;
    params->num_mips = num_mips;
    for (u32 mip = 0; mip < num_mips; ++mip) {
        MipInfo mip_info = info.mips_layout[mip];
        if (info.props.is_block) {
            const u16 pitch = u16((mip_info.pitch + 3) / 4);
            const u16 height = u16((mip_info.height + 3) / 4);
            mip_info.pitch = pitch ? pitch : 1;
            mip_info.height = height ? height : 1;
        }
        params->mips[mip] = mip_info;
    }
    memset(&params->mips[num_mips], 0, (MAX_MIPS - num_mips) * sizeof(MipInfo));
    stream_buffer.Commit();
    return offset;
}

/// The image's tiling parameters, folded into the shader (host_spirv_tiling_comp's @params)
void TilingParams(const ImageInfo& info, bool is_tiler, u32 params[12]) {
    const AmdGpu::MicroTileMode micro_tile_mode = AmdGpu::GetMicroTileMode(info.tile_mode);
    const bool is_macro = AmdGpu::IsMacroTiled(info.array_mode);
    params[0] = info.num_samples;
    params[1] = u32(micro_tile_mode);
    params[2] = AmdGpu::GetMicroTileThickness(info.array_mode);
    params[3] = u32(is_tiler);
    for (u32 i = 4; i < 12; ++i) {
        params[i] = 0;
    }
    if (is_macro) {
        const AmdGpu::MacroTileMode macro_tile_mode =
            AmdGpu::CalculateMacrotileMode(info.tile_mode, info.num_bits, info.num_samples);
        params[4] = u32(info.array_mode);
        params[5] = u32(AmdGpu::GetPipeConfig(info.tile_mode));
        params[6] = AmdGpu::GetBankWidth(macro_tile_mode);
        params[7] = AmdGpu::GetBankHeight(macro_tile_mode);
        params[8] = AmdGpu::GetNumBanks(macro_tile_mode);
        u32 bank_bits = 0;
        for (u32 banks = params[8]; banks > 1; banks >>= 1) {
            ++bank_bits;
        }
        params[9] = bank_bits;
        params[10] = AmdGpu::CalculateTileSplit(info.tile_mode, info.array_mode, micro_tile_mode,
                                                info.num_bits);
        params[11] = AmdGpu::GetMacrotileAspect(macro_tile_mode);
    }
}

/// A compute pipeline of @layout from the @num_words of SPIR-V at @words, named @name
VkPipeline MakeTilingPipeline(VkDevice device, const u32* words, size_t num_words,
                              VkPipelineLayout layout, const char* name) {
    const VkShaderModule module =
        static_cast<VkShaderModule>(Vulkan::CompileSPV(words, num_words, device));
    LOG_INFO(Render_Vulkan, "Creating tiling pipeline %s", name);
    Vulkan::SetObjectName(device, module, "%s", name);

    VkComputePipelineCreateInfo compute_pipeline_ci{};
    compute_pipeline_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    compute_pipeline_ci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    compute_pipeline_ci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    compute_pipeline_ci.stage.module = module;
    compute_pipeline_ci.stage.pName = "main";
    compute_pipeline_ci.layout = layout;

    VkPipeline handle = VK_NULL_HANDLE;
    const VkResult result =
        vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &compute_pipeline_ci, nullptr, &handle);
    ASSERT_MSG(result == VK_SUCCESS, "Detiler pipeline creation failed %s",
               Vulkan::ResultName(result));
    vkDestroyShaderModule(device, module, nullptr);
    return handle;
}

/// Binds the tiling pipeline and pushes its tiled, linear and parameter buffers
void BindTiling(VkCommandBuffer cmdbuf, VkPipeline pipeline, VkPipelineLayout layout,
                VkBuffer tiled, u64 tiled_offset, VkBuffer linear, u64 linear_offset, u64 size,
                VkBuffer params, u64 params_offset, u64 params_size) {
    vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    VkDescriptorBufferInfo infos[3]{};
    infos[0].buffer = tiled;
    infos[0].offset = tiled_offset;
    infos[0].range = size;
    infos[1].buffer = linear;
    infos[1].offset = linear_offset;
    infos[1].range = size;
    infos[2].buffer = params;
    infos[2].offset = params_offset;
    infos[2].range = params_size;
    VkWriteDescriptorSet writes[3]{};
    for (u32 i = 0; i < 3; ++i) {
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = VK_NULL_HANDLE;
        writes[i].dstBinding = i;
        writes[i].dstArrayElement = 0;
        writes[i].descriptorCount = 1;
        writes[i].descriptorType =
            i == 2 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[i].pBufferInfo = &infos[i];
    }
    vkCmdPushDescriptorSetKHR(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 3, writes);
}

} // Anonymous namespace

VkPipeline TileManager::GetTilingPipeline(const ImageInfo& info, bool is_tiler) {
    const u64 key = TilingKey(info, is_tiler);
    if (const VkPipeline* found =
            static_cast<const VkPipeline*>(u64_map_find(&tiling_pipelines, key))) {
        return *found;
    }

    const VkDevice device = instance.GetDevice();
    const bool is_macro = AmdGpu::IsMacroTiled(info.array_mode);
    u32 params[12];
    TilingParams(info, is_tiler, params);

    // Without 8- or 16-bit storage buffer access the 8- and 16-bit shaders read and write
    // garbage (text and some textures in stripes on an Adreno 618): those texels are moved
    // inside 32-bit words instead
    const bool word_access =
        (info.num_bits == 8 && !instance.IsStorageBuffer8BitAccessSupported()) ||
        (info.num_bits == 16 && !instance.IsStorageBuffer16BitAccessSupported());
    switch (info.num_bits) {
    case 8:
    case 16:
    case 32:
    case 64:
    case 96:
    case 128:
        break;
    default:
        UNREACHABLE_MSG("Unsupported tiling pixel width %u", info.num_bits);
    }
    const size_t num_words =
        host_spirv_tiling_comp(spirv_words, HOST_SPIRV_MAX_WORDS, info.num_bits, is_macro,
                               word_access, params, is_macro ? 12 : 4);
    ASSERT_MSG(num_words != 0, "Tiling shader does not fit");
    char module_name[96];
    snprintf(module_name, sizeof(module_name), "%s_%u %s",
             enum_name(&enum_names_AmdGpu_TileMode, static_cast<s64>(info.tile_mode)),
             info.num_bits, is_tiler ? "tiler" : "detiler");
    const VkPipeline handle =
        MakeTilingPipeline(device, spirv_words, num_words, pl_layout, module_name);
    *static_cast<VkPipeline*>(u64_map_insert(&tiling_pipelines, key, nullptr)) = handle;
    return handle;
}

const Buffer* TileManager::DetileImage(const VideoCore::Buffer* in_buffer, u64 in_offset,
                                       const ImageInfo& info, u64* out_offset) {
    if (!info.props.is_tiled) {
        *out_offset = in_offset;
        return in_buffer;
    }

    const u32 num_mips = info.resources.levels;
    const u64 params_offset =
        WriteTilingInfo(stream_buffer, instance.UniformMinAlignment(), info, num_mips);

    const Vulkan::StagingBufferRef staging = runtime.GetStagingPool().Request(
        info.guest_size, MemoryType::DeviceLocal, 256, false, true);

    scheduler.EndRendering();
    runtime.FlushBarriers();

    const VkCommandBuffer cmdbuf = scheduler.CommandBuffer();
    BindTiling(cmdbuf, GetTilingPipeline(info, false), pl_layout, in_buffer->Handle(), in_offset,
               staging.buffer->Handle(), staging.offset, info.guest_size, stream_buffer.Handle(),
               params_offset, sizeof(TilingInfo));

    const u32 dim_x = (info.guest_size / (info.num_bits / 8)) / 64;
    instance.Mark(cmdbuf, "detile: image at, size, mode|bits<<8|mips<<16, w|h<<16|d<<32",
                  info.guest_address, info.guest_size,
                  u64(info.tile_mode) | u64(info.num_bits) << 8 | u64(info.resources.levels) << 16,
                  u64(info.size.width) | u64(info.size.height) << 16 | u64(info.size.depth) << 32);
    vkCmdDispatch(cmdbuf, dim_x, 1, 1);

    runtime.AccessBuffer(staging.buffer, staging.offset, info.guest_size,
                         VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);

    *out_offset = staging.offset;
    return staging.buffer;
}

VkPipeline TileManager::GetDetileImagePipeline(const ImageInfo& info, bool volume) {
    const u64 key = DetileImageKey(info, volume);
    if (const VkPipeline* found =
            static_cast<const VkPipeline*>(u64_map_find(&tiling_pipelines, key))) {
        return *found;
    }
    const bool is_macro = AmdGpu::IsMacroTiled(info.array_mode);
    u32 params[12];
    TilingParams(info, false, params);
    const size_t num_words =
        host_spirv_detile_image_comp(spirv_words, HOST_SPIRV_MAX_WORDS, info.num_bits, is_macro,
                                     volume, params, is_macro ? 12 : 4);
    ASSERT_MSG(num_words != 0, "Detile into image shader does not fit");
    char module_name[96];
    snprintf(module_name, sizeof(module_name), "%s_%u detiler into %s",
             enum_name(&enum_names_AmdGpu_TileMode, static_cast<s64>(info.tile_mode)),
             info.num_bits, volume ? "3D image" : "image");
    const VkPipeline handle = MakeTilingPipeline(instance.GetDevice(), spirv_words, num_words,
                                                 image_pl_layout, module_name);
    *static_cast<VkPipeline*>(u64_map_insert(&tiling_pipelines, key, nullptr)) = handle;
    return handle;
}

bool TileManager::DetileIntoImage(const VideoCore::Buffer* in_buffer, u64 in_offset, Image& image) {
    const ImageInfo& info = image.info;
    if (!info.props.is_tiled || info.props.is_block || info.props.is_depth ||
        info.props.has_stencil || info.num_samples != 1 || image.IsScaled()) {
        return false;
    }
    const bool volume = info.type == AmdGpu::ImageType::Color3D;
    if (!volume && info.type != AmdGpu::ImageType::Color2D &&
        info.type != AmdGpu::ImageType::Color1D) {
        return false;
    }
    // A texel is written as a uint of its whole width, into a view of the image of that width
    const VkFormat view_format = DetileViewFormat(info.num_bits);
    if (view_format == VK_FORMAT_UNDEFINED || !(image.usage_flags & VK_IMAGE_USAGE_STORAGE_BIT) ||
        vk_format_block_size(info.pixel_format) * 8 != info.num_bits ||
        vk_format_block_size(image.backing->image.image_ci.format) * 8 != info.num_bits ||
        !instance.IsFormatSupported(view_format, VK_FORMAT_FEATURE_2_STORAGE_IMAGE_BIT) ||
        (info.num_bits != 32 && info.num_bits != 128 &&
         !instance.IsStorageImageExtendedFormatsSupported())) {
        return false;
    }

    const u32 num_mips = info.resources.levels;
    const u32 num_layers = info.resources.layers;
    runtime.SetBackingSamples(&image, info.num_samples, false);
    // A view of each mip, all its layers (cached on the image after the first time)
    VkImageView views[MAX_MIPS];
    for (u32 mip = 0; mip < num_mips; ++mip) {
        ImageViewInfo view_info;
        view_info.type = volume ? AmdGpu::ImageType::Color3D : AmdGpu::ImageType::Color2DArray;
        view_info.format = view_format;
        view_info.range.base.level = static_cast<u16>(mip);
        view_info.range.extent.levels = 1;
        view_info.range.extent.layers = static_cast<u16>(num_layers);
        view_info.is_storage = true;
        views[mip] = image.FindView(view_info, false).Handle();
    }

    const u64 params_offset =
        WriteTilingInfo(stream_buffer, instance.UniformMinAlignment(), info, num_mips);
    const VkPipeline pipeline = GetDetileImagePipeline(info, volume);

    scheduler.EndRendering();
    runtime.Transit(&image, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    VK_ACCESS_2_SHADER_WRITE_BIT, nullptr);
    runtime.FlushBarriers();

    const VkCommandBuffer cmdbuf = scheduler.CommandBuffer();
    vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    VkDescriptorBufferInfo buffer_infos[2]{};
    buffer_infos[0].buffer = in_buffer->Handle();
    buffer_infos[0].offset = in_offset;
    buffer_infos[0].range = info.guest_size;
    buffer_infos[1].buffer = stream_buffer.Handle();
    buffer_infos[1].offset = params_offset;
    buffer_infos[1].range = sizeof(TilingInfo);
    VkDescriptorImageInfo image_info{};
    image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    VkWriteDescriptorSet writes[3]{};
    for (u32 i = 0; i < 3; ++i) {
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstBinding = i;
        writes[i].descriptorCount = 1;
    }
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[0].pBufferInfo = &buffer_infos[0];
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[1].pImageInfo = &image_info;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[2].pBufferInfo = &buffer_infos[1];

    instance.Mark(cmdbuf, "detile into image: image at, size, mode|bits<<8|mips<<16, w|h<<16|d<<32",
                  info.guest_address, info.guest_size,
                  u64(info.tile_mode) | u64(info.num_bits) << 8 | u64(num_mips) << 16,
                  u64(info.size.width) | u64(info.size.height) << 16 | u64(info.size.depth) << 32);
    const u32 texel_bytes = info.num_bits / 8;
    for (u32 mip = 0; mip < num_mips; ++mip) {
        const u32 texels = info.mips_layout[mip].size / texel_bytes;
        if (texels == 0) {
            continue;
        }
        image_info.imageView = views[mip];
        // Push descriptors are pushed again for each mip: only the view changes
        vkCmdPushDescriptorSetKHR(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, image_pl_layout, 0, 3,
                                  writes);
        DetileImagePush push;
        push.mip = mip;
        push.width = Common::Max(info.size.width >> mip, 1u);
        push.height = Common::Max(info.size.height >> mip, 1u);
        push.depth = volume ? Common::Max(info.size.depth >> mip, 1u) : num_layers;
        vkCmdPushConstants(cmdbuf, image_pl_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push),
                           &push);
        vkCmdDispatch(cmdbuf, (texels + 63) / 64, 1, 1);
    }

    image.flags &= ~ImageFlagBits::Dirty;
    return true;
}

void TileManager::TileImage(Image& in_image, VkBufferImageCopy* buffer_copies, u32 num_copies,
                            const VideoCore::Buffer* out_buffer, u64 out_offset) {
    const ImageInfo& info = in_image.info;
    if (!info.props.is_tiled) {
        for (u32 i = 0; i < num_copies; ++i) {
            buffer_copies[i].bufferOffset += out_offset;
        }
        runtime.DownloadImage(&in_image, out_buffer, buffer_copies, num_copies);
        return;
    }

    const u32 num_mips = num_copies;
    const u64 params_offset =
        WriteTilingInfo(stream_buffer, instance.UniformMinAlignment(), info, num_mips);

    const Vulkan::StagingBufferRef staging = runtime.GetStagingPool().Request(
        info.guest_size, MemoryType::DeviceLocal, 256, false, true);
    for (u32 i = 0; i < num_copies; ++i) {
        buffer_copies[i].bufferOffset += staging.offset;
    }

    runtime.DownloadImage(&in_image, staging.buffer, buffer_copies, num_copies);
    runtime.FlushBarriers();

    const VkCommandBuffer cmdbuf = scheduler.CommandBuffer();
    BindTiling(cmdbuf, GetTilingPipeline(info, true), pl_layout, out_buffer->Handle(), out_offset,
               staging.buffer->Handle(), staging.offset, info.guest_size, stream_buffer.Handle(),
               params_offset, sizeof(TilingInfo));

    const u32 dim_x = (info.guest_size / (info.num_bits / 8)) / 64;
    instance.Mark(cmdbuf, "tile: image at, size, mode|bits<<8|mips<<16, w|h<<16|d<<32",
                  info.guest_address, info.guest_size,
                  u64(info.tile_mode) | u64(info.num_bits) << 8 | u64(num_mips) << 16,
                  u64(info.size.width) | u64(info.size.height) << 16 | u64(info.size.depth) << 32);
    vkCmdDispatch(cmdbuf, dim_x, 1, 1);

    runtime.AccessBuffer(staging.buffer, staging.offset, info.guest_size,
                         VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);
    runtime.AccessBuffer(out_buffer, out_offset, info.guest_size,
                         VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);
}

} // namespace VideoCore
