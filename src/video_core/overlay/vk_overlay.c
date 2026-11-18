/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* ovl's frames onto Vulkan. The pipeline and its state are ImGui's Vulkan backend's
 * (imgui_impl_vulkan.cpp), so that what ovl draws comes out as ImGui drew it: the same vertex
 * and fragment shaders (host_spirv.c), blending, linear repeating sampler and scissors.
 *
 * Everything is made with the swapchain: the atlas, the images' slots, and for each of the
 * swapchain's images a staging buffer for the atlas and a vertex and an index buffer, so that
 * a frame allocates nothing (a frame drawing more than they hold grows them, once). A slot of
 * frame buffers comes round again once the frame that used it is done, as ImGui's backend
 * had it. Nothing waits on the GPU.
 *
 * The splash screen (ovl_splash.c) is drawn on its own, ahead of everything else, as the
 * picture in place of the game's: the image cleared to black and the splash over it. Its
 * memory is made with the rest, at its size, and freed once it is no longer shown. */

#include <string.h>

#include "core/emulator_settings.h"
#include "video_core/host_shaders/host_spirv.h"
#include "video_core/overlay/ovl.h"
#include "video_core/overlay/ovl_layers.h"
#include "video_core/overlay/vk_overlay.h"

/* What a frame can upload of the atlas: new glyphs a frame are a few kilobytes; more waits a
 * frame */
#define STAGING_BYTES (2u << 20)
/* A frame's vertices and indices, room for every overlay at once */
#define VERTEX_BYTES (65536u * sizeof(ovl_vertex_t))
#define INDEX_BYTES (98304u * sizeof(uint32_t))
#define MAX_FRAMES 8

typedef struct host_buffer {
    VkBuffer buffer;
    VkDeviceMemory memory;
    VkDeviceSize size;
    uint8_t* map;
} host_buffer_t;

typedef struct frame_buffers {
    host_buffer_t staging, vertex, index;
} frame_buffers_t;

/* An image's slot (ovl_tex.c): the memory it decodes into, lent to ovl, and its texture */
typedef struct tex_slot {
    host_buffer_t staging;
    VkImage image;
    VkDeviceMemory memory;
    VkImageView view;
    VkDescriptorSet set;
} tex_slot_t;

/* The splash: what it decodes into (freed once copied), its texture, and its quad, a frame's
 * four vertices each and the six indices after */
typedef struct splash {
    int made, uploaded, done;
    host_buffer_t staging, quad;
    VkImage image;
    VkDeviceMemory memory;
    VkImageView view;
    VkDescriptorSet set;
    uint32_t w, h;
    unsigned quad_index;
    unsigned upload_frame, retire_frame;
} splash_t;

#define SPLASH_QUADS_BYTES ((VkDeviceSize)MAX_FRAMES * 4 * sizeof(ovl_vertex_t))

#define FN(name) PFN_##name name
static struct {
    int made;
    VkDevice device;
    VkPhysicalDeviceMemoryProperties mem;
    VkImage atlas;
    VkDeviceMemory atlas_memory;
    VkImageView atlas_view;
    int atlas_sampled; /* in the shader read layout, after its first upload */
    VkSampler sampler;
    VkDescriptorPool pool;
    VkDescriptorSetLayout set_layout;
    VkDescriptorSet atlas_set;
    VkPipelineLayout layout;
    VkPipeline pipeline;
    frame_buffers_t frames[MAX_FRAMES];
    unsigned frame_count, frame_index;
    unsigned frame; /* frames presented */
    tex_slot_t tex[OVL_TEX_SLOTS];
    tex_slot_t icons; /* the icon atlas */
    int icons_sampled;
    splash_t splash;
    const ovl_draw_data_t* draw_data;
    float display_w, display_h;

    FN(vkCreateBuffer);
    FN(vkDestroyBuffer);
    FN(vkGetBufferMemoryRequirements);
    FN(vkAllocateMemory);
    FN(vkFreeMemory);
    FN(vkBindBufferMemory);
    FN(vkMapMemory);
    FN(vkCreateImage);
    FN(vkDestroyImage);
    FN(vkGetImageMemoryRequirements);
    FN(vkBindImageMemory);
    FN(vkCreateImageView);
    FN(vkDestroyImageView);
    FN(vkCreateSampler);
    FN(vkDestroySampler);
    FN(vkCreateDescriptorPool);
    FN(vkDestroyDescriptorPool);
    FN(vkCreateDescriptorSetLayout);
    FN(vkDestroyDescriptorSetLayout);
    FN(vkCreatePipelineLayout);
    FN(vkDestroyPipelineLayout);
    FN(vkAllocateDescriptorSets);
    FN(vkUpdateDescriptorSets);
    FN(vkCreateShaderModule);
    FN(vkDestroyShaderModule);
    FN(vkCreateGraphicsPipelines);
    FN(vkDestroyPipeline);
    FN(vkCmdPipelineBarrier);
    FN(vkCmdClearColorImage);
    FN(vkCmdCopyBufferToImage);
    FN(vkCmdBeginRendering);
    FN(vkCmdEndRendering);
    FN(vkCmdBindPipeline);
    FN(vkCmdBindVertexBuffers);
    FN(vkCmdBindIndexBuffer);
    FN(vkCmdSetViewport);
    FN(vkCmdSetScissor);
    FN(vkCmdPushConstants);
    FN(vkCmdBindDescriptorSets);
    FN(vkCmdDrawIndexed);
    FN(vkCmdBeginDebugUtilsLabelEXT);
    FN(vkCmdEndDebugUtilsLabelEXT);
} vk;
#undef FN

static int load_functions(PFN_vkGetDeviceProcAddr get, VkDevice device) {
#define LOAD(name) vk.name = (PFN_##name)get(device, #name)
    LOAD(vkCreateBuffer);
    LOAD(vkDestroyBuffer);
    LOAD(vkGetBufferMemoryRequirements);
    LOAD(vkAllocateMemory);
    LOAD(vkFreeMemory);
    LOAD(vkBindBufferMemory);
    LOAD(vkMapMemory);
    LOAD(vkCreateImage);
    LOAD(vkDestroyImage);
    LOAD(vkGetImageMemoryRequirements);
    LOAD(vkBindImageMemory);
    LOAD(vkCreateImageView);
    LOAD(vkDestroyImageView);
    LOAD(vkCreateSampler);
    LOAD(vkDestroySampler);
    LOAD(vkCreateDescriptorPool);
    LOAD(vkDestroyDescriptorPool);
    LOAD(vkCreateDescriptorSetLayout);
    LOAD(vkDestroyDescriptorSetLayout);
    LOAD(vkCreatePipelineLayout);
    LOAD(vkDestroyPipelineLayout);
    LOAD(vkAllocateDescriptorSets);
    LOAD(vkUpdateDescriptorSets);
    LOAD(vkCreateShaderModule);
    LOAD(vkDestroyShaderModule);
    LOAD(vkCreateGraphicsPipelines);
    LOAD(vkDestroyPipeline);
    LOAD(vkCmdPipelineBarrier);
    LOAD(vkCmdClearColorImage);
    LOAD(vkCmdCopyBufferToImage);
    LOAD(vkCmdBeginRendering);
    LOAD(vkCmdEndRendering);
    LOAD(vkCmdBindPipeline);
    LOAD(vkCmdBindVertexBuffers);
    LOAD(vkCmdBindIndexBuffer);
    LOAD(vkCmdSetViewport);
    LOAD(vkCmdSetScissor);
    LOAD(vkCmdPushConstants);
    LOAD(vkCmdBindDescriptorSets);
    LOAD(vkCmdDrawIndexed);
    /* Optional: the debug labels, with VK_EXT_debug_utils */
    LOAD(vkCmdBeginDebugUtilsLabelEXT);
    LOAD(vkCmdEndDebugUtilsLabelEXT);
#undef LOAD
    if (!vk.vkCmdBeginRendering) {
        vk.vkCmdBeginRendering = (PFN_vkCmdBeginRendering)get(device, "vkCmdBeginRenderingKHR");
        vk.vkCmdEndRendering = (PFN_vkCmdEndRendering)get(device, "vkCmdEndRenderingKHR");
    }
    return vk.vkCreateBuffer && vk.vkCreateImage && vk.vkCreateGraphicsPipelines &&
           vk.vkCmdBeginRendering && vk.vkCmdEndRendering && vk.vkCmdDrawIndexed;
}

static uint32_t memory_type(VkMemoryPropertyFlags want, uint32_t type_bits) {
    uint32_t i;
    for (i = 0; i < vk.mem.memoryTypeCount; i++)
        if ((vk.mem.memoryTypes[i].propertyFlags & want) == want && (type_bits & (1u << i)))
            return i;
    return UINT32_MAX;
}

static VkDeviceMemory allocate(const VkMemoryRequirements* req, VkMemoryPropertyFlags want) {
    VkMemoryAllocateInfo ai;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    memset(&ai, 0, sizeof(ai));
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = req->size;
    ai.memoryTypeIndex = memory_type(want, req->memoryTypeBits);
    if (ai.memoryTypeIndex == UINT32_MAX ||
        vk.vkAllocateMemory(vk.device, &ai, NULL, &memory) != VK_SUCCESS)
        return VK_NULL_HANDLE;
    return memory;
}

/* A buffer the host writes, mapped for as long as it lives */
static int host_buffer_make(host_buffer_t* b, VkDeviceSize size, VkBufferUsageFlags usage) {
    VkBufferCreateInfo bi;
    VkMemoryRequirements req;
    void* map = NULL;
    memset(b, 0, sizeof(*b));
    memset(&bi, 0, sizeof(bi));
    bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size = size;
    bi.usage = usage;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vk.vkCreateBuffer(vk.device, &bi, NULL, &b->buffer) != VK_SUCCESS)
        return 0;
    vk.vkGetBufferMemoryRequirements(vk.device, b->buffer, &req);
    b->memory =
        allocate(&req, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (!b->memory || vk.vkBindBufferMemory(vk.device, b->buffer, b->memory, 0) != VK_SUCCESS ||
        vk.vkMapMemory(vk.device, b->memory, 0, VK_WHOLE_SIZE, 0, &map) != VK_SUCCESS)
        return 0;
    b->map = (uint8_t*)map;
    b->size = size;
    return 1;
}

static void host_buffer_free(host_buffer_t* b) {
    if (b->buffer)
        vk.vkDestroyBuffer(vk.device, b->buffer, NULL);
    if (b->memory)
        vk.vkFreeMemory(vk.device, b->memory, NULL);
    memset(b, 0, sizeof(*b));
}

/* A sampled image the GPU copies into, and its view */
static int image_make(VkFormat format, uint32_t width, uint32_t height, int swizzle, VkImage* image,
                      VkDeviceMemory* memory, VkImageView* view) {
    VkImageCreateInfo ii;
    VkImageViewCreateInfo vi;
    VkMemoryRequirements req;
    memset(&ii, 0, sizeof(ii));
    ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.format = format;
    ii.extent.width = width;
    ii.extent.height = height;
    ii.extent.depth = 1;
    ii.mipLevels = 1;
    ii.arrayLayers = 1;
    ii.samples = VK_SAMPLE_COUNT_1_BIT;
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vk.vkCreateImage(vk.device, &ii, NULL, image) != VK_SUCCESS)
        return 0;
    vk.vkGetImageMemoryRequirements(vk.device, *image, &req);
    *memory = allocate(&req, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (!*memory || vk.vkBindImageMemory(vk.device, *image, *memory, 0) != VK_SUCCESS)
        return 0;
    memset(&vi, 0, sizeof(vi));
    vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vi.image = *image;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = format;
    if (swizzle) {
        /* Coverage and the placed mask, sampled as (G, G, G, R): ImGui's white RGBA atlas */
        vi.components.r = VK_COMPONENT_SWIZZLE_G;
        vi.components.g = VK_COMPONENT_SWIZZLE_G;
        vi.components.b = VK_COMPONENT_SWIZZLE_G;
        vi.components.a = VK_COMPONENT_SWIZZLE_R;
    }
    vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vi.subresourceRange.levelCount = 1;
    vi.subresourceRange.layerCount = 1;
    return vk.vkCreateImageView(vk.device, &vi, NULL, view) == VK_SUCCESS;
}

static void image_free(VkImage* image, VkDeviceMemory* memory, VkImageView* view) {
    if (*view)
        vk.vkDestroyImageView(vk.device, *view, NULL);
    if (*image)
        vk.vkDestroyImage(vk.device, *image, NULL);
    if (*memory)
        vk.vkFreeMemory(vk.device, *memory, NULL);
    *view = VK_NULL_HANDLE;
    *image = VK_NULL_HANDLE;
    *memory = VK_NULL_HANDLE;
}

/* A descriptor set sampling @view */
static int set_make(VkImageView view, VkDescriptorSet* set) {
    VkDescriptorSetAllocateInfo ai;
    VkDescriptorImageInfo image_info;
    VkWriteDescriptorSet write;
    memset(&ai, 0, sizeof(ai));
    ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool = vk.pool;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts = &vk.set_layout;
    if (vk.vkAllocateDescriptorSets(vk.device, &ai, set) != VK_SUCCESS)
        return 0;
    memset(&image_info, 0, sizeof(image_info));
    image_info.sampler = vk.sampler;
    image_info.imageView = view;
    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    memset(&write, 0, sizeof(write));
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = *set;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &image_info;
    vk.vkUpdateDescriptorSets(vk.device, 1, &write, 0, NULL);
    return 1;
}

/* The sampler, set layout, pool and pipeline layout, as ImGui's backend made them */
static int descriptors_make(void) {
    VkSamplerCreateInfo sci;
    VkDescriptorPoolSize pool_size;
    VkDescriptorPoolCreateInfo dpi;
    VkDescriptorSetLayoutBinding binding;
    VkDescriptorSetLayoutCreateInfo sli;
    VkPushConstantRange push;
    VkPipelineLayoutCreateInfo pli;
    memset(&sci, 0, sizeof(sci));
    sci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sci.magFilter = VK_FILTER_LINEAR;
    sci.minFilter = VK_FILTER_LINEAR;
    sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sci.maxAnisotropy = 1.0f;
    sci.minLod = -1000.0f;
    sci.maxLod = 1000.0f;
    pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    /* The atlas, the slots and the splash */
    pool_size.descriptorCount = 3 + OVL_TEX_SLOTS;
    memset(&dpi, 0, sizeof(dpi));
    dpi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    dpi.maxSets = 3 + OVL_TEX_SLOTS;
    dpi.poolSizeCount = 1;
    dpi.pPoolSizes = &pool_size;
    memset(&binding, 0, sizeof(binding));
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    memset(&sli, 0, sizeof(sli));
    sli.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    sli.bindingCount = 1;
    sli.pBindings = &binding;
    push.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    push.offset = 0;
    push.size = sizeof(float) * 4;
    memset(&pli, 0, sizeof(pli));
    pli.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pli.setLayoutCount = 1;
    pli.pSetLayouts = &vk.set_layout;
    pli.pushConstantRangeCount = 1;
    pli.pPushConstantRanges = &push;
    return vk.vkCreateSampler(vk.device, &sci, NULL, &vk.sampler) == VK_SUCCESS &&
           vk.vkCreateDescriptorPool(vk.device, &dpi, NULL, &vk.pool) == VK_SUCCESS &&
           vk.vkCreateDescriptorSetLayout(vk.device, &sli, NULL, &vk.set_layout) == VK_SUCCESS &&
           vk.vkCreatePipelineLayout(vk.device, &pli, NULL, &vk.layout) == VK_SUCCESS;
}

static VkShaderModule shader_make(const uint32_t* words, size_t n) {
    VkShaderModuleCreateInfo mi;
    VkShaderModule module = VK_NULL_HANDLE;
    if (!n)
        return VK_NULL_HANDLE;
    memset(&mi, 0, sizeof(mi));
    mi.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    mi.codeSize = n * 4;
    mi.pCode = words;
    vk.vkCreateShaderModule(vk.device, &mi, NULL, &module);
    return module;
}

/* ImGui's backend's pipeline (CreatePipeline) for dynamic rendering into @format */
static int pipeline_make(VkFormat format) {
    static uint32_t words[HOST_SPIRV_MAX_WORDS];
    VkShaderModule vert, frag;
    VkPipelineShaderStageCreateInfo stages[2];
    VkVertexInputBindingDescription binding;
    VkVertexInputAttributeDescription attributes[3];
    VkPipelineVertexInputStateCreateInfo vertex_input;
    VkPipelineInputAssemblyStateCreateInfo assembly;
    VkPipelineViewportStateCreateInfo viewport_state;
    VkPipelineRasterizationStateCreateInfo raster;
    VkPipelineMultisampleStateCreateInfo multisample;
    VkPipelineDepthStencilStateCreateInfo depth;
    VkPipelineColorBlendAttachmentState blend_att;
    VkPipelineColorBlendStateCreateInfo blend;
    VkDynamicState dynamic_states[2];
    VkPipelineDynamicStateCreateInfo dynamic;
    VkPipelineRenderingCreateInfo rendering;
    VkGraphicsPipelineCreateInfo pi;
    VkResult result;
    /* HDR Output: every overlay as PQ, as ImGui's backend drew them */
    const float white = emu_settings_get_hdr_allowed()
                            ? (float)emu_settings_get_hdr_paper_white() / 10000.0f
                            : 0.0f;

    vert = shader_make(words, host_spirv_overlay_vert(words, HOST_SPIRV_MAX_WORDS));
    frag = shader_make(words, host_spirv_overlay_frag(words, HOST_SPIRV_MAX_WORDS, white));
    if (!vert || !frag)
        return 0;
    memset(stages, 0, sizeof(stages));
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag;
    stages[1].pName = "main";
    memset(&binding, 0, sizeof(binding));
    binding.stride = sizeof(ovl_vertex_t);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    memset(attributes, 0, sizeof(attributes));
    attributes[0].location = 0;
    attributes[0].format = VK_FORMAT_R32G32_SFLOAT;
    attributes[0].offset = (uint32_t)offsetof(ovl_vertex_t, x);
    attributes[1].location = 1;
    attributes[1].format = VK_FORMAT_R32G32_SFLOAT;
    attributes[1].offset = (uint32_t)offsetof(ovl_vertex_t, u);
    attributes[2].location = 2;
    attributes[2].format = VK_FORMAT_R8G8B8A8_UNORM;
    attributes[2].offset = (uint32_t)offsetof(ovl_vertex_t, col);
    memset(&vertex_input, 0, sizeof(vertex_input));
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.vertexBindingDescriptionCount = 1;
    vertex_input.pVertexBindingDescriptions = &binding;
    vertex_input.vertexAttributeDescriptionCount = 3;
    vertex_input.pVertexAttributeDescriptions = attributes;
    memset(&assembly, 0, sizeof(assembly));
    assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    memset(&viewport_state, 0, sizeof(viewport_state));
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;
    memset(&raster, 0, sizeof(raster));
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = VK_CULL_MODE_NONE;
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.lineWidth = 1.0f;
    memset(&multisample, 0, sizeof(multisample));
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    memset(&depth, 0, sizeof(depth));
    depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    memset(&blend_att, 0, sizeof(blend_att));
    blend_att.blendEnable = VK_TRUE;
    blend_att.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blend_att.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend_att.colorBlendOp = VK_BLEND_OP_ADD;
    blend_att.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blend_att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend_att.alphaBlendOp = VK_BLEND_OP_ADD;
    blend_att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                               VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    memset(&blend, 0, sizeof(blend));
    blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.attachmentCount = 1;
    blend.pAttachments = &blend_att;
    dynamic_states[0] = VK_DYNAMIC_STATE_VIEWPORT;
    dynamic_states[1] = VK_DYNAMIC_STATE_SCISSOR;
    memset(&dynamic, 0, sizeof(dynamic));
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = 2;
    dynamic.pDynamicStates = dynamic_states;
    memset(&rendering, 0, sizeof(rendering));
    rendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    rendering.colorAttachmentCount = 1;
    rendering.pColorAttachmentFormats = &format;
    memset(&pi, 0, sizeof(pi));
    pi.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pi.pNext = &rendering;
    pi.stageCount = 2;
    pi.pStages = stages;
    pi.pVertexInputState = &vertex_input;
    pi.pInputAssemblyState = &assembly;
    pi.pViewportState = &viewport_state;
    pi.pRasterizationState = &raster;
    pi.pMultisampleState = &multisample;
    pi.pDepthStencilState = &depth;
    pi.pColorBlendState = &blend;
    pi.pDynamicState = &dynamic;
    pi.layout = vk.layout;
    result = vk.vkCreateGraphicsPipelines(vk.device, VK_NULL_HANDLE, 1, &pi, NULL, &vk.pipeline);
    vk.vkDestroyShaderModule(vk.device, vert, NULL);
    vk.vkDestroyShaderModule(vk.device, frag, NULL);
    return result == VK_SUCCESS;
}

/* The images' slots: what they decode into, lent to ovl, and their textures */
static int textures_make(void) {
    uint8_t* pixels[OVL_TEX_SLOTS];
    int i;
    for (i = 0; i < OVL_TEX_SLOTS; i++) {
        tex_slot_t* t = &vk.tex[i];
        if (!host_buffer_make(&t->staging, (VkDeviceSize)OVL_TEX_DIM * OVL_TEX_DIM * 4,
                              VK_BUFFER_USAGE_TRANSFER_SRC_BIT) ||
            !image_make(VK_FORMAT_R8G8B8A8_UNORM, OVL_TEX_DIM, OVL_TEX_DIM, 0, &t->image,
                        &t->memory, &t->view) ||
            !set_make(t->view, &t->set))
            return 0;
        pixels[i] = t->staging.map;
    }
    ovl_tex_init(pixels, OVL_TEX_SLOTS);
    if (!host_buffer_make(&vk.icons.staging, (VkDeviceSize)OVL_ICON_ATLAS_W * OVL_ICON_ATLAS_H * 4,
                          VK_BUFFER_USAGE_TRANSFER_SRC_BIT) ||
        !image_make(VK_FORMAT_R8G8B8A8_UNORM, OVL_ICON_ATLAS_W, OVL_ICON_ATLAS_H, 0,
                    &vk.icons.image, &vk.icons.memory, &vk.icons.view) ||
        !set_make(vk.icons.view, &vk.icons.set))
        return 0;
    vk.icons_sampled = 0;
    ovl_tex_init_icons(vk.icons.staging.map);
    return 1;
}

static void splash_free(void) {
    splash_t* s = &vk.splash;
    host_buffer_free(&s->staging);
    host_buffer_free(&s->quad);
    image_free(&s->image, &s->memory, &s->view);
    /* Its descriptor set stays the pool's, never bound again */
    s->made = 0;
}

/* The splash, when the game has one and it is to be shown: its memory at its size, and its
 * decoding started. Without it the game's picture shows, as it does once the splash is gone. */
static void splash_make(const ovl_vk_info_t* info) {
    splash_t* s = &vk.splash;
    unsigned w, h;
    uint32_t* idx;
    if (!emu_settings_get_show_splash() ||
        !ovl_splash_open(info->splash, info->splash_size, &w, &h))
        return;
    s->w = w;
    s->h = h;
    if (!host_buffer_make(&s->staging, (VkDeviceSize)w * h * 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT) ||
        !host_buffer_make(&s->quad, SPLASH_QUADS_BYTES + 6 * sizeof(uint32_t),
                          VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT) ||
        !image_make(VK_FORMAT_R8G8B8A8_UNORM, w, h, 0, &s->image, &s->memory, &s->view) ||
        !set_make(s->view, &s->set) || !ovl_splash_start(s->staging.map)) {
        ovl_splash_close();
        splash_free();
        return;
    }
    /* As ImGui's AddImage */
    idx = (uint32_t*)(s->quad.map + SPLASH_QUADS_BYTES);
    idx[0] = 0;
    idx[1] = 1;
    idx[2] = 2;
    idx[3] = 0;
    idx[4] = 2;
    idx[5] = 3;
    s->made = 1;
}

/* The splash's memory, each part once no frame in flight may still read it: what it decoded
 * into once the copy is done, the rest once it is no longer shown and its thread is done */
static void splash_collect(void) {
    splash_t* s = &vk.splash;
    if (!s->made)
        return;
    if (s->uploaded && s->staging.buffer && vk.frame - s->upload_frame >= vk.frame_count + 1)
        host_buffer_free(&s->staging);
    if (s->done && !ovl_splash_decoding() && vk.frame - s->retire_frame >= vk.frame_count + 1)
        splash_free();
}

static const char* init_error = "";

const char* ovl_vk_error(void) {
    return init_error;
}

/* What failed, and everything made so far let go */
static int init_failed(const char* what) {
    init_error = what;
    ovl_vk_shutdown();
    return 0;
}

int ovl_vk_init(const ovl_vk_info_t* info) {
    ovl_face_desc_t faces[8];
    unsigned face_count = 1;
    ovl_font_t* font;
    unsigned i;
    if (vk.made)
        return 1;
    memset(&vk, 0, sizeof(vk));
    ovl_msg_dialog_init();
    ovl_save_dialog_init();
    ovl_profile_dialog_init();
    ovl_commerce_dialog_init();
    ovl_invitation_dialog_init();
    if (!load_functions(info->get_device_proc, info->device)) {
        init_error = "the device's functions";
        return 0;
    }
    vk.device = info->device;
    vk.mem = info->memory_properties;
    /* The UI font, read in place, at ImGui's size: 32 pixels drawn at half (FontGlobalScale) */
    faces[0].data = info->font;
    faces[0].size = info->font_size;
    faces[0].index = 0;
    for (i = 0; i < info->fallback_count && face_count < sizeof(faces) / sizeof(faces[0]); i++)
        if (info->fallback_faces[i].data && info->fallback_faces[i].size)
            faces[face_count++] = info->fallback_faces[i];
    font = info->font && info->font_size ? ovl_font_add(faces, (int)face_count, 32.0f) : NULL;
    if (!font)
        return init_failed("the UI font");
    if (!ovl_create(font, 0.5f))
        return init_failed("the overlay's state");
    vk.frame_count = info->image_count < MAX_FRAMES ? info->image_count : MAX_FRAMES;
    if (vk.frame_count == 0)
        vk.frame_count = 1;
    if (!image_make(VK_FORMAT_R8G8_UNORM, OVL_ATLAS_SIZE, OVL_ATLAS_SIZE, 1, &vk.atlas,
                    &vk.atlas_memory, &vk.atlas_view))
        return init_failed("the atlas");
    if (!descriptors_make() || !set_make(vk.atlas_view, &vk.atlas_set))
        return init_failed("the descriptors");
    if (!textures_make())
        return init_failed("the images' slots");
    if (!pipeline_make(info->surface_format))
        return init_failed("the pipeline");
    for (i = 0; i < vk.frame_count; i++) {
        frame_buffers_t* f = &vk.frames[i];
        if (!host_buffer_make(&f->staging, STAGING_BYTES, VK_BUFFER_USAGE_TRANSFER_SRC_BIT) ||
            !host_buffer_make(&f->vertex, VERTEX_BYTES, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) ||
            !host_buffer_make(&f->index, INDEX_BYTES, VK_BUFFER_USAGE_INDEX_BUFFER_BIT))
            return init_failed("the frames' buffers");
    }
    splash_make(info);
    vk.made = 1;
    return 1;
}

void ovl_vk_shutdown(void) {
    unsigned i;
    if (!vk.device)
        return;
    ovl_msg_dialog_shutdown();
    ovl_save_dialog_shutdown();
    ovl_profile_dialog_shutdown();
    ovl_commerce_dialog_shutdown();
    ovl_invitation_dialog_shutdown();
    ovl_shadnet_reset();
    ovl_invite_reset();
    ovl_notify_reset();
    ovl_trophy_reset();
    ovl_tex_shutdown();
    /* Its thread ended before what it decodes into goes */
    ovl_splash_close();
    splash_free();
    for (i = 0; i < vk.frame_count; i++) {
        host_buffer_free(&vk.frames[i].staging);
        host_buffer_free(&vk.frames[i].vertex);
        host_buffer_free(&vk.frames[i].index);
    }
    for (i = 0; i < OVL_TEX_SLOTS; i++) {
        host_buffer_free(&vk.tex[i].staging);
        image_free(&vk.tex[i].image, &vk.tex[i].memory, &vk.tex[i].view);
    }
    host_buffer_free(&vk.icons.staging);
    image_free(&vk.icons.image, &vk.icons.memory, &vk.icons.view);
    if (vk.pipeline)
        vk.vkDestroyPipeline(vk.device, vk.pipeline, NULL);
    if (vk.layout)
        vk.vkDestroyPipelineLayout(vk.device, vk.layout, NULL);
    if (vk.set_layout)
        vk.vkDestroyDescriptorSetLayout(vk.device, vk.set_layout, NULL);
    if (vk.pool)
        vk.vkDestroyDescriptorPool(vk.device, vk.pool, NULL);
    if (vk.sampler)
        vk.vkDestroySampler(vk.device, vk.sampler, NULL);
    image_free(&vk.atlas, &vk.atlas_memory, &vk.atlas_view);
    memset(&vk, 0, sizeof(vk));
    ovl_destroy();
}

void ovl_vk_new_frame(unsigned width, unsigned height, float delta, float framerate) {
    if (!vk.made)
        return;
    vk.display_w = (float)width;
    vk.display_h = (float)height;
    ovl_new_frame((float)width, (float)height, delta);
    /* In ImGui's order: the counter, then a notification and a trophy, which came after */
    ovl_fps_draw(framerate);
    ovl_notify_draw();
    ovl_trophy_draw();
    /* shadNet's notifications and the invitation prompt, registered when the presenter was made */
    ovl_shadnet_draw();
    ovl_invite_draw();
    /* The dialogs after, as ImGui's layers registered as they opened; a message (the emulator's
     * own save data errors among them) over the save data dialog */
    ovl_save_dialog_draw();
    ovl_profile_dialog_draw();
    ovl_commerce_dialog_draw();
    ovl_invitation_dialog_draw();
    ovl_store_icon_draw();
    ovl_msg_dialog_draw();
    /* The on-screen keyboard, as ImGui's layer of it was: after what was open before it */
    ovl_ime_draw();
    vk.draw_data = ovl_render();
}

int ovl_vk_must_keep_drawing(void) {
    return vk.made && (ovl_notify_active() || ovl_trophy_active() || ovl_msg_dialog_active() ||
                       ovl_save_dialog_active() || ovl_profile_dialog_active() ||
                       ovl_commerce_dialog_active() || ovl_invitation_dialog_active() ||
                       ovl_store_icon_active() || ovl_shadnet_active() || ovl_invite_active() ||
                       ovl_ime_must_keep_drawing());
}

int ovl_vk_splash_ready(void) {
    return vk.made && vk.splash.made && !vk.splash.done && ovl_splash_decoded();
}

void ovl_vk_splash_done(void) {
    splash_t* s = &vk.splash;
    if (!s->made || s->done)
        return;
    s->done = 1;
    s->retire_frame = vk.frame + 1;
    ovl_splash_stop();
}

static void barrier(VkCommandBuffer cmd, VkImage image, VkAccessFlags src_access,
                    VkAccessFlags dst_access, VkImageLayout old_layout, VkImageLayout new_layout,
                    VkPipelineStageFlags src_stage, VkPipelineStageFlags dst_stage) {
    VkImageMemoryBarrier b;
    memset(&b, 0, sizeof(b));
    b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    b.srcAccessMask = src_access;
    b.dstAccessMask = dst_access;
    b.oldLayout = old_layout;
    b.newLayout = new_layout;
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = image;
    b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    b.subresourceRange.levelCount = 1;
    b.subresourceRange.layerCount = 1;
    vk.vkCmdPipelineBarrier(cmd, src_stage, dst_stage, 0, 0, NULL, 0, NULL, 1, &b);
}

/* What changed of the atlas since the last frame, into the image: cleared when the atlas
 * started again, then the rectangle written, as much of it as the frame's staging holds */
static void upload_atlas(VkCommandBuffer cmd, frame_buffers_t* f) {
    int rect[4], cleared = 0;
    const int discard = !vk.atlas_sampled;
    if (!ovl_atlas_take_dirty(rect, &cleared))
        return;
    barrier(cmd, vk.atlas, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
            (cleared || discard) ? VK_IMAGE_LAYOUT_UNDEFINED
                                 : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT);
    if (cleared || discard) {
        VkClearColorValue zero;
        VkImageSubresourceRange range;
        memset(&zero, 0, sizeof(zero));
        memset(&range, 0, sizeof(range));
        range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        range.levelCount = 1;
        range.layerCount = 1;
        vk.vkCmdClearColorImage(cmd, vk.atlas, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &zero, 1,
                                &range);
        if (rect[2] > 0)
            barrier(cmd, vk.atlas, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    }
    if (rect[2] > 0 && rect[3] > 0) {
        const size_t pitch = (size_t)rect[2] * 2;
        const uint8_t* pixels = ovl_atlas_pixels();
        VkBufferImageCopy region;
        int rows = (int)(STAGING_BYTES / pitch), y;
        if (rows > rect[3])
            rows = rect[3];
        /* The rest waits a frame */
        if (rows < rect[3])
            ovl_atlas_mark_dirty(rect[0], rect[1] + rows, rect[2], rect[3] - rows);
        for (y = 0; y < rows; y++)
            memcpy(f->staging.map + pitch * (size_t)y,
                   pixels + ((size_t)(rect[1] + y) * OVL_ATLAS_SIZE + (size_t)rect[0]) * 2, pitch);
        memset(&region, 0, sizeof(region));
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageOffset.x = rect[0];
        region.imageOffset.y = rect[1];
        region.imageExtent.width = (uint32_t)rect[2];
        region.imageExtent.height = (uint32_t)rows;
        region.imageExtent.depth = 1;
        vk.vkCmdCopyBufferToImage(cmd, f->staging.buffer, vk.atlas,
                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    }
    barrier(cmd, vk.atlas, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    vk.atlas_sampled = 1;
}

/* Images decoded since the last frame, each copied whole into its slot's texture; icons, each
 * with its border into the atlas, the rest of it kept */
static void upload_textures(VkCommandBuffer cmd) {
    int handle;
    while ((handle = ovl_tex_take_upload()) != 0) {
        const tex_slot_t* t;
        VkBufferImageCopy region;
        int rect[4];
        if (ovl_tex_icon_rect(handle, rect)) {
            t = &vk.icons;
            barrier(cmd, t->image, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                    vk.icons_sampled ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                     : VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT);
            memset(&region, 0, sizeof(region));
            region.bufferOffset =
                ((VkDeviceSize)rect[1] * OVL_ICON_ATLAS_W + (VkDeviceSize)rect[0]) * 4;
            region.bufferRowLength = OVL_ICON_ATLAS_W;
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.layerCount = 1;
            region.imageOffset.x = rect[0];
            region.imageOffset.y = rect[1];
            region.imageExtent.width = (uint32_t)rect[2];
            region.imageExtent.height = (uint32_t)rect[3];
            region.imageExtent.depth = 1;
            vk.vkCmdCopyBufferToImage(cmd, t->staging.buffer, t->image,
                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
            barrier(cmd, t->image, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            vk.icons_sampled = 1;
            continue;
        }
        t = &vk.tex[handle - 1];
        barrier(cmd, t->image, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
        memset(&region, 0, sizeof(region));
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageExtent.width = OVL_TEX_DIM;
        region.imageExtent.height = OVL_TEX_DIM;
        region.imageExtent.depth = 1;
        vk.vkCmdCopyBufferToImage(cmd, t->staging.buffer, t->image,
                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        barrier(cmd, t->image, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    }
}

/* The splash in place of the game's picture: the image cleared to black, as the window's
 * background left it, and the splash over it, ImGui's quad through ImGui's pipeline. Copied
 * into its texture the first time. */
void ovl_vk_render_splash(VkCommandBuffer cmd, VkImageView view, VkExtent2D extent) {
    splash_t* s = &vk.splash;
    VkRenderingAttachmentInfo color;
    VkRenderingInfo rendering;
    VkViewport viewport;
    VkRect2D scissor;
    VkDeviceSize offset;
    float push[4];
    if (!ovl_vk_splash_ready() || vk.display_w <= 0.0f || vk.display_h <= 0.0f)
        return;
    if (!s->uploaded) {
        VkBufferImageCopy region;
        barrier(cmd, s->image, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT);
        memset(&region, 0, sizeof(region));
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageExtent.width = s->w;
        region.imageExtent.height = s->h;
        region.imageExtent.depth = 1;
        vk.vkCmdCopyBufferToImage(cmd, s->staging.buffer, s->image,
                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        barrier(cmd, s->image, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        s->uploaded = 1;
        s->upload_frame = vk.frame + 1;
    }
    s->quad_index = (s->quad_index + 1) % vk.frame_count;
    ovl_splash_quad(vk.display_w, vk.display_h,
                    (ovl_vertex_t*)s->quad.map + (size_t)s->quad_index * 4);

    memset(&color, 0, sizeof(color));
    color.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    color.imageView = view;
    color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.clearValue.color.float32[3] = 1.0f;
    memset(&rendering, 0, sizeof(rendering));
    rendering.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering.renderArea.extent = extent;
    rendering.layerCount = 1;
    rendering.colorAttachmentCount = 1;
    rendering.pColorAttachments = &color;
    vk.vkCmdBeginRendering(cmd, &rendering);
    vk.vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline);
    offset = (VkDeviceSize)s->quad_index * 4 * sizeof(ovl_vertex_t);
    vk.vkCmdBindVertexBuffers(cmd, 0, 1, &s->quad.buffer, &offset);
    vk.vkCmdBindIndexBuffer(cmd, s->quad.buffer, SPLASH_QUADS_BYTES, VK_INDEX_TYPE_UINT32);
    memset(&viewport, 0, sizeof(viewport));
    viewport.width = (float)extent.width;
    viewport.height = (float)extent.height;
    viewport.maxDepth = 1.0f;
    vk.vkCmdSetViewport(cmd, 0, 1, &viewport);
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent = extent;
    vk.vkCmdSetScissor(cmd, 0, 1, &scissor);
    push[0] = 2.0f / vk.display_w;
    push[1] = 2.0f / vk.display_h;
    push[2] = -1.0f;
    push[3] = -1.0f;
    vk.vkCmdPushConstants(cmd, vk.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push), push);
    vk.vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.layout, 0, 1, &s->set, 0,
                               NULL);
    vk.vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);
    vk.vkCmdEndRendering(cmd);
}

/* A frame's vertices and indices into its buffers, grown once if the frame needs more */
static int upload_geometry(frame_buffers_t* f, const ovl_draw_data_t* dd) {
    const VkDeviceSize vtx_bytes = (VkDeviceSize)dd->vtx_count * sizeof(ovl_vertex_t);
    const VkDeviceSize idx_bytes = (VkDeviceSize)dd->idx_count * sizeof(uint32_t);
    if (vtx_bytes > f->vertex.size) {
        host_buffer_free(&f->vertex);
        if (!host_buffer_make(&f->vertex, vtx_bytes * 2, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT))
            return 0;
    }
    if (idx_bytes > f->index.size) {
        host_buffer_free(&f->index);
        if (!host_buffer_make(&f->index, idx_bytes * 2, VK_BUFFER_USAGE_INDEX_BUFFER_BIT))
            return 0;
    }
    memcpy(f->vertex.map, dd->vtx, (size_t)vtx_bytes);
    memcpy(f->index.map, dd->idx, (size_t)idx_bytes);
    return 1;
}

void ovl_vk_render(VkCommandBuffer cmd, VkImageView view, VkExtent2D extent) {
    const ovl_draw_data_t* dd = vk.draw_data;
    const float fb_w = (float)extent.width, fb_h = (float)extent.height;
    const int markers = emu_settings_get_vkhost_markers() && vk.vkCmdBeginDebugUtilsLabelEXT &&
                        vk.vkCmdEndDebugUtilsLabelEXT;
    VkRenderingAttachmentInfo color;
    VkRenderingInfo rendering;
    VkViewport viewport;
    VkDeviceSize zero = 0;
    VkDescriptorSet bound = VK_NULL_HANDLE;
    frame_buffers_t* f;
    float push[4];
    uint32_t i;
    if (!vk.made)
        return;
    /* Images released go once every frame that may have drawn them is done */
    vk.frame++;
    ovl_tex_collect(vk.frame, vk.frame_count + 1);
    splash_collect();
    if (!dd || dd->idx_count == 0 || fb_w <= 0.0f || fb_h <= 0.0f)
        return;
    vk.frame_index = (vk.frame_index + 1) % vk.frame_count;
    f = &vk.frames[vk.frame_index];

    if (markers) {
        VkDebugUtilsLabelEXT label;
        memset(&label, 0, sizeof(label));
        label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
        label.pLabelName = "Overlay Render";
        vk.vkCmdBeginDebugUtilsLabelEXT(cmd, &label);
    }
    /* Uploads go ahead of the pass that samples them */
    upload_atlas(cmd, f);
    upload_textures(cmd);
    if (!upload_geometry(f, dd)) {
        if (markers)
            vk.vkCmdEndDebugUtilsLabelEXT(cmd);
        return;
    }

    memset(&color, 0, sizeof(color));
    color.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    color.imageView = view;
    color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    memset(&rendering, 0, sizeof(rendering));
    rendering.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering.renderArea.extent = extent;
    rendering.layerCount = 1;
    rendering.colorAttachmentCount = 1;
    rendering.pColorAttachments = &color;
    vk.vkCmdBeginRendering(cmd, &rendering);
    vk.vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline);
    vk.vkCmdBindVertexBuffers(cmd, 0, 1, &f->vertex.buffer, &zero);
    vk.vkCmdBindIndexBuffer(cmd, f->index.buffer, 0, VK_INDEX_TYPE_UINT32);
    memset(&viewport, 0, sizeof(viewport));
    viewport.width = fb_w;
    viewport.height = fb_h;
    viewport.maxDepth = 1.0f;
    vk.vkCmdSetViewport(cmd, 0, 1, &viewport);
    /* The display's pixels to clip space, as ImGui's backend */
    push[0] = 2.0f / dd->display_w;
    push[1] = 2.0f / dd->display_h;
    push[2] = -1.0f;
    push[3] = -1.0f;
    vk.vkCmdPushConstants(cmd, vk.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push), push);
    for (i = 0; i < dd->cmd_count; i++) {
        const ovl_cmd_t* c = &dd->cmds[i];
        /* Within the image, as vkCmdSetScissor requires */
        const float x0 = c->clip[0] > 0.0f ? c->clip[0] : 0.0f;
        const float y0 = c->clip[1] > 0.0f ? c->clip[1] : 0.0f;
        const float x1 = c->clip[2] < fb_w ? c->clip[2] : fb_w;
        const float y1 = c->clip[3] < fb_h ? c->clip[3] : fb_h;
        VkRect2D scissor;
        VkDescriptorSet set;
        if (x1 <= x0 || y1 <= y0 || c->idx_count == 0)
            continue;
        scissor.offset.x = (int32_t)x0;
        scissor.offset.y = (int32_t)y0;
        scissor.extent.width = (uint32_t)(x1 - x0);
        scissor.extent.height = (uint32_t)(y1 - y0);
        vk.vkCmdSetScissor(cmd, 0, 1, &scissor);
        set = c->tex == OVL_TEX_ICONS                             ? vk.icons.set
              : c->tex == OVL_TEX_ATLAS || c->tex > OVL_TEX_SLOTS ? vk.atlas_set
                                                                  : vk.tex[c->tex - 1].set;
        if (set != bound) {
            vk.vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.layout, 0, 1, &set,
                                       0, NULL);
            bound = set;
        }
        vk.vkCmdDrawIndexed(cmd, c->idx_count, 1, c->idx_offset, 0, 0);
    }
    vk.vkCmdEndRendering(cmd);
    if (markers)
        vk.vkCmdEndDebugUtilsLabelEXT(cmd);
}
