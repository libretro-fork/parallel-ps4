/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The tiling compute shader (host_spirv_tiling_comp) run on a device: texels
 * tiled and detiled again are the texels they were, and a texel that has no
 * place in the image (past the last mip, or past the end of the tiled data
 * that is bound) is neither read nor written. Skipped (exit 0) with no device.
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

#define TEXELS 256u /* of the image: 16 by 16, or 8 by 8 by 4 */
#define GUARD 256u  /* words past it that nothing may touch */
#define SENTINEL 0xdeadbeefu

static VkDevice device;
static VkQueue queue;
static VkCommandBuffer cb;
static VkPipelineLayout layout;
static VkDescriptorSetLayout set_layout;
static VkDescriptorSet set;
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

/* Runs the shader for @thickness (1 or 4) over @groups groups of 64 texels,
 * with @tiled_words of the tiled data bound */
static int run(int is_tiler, uint32_t thickness, uint32_t groups, uint32_t tiled_words) {
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
    n = host_spirv_tiling_comp(words, 32768, 32, 0, 0, params, 4);
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

/* One mip of TEXELS texels of @pitch by @height */
static void set_info(uint32_t num_mips, uint32_t pitch, uint32_t height) {
    memset(info, 0, 64 * 4);
    info[2] = num_mips;
    info[3] = TEXELS * 4;
    info[4] = pitch | height << 16;
    info[5] = 0;
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
    VkPhysicalDevice physical;
    VkPhysicalDeviceMemoryProperties mem;
    VkDeviceQueueCreateInfo queue_info;
    VkDeviceCreateInfo device_info;
    VkDescriptorSetLayoutBinding bindings[3];
    VkDescriptorSetLayoutCreateInfo sli;
    VkPipelineLayoutCreateInfo pli;
    VkDescriptorPoolSize sizes[2];
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
    get_device_proc = (PFN_vkGetDeviceProcAddr)get_instance_proc(instance, "vkGetDeviceProcAddr");
    if (p_vkEnumeratePhysicalDevices(instance, &count, &physical) < 0 || !count) {
        printf("tiling_device: no device, skipped\n");
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
    p_vkGetDeviceQueue(device, 0, 0, &queue);

    tiled = (uint32_t*)make_buffer(&tiled_buf, (TEXELS + GUARD) * 4,
                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, &mem);
    linear = (uint32_t*)make_buffer(&linear_buf, (TEXELS + GUARD) * 4,
                                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, &mem);
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
    sizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    sizes[0].descriptorCount = 2;
    sizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    sizes[1].descriptorCount = 1;
    memset(&dpi, 0, sizeof(dpi));
    dpi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    dpi.maxSets = 1;
    dpi.poolSizeCount = 2;
    dpi.pPoolSizes = sizes;
    p_vkCreateDescriptorPool(device, &dpi, NULL, &dpool);
    memset(&dai, 0, sizeof(dai));
    dai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dai.descriptorPool = dpool;
    dai.descriptorSetCount = 1;
    dai.pSetLayouts = &set_layout;
    p_vkAllocateDescriptorSets(device, &dai, &set);
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

    if (failures) {
        printf("tiling_device: %d failed\n", failures);
        return 1;
    }
    printf("tiling_device: all passed\n");
    return 0;
}
