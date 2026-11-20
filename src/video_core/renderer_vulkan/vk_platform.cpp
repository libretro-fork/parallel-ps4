// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Include the vulkan platform specific header
#if defined(ANDROID)
#define VK_USE_PLATFORM_ANDROID_KHR
#elif defined(_WIN64)
#define VK_USE_PLATFORM_WIN32_KHR
#elif defined(__APPLE__)
#define VK_USE_PLATFORM_METAL_EXT
#endif

#include <vector>

#include "common/assert.h"
#include "common/logging/log.h"
#include "common/path_util.h"
#include "common/string_util.h"
#include "core/emulator_settings.h"
#include "video_core/renderer_vulkan/vk_platform.h"

namespace Vulkan {

static const char* const VALIDATION_LAYER_NAME = "VK_LAYER_KHRONOS_validation";
static const char* const CRASH_DIAGNOSTIC_LAYER_NAME = "VK_LAYER_LUNARG_crash_diagnostic";

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugUtilsCallback(
    vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT type,
    const vk::DebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data) {

    Common::Log::Level level{};
    switch (severity) {
    case vk::DebugUtilsMessageSeverityFlagBitsEXT::eError:
        level = Common::Log::Level::Error;
        break;
    case vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning:
        level = Common::Log::Level::Info;
        break;
    case vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo:
    case vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose:
        level = Common::Log::Level::Debug;
        break;
    default:
        level = Common::Log::Level::Info;
    }

    LOG_GENERIC(Render_Vulkan, level, "%s: %s",
                callback_data->pMessageIdName ? callback_data->pMessageIdName : "<null>",
                callback_data->pMessage ? callback_data->pMessage : "<null>");

    return VK_FALSE;
}

static auto GetLayerExtensions(std::vector<const char*>&& extensions,
                               const std::vector<const char*>& layers) {
    auto all_missing_vk_settings = true;

    for (const auto& layer_name : layers) {
        const auto [layer_properties_result, layer_extensions] =
            vk::enumerateInstanceExtensionProperties(std::string(layer_name));
        if (layer_properties_result != vk::Result::eSuccess) {
            LOG_ERROR(Render_Vulkan, "Failed to query extension properties of %s: %s", layer_name,
                      (vk::to_string(layer_properties_result)).c_str());
        }
        auto found = false;
        for (const auto& extension : layer_extensions) {
            if (extension.extensionName == std::string_view(VK_EXT_LAYER_SETTINGS_EXTENSION_NAME)) {
                found = true;
                all_missing_vk_settings = false;
                break;
            }
        }
        if (!found) {
            LOG_ERROR(Render_Vulkan, "Settings for layer %s not available.", layer_name);
        }
    }

    if (!all_missing_vk_settings) {
        extensions.push_back(VK_EXT_LAYER_SETTINGS_EXTENSION_NAME);
    }

    return extensions;
}

std::vector<const char*> GetInstanceExtensions(bool enable_debug_utils) {
    const auto [properties_result, properties] = vk::enumerateInstanceExtensionProperties();
    if (properties_result != vk::Result::eSuccess || properties.empty()) {
        LOG_ERROR(Render_Vulkan, "Failed to query extension properties: %s",
                  (vk::to_string(properties_result)).c_str());
        return {};
    }

    // No surface: the frontend presents the picture.
    std::vector<const char*> extensions;
    extensions.reserve(2);

    if (enable_debug_utils) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    // Sanitize extension list
    std::erase_if(extensions, [&](const char* extension) -> bool {
        const auto it =
            std::find_if(properties.begin(), properties.end(), [extension](const auto& prop) {
                return std::strcmp(extension, prop.extensionName) == 0;
            });

        if (it == properties.end()) {
            LOG_INFO(Render_Vulkan, "Candidate instance extension %s is not available", extension);
            return true;
        }
        return false;
    });

    return extensions;
}

std::vector<const char*> GetInstanceLayers(bool enable_validation, bool enable_crash_diagnostic) {
    const auto [properties_result, properties] = vk::enumerateInstanceLayerProperties();
    if (properties_result != vk::Result::eSuccess || properties.empty()) {
        LOG_ERROR(Render_Vulkan, "Failed to query layer properties: %s",
                  (vk::to_string(properties_result)).c_str());
        return {};
    }

    std::vector<const char*> layers;
    layers.reserve(2);

    if (enable_validation) {
        layers.push_back(VALIDATION_LAYER_NAME);
    }
    if (enable_crash_diagnostic) {
        layers.push_back(CRASH_DIAGNOSTIC_LAYER_NAME);
    }

    // Sanitize layer list
    std::erase_if(layers, [&](const char* layer) -> bool {
        const auto it = std::ranges::find_if(properties, [layer](const auto& prop) {
            return std::strcmp(layer, prop.layerName) == 0;
        });
        if (it == properties.end()) {
            LOG_ERROR(Render_Vulkan, "Requested layer %s is not available", layer);
            return true;
        }
        return false;
    });

    return layers;
}

vk::UniqueInstance CreateInstance(bool enable_validation, bool enable_crash_diagnostic) {
    LOG_INFO(Render_Vulkan, "Creating vulkan instance");

    // The Vulkan the frontend's environment provides; the core brings none of its own.
    static vk::detail::DynamicLoader dl;
    VULKAN_HPP_DEFAULT_DISPATCHER.init(
        dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr"));

    const auto [available_version_result, available_version] =
        VULKAN_HPP_DEFAULT_DISPATCHER.vkEnumerateInstanceVersion
            ? vk::enumerateInstanceVersion()
            : vk::ResultValue(vk::Result::eSuccess, VK_API_VERSION_1_0);
    ASSERT_MSG(available_version_result == vk::Result::eSuccess,
               "Failed to query Vulkan API version: %s",
               (vk::to_string(available_version_result)).c_str());
    ASSERT_MSG(available_version >= TargetVulkanApiVersion,
               "Vulkan %u.%u is required, but only %u.%u is supported by instance!",
               VK_VERSION_MAJOR(TargetVulkanApiVersion), VK_VERSION_MINOR(TargetVulkanApiVersion),
               VK_VERSION_MAJOR(available_version), VK_VERSION_MINOR(available_version));

    const auto layers = GetInstanceLayers(enable_validation, enable_crash_diagnostic);
    const auto extensions = GetLayerExtensions(GetInstanceExtensions(true), layers);

    const vk::ApplicationInfo application_info = {
        .pApplicationName = "shadPS4",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "shadPS4 Vulkan",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = available_version,
    };

    std::string extensions_string;
    for (const auto& item : extensions) {
        extensions_string += extensions_string.empty() ? "" : ", ";
        extensions_string += item;
    }
    std::string layers_string;
    for (const auto& item : layers) {
        layers_string += layers_string.empty() ? "" : ", ";
        layers_string += item;
    }
    LOG_INFO(Render_Vulkan, "Enabled instance extensions: %s", (extensions_string).c_str());
    LOG_INFO(Render_Vulkan, "Enabled instance layers: %s", (layers_string).c_str());

    // Validation settings
    vk::Bool32 enable_core = emu_settings_get_vkvalidation_core_enabled() ? vk::True : vk::False;
    vk::Bool32 enable_sync = emu_settings_get_vkvalidation_sync_enabled() ? vk::True : vk::False;
    vk::Bool32 enable_gpuav = emu_settings_get_vkvalidation_gpu_enabled() ? vk::True : vk::False;

    // Crash diagnostics settings
    static const auto crash_diagnostic_path =
        Common::FS::GetUserPathString(Common::FS::PathType::LogDir);
    const char* log_path = crash_diagnostic_path.c_str();
    vk::Bool32 enable_force_barriers = vk::True;

    const std::array layer_setings = {
        vk::LayerSettingEXT{
            .pLayerName = VALIDATION_LAYER_NAME,
            .pSettingName = "validate_core",
            .type = vk::LayerSettingTypeEXT::eBool32,
            .valueCount = 1,
            .pValues = &enable_core,
        },
        vk::LayerSettingEXT{
            .pLayerName = VALIDATION_LAYER_NAME,
            .pSettingName = "validate_sync",
            .type = vk::LayerSettingTypeEXT::eBool32,
            .valueCount = 1,
            .pValues = &enable_sync,
        },
        vk::LayerSettingEXT{
            .pLayerName = VALIDATION_LAYER_NAME,
            .pSettingName = "syncval_submit_time_validation",
            .type = vk::LayerSettingTypeEXT::eBool32,
            .valueCount = 1,
            .pValues = &enable_sync,
        },
        vk::LayerSettingEXT{
            .pLayerName = VALIDATION_LAYER_NAME,
            .pSettingName = "gpuav_enable",
            .type = vk::LayerSettingTypeEXT::eBool32,
            .valueCount = 1,
            .pValues = &enable_gpuav,
        },
        vk::LayerSettingEXT{
            .pLayerName = VALIDATION_LAYER_NAME,
            .pSettingName = "gpuav_descriptor_checks",
            .type = vk::LayerSettingTypeEXT::eBool32,
            .valueCount = 1,
            .pValues = &enable_gpuav,
        },
        vk::LayerSettingEXT{
            .pLayerName = VALIDATION_LAYER_NAME,
            .pSettingName = "gpuav_buffers_validation",
            .type = vk::LayerSettingTypeEXT::eBool32,
            .valueCount = 1,
            .pValues = &enable_gpuav,
        },
        vk::LayerSettingEXT{
            .pLayerName = VALIDATION_LAYER_NAME,
            .pSettingName = "gpuav_indirect_draws_buffers",
            .type = vk::LayerSettingTypeEXT::eBool32,
            .valueCount = 1,
            .pValues = &enable_gpuav,
        },
        vk::LayerSettingEXT{
            .pLayerName = VALIDATION_LAYER_NAME,
            .pSettingName = "gpuav_indirect_dispatches_buffers",
            .type = vk::LayerSettingTypeEXT::eBool32,
            .valueCount = 1,
            .pValues = &enable_gpuav,
        },
        vk::LayerSettingEXT{
            .pLayerName = VALIDATION_LAYER_NAME,
            .pSettingName = "gpuav_indirect_trace_rays_buffers",
            .type = vk::LayerSettingTypeEXT::eBool32,
            .valueCount = 1,
            .pValues = &enable_gpuav,
        },
        vk::LayerSettingEXT{
            .pLayerName = VALIDATION_LAYER_NAME,
            .pSettingName = "gpuav_buffer_copies",
            .type = vk::LayerSettingTypeEXT::eBool32,
            .valueCount = 1,
            .pValues = &enable_gpuav,
        },
        vk::LayerSettingEXT{
            .pLayerName = "lunarg_crash_diagnostic",
            .pSettingName = "output_path",
            .type = vk::LayerSettingTypeEXT::eString,
            .valueCount = 1,
            .pValues = &log_path,
        },
        vk::LayerSettingEXT{
            .pLayerName = "lunarg_crash_diagnostic",
            .pSettingName = "sync_after_commands",
            .type = vk::LayerSettingTypeEXT::eBool32,
            .valueCount = 1,
            .pValues = &enable_force_barriers,
        },
    };

    vk::StructureChain<vk::InstanceCreateInfo, vk::LayerSettingsCreateInfoEXT> instance_ci_chain = {
        vk::InstanceCreateInfo{
            .pApplicationInfo = &application_info,
            .enabledLayerCount = static_cast<u32>(layers.size()),
            .ppEnabledLayerNames = layers.data(),
            .enabledExtensionCount = static_cast<u32>(extensions.size()),
            .ppEnabledExtensionNames = extensions.data(),
        },
        vk::LayerSettingsCreateInfoEXT{
            .settingCount = layer_setings.size(),
            .pSettings = layer_setings.data(),
        },
    };

    auto [instance_result, instance] = vk::createInstanceUnique(instance_ci_chain.get());
    ASSERT_MSG(instance_result == vk::Result::eSuccess, "Failed to create instance: %s",
               (vk::to_string(instance_result)).c_str());

    VULKAN_HPP_DEFAULT_DISPATCHER.init(*instance);

    return std::move(instance);
}

vk::UniqueDebugUtilsMessengerEXT CreateDebugCallback(vk::Instance instance) {
    const vk::DebugUtilsMessengerCreateInfoEXT msg_ci = {
        .messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
                           vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
                           vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                           vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose,
        .messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                       vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                       vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
        .pfnUserCallback = DebugUtilsCallback,
    };
    auto [messenger_result, messenger] = instance.createDebugUtilsMessengerEXTUnique(msg_ci);
    ASSERT_MSG(messenger_result == vk::Result::eSuccess, "Failed to create debug callback: %s",
               (vk::to_string(messenger_result)).c_str());
    return std::move(messenger);
}

} // namespace Vulkan
