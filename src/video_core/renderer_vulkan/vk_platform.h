// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstdarg>
#include <cstdio>

#include "common/assert.h"
#include "common/logging/log.h"
#include "common/string_literal.h"
#include "common/types.h"
#include "video_core/renderer_vulkan/vk_common.h"

namespace Vulkan {

constexpr u32 TargetVulkanApiVersion = VK_API_VERSION_1_3;

vk::UniqueInstance CreateInstance(bool enable_validation, bool enable_crash_diagnostic);

vk::UniqueDebugUtilsMessengerEXT CreateDebugCallback(vk::Instance instance);

template <typename T>
concept VulkanHandleType = vk::isVulkanHandleType<T>::value;

template <VulkanHandleType HandleType>
void SetObjectName(vk::Device device, const HandleType& handle, std::string_view debug_name) {
    const vk::DebugUtilsObjectNameInfoEXT name_info = {
        .objectType = HandleType::objectType,
        .objectHandle = reinterpret_cast<u64>(static_cast<typename HandleType::NativeType>(handle)),
        .pObjectName = debug_name.data(),
    };
    auto result = device.setDebugUtilsObjectNameEXT(name_info);
    if (result != vk::Result::eSuccess) {
        LOG_DEBUG(Render_Vulkan, "Could not set object debug name: %s",
                  vk::to_string(result).c_str());
    }
}

/// The name a printf format makes, on the stack
template <VulkanHandleType HandleType>
void SetObjectName(vk::Device device, const HandleType& handle, const char* format, ...)
    SHAD_PRINTF(3, 4);

template <VulkanHandleType HandleType>
void SetObjectName(vk::Device device, const HandleType& handle, const char* format, ...) {
    char name[256];
    va_list args;
    va_start(args, format);
    std::vsnprintf(name, sizeof(name), format, args);
    va_end(args);
    SetObjectName(device, handle, std::string_view{name});
}

template <StringLiteral msg = "">
static void Check(vk::Result r) {
    if constexpr (msg.len <= 1) {
        ASSERT_MSG(r == vk::Result::eSuccess, "vk::Result=%s", vk::to_string(r).c_str());
    } else {
        ASSERT_MSG(r == vk::Result::eSuccess, "Failed to %s: vk::Result=%s", msg.value,
                   vk::to_string(r).c_str());
    }
}

template <StringLiteral msg = "", typename T>
static T Check(vk::ResultValue<T> r) {
    if constexpr (msg.len <= 1) {
        ASSERT_MSG(r.result == vk::Result::eSuccess, "vk::Result=%s",
                   vk::to_string(r.result).c_str());
    } else {
        ASSERT_MSG(r.result == vk::Result::eSuccess, "Failed to %s: vk::Result=%s", msg.value,
                   vk::to_string(r.result).c_str());
    }
    return std::move(r.value);
}

} // namespace Vulkan
