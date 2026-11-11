// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// The picture handed to the frontend on the GPU: the frontend's side. See gpu_frames.h.
//
// The emulator draws each frame into one of a few images of its own (vk_swapchain.cpp). With
// this it makes them of memory that can be exported, and hands over each image's memory once
// (host.cpp): on Linux a descriptor over a socket, on Windows an NT handle in the shared memory,
// which is duplicated from the emulator's process into this one. Here the same images are made in the frontend's
// Vulkan device, on that memory, and a finished frame is shown by telling the frontend which
// image to draw. Nothing is copied.
//
// It takes the two devices being the same GPU, driven by the same driver: an image's memory
// cannot be handed from one GPU to another this way. That is checked, and when it does not
// hold - or the frontend's device cannot import memory at all - Failure() says so; the picture
// stays black then, as there is no other way to draw into a frontend that was asked for a
// Vulkan context. The core option is off by default for that reason.
//
// No Vulkan command is recorded here. The emulator publishes an image only after its GPU work
// on it is done and leaves it in the layout the frontend is told, and it does not draw into the
// image the core holds or into the newest one (shared_state.h, frame_held), so the frontend
// only ever samples a finished image.

#include "gpu_frames.h"

#if defined(__linux__) || defined(_WIN32)

#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <vector>

#if defined(_WIN32)
#define VK_USE_PLATFORM_WIN32_KHR
#include <windows.h>
#else
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "libretro_vulkan.h"
#include "shared_state.h"

namespace Libretro::GpuFrames {

namespace {

// An image's memory as this process holds it.
#if defined(_WIN32)
using MemoryHandle = HANDLE;
constexpr MemoryHandle NO_HANDLE = nullptr;
constexpr VkExternalMemoryHandleTypeFlagBits HANDLE_TYPE =
    VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
constexpr const char* MEMORY_EXTENSION = VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME;
void CloseMemoryHandle(MemoryHandle handle) {
    CloseHandle(handle);
}
#else
using MemoryHandle = int;
constexpr MemoryHandle NO_HANDLE = -1;
constexpr VkExternalMemoryHandleTypeFlagBits HANDLE_TYPE =
    VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
constexpr const char* MEMORY_EXTENSION = VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME;
void CloseMemoryHandle(MemoryHandle handle) {
    close(handle);
}
#endif

retro_environment_t environment_cb = nullptr;
retro_log_printf_t log_cb = nullptr;
SharedState* shared_state = nullptr;
SharedWake* shared_wake = nullptr;

// The frontend's Vulkan, between context_reset and context_destroy.
const retro_hw_render_interface_vulkan* vulkan = nullptr;
bool memory_import_enabled = false;

// What the emulator sent, kept so that it can be imported again into a new device: the
// frontend makes one whenever its video driver is started again.
bool have_message = false;
GpuImagesMessage message{};
static_assert(FRAME_IMAGES == 5);
MemoryHandle memory_handles[FRAME_IMAGES] = {NO_HANDLE, NO_HANDLE, NO_HANDLE, NO_HANDLE,
                                             NO_HANDLE};

struct Imported {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    retro_vulkan_image shown{};
};
Imported images[FRAME_IMAGES];
bool imported = false;

const char* failure = nullptr;
char failure_text[256];

struct {
    PFN_vkCreateImage CreateImage;
    PFN_vkDestroyImage DestroyImage;
    PFN_vkAllocateMemory AllocateMemory;
    PFN_vkFreeMemory FreeMemory;
    PFN_vkBindImageMemory BindImageMemory;
    PFN_vkCreateImageView CreateImageView;
    PFN_vkDestroyImageView DestroyImageView;
    PFN_vkDeviceWaitIdle DeviceWaitIdle;
    // The uploads, where the images cannot be shared
    PFN_vkCreateBuffer CreateBuffer;
    PFN_vkDestroyBuffer DestroyBuffer;
    PFN_vkGetBufferMemoryRequirements GetBufferMemoryRequirements;
    PFN_vkGetImageMemoryRequirements GetImageMemoryRequirements;
    PFN_vkBindBufferMemory BindBufferMemory;
    PFN_vkMapMemory MapMemory;
    PFN_vkCreateCommandPool CreateCommandPool;
    PFN_vkDestroyCommandPool DestroyCommandPool;
    PFN_vkAllocateCommandBuffers AllocateCommandBuffers;
    PFN_vkBeginCommandBuffer BeginCommandBuffer;
    PFN_vkEndCommandBuffer EndCommandBuffer;
    PFN_vkCmdPipelineBarrier CmdPipelineBarrier;
    PFN_vkCmdCopyBufferToImage CmdCopyBufferToImage;
    PFN_vkQueueSubmit QueueSubmit;
    PFN_vkCreateSemaphore CreateSemaphore;
    PFN_vkDestroySemaphore DestroySemaphore;
    PFN_vkCreateFence CreateFence;
    PFN_vkDestroyFence DestroyFence;
    PFN_vkWaitForFences WaitForFences;
    PFN_vkResetFences ResetFences;
} device_calls{};

// Where the emulator's images cannot be shared with the frontend's device (another GPU or
// driver, or no memory import): each frame, read back into the shared memory, is uploaded into
// an image of the frontend's device and handed over as one, one per frame the frontend has in
// flight (its sync index), as a frontend that was given a Vulkan context draws no other kind.
struct Upload {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory buffer_memory = VK_NULL_HANDLE;
    void* mapped = nullptr;
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory image_memory = VK_NULL_HANDLE;
    VkCommandBuffer commands = VK_NULL_HANDLE;
    VkSemaphore done = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    bool submitted = false;
    unsigned width = 0;
    unsigned height = 0;
    retro_vulkan_image shown{};
};
constexpr std::uint32_t MAX_UPLOADS = 8;
Upload uploads[MAX_UPLOADS];
VkCommandPool upload_pool = VK_NULL_HANDLE;

void Log(retro_log_level level, const char* format, ...) {
    char text[512];
    va_list arguments;
    va_start(arguments, format);
    std::vsnprintf(text, sizeof(text), format, arguments);
    va_end(arguments);
    if (log_cb) {
        log_cb(level, "[shadPS4] %s\n", text);
    }
}

void Fail(const char* format, ...) {
    va_list arguments;
    va_start(arguments, format);
    std::vsnprintf(failure_text, sizeof(failure_text), format, arguments);
    va_end(arguments);
    failure = failure_text;
    Log(RETRO_LOG_WARN, "The picture cannot be handed over on the GPU, so it goes through system "
                        "memory: %s", failure_text);
    // The emulator reads its frames back from now on, and they are uploaded here
    if (shared_state) {
        shared_state->gpu_frames_state.store(GPU_FRAMES_FAILED, std::memory_order_release);
    }
}

void CloseMemoryHandles() {
    for (MemoryHandle& handle : memory_handles) {
        if (handle != NO_HANDLE) {
            CloseMemoryHandle(handle);
            handle = NO_HANDLE;
        }
    }
}

void DestroyUploads() {
    if (!vulkan) {
        return;
    }
    bool any = upload_pool != VK_NULL_HANDLE;
    if (any && device_calls.DeviceWaitIdle) {
        vulkan->lock_queue(vulkan->handle);
        device_calls.DeviceWaitIdle(vulkan->device);
        vulkan->unlock_queue(vulkan->handle);
    }
    for (Upload& upload : uploads) {
        if (upload.shown.image_view != VK_NULL_HANDLE) {
            device_calls.DestroyImageView(vulkan->device, upload.shown.image_view, nullptr);
        }
        if (upload.image != VK_NULL_HANDLE) {
            device_calls.DestroyImage(vulkan->device, upload.image, nullptr);
        }
        if (upload.image_memory != VK_NULL_HANDLE) {
            device_calls.FreeMemory(vulkan->device, upload.image_memory, nullptr);
        }
        if (upload.buffer != VK_NULL_HANDLE) {
            device_calls.DestroyBuffer(vulkan->device, upload.buffer, nullptr);
        }
        if (upload.buffer_memory != VK_NULL_HANDLE) {
            device_calls.FreeMemory(vulkan->device, upload.buffer_memory, nullptr);
        }
        if (upload.done != VK_NULL_HANDLE) {
            device_calls.DestroySemaphore(vulkan->device, upload.done, nullptr);
        }
        if (upload.fence != VK_NULL_HANDLE) {
            device_calls.DestroyFence(vulkan->device, upload.fence, nullptr);
        }
        upload = {};
    }
    if (upload_pool != VK_NULL_HANDLE) {
        device_calls.DestroyCommandPool(vulkan->device, upload_pool, nullptr);
        upload_pool = VK_NULL_HANDLE;
    }
}

// A memory type of the frontend's GPU with these properties, or UINT32_MAX
std::uint32_t MemoryType(std::uint32_t allowed, VkMemoryPropertyFlags wanted) {
    const auto get_properties = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(
        vulkan->get_instance_proc_addr(vulkan->instance, "vkGetPhysicalDeviceMemoryProperties"));
    if (!get_properties) {
        return UINT32_MAX;
    }
    VkPhysicalDeviceMemoryProperties properties{};
    get_properties(vulkan->gpu, &properties);
    for (std::uint32_t type = 0; type < properties.memoryTypeCount; ++type) {
        if ((allowed & (1u << type)) &&
            (properties.memoryTypes[type].propertyFlags & wanted) == wanted) {
            return type;
        }
    }
    return UINT32_MAX;
}

// The staging buffer, the image and the commands of one upload slot, for frames of this size.
// Made when the size changes, which is when the game's resolution does.
bool MakeUpload(Upload& upload, unsigned width, unsigned height) {
    if (upload.image != VK_NULL_HANDLE && upload.width == width && upload.height == height) {
        return true;
    }
    if (upload.submitted) {
        device_calls.WaitForFences(vulkan->device, 1, &upload.fence, VK_TRUE, UINT64_MAX);
        upload.submitted = false;
    }
    if (upload.shown.image_view != VK_NULL_HANDLE) {
        device_calls.DestroyImageView(vulkan->device, upload.shown.image_view, nullptr);
        upload.shown.image_view = VK_NULL_HANDLE;
    }
    if (upload.image != VK_NULL_HANDLE) {
        device_calls.DestroyImage(vulkan->device, upload.image, nullptr);
        upload.image = VK_NULL_HANDLE;
    }
    if (upload.image_memory != VK_NULL_HANDLE) {
        device_calls.FreeMemory(vulkan->device, upload.image_memory, nullptr);
        upload.image_memory = VK_NULL_HANDLE;
    }
    if (upload.buffer != VK_NULL_HANDLE) {
        device_calls.DestroyBuffer(vulkan->device, upload.buffer, nullptr);
        upload.buffer = VK_NULL_HANDLE;
    }
    if (upload.buffer_memory != VK_NULL_HANDLE) {
        device_calls.FreeMemory(vulkan->device, upload.buffer_memory, nullptr);
        upload.buffer_memory = VK_NULL_HANDLE;
        upload.mapped = nullptr;
    }
    const VkDevice device = vulkan->device;
    const VkDeviceSize bytes = static_cast<VkDeviceSize>(width) * height * 4;

    if (upload_pool == VK_NULL_HANDLE) {
        VkCommandPoolCreateInfo pool_info{};
        pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pool_info.queueFamilyIndex = vulkan->queue_index;
        if (device_calls.CreateCommandPool(device, &pool_info, nullptr, &upload_pool) !=
            VK_SUCCESS) {
            return false;
        }
    }
    if (upload.commands == VK_NULL_HANDLE) {
        VkCommandBufferAllocateInfo allocate{};
        allocate.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocate.commandPool = upload_pool;
        allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate.commandBufferCount = 1;
        VkSemaphoreCreateInfo semaphore_info{};
        semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VkFenceCreateInfo fence_info{};
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        if (device_calls.AllocateCommandBuffers(device, &allocate, &upload.commands) !=
                VK_SUCCESS ||
            device_calls.CreateSemaphore(device, &semaphore_info, nullptr, &upload.done) !=
                VK_SUCCESS ||
            device_calls.CreateFence(device, &fence_info, nullptr, &upload.fence) != VK_SUCCESS) {
            return false;
        }
    }

    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = bytes;
    buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (device_calls.CreateBuffer(device, &buffer_info, nullptr, &upload.buffer) != VK_SUCCESS) {
        return false;
    }
    VkMemoryRequirements requirements{};
    device_calls.GetBufferMemoryRequirements(device, upload.buffer, &requirements);
    VkMemoryAllocateInfo memory_info{};
    memory_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory_info.allocationSize = requirements.size;
    memory_info.memoryTypeIndex =
        MemoryType(requirements.memoryTypeBits,
                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (memory_info.memoryTypeIndex == UINT32_MAX ||
        device_calls.AllocateMemory(device, &memory_info, nullptr, &upload.buffer_memory) !=
            VK_SUCCESS ||
        device_calls.BindBufferMemory(device, upload.buffer, upload.buffer_memory, 0) !=
            VK_SUCCESS ||
        device_calls.MapMemory(device, upload.buffer_memory, 0, VK_WHOLE_SIZE, 0,
                               &upload.mapped) != VK_SUCCESS) {
        return false;
    }

    // The shared memory's frames are XRGB8888: B, G, R, X in memory
    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.format = VK_FORMAT_B8G8R8A8_UNORM;
    image_info.extent = {width, height, 1};
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                       VK_IMAGE_USAGE_SAMPLED_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (device_calls.CreateImage(device, &image_info, nullptr, &upload.image) != VK_SUCCESS) {
        return false;
    }
    device_calls.GetImageMemoryRequirements(device, upload.image, &requirements);
    memory_info.allocationSize = requirements.size;
    memory_info.memoryTypeIndex =
        MemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (memory_info.memoryTypeIndex == UINT32_MAX ||
        device_calls.AllocateMemory(device, &memory_info, nullptr, &upload.image_memory) !=
            VK_SUCCESS ||
        device_calls.BindImageMemory(device, upload.image, upload.image_memory, 0) !=
            VK_SUCCESS) {
        return false;
    }
    VkImageViewCreateInfo& view = upload.shown.create_info;
    view = {};
    view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view.image = upload.image;
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.format = image_info.format;
    view.components = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B,
                       VK_COMPONENT_SWIZZLE_ONE};
    view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    if (device_calls.CreateImageView(device, &view, nullptr, &upload.shown.image_view) !=
        VK_SUCCESS) {
        return false;
    }
    upload.shown.image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    upload.width = width;
    upload.height = height;
    return true;
}

// With the device still there: the frontend may be drawing the last image shown.
void DestroyImported() {
    if (!vulkan) {
        return;
    }
    bool any = false;
    for (const Imported& entry : images) {
        any = any || entry.image != VK_NULL_HANDLE || entry.memory != VK_NULL_HANDLE;
    }
    if (any && device_calls.DeviceWaitIdle) {
        vulkan->lock_queue(vulkan->handle);
        device_calls.DeviceWaitIdle(vulkan->device);
        vulkan->unlock_queue(vulkan->handle);
    }
    for (Imported& entry : images) {
        if (entry.shown.image_view != VK_NULL_HANDLE) {
            device_calls.DestroyImageView(vulkan->device, entry.shown.image_view, nullptr);
        }
        if (entry.image != VK_NULL_HANDLE) {
            device_calls.DestroyImage(vulkan->device, entry.image, nullptr);
        }
        if (entry.memory != VK_NULL_HANDLE) {
            device_calls.FreeMemory(vulkan->device, entry.memory, nullptr);
        }
        entry = {};
    }
    imported = false;
}

// The frontend's GPU and driver, by their UUIDs. False where they cannot be asked for.
bool FrontendIds(VkPhysicalDeviceIDProperties& ids) {
    auto properties2 = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(
        vulkan->get_instance_proc_addr(vulkan->instance, "vkGetPhysicalDeviceProperties2"));
    if (!properties2) {
        properties2 = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(
            vulkan->get_instance_proc_addr(vulkan->instance, "vkGetPhysicalDeviceProperties2KHR"));
    }
    if (!properties2) {
        return false;
    }
    ids = {};
    ids.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
    VkPhysicalDeviceProperties2 properties{};
    properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    properties.pNext = &ids;
    properties2(vulkan->gpu, &properties);
    return true;
}

// The GPU and the driver the emulator's images are on have to be the frontend's.
bool SameGpu() {
    VkPhysicalDeviceIDProperties ids{};
    if (!FrontendIds(ids)) {
        // Nothing to compare with; the import itself will tell.
        return true;
    }
    return std::memcmp(ids.deviceUUID, message.device_uuid, VK_UUID_SIZE) == 0 &&
           std::memcmp(ids.driverUUID, message.driver_uuid, VK_UUID_SIZE) == 0;
}

// The emulator waits to be told which GPU the frontend is on before it picks its own
// (host.cpp, FrontendGpu), so that both are on the same one where there are several.
void TellFrontendGpu() {
    if (!shared_state || shared_state->frontend_gpu.load() != 0) {
        return;
    }
    VkPhysicalDeviceIDProperties ids{};
    if (vulkan && FrontendIds(ids)) {
        std::memcpy(shared_state->frontend_gpu_uuid, ids.deviceUUID, VK_UUID_SIZE);
        shared_state->frontend_gpu.store(1);
    } else {
        shared_state->frontend_gpu.store(2);
    }
    if (shared_wake) {
        shared_wake->Notify();
    }
}

void Import() {
    if (imported || failure || !vulkan || !have_message) {
        return;
    }
    if (!memory_import_enabled) {
        Fail("the frontend's Vulkan device was made without %s", MEMORY_EXTENSION);
        return;
    }
    if (!SameGpu()) {
        Fail("the frontend and the emulator are on different GPUs or drivers");
        return;
    }

    for (std::uint32_t i = 0; i < message.count; ++i) {
        Imported& entry = images[i];

        VkExternalMemoryImageCreateInfo external{};
        external.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
        external.handleTypes = HANDLE_TYPE;
        // The same image as the emulator's (vk_swapchain.cpp, CreateOffscreenImages).
        VkImageCreateInfo image_info{};
        image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.pNext = &external;
        image_info.imageType = VK_IMAGE_TYPE_2D;
        image_info.format = static_cast<VkFormat>(message.format);
        image_info.extent = {message.width, message.height, 1};
        image_info.mipLevels = 1;
        image_info.arrayLayers = 1;
        image_info.samples = VK_SAMPLE_COUNT_1_BIT;
        image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_info.usage = static_cast<VkImageUsageFlags>(message.usage);
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkResult result =
            device_calls.CreateImage(vulkan->device, &image_info, nullptr, &entry.image);
        if (result != VK_SUCCESS) {
            Fail("image %u could not be made in the frontend's device (%d)", i, result);
            DestroyImported();
            return;
        }

#if defined(_WIN32)
        // An NT handle stays this process's after the import: it is kept for the next device.
        VkImportMemoryWin32HandleInfoKHR import{};
        import.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR;
        import.handleType = HANDLE_TYPE;
        import.handle = memory_handles[i];
#else
        // A descriptor that is imported belongs to the device from then on, so it is a copy
        // that goes in: the original is kept for the next device.
        const int fd = dup(memory_handles[i]);
        if (fd < 0) {
            Fail("a descriptor of image memory could not be copied (%s)", std::strerror(errno));
            DestroyImported();
            return;
        }
        VkImportMemoryFdInfoKHR import{};
        import.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR;
        import.handleType = HANDLE_TYPE;
        import.fd = fd;
#endif
        VkMemoryAllocateInfo allocate{};
        allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocate.pNext = &import;
        allocate.allocationSize = message.allocation_size[i];
        allocate.memoryTypeIndex = message.memory_type;
        result = device_calls.AllocateMemory(vulkan->device, &allocate, nullptr, &entry.memory);
        if (result != VK_SUCCESS) {
#if !defined(_WIN32)
            close(fd);
#endif
            Fail("the memory of image %u could not be imported (%d)", i, result);
            DestroyImported();
            return;
        }
        result = device_calls.BindImageMemory(vulkan->device, entry.image, entry.memory, 0);
        if (result != VK_SUCCESS) {
            Fail("the memory of image %u could not be bound (%d)", i, result);
            DestroyImported();
            return;
        }

        VkImageViewCreateInfo& view = entry.shown.create_info;
        view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view.image = entry.image;
        view.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view.format = image_info.format;
        // The emulator's alpha is not the picture's.
        view.components = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B,
                           VK_COMPONENT_SWIZZLE_ONE};
        view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        result = device_calls.CreateImageView(vulkan->device, &view, nullptr,
                                              &entry.shown.image_view);
        if (result != VK_SUCCESS) {
            Fail("a view of image %u could not be made (%d)", i, result);
            DestroyImported();
            return;
        }
        // The layout the emulator leaves a finished image in (vk_presenter.cpp).
        entry.shown.image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
    imported = true;
    if (shared_state) {
        shared_state->gpu_frames_state.store(GPU_FRAMES_SHOWN, std::memory_order_release);
    }
    Log(RETRO_LOG_INFO, "The picture is handed over on the GPU: %u images of %ux%u", message.count,
        message.width, message.height);
}

void ContextReset() {
    vulkan = nullptr;
    const retro_hw_render_interface_vulkan* frontend_interface = nullptr;
    if (!environment_cb(RETRO_ENVIRONMENT_GET_HW_RENDER_INTERFACE, &frontend_interface) ||
        !frontend_interface ||
        frontend_interface->interface_type != RETRO_HW_RENDER_INTERFACE_VULKAN) {
        Fail("the frontend gave no Vulkan interface");
        TellFrontendGpu();
        return;
    }
    vulkan = frontend_interface;
    const auto load = [](const char* name) {
        return vulkan->get_device_proc_addr(vulkan->device, name);
    };
    device_calls.CreateImage = reinterpret_cast<PFN_vkCreateImage>(load("vkCreateImage"));
    device_calls.DestroyImage = reinterpret_cast<PFN_vkDestroyImage>(load("vkDestroyImage"));
    device_calls.AllocateMemory = reinterpret_cast<PFN_vkAllocateMemory>(load("vkAllocateMemory"));
    device_calls.FreeMemory = reinterpret_cast<PFN_vkFreeMemory>(load("vkFreeMemory"));
    device_calls.BindImageMemory =
        reinterpret_cast<PFN_vkBindImageMemory>(load("vkBindImageMemory"));
    device_calls.CreateImageView =
        reinterpret_cast<PFN_vkCreateImageView>(load("vkCreateImageView"));
    device_calls.DestroyImageView =
        reinterpret_cast<PFN_vkDestroyImageView>(load("vkDestroyImageView"));
    device_calls.DeviceWaitIdle = reinterpret_cast<PFN_vkDeviceWaitIdle>(load("vkDeviceWaitIdle"));
#define LOAD_UPLOAD_CALL(name) device_calls.name = reinterpret_cast<PFN_vk##name>(load("vk" #name))
    LOAD_UPLOAD_CALL(CreateBuffer);
    LOAD_UPLOAD_CALL(DestroyBuffer);
    LOAD_UPLOAD_CALL(GetBufferMemoryRequirements);
    LOAD_UPLOAD_CALL(GetImageMemoryRequirements);
    LOAD_UPLOAD_CALL(BindBufferMemory);
    LOAD_UPLOAD_CALL(MapMemory);
    LOAD_UPLOAD_CALL(CreateCommandPool);
    LOAD_UPLOAD_CALL(DestroyCommandPool);
    LOAD_UPLOAD_CALL(AllocateCommandBuffers);
    LOAD_UPLOAD_CALL(BeginCommandBuffer);
    LOAD_UPLOAD_CALL(EndCommandBuffer);
    LOAD_UPLOAD_CALL(CmdPipelineBarrier);
    LOAD_UPLOAD_CALL(CmdCopyBufferToImage);
    LOAD_UPLOAD_CALL(QueueSubmit);
    LOAD_UPLOAD_CALL(CreateSemaphore);
    LOAD_UPLOAD_CALL(DestroySemaphore);
    LOAD_UPLOAD_CALL(CreateFence);
    LOAD_UPLOAD_CALL(DestroyFence);
    LOAD_UPLOAD_CALL(WaitForFences);
    LOAD_UPLOAD_CALL(ResetFences);
#undef LOAD_UPLOAD_CALL
    if (!device_calls.CreateImage || !device_calls.DestroyImage || !device_calls.AllocateMemory ||
        !device_calls.FreeMemory || !device_calls.BindImageMemory ||
        !device_calls.CreateImageView || !device_calls.DestroyImageView) {
        vulkan = nullptr;
        Fail("the frontend's Vulkan device lacks a function");
        TellFrontendGpu();
        return;
    }
    TellFrontendGpu();
    Import();
}

void ContextDestroy() {
    DestroyUploads();
    DestroyImported();
    vulkan = nullptr;
    // What the device was made with goes with it. A reset keeps the frontend's device, and the
    // core is not asked to make it again, so this is not cleared when a game is loaded.
    memory_import_enabled = false;
}

// ---- The device: made with the extension that imports memory ----
//
// The frontend makes the device, and asks the core what it has to have. The core picks the GPU
// when the frontend leaves that open, and the queue.

const VkApplicationInfo* ApplicationInfo() {
    static const VkApplicationInfo info = {
        VK_STRUCTURE_TYPE_APPLICATION_INFO, nullptr, "shadPS4", 0, "shadPS4", 0, VK_API_VERSION_1_1,
    };
    return &info;
}

bool HasExtension(PFN_vkGetInstanceProcAddr get_instance_proc_addr, VkInstance instance,
                  VkPhysicalDevice gpu, const char* name) {
    const auto enumerate = reinterpret_cast<PFN_vkEnumerateDeviceExtensionProperties>(
        get_instance_proc_addr(instance, "vkEnumerateDeviceExtensionProperties"));
    std::uint32_t count = 0;
    if (!enumerate || enumerate(gpu, nullptr, &count, nullptr) != VK_SUCCESS || count == 0) {
        return false;
    }
    std::vector<VkExtensionProperties> extensions(count);
    if (enumerate(gpu, nullptr, &count, extensions.data()) < VK_SUCCESS) {
        return false;
    }
    for (std::uint32_t i = 0; i < count; ++i) {
        if (std::strcmp(extensions[i].extensionName, name) == 0) {
            return true;
        }
    }
    return false;
}

// A queue family that draws and, with a surface, presents to it.
bool FindQueueFamily(PFN_vkGetInstanceProcAddr get_instance_proc_addr, VkInstance instance,
                     VkPhysicalDevice gpu, VkSurfaceKHR surface, std::uint32_t& family) {
    const auto get_families = reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(
        get_instance_proc_addr(instance, "vkGetPhysicalDeviceQueueFamilyProperties"));
    const auto surface_support = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>(
        get_instance_proc_addr(instance, "vkGetPhysicalDeviceSurfaceSupportKHR"));
    if (!get_families) {
        return false;
    }
    std::uint32_t count = 0;
    get_families(gpu, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    get_families(gpu, &count, families.data());
    for (std::uint32_t i = 0; i < count; ++i) {
        if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) {
            continue;
        }
        if (surface != VK_NULL_HANDLE) {
            VkBool32 presents = VK_FALSE;
            if (!surface_support || surface_support(gpu, i, surface, &presents) != VK_SUCCESS ||
                !presents) {
                continue;
            }
        }
        family = i;
        return true;
    }
    return false;
}

bool PickGpu(PFN_vkGetInstanceProcAddr get_instance_proc_addr, VkInstance instance,
             VkSurfaceKHR surface, VkPhysicalDevice& gpu, std::uint32_t& family) {
    if (gpu != VK_NULL_HANDLE) {
        return FindQueueFamily(get_instance_proc_addr, instance, gpu, surface, family);
    }
    const auto enumerate = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(
        get_instance_proc_addr(instance, "vkEnumeratePhysicalDevices"));
    std::uint32_t count = 0;
    if (!enumerate || enumerate(instance, &count, nullptr) != VK_SUCCESS || count == 0) {
        return false;
    }
    std::vector<VkPhysicalDevice> gpus(count);
    if (enumerate(instance, &count, gpus.data()) < VK_SUCCESS) {
        return false;
    }
    for (std::uint32_t i = 0; i < count; ++i) {
        if (FindQueueFamily(get_instance_proc_addr, instance, gpus[i], surface, family)) {
            gpu = gpus[i];
            return true;
        }
    }
    return false;
}

bool FinishContext(retro_vulkan_context* context, PFN_vkGetInstanceProcAddr get_instance_proc_addr,
                   VkInstance instance, VkPhysicalDevice gpu, VkDevice device,
                   std::uint32_t family) {
    const auto get_queue = reinterpret_cast<PFN_vkGetDeviceQueue>(
        get_instance_proc_addr(instance, "vkGetDeviceQueue"));
    if (device == VK_NULL_HANDLE || !get_queue) {
        return false;
    }
    context->gpu = gpu;
    context->device = device;
    context->queue_family_index = family;
    context->presentation_queue_family_index = family;
    get_queue(device, family, 0, &context->queue);
    context->presentation_queue = context->queue;
    return true;
}

bool CreateDevice2(retro_vulkan_context* context, VkInstance instance, VkPhysicalDevice gpu,
                   VkSurfaceKHR surface, PFN_vkGetInstanceProcAddr get_instance_proc_addr,
                   retro_vulkan_create_device_wrapper_t create_device_wrapper, void* opaque) {
    std::uint32_t family = 0;
    if (!PickGpu(get_instance_proc_addr, instance, surface, gpu, family)) {
        return false;
    }
    // Without the extension the device is still made: the picture is then uploaded from the
    // shared memory instead (ShowPixels).
    const char* extensions[] = {MEMORY_EXTENSION};
    memory_import_enabled = HasExtension(get_instance_proc_addr, instance, gpu, extensions[0]);

    const float priority = 1.0f;
    VkDeviceQueueCreateInfo queue{};
    queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue.queueFamilyIndex = family;
    queue.queueCount = 1;
    queue.pQueuePriorities = &priority;
    VkDeviceCreateInfo device_info{};
    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.queueCreateInfoCount = 1;
    device_info.pQueueCreateInfos = &queue;
    device_info.enabledExtensionCount = memory_import_enabled ? 1 : 0;
    device_info.ppEnabledExtensionNames = extensions;
    const VkDevice device = create_device_wrapper(gpu, opaque, &device_info);
    return FinishContext(context, get_instance_proc_addr, instance, gpu, device, family);
}

// The older form: the frontend names what it needs and the core makes the device.
bool CreateDevice(retro_vulkan_context* context, VkInstance instance, VkPhysicalDevice gpu,
                  VkSurfaceKHR surface, PFN_vkGetInstanceProcAddr get_instance_proc_addr,
                  const char** required_device_extensions, unsigned num_required_device_extensions,
                  const char** required_device_layers, unsigned num_required_device_layers,
                  const VkPhysicalDeviceFeatures* required_features) {
    std::uint32_t family = 0;
    if (!PickGpu(get_instance_proc_addr, instance, surface, gpu, family)) {
        return false;
    }
    std::vector<const char*> extensions(required_device_extensions,
                                        required_device_extensions +
                                            num_required_device_extensions);
    memory_import_enabled =
        HasExtension(get_instance_proc_addr, instance, gpu, MEMORY_EXTENSION);
    if (memory_import_enabled) {
        extensions.push_back(MEMORY_EXTENSION);
    }

    const float priority = 1.0f;
    VkDeviceQueueCreateInfo queue{};
    queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue.queueFamilyIndex = family;
    queue.queueCount = 1;
    queue.pQueuePriorities = &priority;
    VkDeviceCreateInfo device_info{};
    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.queueCreateInfoCount = 1;
    device_info.pQueueCreateInfos = &queue;
    device_info.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
    device_info.ppEnabledExtensionNames = extensions.data();
    device_info.enabledLayerCount = num_required_device_layers;
    device_info.ppEnabledLayerNames = required_device_layers;
    device_info.pEnabledFeatures = required_features;
    const auto create_device = reinterpret_cast<PFN_vkCreateDevice>(
        get_instance_proc_addr(instance, "vkCreateDevice"));
    VkDevice device = VK_NULL_HANDLE;
    if (!create_device || create_device(gpu, &device_info, nullptr, &device) != VK_SUCCESS) {
        return false;
    }
    return FinishContext(context, get_instance_proc_addr, instance, gpu, device, family);
}

} // namespace

bool Request(retro_environment_t environment, retro_log_printf_t log) {
    environment_cb = environment;
    log_cb = log;
    failure = nullptr;

    // Asked for whatever the frontend's video driver is: a frontend that can switch to its
    // Vulkan driver does, as for any core that draws with Vulkan.
    static retro_hw_render_callback hw_render{};
    hw_render = {};
    hw_render.context_type = RETRO_HW_CONTEXT_VULKAN;
    hw_render.version_major = VK_MAKE_VERSION(1, 1, 0);
    hw_render.version_minor = 0;
    hw_render.context_reset = ContextReset;
    hw_render.context_destroy = ContextDestroy;
    hw_render.cache_context = false;
    if (!environment(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render)) {
        Log(RETRO_LOG_INFO, "The frontend gives no Vulkan context: the picture goes through "
                            "system memory");
        return false;
    }

    static retro_hw_render_context_negotiation_interface_vulkan negotiation{};
    negotiation = {};
    negotiation.interface_type = RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_VULKAN;
    negotiation.interface_version = RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_VULKAN_VERSION;
    negotiation.get_application_info = ApplicationInfo;
    negotiation.create_device = CreateDevice;
    negotiation.create_device2 = CreateDevice2;
    // A frontend that does not take it makes the device by itself, without the extension, and
    // Import() says so.
    environment(RETRO_ENVIRONMENT_SET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE, &negotiation);
    return true;
}

void SetShared(SharedState* shared, SharedWake* wake) {
    shared_state = shared;
    shared_wake = wake;
}

#if defined(_WIN32)
void Receive(int, void* emulator_process) {
    if (!shared_state || have_message || shared_state->gpu_images_ready.load() == 0) {
        return;
    }
    const GpuImagesMessage received = shared_state->gpu_images;
    if (received.magic != GPU_IMAGES_MAGIC || received.count == 0 ||
        received.count > FRAME_IMAGES) {
        have_message = true;
        Fail("what the emulator wrote about its images cannot be read");
        return;
    }
    // The handles are the emulator's: duplicated into this process, they are the core's.
    for (std::uint32_t i = 0; i < received.count; ++i) {
        const HANDLE theirs = reinterpret_cast<HANDLE>(
            static_cast<std::uintptr_t>(shared_state->gpu_image_handles[i]));
        if (!DuplicateHandle(static_cast<HANDLE>(emulator_process), theirs, GetCurrentProcess(),
                             &memory_handles[i], 0, FALSE, DUPLICATE_SAME_ACCESS)) {
            memory_handles[i] = NO_HANDLE;
            CloseMemoryHandles();
            have_message = true;
            Fail("the emulator's image %u could not be taken over (error %lu)", i,
                 GetLastError());
            return;
        }
    }
    message = received;
    have_message = true;
    Import();
}
#else
void Receive(int socket, void*) {
    if (socket < 0 || have_message) {
        return;
    }
    GpuImagesMessage received{};
    iovec data{&received, sizeof(received)};
    alignas(cmsghdr) char control[CMSG_SPACE(sizeof(int) * FRAME_IMAGES)];
    msghdr header{};
    header.msg_iov = &data;
    header.msg_iovlen = 1;
    header.msg_control = control;
    header.msg_controllen = sizeof(control);
    const ssize_t size = recvmsg(socket, &header, MSG_DONTWAIT | MSG_CMSG_CLOEXEC);
    if (size < 0) {
        return;
    }
    int fds[FRAME_IMAGES];
    std::uint32_t fd_count = 0;
    for (cmsghdr* part = CMSG_FIRSTHDR(&header); part; part = CMSG_NXTHDR(&header, part)) {
        if (part->cmsg_level != SOL_SOCKET || part->cmsg_type != SCM_RIGHTS) {
            continue;
        }
        const std::size_t bytes = part->cmsg_len - CMSG_LEN(0);
        const std::uint32_t count = static_cast<std::uint32_t>(bytes / sizeof(int));
        for (std::uint32_t i = 0; i < count; ++i) {
            int fd = -1;
            std::memcpy(&fd, CMSG_DATA(part) + i * sizeof(int), sizeof(int));
            if (fd_count < FRAME_IMAGES) {
                fds[fd_count++] = fd;
            } else {
                close(fd);
            }
        }
    }
    if (size != static_cast<ssize_t>(sizeof(received)) || received.magic != GPU_IMAGES_MAGIC ||
        received.count == 0 || received.count > FRAME_IMAGES || fd_count != received.count) {
        for (std::uint32_t i = 0; i < fd_count; ++i) {
            close(fds[i]);
        }
        if (size != 0) {
            Fail("what the emulator sent about its images cannot be read");
        }
        return;
    }
    message = received;
    for (std::uint32_t i = 0; i < fd_count; ++i) {
        memory_handles[i] = fds[i];
    }
    have_message = true;
    Import();
}
#endif

void Show(retro_video_refresh_t video, std::uint32_t slot, bool have_frame, unsigned width,
          unsigned height) {
    if (!video) {
        return;
    }
    if (!have_frame || !imported || !vulkan || slot >= message.count) {
        // Nothing to show yet: the frontend draws what it drew last.
        video(nullptr, width, height, 0);
        return;
    }
    vulkan->set_image(vulkan->handle, &images[slot].shown, 0, nullptr, VK_QUEUE_FAMILY_IGNORED);
    video(RETRO_HW_FRAME_BUFFER_VALID, width, height, 0);
}

void ShowPixels(retro_video_refresh_t video, const void* pixels, unsigned width,
                unsigned height) {
    if (!video) {
        return;
    }
    if (!vulkan || !pixels || width == 0 || height == 0) {
        video(nullptr, width, height, 0);
        return;
    }
    const std::uint32_t index = vulkan->get_sync_index(vulkan->handle) % MAX_UPLOADS;
    Upload& upload = uploads[index];
    // The slot's last upload, a frame the frontend has in flight at most ago: done by now as
    // a rule, so this does not wait
    if (upload.submitted) {
        device_calls.WaitForFences(vulkan->device, 1, &upload.fence, VK_TRUE, UINT64_MAX);
        upload.submitted = false;
    }
    if (!MakeUpload(upload, width, height)) {
        Log(RETRO_LOG_ERROR, "No image to upload the picture into (%ux%u)", width, height);
        video(nullptr, width, height, 0);
        return;
    }
    std::memcpy(upload.mapped, pixels, static_cast<std::size_t>(width) * height * 4);

    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    device_calls.BeginCommandBuffer(upload.commands, &begin);
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    // All of it is written again: what it held does not matter
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = upload.image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    device_calls.CmdPipelineBarrier(upload.commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                    VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                                    &barrier);
    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent = {width, height, 1};
    device_calls.CmdCopyBufferToImage(upload.commands, upload.buffer, upload.image,
                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    device_calls.CmdPipelineBarrier(upload.commands, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                                    nullptr, 1, &barrier);
    device_calls.EndCommandBuffer(upload.commands);

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &upload.commands;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &upload.done;
    device_calls.ResetFences(vulkan->device, 1, &upload.fence);
    vulkan->lock_queue(vulkan->handle);
    const VkResult result = device_calls.QueueSubmit(vulkan->queue, 1, &submit, upload.fence);
    vulkan->unlock_queue(vulkan->handle);
    if (result != VK_SUCCESS) {
        video(nullptr, width, height, 0);
        return;
    }
    upload.submitted = true;
    // The frontend waits for the upload before it samples the image
    vulkan->set_image(vulkan->handle, &upload.shown, 1, &upload.done, VK_QUEUE_FAMILY_IGNORED);
    video(RETRO_HW_FRAME_BUFFER_VALID, width, height, 0);
}

bool Shared() {
    return imported;
}

const char* Failure() {
    return failure;
}

void Close() {
    DestroyUploads();
    DestroyImported();
    CloseMemoryHandles();
    have_message = false;
    message = {};
    failure = nullptr;
}

} // namespace Libretro::GpuFrames

#else

// Elsewhere there is no hand-over on the GPU: the picture goes through system memory.
namespace Libretro::GpuFrames {

bool Request(retro_environment_t, retro_log_printf_t) {
    return false;
}
void SetShared(SharedState*, SharedWake*) {}
void Receive(int, void*) {}
void Show(retro_video_refresh_t, std::uint32_t, bool, unsigned, unsigned) {}
void ShowPixels(retro_video_refresh_t, const void*, unsigned, unsigned) {}
bool Shared() {
    return false;
}
const char* Failure() {
    return nullptr;
}
void Close() {}

} // namespace Libretro::GpuFrames

#endif
