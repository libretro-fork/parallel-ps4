// SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdlib.h>

#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_shader_util.h"
#include "video_core/texture_cache/blit_helper.h"
#include "video_core/texture_cache/image.h"

namespace VideoCore {

static VkSampleCountFlagBits ToSampleCount(u32 num_samples) {
    switch (num_samples) {
    case 1:
        return VK_SAMPLE_COUNT_1_BIT;
    case 2:
        return VK_SAMPLE_COUNT_2_BIT;
    case 4:
        return VK_SAMPLE_COUNT_4_BIT;
    case 8:
        return VK_SAMPLE_COUNT_8_BIT;
    case 16:
        return VK_SAMPLE_COUNT_16_BIT;
    default:
        UNREACHABLE_MSG("Unknown samples count = %u", num_samples);
    }
}

template <typename Key>
void BlitHelper::PipelineList<Key>::Add(const Key& key, VkPipeline pipeline) {
    if (count == room) {
        const u32 grown_room = room ? room * 2 : 4;
        Entry* const grown = static_cast<Entry*>(realloc(entries, grown_room * sizeof(Entry)));
        ASSERT_MSG(grown, "Out of memory for the blit pipelines");
        entries = grown;
        room = grown_room;
    }
    entries[count].key = key;
    entries[count].pipeline = pipeline;
    ++count;
}

template <typename Key>
void BlitHelper::PipelineList<Key>::Destroy(VkDevice device) {
    for (u32 i = 0; i < count; ++i) {
        vkDestroyPipeline(device, entries[i].pipeline, nullptr);
    }
    free(entries);
    entries = nullptr;
    count = 0;
    room = 0;
}

BlitHelper::BlitHelper(const Vulkan::Instance& instance_, Vulkan::Scheduler& scheduler_)
    : instance{instance_}, scheduler{scheduler_}, device{instance_.GetDevice()} {
    CreateShaders();
    CreatePipelineLayouts();
}

BlitHelper::~BlitHelper() {
    vkDestroyShaderModule(device, fs_tri_vertex, nullptr);
    vkDestroyShaderModule(device, color_to_ms_depth_frag, nullptr);
    vkDestroyShaderModule(device, src_msaa_copy_frag, nullptr);
    vkDestroyShaderModule(device, src_non_msaa_copy_frag, nullptr);
    depth_to_color_pl.Destroy(device);
    ms_image_copy_pl.Destroy(device);
    color_to_ms_depth_pl.Destroy(device);
    vkDestroyPipelineLayout(device, single_texture_pl_layout, nullptr);
    vkDestroyDescriptorSetLayout(device, single_texture_descriptor_set_layout, nullptr);
}

VkImageView BlitHelper::TransientView(VkImage image, VkFormat format, VkImageAspectFlags aspect,
                                      VkImageUsageFlags usage, u32 layer) {
    VkImageViewUsageCreateInfo usage_ci{};
    usage_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO;
    usage_ci.usage = usage;
    VkImageViewCreateInfo view_ci{};
    view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_ci.pNext = &usage_ci;
    view_ci.image = image;
    view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_ci.format = format;
    view_ci.subresourceRange.aspectMask = aspect;
    view_ci.subresourceRange.baseMipLevel = 0U;
    view_ci.subresourceRange.levelCount = 1U;
    view_ci.subresourceRange.baseArrayLayer = layer;
    view_ci.subresourceRange.layerCount = 1U;
    VkImageView view = VK_NULL_HANDLE;
    const VkResult result = vkCreateImageView(device, &view_ci, nullptr, &view);
    ASSERT_MSG(result == VK_SUCCESS, "Failed to create image view: %s", Vulkan::ResultName(result));
    scheduler.DeferOperation(
        [device = device, view] { vkDestroyImageView(device, view, nullptr); });
    return view;
}

void BlitHelper::Draw(const Vulkan::RenderState& state, VkPipeline pipeline, VkImageView src_view) {
    scheduler.BeginRendering(state);

    const VkCommandBuffer cmdbuf = scheduler.CommandBuffer();
    VkDescriptorImageInfo image_info{};
    image_info.sampler = VK_NULL_HANDLE;
    image_info.imageView = src_view;
    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkWriteDescriptorSet texture_write{};
    texture_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    texture_write.dstSet = VK_NULL_HANDLE;
    texture_write.dstBinding = 0U;
    texture_write.dstArrayElement = 0U;
    texture_write.descriptorCount = 1U;
    texture_write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    texture_write.pImageInfo = &image_info;
    vkCmdPushDescriptorSetKHR(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, single_texture_pl_layout, 0U,
                              1U, &texture_write);
    vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    const VkViewport viewport = {0.f, 0.f, float(state.width), float(state.height), 0.f, 1.f};
    vkCmdSetViewportWithCount(cmdbuf, 1U, &viewport);
    const VkRect2D scissor = {{0, 0}, {state.width, state.height}};
    vkCmdSetScissorWithCount(cmdbuf, 1U, &scissor);

    vkCmdDraw(cmdbuf, 3, 1, 0, 0);

    scheduler.EndRendering();
    scheduler.GetDynamicState().Invalidate();
}

void BlitHelper::ReinterpretColorAsMsDepth(u32 width, u32 height, u32 num_samples,
                                           VkFormat src_pixel_format, VkFormat dst_pixel_format,
                                           VkImage source, VkImage dest) {
    const VkImageView color_view = TransientView(
        source, src_pixel_format, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_USAGE_SAMPLED_BIT, 0U);
    const VkImageView depth_view = TransientView(dest, dst_pixel_format, VK_IMAGE_ASPECT_DEPTH_BIT,
                                                 VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 0U);

    Vulkan::RenderState state{};
    state.width = width;
    state.height = height;
    state.num_layers = 1;
    state.depth_stencil_attachment.image_view = depth_view;
    state.depth_stencil_attachment.image_layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    state.depth_stencil_attachment.has_depth = true;
    state.depth_stencil_attachment.depth_clear = true;

    const MsPipelineKey key{num_samples, dst_pixel_format, false};
    VkPipeline pipeline = color_to_ms_depth_pl.Find(key);
    if (pipeline == VK_NULL_HANDLE) {
        pipeline = CreateColorToMSDepthPipeline(key);
        color_to_ms_depth_pl.Add(key, pipeline);
    }
    Draw(state, pipeline, color_view);
}

void BlitHelper::CopyBetweenMsImages(u32 width, u32 height, u32 num_samples, VkFormat pixel_format,
                                     bool src_msaa, VkImage source, VkImage dest) {
    const VkImageView src_view = TransientView(source, pixel_format, VK_IMAGE_ASPECT_COLOR_BIT,
                                               VK_IMAGE_USAGE_SAMPLED_BIT, 0U);
    const VkImageView dst_view = TransientView(dest, pixel_format, VK_IMAGE_ASPECT_COLOR_BIT,
                                               VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, 0U);

    Vulkan::RenderState state{};
    state.width = width;
    state.height = height;
    state.num_layers = 1;
    state.num_color_attachments = 1;
    state.color_attachments[0].image_view = dst_view;
    state.color_attachments[0].image_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    state.color_attachments[0].is_clear = true;

    const MsPipelineKey key{num_samples, pixel_format, src_msaa};
    VkPipeline pipeline = ms_image_copy_pl.Find(key);
    if (pipeline == VK_NULL_HANDLE) {
        pipeline = CreateMsCopyPipeline(key);
        ms_image_copy_pl.Add(key, pipeline);
    }
    Draw(state, pipeline, src_view);
}

void BlitHelper::CopyDepthToColor(u32 width, u32 height, u32 dst_samples, VkFormat src_format,
                                  VkFormat dst_format, bool src_msaa, bool stencil, bool uint_out,
                                  VkColorComponentFlags write_mask, u32 sample, VkImage source,
                                  u32 src_layer, VkImage dest, u32 dst_layer) {
    const VkImageView src_view = TransientView(
        source, src_format, stencil ? VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_DEPTH_BIT,
        VK_IMAGE_USAGE_SAMPLED_BIT, src_layer);
    const VkImageView dst_view = TransientView(dest, dst_format, VK_IMAGE_ASPECT_COLOR_BIT,
                                               VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, dst_layer);

    Vulkan::RenderState state{};
    state.width = width;
    state.height = height;
    state.num_layers = 1;
    state.num_color_attachments = 1;
    state.color_attachments[0].image_view = dst_view;
    state.color_attachments[0].image_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    const DepthToColorKey key{dst_samples, dst_format, sample,  static_cast<u32>(write_mask),
                              src_msaa,    stencil,    uint_out};
    VkPipeline pipeline = depth_to_color_pl.Find(key);
    if (pipeline == VK_NULL_HANDLE) {
        pipeline = CreateDepthToColorPipeline(key);
        depth_to_color_pl.Add(key, pipeline);
    }
    Draw(state, pipeline, src_view);
}

void BlitHelper::CreateShaders() {
    const VkDevice vk_device{device};
    fs_tri_vertex = static_cast<VkShaderModule>(
        Vulkan::CompileSPV(words, host_spirv_fs_tri_vert(words, HOST_SPIRV_MAX_WORDS), vk_device));
    color_to_ms_depth_frag = static_cast<VkShaderModule>(Vulkan::CompileSPV(
        words, host_spirv_color_to_ms_depth_frag(words, HOST_SPIRV_MAX_WORDS), vk_device));
    src_msaa_copy_frag = static_cast<VkShaderModule>(Vulkan::CompileSPV(
        words, host_spirv_ms_image_blit_frag(words, HOST_SPIRV_MAX_WORDS, 1), vk_device));
    src_non_msaa_copy_frag = static_cast<VkShaderModule>(Vulkan::CompileSPV(
        words, host_spirv_ms_image_blit_frag(words, HOST_SPIRV_MAX_WORDS, 0), vk_device));
}

void BlitHelper::CreatePipelineLayouts() {
    VkDescriptorSetLayoutBinding texture_binding{};
    texture_binding.binding = 0;
    texture_binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    texture_binding.descriptorCount = 1;
    texture_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo desc_layout_ci{};
    desc_layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    desc_layout_ci.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
    desc_layout_ci.bindingCount = 1U;
    desc_layout_ci.pBindings = &texture_binding;
    const VkResult desc_layout_result = vkCreateDescriptorSetLayout(
        device, &desc_layout_ci, nullptr, &single_texture_descriptor_set_layout);
    ASSERT_MSG(desc_layout_result == VK_SUCCESS, "Failed to create descriptor set layout: %s",
               Vulkan::ResultName(desc_layout_result));

    VkPipelineLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.setLayoutCount = 1U;
    layout_info.pSetLayouts = &single_texture_descriptor_set_layout;
    layout_info.pushConstantRangeCount = 0U;
    layout_info.pPushConstantRanges = nullptr;
    const VkResult layout_result =
        vkCreatePipelineLayout(device, &layout_info, nullptr, &single_texture_pl_layout);
    ASSERT_MSG(layout_result == VK_SUCCESS, "Failed to create graphics pipeline layout: %s",
               Vulkan::ResultName(layout_result));
    Vulkan::SetObjectName(instance.GetDevice(), single_texture_pl_layout,
                          "Single texture pipeline layout");
}

VkPipeline BlitHelper::CreateBlitPipeline(VkShaderModule frag, u32 num_samples,
                                          VkFormat color_format, VkColorComponentFlags write_mask,
                                          VkFormat depth_format) {
    const bool has_depth = depth_format != VK_FORMAT_UNDEFINED;
    VkPipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = ToSampleCount(num_samples);
    VkPipelineDepthStencilStateCreateInfo depth_state{};
    depth_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_state.depthTestEnable = has_depth;
    depth_state.depthWriteEnable = has_depth;
    depth_state.depthCompareOp = VK_COMPARE_OP_ALWAYS;
    const VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT,
                                             VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT};
    VkPipelineDynamicStateCreateInfo dynamic_info{};
    dynamic_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_info.dynamicStateCount = 2U;
    dynamic_info.pDynamicStates = dynamic_states;

    VkPipelineShaderStageCreateInfo shader_stages[2]{};
    shader_stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shader_stages[0].module = fs_tri_vertex;
    shader_stages[0].pName = "main";
    shader_stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shader_stages[1].module = frag;
    shader_stages[1].pName = "main";

    VkPipelineRenderingCreateInfo pipeline_rendering_ci{};
    pipeline_rendering_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    pipeline_rendering_ci.colorAttachmentCount = has_depth ? 0U : 1U;
    pipeline_rendering_ci.pColorAttachmentFormats = has_depth ? nullptr : &color_format;
    pipeline_rendering_ci.depthAttachmentFormat = depth_format;
    pipeline_rendering_ci.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

    VkPipelineColorBlendAttachmentState attachment{};
    attachment.blendEnable = VK_FALSE;
    attachment.colorWriteMask = write_mask;
    VkPipelineColorBlendStateCreateInfo color_blending{};
    color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    if (!has_depth) {
        color_blending.logicOpEnable = VK_FALSE;
        color_blending.logicOp = VK_LOGIC_OP_COPY;
        color_blending.attachmentCount = 1u;
        color_blending.pAttachments = &attachment;
    }
    VkPipelineViewportStateCreateInfo viewport_info{};
    viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    VkPipelineVertexInputStateCreateInfo vertex_input_info{};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    VkPipelineRasterizationStateCreateInfo raster_state{};
    raster_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster_state.lineWidth = 1.f;

    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.pNext = &pipeline_rendering_ci;
    pipeline_info.stageCount = 2U;
    pipeline_info.pStages = shader_stages;
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_info;
    pipeline_info.pRasterizationState = &raster_state;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pDepthStencilState = &depth_state;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.pDynamicState = &dynamic_info;
    pipeline_info.layout = single_texture_pl_layout;

    VkPipeline pipeline = VK_NULL_HANDLE;
    const VkResult result =
        vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline);
    ASSERT_MSG(result == VK_SUCCESS, "Failed to create graphics pipeline: %s",
               Vulkan::ResultName(result));
    return pipeline;
}

VkPipeline BlitHelper::CreateColorToMSDepthPipeline(const MsPipelineKey& key) {
    const VkPipeline pipeline = CreateBlitPipeline(color_to_ms_depth_frag, key.num_samples,
                                                   VK_FORMAT_UNDEFINED, 0, key.attachment_format);
    Vulkan::SetObjectName(instance.GetDevice(), pipeline, "Color to MS Depth %u", key.num_samples);
    return pipeline;
}

VkPipeline BlitHelper::CreateMsCopyPipeline(const MsPipelineKey& key) {
    const VkPipeline pipeline =
        CreateBlitPipeline(key.src_msaa ? src_msaa_copy_frag : src_non_msaa_copy_frag,
                           key.num_samples, key.attachment_format,
                           VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                               VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
                           VK_FORMAT_UNDEFINED);
    Vulkan::SetObjectName(instance.GetDevice(), pipeline, "Non MS Image to MS Image %u",
                          key.num_samples);
    return pipeline;
}

VkPipeline BlitHelper::CreateDepthToColorPipeline(const DepthToColorKey& key) {
    const size_t num_words = host_spirv_depth_to_color_frag(
        words, HOST_SPIRV_MAX_WORDS, key.src_msaa, key.stencil, key.uint_out, key.sample);
    const VkShaderModule frag =
        static_cast<VkShaderModule>(Vulkan::CompileSPV(words, num_words, VkDevice{device}));
    const VkPipeline pipeline = CreateBlitPipeline(frag, key.dst_samples, key.attachment_format,
                                                   key.write_mask, VK_FORMAT_UNDEFINED);
    Vulkan::SetObjectName(instance.GetDevice(), pipeline, "Depth plane to colour %u",
                          key.dst_samples);
    vkDestroyShaderModule(device, frag, nullptr);
    return pipeline;
}

} // namespace VideoCore
