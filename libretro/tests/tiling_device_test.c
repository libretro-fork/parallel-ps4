/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The tiling compute shader (host_spirv_tiling_comp) run on a device: texels
 * tiled and detiled again are the texels they were, and a texel that has no
 * place in the image (past the last mip, or past the end of the tiled data
 * that is bound) is neither read nor written. Detiled straight into an image
 * (host_spirv_detile_image_comp), texels are those the linear path gives,
 * those past the image's width left out. Skipped (exit 0) with no device.
 *
 *   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json build-tests/tiling_device_test */

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
FN(vkCreateShaderModule);
FN(vkDestroyShaderModule);
FN(vkCreateDescriptorSetLayout);
FN(vkCreatePipelineLayout);
FN(vkCreateComputePipelines);
FN(vkDestroyPipeline);
FN(vkCreateDescriptorPool);
FN(vkAllocateDescriptorSets);
FN(vkUpdateDescriptorSets);
FN(vkCreateCommandPool);
FN(vkAllocateCommandBuffers);
FN(vkBeginCommandBuffer);
FN(vkEndCommandBuffer);
FN(vkCmdBindPipeline);
FN(vkCmdBindDescriptorSets);
FN(vkCmdDispatch);
FN(vkQueueSubmit);
FN(vkQueueWaitIdle);
FN(vkGetPhysicalDeviceFeatures);
FN(vkGetPhysicalDeviceFormatProperties);
FN(vkCreateImage);
FN(vkDestroyImage);
FN(vkGetImageMemoryRequirements);
FN(vkBindImageMemory);
FN(vkCreateImageView);
FN(vkDestroyImageView);
FN(vkFreeMemory);
FN(vkCmdPipelineBarrier);
FN(vkCmdCopyImageToBuffer);
FN(vkCmdPushConstants);

#define TEXELS 256u /* of the image: 16 by 16, or 8 by 8 by 4 */
#define GUARD 256u  /* words past it that nothing may touch */
#define SENTINEL 0xdeadbeefu

static VkDevice device;
static VkQueue queue;
static VkCommandBuffer cb;
static VkPipelineLayout layout;
static VkDescriptorSetLayout set_layout;
static VkDescriptorSet set;
static VkPipelineLayout image_layout;
static VkDescriptorSetLayout image_set_layout;
static VkDescriptorSet image_set;
static VkPhysicalDevice physical;
static VkPhysicalDeviceMemoryProperties mem;
static int extended_formats;
static VkBuffer tiled_buf, linear_buf, info_buf;
static uint32_t *tiled, *linear, *info;

static void* make_buffer(VkBuffer* buffer, VkDeviceSize size, VkBufferUsageFlags usage,
                         const VkPhysicalDeviceMemoryProperties* mem) {
    VkBufferCreateInfo bi;
    VkMemoryRequirements req;
    VkMemoryAllocateInfo ai;
    VkDeviceMemory memory;
    void* mapped = NULL;
    uint32_t i;
    memset(&bi, 0, sizeof(bi));
    bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size = size;
    bi.usage = usage;
    p_vkCreateBuffer(device, &bi, NULL, buffer);
    p_vkGetBufferMemoryRequirements(device, *buffer, &req);
    memset(&ai, 0, sizeof(ai));
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = req.size;
    for (i = 0; i < mem->memoryTypeCount; i++)
        if ((req.memoryTypeBits & (1u << i)) &&
            (mem->memoryTypes[i].propertyFlags &
             (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
                (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
            break;
    ai.memoryTypeIndex = i;
    if (i == mem->memoryTypeCount || p_vkAllocateMemory(device, &ai, NULL, &memory) != VK_SUCCESS)
        return NULL;
    p_vkBindBufferMemory(device, *buffer, memory, 0);
    p_vkMapMemory(device, memory, 0, VK_WHOLE_SIZE, 0, &mapped);
    return mapped;
}

/* Runs the shader for @bits per texel and @thickness (1 or 4) over @groups
 * groups of 64 texels, with @tiled_words of the tiled data bound */
static int run_bits(int is_tiler, unsigned bits, int word_access, uint32_t thickness,
                    uint32_t groups, uint32_t tiled_words) {
    static uint32_t words[32768];
    uint32_t params[4];
    VkShaderModuleCreateInfo mi;
    VkShaderModule module;
    VkComputePipelineCreateInfo pi;
    VkPipeline pipeline;
    VkDescriptorBufferInfo bufs[3];
    VkWriteDescriptorSet writes[3];
    VkCommandBufferBeginInfo begin;
    VkSubmitInfo submit;
    size_t n;
    uint32_t i;
    params[0] = 1; /* samples */
    params[1] = thickness > 1 ? 4 : 1; /* micro tile mode: thick, thin */
    params[2] = thickness;
    params[3] = (uint32_t)is_tiler;
    n = host_spirv_tiling_comp(words, 32768, bits, 0, word_access, params, 4);
    if (!n)
        return 0;
    memset(&mi, 0, sizeof(mi));
    mi.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    mi.codeSize = n * 4;
    mi.pCode = words;
    if (p_vkCreateShaderModule(device, &mi, NULL, &module) != VK_SUCCESS)
        return 0;
    memset(&pi, 0, sizeof(pi));
    pi.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pi.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pi.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pi.stage.module = module;
    pi.stage.pName = "main";
    pi.layout = layout;
    if (p_vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pi, NULL, &pipeline) != VK_SUCCESS)
        return 0;
    memset(bufs, 0, sizeof(bufs));
    bufs[0].buffer = tiled_buf;
    bufs[0].range = (VkDeviceSize)tiled_words * 4;
    bufs[1].buffer = linear_buf;
    bufs[1].range = VK_WHOLE_SIZE;
    bufs[2].buffer = info_buf;
    bufs[2].range = VK_WHOLE_SIZE;
    memset(writes, 0, sizeof(writes));
    for (i = 0; i < 3; i++) {
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = set;
        writes[i].dstBinding = i;
        writes[i].descriptorCount = 1;
        writes[i].descriptorType =
            i == 2 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[i].pBufferInfo = &bufs[i];
    }
    p_vkUpdateDescriptorSets(device, 3, writes, 0, NULL);
    memset(&begin, 0, sizeof(begin));
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    p_vkBeginCommandBuffer(cb, &begin);
    p_vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    p_vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 1, &set, 0, NULL);
    p_vkCmdDispatch(cb, groups, 1, 1);
    p_vkEndCommandBuffer(cb);
    memset(&submit, 0, sizeof(submit));
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cb;
    p_vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE);
    p_vkQueueWaitIdle(queue);
    p_vkDestroyPipeline(device, pipeline, NULL);
    p_vkDestroyShaderModule(device, module, NULL);
    return 1;
}

static int run(int is_tiler, uint32_t thickness, uint32_t groups, uint32_t tiled_words) {
    return run_bits(is_tiler, 32, 0, thickness, groups, tiled_words);
}

/* One mip of @bytes, @pitch by @height */
static void set_info_bytes(uint32_t num_mips, uint32_t pitch, uint32_t height, uint32_t bytes) {
    memset(info, 0, 64 * 4);
    info[2] = num_mips;
    info[3] = bytes;
    info[4] = pitch | height << 16;
    info[5] = 0;
}

/* One mip of TEXELS texels of @pitch by @height */
static void set_info(uint32_t num_mips, uint32_t pitch, uint32_t height) {
    set_info_bytes(num_mips, pitch, height, TEXELS * 4);
}

static VkFormat uint_format(unsigned bits) {
    switch (bits) {
    case 8:
        return VK_FORMAT_R8_UINT;
    case 16:
        return VK_FORMAT_R16_UINT;
    case 32:
        return VK_FORMAT_R32_UINT;
    case 64:
        return VK_FORMAT_R32G32_UINT;
    default:
        return VK_FORMAT_R32G32B32A32_UINT;
    }
}

static void image_barrier(VkImage image, VkImageLayout from, VkImageLayout to,
                          VkPipelineStageFlags src_stage, VkAccessFlags src_access,
                          VkPipelineStageFlags dst_stage, VkAccessFlags dst_access) {
    VkImageMemoryBarrier b;
    memset(&b, 0, sizeof(b));
    b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    b.srcAccessMask = src_access;
    b.dstAccessMask = dst_access;
    b.oldLayout = from;
    b.newLayout = to;
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = image;
    b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    b.subresourceRange.levelCount = 1;
    b.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
    p_vkCmdPipelineBarrier(cb, src_stage, dst_stage, 0, 0, NULL, 0, NULL, 1, &b);
}

/* A mip of @pitch by @height by @slices texels of @bits, @thickness thick,
 * detiled into a @width by @img_height image (3D for @volume, else that many
 * layers) is what the linear path gives where the image has the texel */
static void into_image(unsigned bits, int volume, uint32_t thickness, uint32_t pitch,
                       uint32_t height, uint32_t slices, uint32_t width, uint32_t img_height,
                       const char* what) {
    static uint32_t words[32768];
    static unsigned char reference[TEXELS * 16];
    const uint32_t texels = pitch * height * slices, bytes = texels * bits / 8;
    const uint32_t texel_bytes = bits / 8, groups = (texels + 63) / 64;
    const VkFormat format = uint_format(bits);
    const unsigned char* got;
    VkFormatProperties props;
    VkImageCreateInfo ici;
    VkImageViewCreateInfo vci;
    VkMemoryRequirements req;
    VkMemoryAllocateInfo ai;
    VkDeviceMemory memory;
    VkImage image;
    VkImageView view;
    VkShaderModuleCreateInfo mi;
    VkShaderModule module;
    VkComputePipelineCreateInfo pi;
    VkPipeline pipeline;
    VkDescriptorBufferInfo bufs[2];
    VkDescriptorImageInfo image_info;
    VkWriteDescriptorSet writes[3];
    VkCommandBufferBeginInfo begin;
    VkSubmitInfo submit;
    VkBufferImageCopy copy;
    uint32_t params[4], pc[4], i, x, y, z, bad = 0;
    size_t n;

    p_vkGetPhysicalDeviceFormatProperties(physical, format, &props);
    if ((bits != 32 && bits != 128 && !extended_formats) ||
        !(props.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT)) {
        printf("tiling_device: no %u-bit storage image, %s skipped\n", bits, what);
        return;
    }

    /* The linear path's texels, from tiled data that is all different */
    set_info_bytes(1, pitch, height, bytes);
    for (i = 0; i < TEXELS + GUARD; i++) {
        tiled[i] = i * 2654435761u ^ 0x5a5a5a5au;
        linear[i] = SENTINEL;
    }
    CHECK(run_bits(0, bits, bits < 32, thickness, groups, bytes / 4), what);
    memcpy(reference, linear, bytes);

    memset(&ici, 0, sizeof(ici));
    ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ici.imageType = volume ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D;
    ici.format = format;
    ici.extent.width = width;
    ici.extent.height = img_height;
    ici.extent.depth = volume ? slices : 1;
    ici.mipLevels = 1;
    ici.arrayLayers = volume ? 1 : slices;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (p_vkCreateImage(device, &ici, NULL, &image) != VK_SUCCESS) {
        CHECK(0, what);
        return;
    }
    p_vkGetImageMemoryRequirements(device, image, &req);
    memset(&ai, 0, sizeof(ai));
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = req.size;
    for (i = 0; i < mem.memoryTypeCount; i++)
        if (req.memoryTypeBits & (1u << i))
            break;
    ai.memoryTypeIndex = i;
    CHECK(p_vkAllocateMemory(device, &ai, NULL, &memory) == VK_SUCCESS, what);
    p_vkBindImageMemory(device, image, memory, 0);
    memset(&vci, 0, sizeof(vci));
    vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vci.image = image;
    vci.viewType = volume ? VK_IMAGE_VIEW_TYPE_3D : VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    vci.format = format;
    vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vci.subresourceRange.levelCount = 1;
    vci.subresourceRange.layerCount = ici.arrayLayers;
    CHECK(p_vkCreateImageView(device, &vci, NULL, &view) == VK_SUCCESS, what);

    params[0] = 1;
    params[1] = thickness > 1 ? 4 : 1;
    params[2] = thickness;
    params[3] = 0;
    n = host_spirv_detile_image_comp(words, 32768, bits, 0, volume, params, 4);
    CHECK(n != 0, what);
    memset(&mi, 0, sizeof(mi));
    mi.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    mi.codeSize = n * 4;
    mi.pCode = words;
    CHECK(p_vkCreateShaderModule(device, &mi, NULL, &module) == VK_SUCCESS, what);
    memset(&pi, 0, sizeof(pi));
    pi.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pi.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pi.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pi.stage.module = module;
    pi.stage.pName = "main";
    pi.layout = image_layout;
    CHECK(p_vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pi, NULL, &pipeline) ==
              VK_SUCCESS,
          what);

    memset(bufs, 0, sizeof(bufs));
    bufs[0].buffer = tiled_buf;
    bufs[0].range = bytes;
    bufs[1].buffer = info_buf;
    bufs[1].range = VK_WHOLE_SIZE;
    memset(&image_info, 0, sizeof(image_info));
    image_info.imageView = view;
    image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    memset(writes, 0, sizeof(writes));
    for (i = 0; i < 3; i++) {
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = image_set;
        writes[i].dstBinding = i;
        writes[i].descriptorCount = 1;
    }
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[0].pBufferInfo = &bufs[0];
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[1].pImageInfo = &image_info;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[2].pBufferInfo = &bufs[1];
    p_vkUpdateDescriptorSets(device, 3, writes, 0, NULL);

    for (i = 0; i < TEXELS + GUARD; i++)
        linear[i] = SENTINEL;
    pc[0] = 0;
    pc[1] = width;
    pc[2] = img_height;
    pc[3] = slices;
    memset(&begin, 0, sizeof(begin));
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    p_vkBeginCommandBuffer(cb, &begin);
    image_barrier(image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                  VK_ACCESS_SHADER_WRITE_BIT);
    p_vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    p_vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, image_layout, 0, 1, &image_set,
                              0, NULL);
    p_vkCmdPushConstants(cb, image_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), pc);
    p_vkCmdDispatch(cb, groups, 1, 1);
    image_barrier(image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                  VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    memset(&copy, 0, sizeof(copy));
    copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.imageSubresource.layerCount = ici.arrayLayers;
    copy.imageExtent = ici.extent;
    p_vkCmdCopyImageToBuffer(cb, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, linear_buf, 1,
                             &copy);
    p_vkEndCommandBuffer(cb);
    memset(&submit, 0, sizeof(submit));
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cb;
    p_vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE);
    p_vkQueueWaitIdle(queue);

    got = (const unsigned char*)linear;
    for (z = 0; z < slices; z++)
        for (y = 0; y < img_height; y++)
            for (x = 0; x < width; x++)
                if (memcmp(got + ((z * img_height + y) * width + x) * texel_bytes,
                           reference + ((z * height + y) * pitch + x) * texel_bytes,
                           texel_bytes) != 0)
                    ++bad;
    CHECK(bad == 0, what);
    /* Nothing past the tiled data bound was read: the reference is not all one value */
    for (i = 1; i < bytes && reference[i] == reference[0]; i++)
        ;
    CHECK(i < bytes, what);

    p_vkDestroyPipeline(device, pipeline, NULL);
    p_vkDestroyShaderModule(device, module, NULL);
    p_vkDestroyImageView(device, view, NULL);
    p_vkDestroyImage(device, image, NULL);
    p_vkFreeMemory(device, memory, NULL);
}

static void round_trip(uint32_t thickness, uint32_t pitch, uint32_t height, const char* what) {
    uint32_t i, same = 0, sum = 0;
    set_info(1, pitch, height);
    for (i = 0; i < TEXELS + GUARD; i++) {
        linear[i] = i < TEXELS ? i + 1u : SENTINEL;
        tiled[i] = SENTINEL;
    }
    CHECK(run(1, thickness, TEXELS / 64, TEXELS), what);
    for (i = 0; i < TEXELS; i++) {
        same += tiled[i] == i + 1u;
        sum += tiled[i];
    }
    /* Every texel is there once, and not where it was */
    CHECK(sum == TEXELS * (TEXELS + 1u) / 2u && same < TEXELS / 2, what);
    for (i = 0; i < TEXELS; i++)
        linear[i] = 0;
    CHECK(run(0, thickness, TEXELS / 64, TEXELS), what);
    for (i = 0; i < TEXELS; i++)
        if (linear[i] != i + 1u)
            break;
    CHECK(i == TEXELS, what);
    for (i = TEXELS; i < TEXELS + GUARD; i++)
        if (linear[i] != SENTINEL || tiled[i] != SENTINEL)
            break;
    CHECK(i == TEXELS + GUARD, what);
}

int main(void) {
    void* lib = dlopen("libvulkan.so.1", RTLD_NOW);
    VkInstanceCreateInfo instance_info;
    VkApplicationInfo app;
    VkInstance instance;
    VkPhysicalDeviceFeatures supported, enabled;
    VkDeviceQueueCreateInfo queue_info;
    VkDeviceCreateInfo device_info;
    VkDescriptorSetLayoutBinding bindings[3];
    VkDescriptorSetLayoutCreateInfo sli;
    VkPipelineLayoutCreateInfo pli;
    VkDescriptorPoolSize sizes[3];
    VkPushConstantRange push_range;
    VkDescriptorPoolCreateInfo dpi;
    VkDescriptorSetAllocateInfo dai;
    VkDescriptorPool dpool;
    VkCommandPoolCreateInfo pool_info;
    VkCommandPool pool;
    VkCommandBufferAllocateInfo cb_info;
    float priority = 1.0f;
    uint32_t count = 1, i;

    if (lib)
        *(void**)&get_instance_proc = dlsym(lib, "vkGetInstanceProcAddr");
    if (!lib || !get_instance_proc) {
        printf("tiling_device: no Vulkan loader, skipped\n");
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
        printf("tiling_device: no Vulkan instance, skipped\n");
        return 0;
    }
#define LOADI(name) p_##name = (PFN_##name)get_instance_proc(instance, #name)
    LOADI(vkEnumeratePhysicalDevices);
    LOADI(vkGetPhysicalDeviceMemoryProperties);
    LOADI(vkCreateDevice);
    LOADI(vkGetPhysicalDeviceFeatures);
    LOADI(vkGetPhysicalDeviceFormatProperties);
    get_device_proc = (PFN_vkGetDeviceProcAddr)get_instance_proc(instance, "vkGetDeviceProcAddr");
    if (p_vkEnumeratePhysicalDevices(instance, &count, &physical) < 0 || !count) {
        printf("tiling_device: no device, skipped\n");
        return 0;
    }
    p_vkGetPhysicalDeviceMemoryProperties(physical, &mem);
    p_vkGetPhysicalDeviceFeatures(physical, &supported);
    memset(&enabled, 0, sizeof(enabled));
    enabled.shaderStorageImageExtendedFormats = supported.shaderStorageImageExtendedFormats;
    extended_formats = supported.shaderStorageImageExtendedFormats != 0;
    memset(&queue_info, 0, sizeof(queue_info));
    queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info.queueCount = 1;
    queue_info.pQueuePriorities = &priority;
    memset(&device_info, 0, sizeof(device_info));
    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.queueCreateInfoCount = 1;
    device_info.pQueueCreateInfos = &queue_info;
    device_info.pEnabledFeatures = &enabled;
    if (p_vkCreateDevice(physical, &device_info, NULL, &device) != VK_SUCCESS) {
        printf("tiling_device: no device, skipped\n");
        return 0;
    }
#define LOADD(name) p_##name = (PFN_##name)get_device_proc(device, #name)
    LOADD(vkGetDeviceQueue);
    LOADD(vkAllocateMemory);
    LOADD(vkMapMemory);
    LOADD(vkCreateBuffer);
    LOADD(vkGetBufferMemoryRequirements);
    LOADD(vkBindBufferMemory);
    LOADD(vkCreateShaderModule);
    LOADD(vkDestroyShaderModule);
    LOADD(vkCreateDescriptorSetLayout);
    LOADD(vkCreatePipelineLayout);
    LOADD(vkCreateComputePipelines);
    LOADD(vkDestroyPipeline);
    LOADD(vkCreateDescriptorPool);
    LOADD(vkAllocateDescriptorSets);
    LOADD(vkUpdateDescriptorSets);
    LOADD(vkCreateCommandPool);
    LOADD(vkAllocateCommandBuffers);
    LOADD(vkBeginCommandBuffer);
    LOADD(vkEndCommandBuffer);
    LOADD(vkCmdBindPipeline);
    LOADD(vkCmdBindDescriptorSets);
    LOADD(vkCmdDispatch);
    LOADD(vkQueueSubmit);
    LOADD(vkQueueWaitIdle);
    LOADD(vkCreateImage);
    LOADD(vkDestroyImage);
    LOADD(vkGetImageMemoryRequirements);
    LOADD(vkBindImageMemory);
    LOADD(vkCreateImageView);
    LOADD(vkDestroyImageView);
    LOADD(vkFreeMemory);
    LOADD(vkCmdPipelineBarrier);
    LOADD(vkCmdCopyImageToBuffer);
    LOADD(vkCmdPushConstants);
    p_vkGetDeviceQueue(device, 0, 0, &queue);

    tiled = (uint32_t*)make_buffer(&tiled_buf, (TEXELS + GUARD) * 4,
                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, &mem);
    linear = (uint32_t*)make_buffer(&linear_buf, (TEXELS + GUARD) * 4,
                                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                    &mem);
    info = (uint32_t*)make_buffer(&info_buf, 64 * 4, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &mem);
    if (!tiled || !linear || !info) {
        printf("tiling_device: no memory the host can see, skipped\n");
        return 0;
    }
    memset(bindings, 0, sizeof(bindings));
    for (i = 0; i < 3; i++) {
        bindings[i].binding = i;
        bindings[i].descriptorCount = 1;
        bindings[i].descriptorType =
            i == 2 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }
    memset(&sli, 0, sizeof(sli));
    sli.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    sli.bindingCount = 3;
    sli.pBindings = bindings;
    p_vkCreateDescriptorSetLayout(device, &sli, NULL, &set_layout);
    memset(&pli, 0, sizeof(pli));
    pli.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pli.setLayoutCount = 1;
    pli.pSetLayouts = &set_layout;
    p_vkCreatePipelineLayout(device, &pli, NULL, &layout);
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    p_vkCreateDescriptorSetLayout(device, &sli, NULL, &image_set_layout);
    push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    push_range.offset = 0;
    push_range.size = 16;
    pli.pSetLayouts = &image_set_layout;
    pli.pushConstantRangeCount = 1;
    pli.pPushConstantRanges = &push_range;
    p_vkCreatePipelineLayout(device, &pli, NULL, &image_layout);
    sizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    sizes[0].descriptorCount = 3;
    sizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    sizes[1].descriptorCount = 2;
    sizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    sizes[2].descriptorCount = 1;
    memset(&dpi, 0, sizeof(dpi));
    dpi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    dpi.maxSets = 2;
    dpi.poolSizeCount = 3;
    dpi.pPoolSizes = sizes;
    p_vkCreateDescriptorPool(device, &dpi, NULL, &dpool);
    memset(&dai, 0, sizeof(dai));
    dai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dai.descriptorPool = dpool;
    dai.descriptorSetCount = 1;
    dai.pSetLayouts = &set_layout;
    p_vkAllocateDescriptorSets(device, &dai, &set);
    dai.pSetLayouts = &image_set_layout;
    p_vkAllocateDescriptorSets(device, &dai, &image_set);
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

    round_trip(1, 16, 16, "thin round trip");
    round_trip(4, 8, 8, "thick round trip");

    /* Twice the texels the one mip has: those past it leave everything alone */
    set_info(1, 16, 16);
    for (i = 0; i < TEXELS + GUARD; i++) {
        linear[i] = i + 1u;
        tiled[i] = SENTINEL;
    }
    CHECK(run(1, 1, (TEXELS + GUARD) / 64, TEXELS + GUARD), "tiler past the last mip");
    for (i = TEXELS; i < TEXELS + GUARD; i++)
        if (tiled[i] != SENTINEL)
            break;
    CHECK(i == TEXELS + GUARD, "tiler past the last mip");
    for (i = 0; i < TEXELS; i++)
        if (tiled[i] == SENTINEL || tiled[i] > TEXELS)
            break;
    CHECK(i == TEXELS, "tiler past the last mip");
    for (i = 0; i < TEXELS + GUARD; i++)
        linear[i] = SENTINEL;
    CHECK(run(0, 1, (TEXELS + GUARD) / 64, TEXELS + GUARD), "detiler past the last mip");
    for (i = TEXELS; i < TEXELS + GUARD; i++)
        if (linear[i] != SENTINEL)
            break;
    CHECK(i == TEXELS + GUARD, "detiler past the last mip");

    /* Half of the tiled data bound: the half that is not is not written */
    for (i = 0; i < TEXELS + GUARD; i++) {
        linear[i] = i + 1u;
        tiled[i] = SENTINEL;
    }
    CHECK(run(1, 1, TEXELS / 64, TEXELS / 2), "tiler past the bound data");
    for (i = TEXELS / 2; i < TEXELS + GUARD; i++)
        if (tiled[i] != SENTINEL)
            break;
    CHECK(i == TEXELS + GUARD, "tiler past the bound data");

    into_image(32, 0, 1, 16, 16, 1, 16, 16, "32-bit into an image");
    into_image(32, 0, 1, 16, 16, 1, 12, 10, "32-bit into an image narrower than its pitch");
    into_image(8, 0, 1, 16, 16, 1, 13, 16, "8-bit into an image");
    into_image(16, 0, 1, 16, 16, 1, 16, 15, "16-bit into an image");
    into_image(64, 0, 1, 16, 16, 1, 16, 16, "64-bit into an image");
    into_image(128, 0, 1, 8, 8, 1, 7, 8, "128-bit into an image");
    into_image(32, 0, 1, 8, 8, 4, 8, 8, "32-bit into the layers of an image");
    into_image(32, 1, 4, 8, 8, 4, 8, 8, "thick 32-bit into a 3D image");

    if (failures) {
        printf("tiling_device: %d failed\n", failures);
        return 1;
    }
    printf("tiling_device: all passed\n");
    return 0;
}
