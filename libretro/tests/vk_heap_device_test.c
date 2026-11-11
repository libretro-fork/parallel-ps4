/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The renderer's Vulkan memory heap (src/video_core/renderer_vulkan/vk_heap.c) on a real
 * device: buffers with device addresses and images of both tilings bound to its memory as
 * the renderer binds them, mapped pieces written and read back through a copy on the GPU,
 * dedicated memory where the driver asks, all freed. Run with the validation layer present
 * and it is enabled: any message from it is a failure. Skipped (exit 0) with no device.
 *
 *   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json build-tests/vk_heap_device_test */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dlfcn.h>

#include "../../src/video_core/renderer_vulkan/vk_heap.h"

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
#define IFN(name) PFN_##name p_##name
static IFN(vkCreateInstance);
static IFN(vkEnumeratePhysicalDevices);
static IFN(vkGetPhysicalDeviceProperties);
static IFN(vkGetPhysicalDeviceMemoryProperties);
static IFN(vkGetPhysicalDeviceMemoryProperties2);
static IFN(vkGetPhysicalDeviceQueueFamilyProperties);
static IFN(vkCreateDevice);
static IFN(vkDestroyInstance);
static IFN(vkCreateDebugUtilsMessengerEXT);
static IFN(vkDestroyDebugUtilsMessengerEXT);
static IFN(vkDestroyDevice);
static IFN(vkGetDeviceQueue);
static IFN(vkAllocateMemory);
static IFN(vkFreeMemory);
static IFN(vkMapMemory);
static IFN(vkUnmapMemory);
static IFN(vkFlushMappedMemoryRanges);
static IFN(vkInvalidateMappedMemoryRanges);
static IFN(vkCreateBuffer);
static IFN(vkDestroyBuffer);
static IFN(vkCreateImage);
static IFN(vkDestroyImage);
static IFN(vkGetBufferMemoryRequirements2);
static IFN(vkGetImageMemoryRequirements2);
static IFN(vkBindBufferMemory);
static IFN(vkBindImageMemory);
static IFN(vkGetBufferDeviceAddress);
static IFN(vkCreateCommandPool);
static IFN(vkDestroyCommandPool);
static IFN(vkAllocateCommandBuffers);
static IFN(vkBeginCommandBuffer);
static IFN(vkEndCommandBuffer);
static IFN(vkCmdCopyBuffer);
static IFN(vkQueueSubmit);
static IFN(vkQueueWaitIdle);

static long validation_messages = 0;

static VkBool32 VKAPI_CALL on_message(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                      VkDebugUtilsMessageTypeFlagsEXT types,
                                      const VkDebugUtilsMessengerCallbackDataEXT* data,
                                      void* user) {
    (void)types;
    (void)user;
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        printf("validation: %s\n", data->pMessage);
        ++validation_messages;
    }
    return VK_FALSE;
}

typedef struct requirements_t {
    VkMemoryRequirements memory;
    int dedicated;
} requirements_t;

static requirements_t buffer_requirements(VkDevice device, VkBuffer buffer) {
    VkBufferMemoryRequirementsInfo2 info;
    VkMemoryDedicatedRequirements dedicated;
    VkMemoryRequirements2 out;
    requirements_t r;
    memset(&info, 0, sizeof(info));
    info.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2;
    info.buffer = buffer;
    memset(&dedicated, 0, sizeof(dedicated));
    dedicated.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS;
    memset(&out, 0, sizeof(out));
    out.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
    out.pNext = &dedicated;
    p_vkGetBufferMemoryRequirements2(device, &info, &out);
    r.memory = out.memoryRequirements;
    r.dedicated = dedicated.prefersDedicatedAllocation || dedicated.requiresDedicatedAllocation;
    return r;
}

static requirements_t image_requirements(VkDevice device, VkImage image) {
    VkImageMemoryRequirementsInfo2 info;
    VkMemoryDedicatedRequirements dedicated;
    VkMemoryRequirements2 out;
    requirements_t r;
    memset(&info, 0, sizeof(info));
    info.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2;
    info.image = image;
    memset(&dedicated, 0, sizeof(dedicated));
    dedicated.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS;
    memset(&out, 0, sizeof(out));
    out.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
    out.pNext = &dedicated;
    p_vkGetImageMemoryRequirements2(device, &info, &out);
    r.memory = out.memoryRequirements;
    r.dedicated = dedicated.prefersDedicatedAllocation || dedicated.requiresDedicatedAllocation;
    return r;
}

#define BUFFERS 300
#define IMAGES 120

int main(void) {
    void* lib = dlopen("libvulkan.so.1", RTLD_NOW);
    VkInstanceCreateInfo instance_info;
    VkApplicationInfo app;
    VkDebugUtilsMessengerCreateInfoEXT messenger_info;
    VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
    VkInstance instance;
    VkPhysicalDevice physical;
    VkPhysicalDeviceProperties props;
    VkPhysicalDeviceVulkan12Features features12;
    VkPhysicalDeviceFeatures2 features;
    VkDeviceQueueCreateInfo queue_info;
    VkDeviceCreateInfo device_info;
    VkDevice device;
    VkQueue queue;
    vk_heap_info_t heap_info;
    vk_heap_t* heap;
    vk_heap_request_t request;
    VkBufferCreateInfo buffer_info;
    VkImageCreateInfo image_info;
    VkBuffer buffers[BUFFERS];
    vk_heap_alloc_t* buffer_allocs[BUFFERS];
    VkImage images[IMAGES];
    vk_heap_alloc_t* image_allocs[IMAGES];
    requirements_t req;
    VkBufferDeviceAddressInfo address_info;
    VkCommandPoolCreateInfo pool_info;
    VkCommandPool pool;
    VkCommandBufferAllocateInfo cb_info;
    VkCommandBuffer cb;
    VkCommandBufferBeginInfo begin;
    VkBufferCopy copy;
    VkSubmitInfo submit;
    VkDeviceSize reserved, used;
    const char* layer = "VK_LAYER_KHRONOS_validation";
    const char* debug_ext = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    float priority = 1.0f;
    uint32_t count = 1, i;
    int with_layer = 1, dedicated_seen = 0, mapped_ok = 1;
    VkResult result;

    if (lib) {
        /* A function pointer from dlsym's object pointer, as POSIX has it */
        *(void**)&get_instance_proc = dlsym(lib, "vkGetInstanceProcAddr");
    }
    if (!lib || !get_instance_proc) {
        printf("vk_heap_device: no Vulkan loader, skipped\n");
        return 0;
    }
    p_vkCreateInstance = (PFN_vkCreateInstance)get_instance_proc(NULL, "vkCreateInstance");
    memset(&app, 0, sizeof(app));
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.apiVersion = VK_API_VERSION_1_3;
    memset(&messenger_info, 0, sizeof(messenger_info));
    messenger_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    messenger_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    messenger_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;
    messenger_info.pfnUserCallback = on_message;
    memset(&instance_info, 0, sizeof(instance_info));
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pApplicationInfo = &app;
    instance_info.enabledLayerCount = 1;
    instance_info.ppEnabledLayerNames = &layer;
    instance_info.enabledExtensionCount = 1;
    instance_info.ppEnabledExtensionNames = &debug_ext;
    if (p_vkCreateInstance(&instance_info, NULL, &instance) != VK_SUCCESS) {
        with_layer = 0;
        instance_info.enabledLayerCount = 0;
        instance_info.enabledExtensionCount = 0;
        if (p_vkCreateInstance(&instance_info, NULL, &instance) != VK_SUCCESS) {
            printf("vk_heap_device: no Vulkan instance, skipped\n");
            return 0;
        }
    }
#define LOADI(name) p_##name = (PFN_##name)get_instance_proc(instance, #name)
    LOADI(vkEnumeratePhysicalDevices);
    LOADI(vkGetPhysicalDeviceProperties);
    LOADI(vkGetPhysicalDeviceMemoryProperties);
    LOADI(vkGetPhysicalDeviceMemoryProperties2);
    LOADI(vkGetPhysicalDeviceQueueFamilyProperties);
    LOADI(vkCreateDevice);
    LOADI(vkDestroyInstance);
    LOADI(vkCreateDebugUtilsMessengerEXT);
    LOADI(vkDestroyDebugUtilsMessengerEXT);
    get_device_proc = (PFN_vkGetDeviceProcAddr)get_instance_proc(instance, "vkGetDeviceProcAddr");
    if (with_layer) {
        p_vkCreateDebugUtilsMessengerEXT(instance, &messenger_info, NULL, &messenger);
    }
    if (p_vkEnumeratePhysicalDevices(instance, &count, &physical) < 0 || !count) {
        printf("vk_heap_device: no device, skipped\n");
        return 0;
    }
    p_vkGetPhysicalDeviceProperties(physical, &props);
    printf("vk_heap_device: %s%s\n", props.deviceName, with_layer ? ", validation on" : "");

    memset(&features12, 0, sizeof(features12));
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.bufferDeviceAddress = VK_TRUE;
    memset(&features, 0, sizeof(features));
    features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features.pNext = &features12;
    memset(&queue_info, 0, sizeof(queue_info));
    queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info.queueFamilyIndex = 0;
    queue_info.queueCount = 1;
    queue_info.pQueuePriorities = &priority;
    memset(&device_info, 0, sizeof(device_info));
    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.pNext = &features;
    device_info.queueCreateInfoCount = 1;
    device_info.pQueueCreateInfos = &queue_info;
    result = p_vkCreateDevice(physical, &device_info, NULL, &device);
    CHECK(result == VK_SUCCESS, "device made");
    if (result != VK_SUCCESS) {
        return 1;
    }
#define LOADD(name) p_##name = (PFN_##name)get_device_proc(device, #name)
    LOADD(vkDestroyDevice);
    LOADD(vkGetDeviceQueue);
    LOADD(vkAllocateMemory);
    LOADD(vkFreeMemory);
    LOADD(vkMapMemory);
    LOADD(vkUnmapMemory);
    LOADD(vkFlushMappedMemoryRanges);
    LOADD(vkInvalidateMappedMemoryRanges);
    LOADD(vkCreateBuffer);
    LOADD(vkDestroyBuffer);
    LOADD(vkCreateImage);
    LOADD(vkDestroyImage);
    LOADD(vkGetBufferMemoryRequirements2);
    LOADD(vkGetImageMemoryRequirements2);
    LOADD(vkBindBufferMemory);
    LOADD(vkBindImageMemory);
    LOADD(vkGetBufferDeviceAddress);
    LOADD(vkCreateCommandPool);
    LOADD(vkDestroyCommandPool);
    LOADD(vkAllocateCommandBuffers);
    LOADD(vkBeginCommandBuffer);
    LOADD(vkEndCommandBuffer);
    LOADD(vkCmdCopyBuffer);
    LOADD(vkQueueSubmit);
    LOADD(vkQueueWaitIdle);
    p_vkGetDeviceQueue(device, 0, 0, &queue);

    memset(&heap_info, 0, sizeof(heap_info));
    heap_info.physical_device = physical;
    heap_info.device = device;
    p_vkGetPhysicalDeviceMemoryProperties(physical, &heap_info.memory);
    heap_info.buffer_image_granularity = props.limits.bufferImageGranularity;
    heap_info.non_coherent_atom_size = props.limits.nonCoherentAtomSize;
    heap_info.device_address = 1;
    heap_info.fns.allocate_memory = p_vkAllocateMemory;
    heap_info.fns.free_memory = p_vkFreeMemory;
    heap_info.fns.map_memory = p_vkMapMemory;
    heap_info.fns.unmap_memory = p_vkUnmapMemory;
    heap_info.fns.flush_ranges = p_vkFlushMappedMemoryRanges;
    heap_info.fns.invalidate_ranges = p_vkInvalidateMappedMemoryRanges;
    heap_info.fns.get_memory_properties2 = NULL;
    heap = vk_heap_create(&heap_info);
    CHECK(heap != NULL, "heap made");

    /* Buffers as the buffer cache makes them: every usage, a device address, mapped or not */
    memset(&buffer_info, 0, sizeof(buffer_info));
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    for (i = 0; i < BUFFERS; ++i) {
        buffer_info.size = 256 + (VkDeviceSize)(i * 7919u % 2000000u);
        result = p_vkCreateBuffer(device, &buffer_info, NULL, &buffers[i]);
        CHECK(result == VK_SUCCESS, "buffer made");
        req = buffer_requirements(device, buffers[i]);
        memset(&request, 0, sizeof(request));
        request.flags = VK_HEAP_LINEAR | (i % 3 ? VK_HEAP_MAPPED : 0) | (i % 5 == 0 ? VK_HEAP_DEDICATED : 0);
        request.preferred = i % 3 ? VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
                                  : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        request.buffer = buffers[i];
        result = vk_heap_alloc(heap, &req.memory, req.dedicated, &request, &buffer_allocs[i]);
        CHECK(result == VK_SUCCESS, "buffer memory");
        if (result != VK_SUCCESS) {
            return 1;
        }
        dedicated_seen |= req.dedicated;
        CHECK(buffer_allocs[i]->offset % req.memory.alignment == 0, "buffer aligned");
        CHECK(p_vkBindBufferMemory(device, buffers[i], buffer_allocs[i]->memory,
                                   buffer_allocs[i]->offset) == VK_SUCCESS,
              "buffer bound");
        memset(&address_info, 0, sizeof(address_info));
        address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        address_info.buffer = buffers[i];
        CHECK(p_vkGetBufferDeviceAddress(device, &address_info) != 0, "device address");
        if (buffer_allocs[i]->mapped) {
            memset(buffer_allocs[i]->mapped, (int)(i & 0xFF), (size_t)buffer_info.size);
            vk_heap_flush(heap, buffer_allocs[i], 0, VK_WHOLE_SIZE);
        } else if (i % 3) {
            mapped_ok = 0;
        }
    }
    CHECK(mapped_ok, "mapped buffers mapped");

    /* The GPU copies one mapped buffer into another; the host reads it back */
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
    copy.srcOffset = 0;
    copy.dstOffset = 0;
    copy.size = 256;
    p_vkCmdCopyBuffer(cb, buffers[1], buffers[2], 1, &copy);
    p_vkEndCommandBuffer(cb);
    memset(&submit, 0, sizeof(submit));
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cb;
    p_vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE);
    p_vkQueueWaitIdle(queue);
    vk_heap_invalidate(heap, buffer_allocs[2], 0, 256);
    CHECK(((unsigned char*)buffer_allocs[2]->mapped)[0] == 1 &&
              ((unsigned char*)buffer_allocs[2]->mapped)[255] == 1 &&
              ((unsigned char*)buffer_allocs[2]->mapped)[256] == 2,
          "the GPU's copy seen through the mapping");

    /* Images as the texture cache makes them, optimal and linear */
    memset(&image_info, 0, sizeof(image_info));
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.format = VK_FORMAT_R8G8B8A8_UNORM;
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                       VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    for (i = 0; i < IMAGES; ++i) {
        image_info.extent.width = 16u << (i % 7);
        image_info.extent.height = 16u << ((i / 7) % 6);
        image_info.tiling = i % 4 == 3 ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL;
        image_info.mipLevels = image_info.tiling == VK_IMAGE_TILING_LINEAR ? 1 : 1 + i % 3;
        result = p_vkCreateImage(device, &image_info, NULL, &images[i]);
        CHECK(result == VK_SUCCESS, "image made");
        req = image_requirements(device, images[i]);
        memset(&request, 0, sizeof(request));
        request.preferred = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        request.not_preferred = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        request.flags = image_info.tiling == VK_IMAGE_TILING_LINEAR ? VK_HEAP_LINEAR : 0;
        request.image = images[i];
        result = vk_heap_alloc(heap, &req.memory, req.dedicated, &request, &image_allocs[i]);
        CHECK(result == VK_SUCCESS, "image memory");
        if (result != VK_SUCCESS) {
            return 1;
        }
        dedicated_seen |= req.dedicated;
        CHECK(p_vkBindImageMemory(device, images[i], image_allocs[i]->memory,
                                  image_allocs[i]->offset) == VK_SUCCESS,
              "image bound");
    }
    vk_heap_usage(heap, 0, &reserved, &used);
    printf("vk_heap_device: heap 0 %llu KiB held, %llu KiB handed out%s\n",
           (unsigned long long)(reserved >> 10), (unsigned long long)(used >> 10),
           dedicated_seen ? ", the driver asked for dedicated memory" : "");

    /* Freed in another order than made */
    for (i = 0; i < IMAGES; i += 2) {
        p_vkDestroyImage(device, images[i], NULL);
        vk_heap_free(heap, image_allocs[i]);
    }
    for (i = BUFFERS; i-- > 0;) {
        p_vkDestroyBuffer(device, buffers[i], NULL);
        vk_heap_free(heap, buffer_allocs[i]);
    }
    for (i = 1; i < IMAGES; i += 2) {
        p_vkDestroyImage(device, images[i], NULL);
        vk_heap_free(heap, image_allocs[i]);
    }
    for (i = 0; i < heap_info.memory.memoryHeapCount; ++i) {
        vk_heap_usage(heap, i, &reserved, &used);
        CHECK(used == 0, "all handed back");
    }
    vk_heap_destroy(heap);
    p_vkDestroyCommandPool(device, pool, NULL);
    p_vkDestroyDevice(device, NULL);
    if (messenger) {
        p_vkDestroyDebugUtilsMessengerEXT(instance, messenger, NULL);
    }
    p_vkDestroyInstance(instance, NULL);
    CHECK(validation_messages == 0, "no validation messages");
    if (failures) {
        printf("%d failures\n", failures);
        return 1;
    }
    printf("vk_heap_device: all passed\n");
    return 0;
}
