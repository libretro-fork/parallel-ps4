/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The depth-to-colour shader (host_spirv_depth_to_color_frag) run on a device:
 * a depth buffer whose samples hold different depth and stencil values comes
 * out, sample by sample, as the colour it should: depth in a float target,
 * stencil in an integer one, stencil as a fraction of 255 in a float one. Run
 * for a depth buffer of one sample and of two. Skipped (exit 0) with no device.
 *
 *   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json \
 *       build-tests/depth_to_color_device_test */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dlfcn.h>

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>

#include "../../src/video_core/host_shaders/host_spirv.h"

static int failures = 0;

#define CHECK(cond, what)                                                                      \
    do {                                                                                       \
        if (!(cond)) {                                                                         \
            printf("FAIL %s (line %d)\n", what, __LINE__);                                     \
            ++failures;                                                                        \
        }                                                                                      \
    } while (0)

static PFN_vkGetInstanceProcAddr get_instance_proc;
static PFN_vkGetDeviceProcAddr get_device_proc;
#define FN(name) static PFN_##name p_##name
FN(vkCreateInstance);
FN(vkEnumeratePhysicalDevices);
FN(vkGetPhysicalDeviceMemoryProperties);
FN(vkCreateDevice);
FN(vkGetDeviceQueue);
FN(vkAllocateMemory);
FN(vkMapMemory);
FN(vkCreateBuffer);
FN(vkGetBufferMemoryRequirements);
FN(vkBindBufferMemory);
FN(vkCreateImage);
FN(vkGetImageMemoryRequirements);
FN(vkBindImageMemory);
FN(vkCreateImageView);
FN(vkCreateRenderPass);
FN(vkCreateFramebuffer);
FN(vkCreateShaderModule);
FN(vkCreateDescriptorSetLayout);
FN(vkCreatePipelineLayout);
FN(vkCreateGraphicsPipelines);
FN(vkCreateDescriptorPool);
FN(vkAllocateDescriptorSets);
FN(vkUpdateDescriptorSets);
FN(vkCreateCommandPool);
FN(vkAllocateCommandBuffers);
FN(vkBeginCommandBuffer);
FN(vkEndCommandBuffer);
FN(vkCmdBeginRenderPass);
FN(vkCmdEndRenderPass);
FN(vkCmdBindPipeline);
FN(vkCmdBindDescriptorSets);
FN(vkCmdDraw);
FN(vkCmdCopyImageToBuffer);
FN(vkQueueSubmit);
FN(vkQueueWaitIdle);

#define SIZE 8u
#define CLEAR_DEPTH 0.75f
#define CLEAR_STENCIL 7u
#define DRAWN_DEPTH 0.5f
#define DRAWN_STENCIL 9u

static VkDevice device;
static VkQueue queue;
static VkCommandBuffer cb;
static VkPhysicalDeviceMemoryProperties mem;
static VkPipelineLayout layout;
static VkDescriptorSetLayout set_layout;
static VkDescriptorPool dpool;
static VkShaderModule vert;
static VkBuffer out_buf;
static void* out_data;

static VkDeviceMemory alloc(const VkMemoryRequirements* req, VkMemoryPropertyFlags want) {
    VkMemoryAllocateInfo ai;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    uint32_t i;
    memset(&ai, 0, sizeof(ai));
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = req->size;
    for (i = 0; i < mem.memoryTypeCount; i++)
        if ((req->memoryTypeBits & (1u << i)) &&
            (mem.memoryTypes[i].propertyFlags & want) == want)
            break;
    ai.memoryTypeIndex = i;
    if (i == mem.memoryTypeCount || p_vkAllocateMemory(device, &ai, NULL, &memory) != VK_SUCCESS)
        return VK_NULL_HANDLE;
    return memory;
}

static VkImage make_image(VkFormat format, uint32_t samples, VkImageUsageFlags usage) {
    VkImageCreateInfo ii;
    VkMemoryRequirements req;
    VkDeviceMemory memory;
    VkImage image;
    memset(&ii, 0, sizeof(ii));
    ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.format = format;
    ii.extent.width = SIZE;
    ii.extent.height = SIZE;
    ii.extent.depth = 1;
    ii.mipLevels = 1;
    ii.arrayLayers = 1;
    ii.samples = (VkSampleCountFlagBits)samples;
    ii.usage = usage;
    if (p_vkCreateImage(device, &ii, NULL, &image) != VK_SUCCESS)
        return VK_NULL_HANDLE;
    p_vkGetImageMemoryRequirements(device, image, &req);
    memory = alloc(&req, 0);
    if (!memory)
        return VK_NULL_HANDLE;
    p_vkBindImageMemory(device, image, memory, 0);
    return image;
}

static VkImageView make_view(VkImage image, VkFormat format, VkImageAspectFlags aspect) {
    VkImageViewCreateInfo vi;
    VkImageView view = VK_NULL_HANDLE;
    memset(&vi, 0, sizeof(vi));
    vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vi.image = image;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = format;
    vi.subresourceRange.aspectMask = aspect;
    vi.subresourceRange.levelCount = 1;
    vi.subresourceRange.layerCount = 1;
    p_vkCreateImageView(device, &vi, NULL, &view);
    return view;
}

static VkShaderModule make_module(const uint32_t* words, size_t n) {
    VkShaderModuleCreateInfo mi;
    VkShaderModule module = VK_NULL_HANDLE;
    memset(&mi, 0, sizeof(mi));
    mi.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    mi.codeSize = n * 4;
    mi.pCode = words;
    p_vkCreateShaderModule(device, &mi, NULL, &module);
    return module;
}

/* A pass over one attachment that is cleared, kept, and left in @final */
static VkRenderPass make_pass(VkFormat format, uint32_t samples, int is_depth,
                              VkImageLayout final) {
    VkAttachmentDescription att;
    VkAttachmentReference ref;
    VkSubpassDescription sub;
    VkRenderPassCreateInfo ri;
    VkRenderPass pass = VK_NULL_HANDLE;
    memset(&att, 0, sizeof(att));
    att.format = format;
    att.samples = (VkSampleCountFlagBits)samples;
    att.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    att.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
    att.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    att.finalLayout = final;
    ref.attachment = 0;
    ref.layout = is_depth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                          : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    memset(&sub, 0, sizeof(sub));
    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    if (is_depth)
        sub.pDepthStencilAttachment = &ref;
    else {
        sub.colorAttachmentCount = 1;
        sub.pColorAttachments = &ref;
    }
    memset(&ri, 0, sizeof(ri));
    ri.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    ri.attachmentCount = 1;
    ri.pAttachments = &att;
    ri.subpassCount = 1;
    ri.pSubpasses = &sub;
    p_vkCreateRenderPass(device, &ri, NULL, &pass);
    return pass;
}

static VkFramebuffer make_framebuffer(VkRenderPass pass, VkImageView view) {
    VkFramebufferCreateInfo fi;
    VkFramebuffer fb = VK_NULL_HANDLE;
    memset(&fi, 0, sizeof(fi));
    fi.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fi.renderPass = pass;
    fi.attachmentCount = 1;
    fi.pAttachments = &view;
    fi.width = SIZE;
    fi.height = SIZE;
    fi.layers = 1;
    p_vkCreateFramebuffer(device, &fi, NULL, &fb);
    return fb;
}

/* A full-screen triangle's pipeline. With no @frag it writes DRAWN_DEPTH and
 * DRAWN_STENCIL to the samples in @sample_mask of a depth buffer. */
static VkPipeline make_pipeline(VkRenderPass pass, VkShaderModule frag, uint32_t samples,
                                uint32_t sample_mask) {
    VkPipelineShaderStageCreateInfo stages[2];
    VkPipelineVertexInputStateCreateInfo vertex_input;
    VkPipelineInputAssemblyStateCreateInfo assembly;
    VkPipelineViewportStateCreateInfo viewport_state;
    VkPipelineRasterizationStateCreateInfo raster;
    VkPipelineMultisampleStateCreateInfo multisample;
    VkPipelineDepthStencilStateCreateInfo depth;
    VkPipelineColorBlendAttachmentState blend_att;
    VkPipelineColorBlendStateCreateInfo blend;
    VkGraphicsPipelineCreateInfo pi;
    VkViewport viewport;
    VkRect2D scissor;
    VkSampleMask mask = sample_mask;
    VkPipeline pipeline = VK_NULL_HANDLE;
    memset(stages, 0, sizeof(stages));
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag;
    stages[1].pName = "main";
    memset(&vertex_input, 0, sizeof(vertex_input));
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    memset(&assembly, 0, sizeof(assembly));
    assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    memset(&viewport, 0, sizeof(viewport));
    viewport.width = (float)SIZE;
    viewport.height = (float)SIZE;
    viewport.minDepth = DRAWN_DEPTH;
    viewport.maxDepth = DRAWN_DEPTH;
    memset(&scissor, 0, sizeof(scissor));
    scissor.extent.width = SIZE;
    scissor.extent.height = SIZE;
    memset(&viewport_state, 0, sizeof(viewport_state));
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &scissor;
    memset(&raster, 0, sizeof(raster));
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = VK_CULL_MODE_NONE;
    raster.lineWidth = 1.0f;
    memset(&multisample, 0, sizeof(multisample));
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = (VkSampleCountFlagBits)samples;
    multisample.pSampleMask = &mask;
    memset(&depth, 0, sizeof(depth));
    depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    if (!frag) {
        depth.depthTestEnable = VK_TRUE;
        depth.depthWriteEnable = VK_TRUE;
        depth.depthCompareOp = VK_COMPARE_OP_ALWAYS;
        depth.stencilTestEnable = VK_TRUE;
        depth.front.failOp = VK_STENCIL_OP_REPLACE;
        depth.front.passOp = VK_STENCIL_OP_REPLACE;
        depth.front.depthFailOp = VK_STENCIL_OP_REPLACE;
        depth.front.compareOp = VK_COMPARE_OP_ALWAYS;
        depth.front.compareMask = 0xff;
        depth.front.writeMask = 0xff;
        depth.front.reference = DRAWN_STENCIL;
        depth.back = depth.front;
    }
    memset(&blend_att, 0, sizeof(blend_att));
    blend_att.colorWriteMask = 0xf;
    memset(&blend, 0, sizeof(blend));
    blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.attachmentCount = frag ? 1 : 0;
    blend.pAttachments = &blend_att;
    memset(&pi, 0, sizeof(pi));
    pi.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pi.stageCount = frag ? 2 : 1;
    pi.pStages = stages;
    pi.pVertexInputState = &vertex_input;
    pi.pInputAssemblyState = &assembly;
    pi.pViewportState = &viewport_state;
    pi.pRasterizationState = &raster;
    pi.pMultisampleState = &multisample;
    pi.pDepthStencilState = &depth;
    pi.pColorBlendState = &blend;
    pi.layout = layout;
    pi.renderPass = pass;
    p_vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pi, NULL, &pipeline);
    return pipeline;
}

static void begin(void) {
    VkCommandBufferBeginInfo bi;
    memset(&bi, 0, sizeof(bi));
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    p_vkBeginCommandBuffer(cb, &bi);
}

static void submit(void) {
    VkSubmitInfo si;
    p_vkEndCommandBuffer(cb);
    memset(&si, 0, sizeof(si));
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cb;
    p_vkQueueSubmit(queue, 1, &si, VK_NULL_HANDLE);
    p_vkQueueWaitIdle(queue);
}

static void draw_pass(VkRenderPass pass, VkFramebuffer fb, VkPipeline pipeline,
                      VkDescriptorSet set, const VkClearValue* clear) {
    VkRenderPassBeginInfo rb;
    memset(&rb, 0, sizeof(rb));
    rb.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rb.renderPass = pass;
    rb.framebuffer = fb;
    rb.renderArea.extent.width = SIZE;
    rb.renderArea.extent.height = SIZE;
    rb.clearValueCount = 1;
    rb.pClearValues = clear;
    p_vkCmdBeginRenderPass(cb, &rb, VK_SUBPASS_CONTENTS_INLINE);
    p_vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    if (set)
        p_vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &set, 0,
                                  NULL);
    p_vkCmdDraw(cb, 3, 1, 0, 0);
    p_vkCmdEndRenderPass(cb);
}

/* The plane of @depth_image, sample @sample of @samples, copied by the shader
 * into a target of @format: every texel of it is @want_f (a float target) or
 * @want_u (an integer one) */
static void copy_and_check(VkImage depth_image, uint32_t samples, int stencil, uint32_t sample,
                           VkFormat format, float want_f, uint32_t want_u, const char* what) {
    static uint32_t words[HOST_SPIRV_MAX_WORDS];
    const int uint_out = format == VK_FORMAT_R8_UINT;
    const size_t n =
        host_spirv_depth_to_color_frag(words, HOST_SPIRV_MAX_WORDS, samples > 1, stencil, uint_out,
                                       sample);
    const VkShaderModule frag = n ? make_module(words, n) : VK_NULL_HANDLE;
    const VkImage target = make_image(
        format, 1, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    const VkImageView target_view = make_view(target, format, VK_IMAGE_ASPECT_COLOR_BIT);
    const VkImageView plane =
        make_view(depth_image, VK_FORMAT_D32_SFLOAT_S8_UINT,
                  stencil ? VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_DEPTH_BIT);
    const VkRenderPass pass = make_pass(format, 1, 0, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    const VkFramebuffer fb = make_framebuffer(pass, target_view);
    VkPipeline pipeline;
    VkDescriptorSetAllocateInfo dai;
    VkDescriptorSet set = VK_NULL_HANDLE;
    VkDescriptorImageInfo image_info;
    VkWriteDescriptorSet write;
    VkBufferImageCopy region;
    VkClearValue clear;
    uint32_t i, bad = 0;
    if (!frag || !target || !target_view || !plane || !pass || !fb) {
        CHECK(0, what);
        return;
    }
    pipeline = make_pipeline(pass, frag, 1, 1);
    memset(&dai, 0, sizeof(dai));
    dai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dai.descriptorPool = dpool;
    dai.descriptorSetCount = 1;
    dai.pSetLayouts = &set_layout;
    if (!pipeline || p_vkAllocateDescriptorSets(device, &dai, &set) != VK_SUCCESS) {
        CHECK(0, what);
        return;
    }
    memset(&image_info, 0, sizeof(image_info));
    image_info.imageView = plane;
    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    memset(&write, 0, sizeof(write));
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = set;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    write.pImageInfo = &image_info;
    p_vkUpdateDescriptorSets(device, 1, &write, 0, NULL);

    memset(out_data, 0xa5, SIZE * SIZE * 4);
    memset(&clear, 0xff, sizeof(clear));
    begin();
    draw_pass(pass, fb, pipeline, set, &clear);
    memset(&region, 0, sizeof(region));
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent.width = SIZE;
    region.imageExtent.height = SIZE;
    region.imageExtent.depth = 1;
    p_vkCmdCopyImageToBuffer(cb, target, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, out_buf, 1,
                             &region);
    submit();
    for (i = 0; i < SIZE * SIZE; i++) {
        if (uint_out) {
            if (((const unsigned char*)out_data)[i] != want_u)
                ++bad;
        } else {
            const float got = ((const float*)out_data)[i];
            if (got < want_f - 1e-6f || got > want_f + 1e-6f)
                ++bad;
        }
    }
    if (bad)
        printf("  %s: %u of %u texels wrong, first is %f / %u\n", what, bad, SIZE * SIZE,
               (double)((const float*)out_data)[0], ((const unsigned char*)out_data)[0]);
    CHECK(bad == 0, what);
}

/* A depth buffer of @samples: cleared, then its last sample drawn over */
static void run(uint32_t samples) {
    const VkFormat format = VK_FORMAT_D32_SFLOAT_S8_UINT;
    const VkImage image = make_image(
        format, samples, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
    const VkImageView view =
        make_view(image, format, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
    const VkRenderPass pass =
        make_pass(format, samples, 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    const VkFramebuffer fb = make_framebuffer(pass, view);
    const uint32_t last = samples - 1;
    VkPipeline pipeline;
    VkClearValue clear;
    uint32_t s;
    if (!image || !view || !pass || !fb) {
        CHECK(0, "depth buffer");
        return;
    }
    pipeline = make_pipeline(pass, VK_NULL_HANDLE, samples, 1u << last);
    if (!pipeline) {
        CHECK(0, "depth buffer");
        return;
    }
    memset(&clear, 0, sizeof(clear));
    clear.depthStencil.depth = CLEAR_DEPTH;
    clear.depthStencil.stencil = CLEAR_STENCIL;
    begin();
    draw_pass(pass, fb, pipeline, VK_NULL_HANDLE, &clear);
    submit();

    for (s = 0; s < samples; s++) {
        const int drawn = s == last;
        const float d = drawn ? DRAWN_DEPTH : CLEAR_DEPTH;
        const uint32_t st = drawn ? DRAWN_STENCIL : CLEAR_STENCIL;
        copy_and_check(image, samples, 0, s, VK_FORMAT_R32_SFLOAT, d, 0,
                       samples > 1 ? (s ? "2x depth, sample 1" : "2x depth, sample 0")
                                   : "1x depth");
        copy_and_check(image, samples, 1, s, VK_FORMAT_R8_UINT, 0.0f, st,
                       samples > 1 ? (s ? "2x stencil, sample 1" : "2x stencil, sample 0")
                                   : "1x stencil");
        copy_and_check(image, samples, 1, s, VK_FORMAT_R32_SFLOAT, (float)st / 255.0f, 0,
                       samples > 1 ? (s ? "2x stencil as float, sample 1"
                                        : "2x stencil as float, sample 0")
                                   : "1x stencil as float");
    }
}

int main(void) {
    static uint32_t words[HOST_SPIRV_MAX_WORDS];
    void* lib = dlopen("libvulkan.so.1", RTLD_NOW);
    VkApplicationInfo app;
    VkInstanceCreateInfo instance_info;
    VkInstance instance;
    VkPhysicalDevice physical;
    VkDeviceQueueCreateInfo queue_info;
    VkDeviceCreateInfo device_info;
    VkDescriptorSetLayoutBinding binding;
    VkDescriptorSetLayoutCreateInfo sli;
    VkPipelineLayoutCreateInfo pli;
    VkDescriptorPoolSize pool_size;
    VkDescriptorPoolCreateInfo dpi;
    VkCommandPoolCreateInfo pool_info;
    VkCommandPool pool;
    VkCommandBufferAllocateInfo cb_info;
    VkBufferCreateInfo bi;
    VkMemoryRequirements req;
    VkDeviceMemory memory;
    float priority = 1.0f;
    uint32_t count = 1;
    size_t n;

    if (lib)
        *(void**)&get_instance_proc = dlsym(lib, "vkGetInstanceProcAddr");
    if (!lib || !get_instance_proc) {
        printf("depth_to_color_device: no Vulkan loader, skipped\n");
        return 0;
    }
    p_vkCreateInstance = (PFN_vkCreateInstance)get_instance_proc(NULL, "vkCreateInstance");
    memset(&app, 0, sizeof(app));
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.apiVersion = VK_API_VERSION_1_1;
    memset(&instance_info, 0, sizeof(instance_info));
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pApplicationInfo = &app;
    if (p_vkCreateInstance(&instance_info, NULL, &instance) != VK_SUCCESS) {
        printf("depth_to_color_device: no Vulkan instance, skipped\n");
        return 0;
    }
#define LOADI(name) p_##name = (PFN_##name)get_instance_proc(instance, #name)
    LOADI(vkEnumeratePhysicalDevices);
    LOADI(vkGetPhysicalDeviceMemoryProperties);
    LOADI(vkCreateDevice);
    get_device_proc = (PFN_vkGetDeviceProcAddr)get_instance_proc(instance, "vkGetDeviceProcAddr");
    if (p_vkEnumeratePhysicalDevices(instance, &count, &physical) < 0 || !count) {
        printf("depth_to_color_device: no device, skipped\n");
        return 0;
    }
    p_vkGetPhysicalDeviceMemoryProperties(physical, &mem);
    memset(&queue_info, 0, sizeof(queue_info));
    queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info.queueCount = 1;
    queue_info.pQueuePriorities = &priority;
    memset(&device_info, 0, sizeof(device_info));
    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.queueCreateInfoCount = 1;
    device_info.pQueueCreateInfos = &queue_info;
    if (p_vkCreateDevice(physical, &device_info, NULL, &device) != VK_SUCCESS) {
        printf("depth_to_color_device: no device, skipped\n");
        return 0;
    }
#define LOADD(name) p_##name = (PFN_##name)get_device_proc(device, #name)
    LOADD(vkGetDeviceQueue);
    LOADD(vkAllocateMemory);
    LOADD(vkMapMemory);
    LOADD(vkCreateBuffer);
    LOADD(vkGetBufferMemoryRequirements);
    LOADD(vkBindBufferMemory);
    LOADD(vkCreateImage);
    LOADD(vkGetImageMemoryRequirements);
    LOADD(vkBindImageMemory);
    LOADD(vkCreateImageView);
    LOADD(vkCreateRenderPass);
    LOADD(vkCreateFramebuffer);
    LOADD(vkCreateShaderModule);
    LOADD(vkCreateDescriptorSetLayout);
    LOADD(vkCreatePipelineLayout);
    LOADD(vkCreateGraphicsPipelines);
    LOADD(vkCreateDescriptorPool);
    LOADD(vkAllocateDescriptorSets);
    LOADD(vkUpdateDescriptorSets);
    LOADD(vkCreateCommandPool);
    LOADD(vkAllocateCommandBuffers);
    LOADD(vkBeginCommandBuffer);
    LOADD(vkEndCommandBuffer);
    LOADD(vkCmdBeginRenderPass);
    LOADD(vkCmdEndRenderPass);
    LOADD(vkCmdBindPipeline);
    LOADD(vkCmdBindDescriptorSets);
    LOADD(vkCmdDraw);
    LOADD(vkCmdCopyImageToBuffer);
    LOADD(vkQueueSubmit);
    LOADD(vkQueueWaitIdle);
    p_vkGetDeviceQueue(device, 0, 0, &queue);

    memset(&bi, 0, sizeof(bi));
    bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size = SIZE * SIZE * 4;
    bi.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    p_vkCreateBuffer(device, &bi, NULL, &out_buf);
    p_vkGetBufferMemoryRequirements(device, out_buf, &req);
    memory = alloc(&req, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (!memory) {
        printf("depth_to_color_device: no memory the host can see, skipped\n");
        return 0;
    }
    p_vkBindBufferMemory(device, out_buf, memory, 0);
    p_vkMapMemory(device, memory, 0, VK_WHOLE_SIZE, 0, &out_data);

    memset(&binding, 0, sizeof(binding));
    binding.descriptorCount = 1;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    memset(&sli, 0, sizeof(sli));
    sli.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    sli.bindingCount = 1;
    sli.pBindings = &binding;
    p_vkCreateDescriptorSetLayout(device, &sli, NULL, &set_layout);
    memset(&pli, 0, sizeof(pli));
    pli.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pli.setLayoutCount = 1;
    pli.pSetLayouts = &set_layout;
    p_vkCreatePipelineLayout(device, &pli, NULL, &layout);
    pool_size.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    pool_size.descriptorCount = 16;
    memset(&dpi, 0, sizeof(dpi));
    dpi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    dpi.maxSets = 16;
    dpi.poolSizeCount = 1;
    dpi.pPoolSizes = &pool_size;
    p_vkCreateDescriptorPool(device, &dpi, NULL, &dpool);
    memset(&pool_info, 0, sizeof(pool_info));
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    p_vkCreateCommandPool(device, &pool_info, NULL, &pool);
    memset(&cb_info, 0, sizeof(cb_info));
    cb_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cb_info.commandPool = pool;
    cb_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cb_info.commandBufferCount = 1;
    p_vkAllocateCommandBuffers(device, &cb_info, &cb);

    n = host_spirv_fs_tri_vert(words, HOST_SPIRV_MAX_WORDS);
    vert = n ? make_module(words, n) : VK_NULL_HANDLE;
    if (!vert) {
        printf("FAIL the vertex shader\n");
        return 1;
    }

    run(1);
    run(2);

    if (failures) {
        printf("depth_to_color_device: %d failed\n", failures);
        return 1;
    }
    printf("depth_to_color_device: all passed\n");
    return 0;
}
