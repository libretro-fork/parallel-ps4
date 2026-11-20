// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/string_util.h"
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

struct TilingInfo {
    u32 bank_swizzle;
    u32 micro_tiled_mips;
    u32 num_mips;
    std::array<MipInfo, MAX_MIPS> mips;
};

TileManager::TileManager(const Vulkan::Instance& instance_, Vulkan::Scheduler& scheduler_,
                         Vulkan::Runtime& runtime_, StreamBuffer& stream_buffer_)
    : instance{instance_}, scheduler{scheduler_}, runtime{runtime_}, stream_buffer{stream_buffer_} {
    const auto device = static_cast<VkDevice>(instance.GetDevice());
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
    const auto device = static_cast<VkDevice>(instance.GetDevice());
    for (const auto& [key, pipeline] : tiling_pipelines) {
        vkDestroyPipeline(device, pipeline, nullptr);
    }
    vkDestroyPipelineLayout(device, pl_layout, nullptr);
    vkDestroyDescriptorSetLayout(device, desc_layout, nullptr);
}

namespace {

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
    const TilingKey key{
        .tile_mode = info.tile_mode,
        .num_bits = info.num_bits,
        .num_samples = info.num_samples,
        .is_tiler = is_tiler,
    };
    if (const auto it = tiling_pipelines.find(key); it != tiling_pipelines.end()) {
        return it->second;
    }

    const auto device = instance.GetDevice();
    const auto micro_tile_mode = AmdGpu::GetMicroTileMode(info.tile_mode);
    const bool is_macro = AmdGpu::IsMacroTiled(info.array_mode);
    // The image's tiling parameters, folded into the shader
    std::array<u32, 12> params{
        info.num_samples,
        u32(micro_tile_mode),
        AmdGpu::GetMicroTileThickness(info.array_mode),
        u32(is_tiler),
    };
    if (is_macro) {
        const auto macro_tile_mode =
            AmdGpu::CalculateMacrotileMode(info.tile_mode, info.num_bits, info.num_samples);
        params[4] = u32(info.array_mode);
        params[5] = u32(AmdGpu::GetPipeConfig(info.tile_mode));
        params[6] = AmdGpu::GetBankWidth(macro_tile_mode);
        params[7] = AmdGpu::GetBankHeight(macro_tile_mode);
        params[8] = AmdGpu::GetNumBanks(macro_tile_mode);
        params[9] = std::bit_width(params[8]) - 1;
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
        host_spirv_tiling_comp(spirv_words.data(), spirv_words.size(), info.num_bits, is_macro,
                               word_access, params.data(), is_macro ? 12 : 4);
    ASSERT_MSG(num_words != 0, "Tiling shader does not fit");
    const auto module = Vulkan::CompileSPV({spirv_words.data(), num_words}, device);

    const auto module_name = Common::StringFromFormat(
        "%s_%u %s", enum_name(&enum_names_AmdGpu_TileMode, static_cast<s64>(info.tile_mode)),
        info.num_bits, is_tiler ? "tiler" : "detiler");
    LOG_INFO(Render_Vulkan, "Creating tiling pipeline %s", (module_name).c_str());
    Vulkan::SetObjectName(device, module, module_name);

    VkComputePipelineCreateInfo compute_pipeline_ci{};
    compute_pipeline_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    compute_pipeline_ci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    compute_pipeline_ci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    compute_pipeline_ci.stage.module = static_cast<VkShaderModule>(module);
    compute_pipeline_ci.stage.pName = "main";
    compute_pipeline_ci.layout = pl_layout;

    const auto raw_device = static_cast<VkDevice>(device);
    VkPipeline handle = VK_NULL_HANDLE;
    const VkResult result = vkCreateComputePipelines(raw_device, VK_NULL_HANDLE, 1,
                                                     &compute_pipeline_ci, nullptr, &handle);
    ASSERT_MSG(result == VK_SUCCESS, "Detiler pipeline creation failed %s",
               vk::to_string(static_cast<vk::Result>(result)).c_str());
    tiling_pipelines.emplace(key, handle);
    vkDestroyShaderModule(raw_device, static_cast<VkShaderModule>(module), nullptr);
    return handle;
}

std::pair<const Buffer*, u64> TileManager::DetileImage(const VideoCore::Buffer* in_buffer,
                                                       u64 in_offset, const ImageInfo& info) {
    if (!info.props.is_tiled) {
        return {in_buffer, in_offset};
    }

    TilingInfo params{};
    params.bank_swizzle = info.bank_swizzle;
    params.micro_tiled_mips = info.micro_tiled_mips;
    params.num_mips = info.resources.levels;
    for (u32 mip = 0; mip < params.num_mips; ++mip) {
        auto& mip_info = params.mips[mip];
        mip_info = info.mips_layout[mip];
        if (info.props.is_block) {
            mip_info.pitch = std::max<u16>((mip_info.pitch + 3) / 4, 1U);
            mip_info.height = std::max<u16>((mip_info.height + 3) / 4, 1U);
        }
    }

    const u64 params_offset =
        stream_buffer.Copy(&params, sizeof(params), instance.UniformMinAlignment());

    const auto staging = runtime.GetStagingPool().Request(info.guest_size, MemoryType::DeviceLocal,
                                                          256, false, true);

    scheduler.EndRendering();
    runtime.FlushBarriers();

    const auto cmdbuf = scheduler.CommandBuffer();
    BindTiling(static_cast<VkCommandBuffer>(cmdbuf), GetTilingPipeline(info, false), pl_layout,
               static_cast<VkBuffer>(in_buffer->Handle()), in_offset,
               static_cast<VkBuffer>(staging.buffer->Handle()), staging.offset, info.guest_size,
               static_cast<VkBuffer>(stream_buffer.Handle()), params_offset, sizeof(params));

    const auto dim_x = (info.guest_size / (info.num_bits / 8)) / 64;
    instance.Mark(cmdbuf, "detile: image at, size, mode|bits<<8|mips<<16, w|h<<16|d<<32",
                  info.guest_address, info.guest_size,
                  u64(info.tile_mode) | u64(info.num_bits) << 8 | u64(info.resources.levels) << 16,
                  u64(info.size.width) | u64(info.size.height) << 16 | u64(info.size.depth) << 32);
    vkCmdDispatch(static_cast<VkCommandBuffer>(cmdbuf), dim_x, 1, 1);

    runtime.AccessBuffer(staging.buffer, staging.offset, info.guest_size,
                         vk::PipelineStageFlagBits2::eComputeShader,
                         vk::AccessFlagBits2::eShaderWrite);

    return {staging.buffer, staging.offset};
}

void TileManager::TileImage(Image& in_image, std::span<vk::BufferImageCopy> buffer_copies,
                            const VideoCore::Buffer* out_buffer, u64 out_offset) {
    const auto& info = in_image.info;
    if (!info.props.is_tiled) {
        for (auto& copy : buffer_copies) {
            copy.bufferOffset += out_offset;
        }
        runtime.DownloadImage(&in_image, out_buffer, buffer_copies);
        return;
    }

    TilingInfo params{};
    params.bank_swizzle = info.bank_swizzle;
    params.micro_tiled_mips = info.micro_tiled_mips;
    params.num_mips = static_cast<u32>(buffer_copies.size());
    for (u32 mip = 0; mip < params.num_mips; ++mip) {
        auto& mip_info = params.mips[mip];
        mip_info = info.mips_layout[mip];
        if (info.props.is_block) {
            mip_info.pitch = std::max<u16>((mip_info.pitch + 3) / 4, 1U);
            mip_info.height = std::max<u16>((mip_info.height + 3) / 4, 1U);
        }
    }

    const u64 params_offset =
        stream_buffer.Copy(&params, sizeof(params), instance.UniformMinAlignment());

    const auto staging = runtime.GetStagingPool().Request(info.guest_size, MemoryType::DeviceLocal,
                                                          256, false, true);
    for (auto& copy : buffer_copies) {
        copy.bufferOffset += staging.offset;
    }

    runtime.DownloadImage(&in_image, staging.buffer, buffer_copies);
    runtime.FlushBarriers();

    const auto cmdbuf = scheduler.CommandBuffer();
    BindTiling(static_cast<VkCommandBuffer>(cmdbuf), GetTilingPipeline(info, true), pl_layout,
               static_cast<VkBuffer>(out_buffer->Handle()), out_offset,
               static_cast<VkBuffer>(staging.buffer->Handle()), staging.offset, info.guest_size,
               static_cast<VkBuffer>(stream_buffer.Handle()), params_offset, sizeof(params));

    const auto dim_x = (info.guest_size / (info.num_bits / 8)) / 64;
    instance.Mark(cmdbuf, "tile: image at, size, mode|bits<<8|mips<<16, w|h<<16|d<<32",
                  info.guest_address, info.guest_size,
                  u64(info.tile_mode) | u64(info.num_bits) << 8 | u64(params.num_mips) << 16,
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
