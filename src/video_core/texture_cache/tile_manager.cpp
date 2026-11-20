// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdio.h>
#include <string.h>

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
    const VkDevice device = static_cast<VkDevice>(instance.GetDevice());
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
               vk::to_string(static_cast<vk::Result>(desc_layout_result)).c_str());

    VkPipelineLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.setLayoutCount = 1U;
    layout_info.pSetLayouts = &desc_layout;
    const VkResult layout_result =
        vkCreatePipelineLayout(device, &layout_info, nullptr, &pl_layout);
    ASSERT_MSG(layout_result == VK_SUCCESS, "Failed to create pipeline layout: %s",
               vk::to_string(static_cast<vk::Result>(layout_result)).c_str());
}

TileManager::~TileManager() {
    const VkDevice device = static_cast<VkDevice>(instance.GetDevice());
    size_t pos = 0;
    while (const VkPipeline* pipeline =
               static_cast<const VkPipeline*>(u64_map_next(&tiling_pipelines, &pos, nullptr))) {
        vkDestroyPipeline(device, *pipeline, nullptr);
    }
    u64_map_free(&tiling_pipelines);
    vkDestroyPipelineLayout(device, pl_layout, nullptr);
    vkDestroyDescriptorSetLayout(device, desc_layout, nullptr);
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

    const vk::Device device = instance.GetDevice();
    const AmdGpu::MicroTileMode micro_tile_mode = AmdGpu::GetMicroTileMode(info.tile_mode);
    const bool is_macro = AmdGpu::IsMacroTiled(info.array_mode);
    // The image's tiling parameters, folded into the shader
    u32 params[12] = {
        info.num_samples,
        u32(micro_tile_mode),
        AmdGpu::GetMicroTileThickness(info.array_mode),
        u32(is_tiler),
    };
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
    const vk::ShaderModule module = Vulkan::CompileSPV(spirv_words, num_words, device);

    char module_name[96];
    snprintf(module_name, sizeof(module_name), "%s_%u %s",
             enum_name(&enum_names_AmdGpu_TileMode, static_cast<s64>(info.tile_mode)),
             info.num_bits, is_tiler ? "tiler" : "detiler");
    LOG_INFO(Render_Vulkan, "Creating tiling pipeline %s", module_name);
    Vulkan::SetObjectName(device, module, "%s", module_name);

    VkComputePipelineCreateInfo compute_pipeline_ci{};
    compute_pipeline_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    compute_pipeline_ci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    compute_pipeline_ci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    compute_pipeline_ci.stage.module = static_cast<VkShaderModule>(module);
    compute_pipeline_ci.stage.pName = "main";
    compute_pipeline_ci.layout = pl_layout;

    const VkDevice raw_device = static_cast<VkDevice>(device);
    VkPipeline handle = VK_NULL_HANDLE;
    const VkResult result = vkCreateComputePipelines(raw_device, VK_NULL_HANDLE, 1,
                                                     &compute_pipeline_ci, nullptr, &handle);
    ASSERT_MSG(result == VK_SUCCESS, "Detiler pipeline creation failed %s",
               vk::to_string(static_cast<vk::Result>(result)).c_str());
    *static_cast<VkPipeline*>(u64_map_insert(&tiling_pipelines, key, nullptr)) = handle;
    vkDestroyShaderModule(raw_device, static_cast<VkShaderModule>(module), nullptr);
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

    const vk::CommandBuffer cmdbuf = scheduler.CommandBuffer();
    BindTiling(static_cast<VkCommandBuffer>(cmdbuf), GetTilingPipeline(info, false), pl_layout,
               static_cast<VkBuffer>(in_buffer->Handle()), in_offset,
               static_cast<VkBuffer>(staging.buffer->Handle()), staging.offset, info.guest_size,
               static_cast<VkBuffer>(stream_buffer.Handle()), params_offset, sizeof(TilingInfo));

    const u32 dim_x = (info.guest_size / (info.num_bits / 8)) / 64;
    instance.Mark(cmdbuf, "detile: image at, size, mode|bits<<8|mips<<16, w|h<<16|d<<32",
                  info.guest_address, info.guest_size,
                  u64(info.tile_mode) | u64(info.num_bits) << 8 | u64(info.resources.levels) << 16,
                  u64(info.size.width) | u64(info.size.height) << 16 | u64(info.size.depth) << 32);
    vkCmdDispatch(static_cast<VkCommandBuffer>(cmdbuf), dim_x, 1, 1);

    runtime.AccessBuffer(staging.buffer, staging.offset, info.guest_size,
                         vk::PipelineStageFlagBits2::eComputeShader,
                         vk::AccessFlagBits2::eShaderWrite);

    *out_offset = staging.offset;
    return staging.buffer;
}

void TileManager::TileImage(Image& in_image, vk::BufferImageCopy* buffer_copies, u32 num_copies,
                            const VideoCore::Buffer* out_buffer, u64 out_offset) {
    const ImageInfo& info = in_image.info;
    if (!info.props.is_tiled) {
        for (u32 i = 0; i < num_copies; ++i) {
            buffer_copies[i].bufferOffset += out_offset;
        }
        runtime.DownloadImage(&in_image, out_buffer, {buffer_copies, num_copies});
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

    runtime.DownloadImage(&in_image, staging.buffer, {buffer_copies, num_copies});
    runtime.FlushBarriers();

    const vk::CommandBuffer cmdbuf = scheduler.CommandBuffer();
    BindTiling(static_cast<VkCommandBuffer>(cmdbuf), GetTilingPipeline(info, true), pl_layout,
               static_cast<VkBuffer>(out_buffer->Handle()), out_offset,
               static_cast<VkBuffer>(staging.buffer->Handle()), staging.offset, info.guest_size,
               static_cast<VkBuffer>(stream_buffer.Handle()), params_offset, sizeof(TilingInfo));

    const u32 dim_x = (info.guest_size / (info.num_bits / 8)) / 64;
    instance.Mark(cmdbuf, "tile: image at, size, mode|bits<<8|mips<<16, w|h<<16|d<<32",
                  info.guest_address, info.guest_size,
                  u64(info.tile_mode) | u64(info.num_bits) << 8 | u64(num_mips) << 16,
                  u64(info.size.width) | u64(info.size.height) << 16 | u64(info.size.depth) << 32);
    vkCmdDispatch(static_cast<VkCommandBuffer>(cmdbuf), dim_x, 1, 1);

    runtime.AccessBuffer(staging.buffer, staging.offset, info.guest_size,
                         vk::PipelineStageFlagBits2::eComputeShader,
                         vk::AccessFlagBits2::eShaderRead);
    runtime.AccessBuffer(out_buffer, out_offset, info.guest_size,
                         vk::PipelineStageFlagBits2::eComputeShader,
                         vk::AccessFlagBits2::eShaderWrite);
}

} // namespace VideoCore
