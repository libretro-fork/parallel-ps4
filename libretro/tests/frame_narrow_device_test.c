/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The frame's ten bits a channel, on a device: the format the frames are made in
 * (vk_swapchain.cpp) keeps a ten-bit value, and the narrowing a readback does
 * (vk_presenter.cpp, NarrowToReadback: a blit to B8G8R8A8, then a copy to a
 * buffer) gives the frontend's XRGB8888 bytes, B, G, R, X, each rounded to the
 * nearest step. Skipped (exit 0) with no device.
 *
 *   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json \
 *       build-tests/frame_narrow_device_test */

#include <stdio.h>
#include <string.h>

#include <dlfcn.h>

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>

static int failures = 0;

#define CHECK(cond, what)                                                                      \
    do {                                                                                       \
        if (!(cond)) {                                                                         \
            printf("FAIL %s (line %d)\n", what, __LINE__);                                     \
            ++failures;                                                                        \
        }                                                                                      \
    } while (0)

#define FN(name) static PFN_##name p_##name
FN(vkCreateInstance);
FN(vkEnumeratePhysicalDevices);
FN(vkGetPhysicalDeviceMemoryProperties);
FN(vkGetPhysicalDeviceFormatProperties);
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
FN(vkCreateCommandPool);
FN(vkAllocateCommandBuffers);
FN(vkBeginCommandBuffer);
FN(vkEndCommandBuffer);
FN(vkCmdPipelineBarrier);
FN(vkCmdClearColorImage);
FN(vkCmdBlitImage);
FN(vkCmdCopyImageToBuffer);
FN(vkQueueSubmit);
FN(vkQueueWaitIdle);

#define W 4u
#define H 2u

static VkDevice device;
static VkPhysicalDeviceMemoryProperties mem;

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

static VkImage make_image(VkFormat format) {
    VkImageCreateInfo ii;
    VkMemoryRequirements req;
    VkDeviceMemory memory;
    VkImage image = VK_NULL_HANDLE;
    memset(&ii, 0, sizeof(ii));
    ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.format = format;
    ii.extent.width = W;
    ii.extent.height = H;
    ii.extent.depth = 1;
    ii.mipLevels = 1;
    ii.arrayLayers = 1;
    ii.samples = VK_SAMPLE_COUNT_1_BIT;
    ii.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (p_vkCreateImage(device, &ii, NULL, &image) != VK_SUCCESS)
        return VK_NULL_HANDLE;
    p_vkGetImageMemoryRequirements(device, image, &req);
    memory = alloc(&req, 0);
    if (!memory)
        return VK_NULL_HANDLE;
    p_vkBindImageMemory(device, image, memory, 0);
    return image;
}

static void barrier(VkCommandBuffer cb, VkImage image, VkImageLayout from, VkImageLayout to) {
    VkImageMemoryBarrier b;
    memset(&b, 0, sizeof(b));
    b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT;
    b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT;
    b.oldLayout = from;
    b.newLayout = to;
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = image;
    b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    b.subresourceRange.levelCount = 1;
    b.subresourceRange.layerCount = 1;
    p_vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                           0, NULL, 0, NULL, 1, &b);
}

int main(void) {
    void* lib = dlopen("libvulkan.so.1", RTLD_NOW);
    PFN_vkGetInstanceProcAddr gip = NULL;
    PFN_vkGetDeviceProcAddr gdp;
    VkApplicationInfo app;
    VkInstanceCreateInfo instance_info;
    VkInstance instance;
    VkPhysicalDevice physical;
    VkDeviceQueueCreateInfo queue_info;
    VkDeviceCreateInfo device_info;
    VkFormatProperties props;
    VkQueue queue;
    VkCommandPoolCreateInfo pool_info;
    VkCommandPool pool;
    VkCommandBufferAllocateInfo cb_info;
    VkCommandBuffer cb;
    VkCommandBufferBeginInfo begin;
    VkSubmitInfo si;
    VkBufferCreateInfo bi;
    VkBuffer buffer;
    VkMemoryRequirements req;
    VkDeviceMemory buffer_memory;
    void* mapped = NULL;
    VkImage wide, wide_back, narrow;
    VkClearColorValue colour;
    VkImageSubresourceRange range;
    VkImageBlit blit;
    VkBufferImageCopy copy;
    const unsigned char* px;
    uint32_t wide_texel;
    float priority = 1.0f;
    uint32_t count = 1;

    if (lib)
        *(void**)&gip = dlsym(lib, "vkGetInstanceProcAddr");
    if (!gip) {
        printf("frame_narrow_device: no Vulkan loader, skipped\n");
        return 0;
    }
    p_vkCreateInstance = (PFN_vkCreateInstance)gip(NULL, "vkCreateInstance");
    memset(&app, 0, sizeof(app));
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.apiVersion = VK_API_VERSION_1_1;
    memset(&instance_info, 0, sizeof(instance_info));
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pApplicationInfo = &app;
    if (p_vkCreateInstance(&instance_info, NULL, &instance) != VK_SUCCESS) {
        printf("frame_narrow_device: no Vulkan instance, skipped\n");
        return 0;
    }
#define LOADI(name) p_##name = (PFN_##name)gip(instance, #name)
    LOADI(vkEnumeratePhysicalDevices);
    LOADI(vkGetPhysicalDeviceMemoryProperties);
    LOADI(vkGetPhysicalDeviceFormatProperties);
    LOADI(vkCreateDevice);
    gdp = (PFN_vkGetDeviceProcAddr)gip(instance, "vkGetDeviceProcAddr");
    if (p_vkEnumeratePhysicalDevices(instance, &count, &physical) < 0 || !count) {
        printf("frame_narrow_device: no device, skipped\n");
        return 0;
    }
    p_vkGetPhysicalDeviceMemoryProperties(physical, &mem);

    /* What the swapchain asks of the format, which Vulkan requires of every device */
    p_vkGetPhysicalDeviceFormatProperties(physical, VK_FORMAT_A2B10G10R10_UNORM_PACK32, &props);
    CHECK((props.optimalTilingFeatures &
           (VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT |
            VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_BLIT_SRC_BIT)) ==
              (VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
               VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
               VK_FORMAT_FEATURE_BLIT_SRC_BIT),
          "ten-bit frames can be drawn, blended, sampled and blitted from");

    memset(&queue_info, 0, sizeof(queue_info));
    queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info.queueCount = 1;
    queue_info.pQueuePriorities = &priority;
    memset(&device_info, 0, sizeof(device_info));
    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.queueCreateInfoCount = 1;
    device_info.pQueueCreateInfos = &queue_info;
    if (p_vkCreateDevice(physical, &device_info, NULL, &device) != VK_SUCCESS) {
        printf("frame_narrow_device: no device, skipped\n");
        return 0;
    }
#define LOADD(name) p_##name = (PFN_##name)gdp(device, #name)
    LOADD(vkGetDeviceQueue);
    LOADD(vkAllocateMemory);
    LOADD(vkMapMemory);
    LOADD(vkCreateBuffer);
    LOADD(vkGetBufferMemoryRequirements);
    LOADD(vkBindBufferMemory);
    LOADD(vkCreateImage);
    LOADD(vkGetImageMemoryRequirements);
    LOADD(vkBindImageMemory);
    LOADD(vkCreateCommandPool);
    LOADD(vkAllocateCommandBuffers);
    LOADD(vkBeginCommandBuffer);
    LOADD(vkEndCommandBuffer);
    LOADD(vkCmdPipelineBarrier);
    LOADD(vkCmdClearColorImage);
    LOADD(vkCmdBlitImage);
    LOADD(vkCmdCopyImageToBuffer);
    LOADD(vkQueueSubmit);
    LOADD(vkQueueWaitIdle);
    p_vkGetDeviceQueue(device, 0, 0, &queue);

    memset(&bi, 0, sizeof(bi));
    bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size = W * H * 4 * 2;
    bi.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    p_vkCreateBuffer(device, &bi, NULL, &buffer);
    p_vkGetBufferMemoryRequirements(device, buffer, &req);
    buffer_memory =
        alloc(&req, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (!buffer_memory) {
        printf("frame_narrow_device: no memory the host can see, skipped\n");
        return 0;
    }
    p_vkBindBufferMemory(device, buffer, buffer_memory, 0);
    p_vkMapMemory(device, buffer_memory, 0, VK_WHOLE_SIZE, 0, &mapped);

    wide = make_image(VK_FORMAT_A2B10G10R10_UNORM_PACK32);
    wide_back = wide;
    narrow = make_image(VK_FORMAT_B8G8R8A8_UNORM);
    if (!wide || !narrow) {
        printf("FAIL images\n");
        return 1;
    }

    memset(&pool_info, 0, sizeof(pool_info));
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    p_vkCreateCommandPool(device, &pool_info, NULL, &pool);
    memset(&cb_info, 0, sizeof(cb_info));
    cb_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cb_info.commandPool = pool;
    cb_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cb_info.commandBufferCount = 1;
    p_vkAllocateCommandBuffers(device, &cb_info, &cb);
    memset(&begin, 0, sizeof(begin));
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    p_vkBeginCommandBuffer(cb, &begin);

    /* Red 512 of 1023 (0.5005: 127.6 of 255, rounds to 128 where a cut gives 127), green 100
     * of 1023 (24.9: 25 where a cut gives 24), blue full */
    memset(&colour, 0, sizeof(colour));
    colour.float32[0] = 512.0f / 1023.0f;
    colour.float32[1] = 100.0f / 1023.0f;
    colour.float32[2] = 1.0f;
    colour.float32[3] = 1.0f;
    memset(&range, 0, sizeof(range));
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.levelCount = 1;
    range.layerCount = 1;
    barrier(cb, wide, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    p_vkCmdClearColorImage(cb, wide, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &colour, 1, &range);
    barrier(cb, wide, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    /* As NarrowToReadback does */
    barrier(cb, narrow, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    memset(&blit, 0, sizeof(blit));
    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.layerCount = 1;
    blit.srcOffsets[1].x = W;
    blit.srcOffsets[1].y = H;
    blit.srcOffsets[1].z = 1;
    blit.dstSubresource = blit.srcSubresource;
    blit.dstOffsets[1] = blit.srcOffsets[1];
    p_vkCmdBlitImage(cb, wide, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, narrow,
                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_NEAREST);
    barrier(cb, narrow, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    memset(&copy, 0, sizeof(copy));
    copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.imageSubresource.layerCount = 1;
    copy.imageExtent.width = W;
    copy.imageExtent.height = H;
    copy.imageExtent.depth = 1;
    p_vkCmdCopyImageToBuffer(cb, narrow, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1, &copy);
    /* And the ten-bit image itself, after it */
    copy.bufferOffset = W * H * 4;
    p_vkCmdCopyImageToBuffer(cb, wide_back, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1,
                             &copy);
    p_vkEndCommandBuffer(cb);
    memset(&si, 0, sizeof(si));
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cb;
    p_vkQueueSubmit(queue, 1, &si, VK_NULL_HANDLE);
    p_vkQueueWaitIdle(queue);

    px = (const unsigned char*)mapped;
    CHECK(px[0] == 255 && px[1] == 25 && px[2] == 128,
          "XRGB8888 bytes, B G R, each rounded to the nearest step");
    if (failures)
        printf("  got B %u G %u R %u\n", px[0], px[1], px[2]);
    memcpy(&wide_texel, px + W * H * 4, 4);
    CHECK((wide_texel & 1023u) == 512u && ((wide_texel >> 10) & 1023u) == 100u &&
              ((wide_texel >> 20) & 1023u) == 1023u,
          "the frame's format keeps all ten bits");

    if (failures) {
        printf("frame_narrow_device: %d failed\n", failures);
        return 1;
    }
    printf("frame_narrow_device: all passed\n");
    return 0;
}
