// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <stdarg.h>

#include "common/assert.h"
#include "common/logging/log.h"
#include "common/types.h"
#include "video_core/renderer_vulkan/vk_common.h"

namespace Vulkan {

constexpr u32 TargetVulkanApiVersion = VK_API_VERSION_1_3;

/// Loads the Vulkan the frontend's environment provides and makes an instance of it, with the
/// validation and crash diagnostic layers where asked for and present. Whether the debug utils
/// are on it goes into *@out_debug_utils.
VkInstance CreateInstance(bool enable_validation, bool enable_crash_diagnostic,
                          bool* out_debug_utils);

/// A messenger putting what the validation layers say into the log
VkDebugUtilsMessengerEXT CreateDebugCallback(VkInstance instance);

/// The object type of a Vulkan handle type, as debug utils names it
template <typename Handle>
struct ObjectTypeOf;
#define VULKAN_OBJECT_TYPE_OF(handle, type)                                                        \
    template <>                                                                                    \
    struct ObjectTypeOf<handle> {                                                                  \
        static constexpr VkObjectType value = type;                                                \
    };
VULKAN_OBJECT_TYPE_OF(VkBuffer, VK_OBJECT_TYPE_BUFFER)
VULKAN_OBJECT_TYPE_OF(VkImage, VK_OBJECT_TYPE_IMAGE)
VULKAN_OBJECT_TYPE_OF(VkImageView, VK_OBJECT_TYPE_IMAGE_VIEW)
VULKAN_OBJECT_TYPE_OF(VkSampler, VK_OBJECT_TYPE_SAMPLER)
VULKAN_OBJECT_TYPE_OF(VkPipeline, VK_OBJECT_TYPE_PIPELINE)
VULKAN_OBJECT_TYPE_OF(VkPipelineLayout, VK_OBJECT_TYPE_PIPELINE_LAYOUT)
VULKAN_OBJECT_TYPE_OF(VkDescriptorSetLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT)
VULKAN_OBJECT_TYPE_OF(VkShaderModule, VK_OBJECT_TYPE_SHADER_MODULE)
VULKAN_OBJECT_TYPE_OF(VkCommandPool, VK_OBJECT_TYPE_COMMAND_POOL)
VULKAN_OBJECT_TYPE_OF(VkCommandBuffer, VK_OBJECT_TYPE_COMMAND_BUFFER)
VULKAN_OBJECT_TYPE_OF(VkSemaphore, VK_OBJECT_TYPE_SEMAPHORE)
VULKAN_OBJECT_TYPE_OF(VkFence, VK_OBJECT_TYPE_FENCE)
VULKAN_OBJECT_TYPE_OF(VkDeviceMemory, VK_OBJECT_TYPE_DEVICE_MEMORY)
#undef VULKAN_OBJECT_TYPE_OF

/// Names the object @handle of @type for debuggers, from a printf @format; nothing where the
/// debug utils are off
void SetObjectNameV(VkDevice device, VkObjectType type, u64 handle, const char* format,
                    va_list args);

/// The name a printf format makes, on the stack
template <typename Handle>
void SetObjectName(VkDevice device, Handle handle, const char* format, ...) SHAD_PRINTF(3, 4);

template <typename Handle>
void SetObjectName(VkDevice device, Handle handle, const char* format, ...) {
    va_list args;
    va_start(args, format);
    SetObjectNameV(device, ObjectTypeOf<Handle>::value, reinterpret_cast<u64>(handle), format,
                   args);
    va_end(args);
}

} // namespace Vulkan
