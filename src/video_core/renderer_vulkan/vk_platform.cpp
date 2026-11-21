// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dynamic/dylib.h>

#include "common/assert.h"
#include "common/logging/log.h"
#include "common/path_util.h"
#include "core/emulator_settings.h"
#include "video_core/renderer_vulkan/vk_platform.h"

namespace Vulkan {

static const char* const VALIDATION_LAYER_NAME = "VK_LAYER_KHRONOS_validation";
static const char* const CRASH_DIAGNOSTIC_LAYER_NAME = "VK_LAYER_LUNARG_crash_diagnostic";

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugUtilsCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data) {
    (void)type;
    (void)user_data;
    Common::Log::Level level{};
    switch (severity) {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        level = Common::Log::Level::Error;
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        level = Common::Log::Level::Info;
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
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

/// The most layers or instance extensions asked for
static constexpr u32 MaxNames = 4;

/// The names in @names that @layers offer or the loader does (@layer null), kept in place;
/// how many are left. What @kind names is logged as missing at @level.
static u32 KeepAvailable(const char** names, u32 count, const char* layer, bool layers,
                         const char* kind) {
    u32 num_props = 0;
    VkResult result = layers ? vkEnumerateInstanceLayerProperties(&num_props, nullptr)
                             : vkEnumerateInstanceExtensionProperties(layer, &num_props, nullptr);
    if (result != VK_SUCCESS || num_props == 0) {
        LOG_ERROR(Render_Vulkan, "Failed to query %s: %s", kind, ResultName(result));
        return 0;
    }
    // Layer and extension properties are each a name and a little more
    const size_t prop_size = layers ? sizeof(VkLayerProperties) : sizeof(VkExtensionProperties);
    unsigned char* const props = static_cast<unsigned char*>(malloc(num_props * prop_size));
    ASSERT_MSG(props, "Out of memory querying Vulkan %s", kind);
    result = layers ? vkEnumerateInstanceLayerProperties(
                          &num_props, reinterpret_cast<VkLayerProperties*>(props))
                    : vkEnumerateInstanceExtensionProperties(
                          layer, &num_props, reinterpret_cast<VkExtensionProperties*>(props));
    u32 kept = 0;
    for (u32 i = 0; i < count && result == VK_SUCCESS; ++i) {
        bool found = false;
        for (u32 p = 0; p < num_props && !found; ++p) {
            const char* const name =
                layers ? reinterpret_cast<VkLayerProperties*>(props)[p].layerName
                       : reinterpret_cast<VkExtensionProperties*>(props)[p].extensionName;
            found = strcmp(names[i], name) == 0;
        }
        if (found) {
            names[kept++] = names[i];
        } else {
            LOG_INFO(Render_Vulkan, "Requested %s %s is not available", kind, names[i]);
        }
    }
    free(props);
    return kept;
}

/// Whether @layer offers the layer settings extension
static bool LayerHasSettings(const char* layer) {
    const char* name = VK_EXT_LAYER_SETTINGS_EXTENSION_NAME;
    if (KeepAvailable(&name, 1, layer, false, "layer extension") == 0) {
        LOG_ERROR(Render_Vulkan, "Settings for layer %s not available.", layer);
        return false;
    }
    return true;
}

/// Loads the Vulkan loader of the system, which the frontend's own Vulkan comes from
static PFN_vkGetInstanceProcAddr LoadVulkan() {
    static constexpr const char* names[] = {
#if defined(_WIN32)
        "vulkan-1.dll",
#elif defined(__APPLE__)
        "libvulkan.dylib",
        "libvulkan.1.dylib",
        "libMoltenVK.dylib",
#else
        "libvulkan.so",
        "libvulkan.so.1",
#endif
    };
    // Kept loaded for as long as the core is
    for (const char* name : names) {
        const dylib_t lib = dylib_load(name);
        if (lib) {
            const PFN_vkGetInstanceProcAddr get_instance_proc_addr =
                reinterpret_cast<PFN_vkGetInstanceProcAddr>(
                    dylib_proc(lib, "vkGetInstanceProcAddr"));
            if (get_instance_proc_addr) {
                return get_instance_proc_addr;
            }
            dylib_close(lib);
        }
    }
    return nullptr;
}

VkInstance CreateInstance(bool enable_validation, bool enable_crash_diagnostic,
                          bool* out_debug_utils) {
    LOG_INFO(Render_Vulkan, "Creating vulkan instance");

    // The Vulkan the frontend's environment provides; the core brings none of its own.
    static const PFN_vkGetInstanceProcAddr get_instance_proc_addr = LoadVulkan();
    ASSERT_MSG(get_instance_proc_addr, "No Vulkan loader to be found");
    vulkan_symbol_wrapper_init(get_instance_proc_addr);
    ASSERT_MSG(vulkan_symbol_wrapper_load_global_symbols(),
               "Failed to load Vulkan's global entry points");

    u32 available_version = VK_API_VERSION_1_0;
    if (vkEnumerateInstanceVersion) {
        const VkResult result = vkEnumerateInstanceVersion(&available_version);
        ASSERT_MSG(result == VK_SUCCESS, "Failed to query Vulkan API version: %s",
                   ResultName(result));
    }
    ASSERT_MSG(available_version >= TargetVulkanApiVersion,
               "Vulkan %u.%u is required, but only %u.%u is supported by instance!",
               VK_VERSION_MAJOR(TargetVulkanApiVersion), VK_VERSION_MINOR(TargetVulkanApiVersion),
               VK_VERSION_MAJOR(available_version), VK_VERSION_MINOR(available_version));

    const char* layers[MaxNames];
    u32 num_layers = 0;
    if (enable_validation) {
        layers[num_layers++] = VALIDATION_LAYER_NAME;
    }
    if (enable_crash_diagnostic) {
        layers[num_layers++] = CRASH_DIAGNOSTIC_LAYER_NAME;
    }
    if (num_layers != 0) {
        num_layers = KeepAvailable(layers, num_layers, nullptr, true, "layer");
    }

    // No surface: the frontend presents the picture.
    const char* extensions[MaxNames];
    extensions[0] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    u32 num_extensions = KeepAvailable(extensions, 1, nullptr, false, "instance extension");
    *out_debug_utils = num_extensions != 0;
    bool any_settings = false;
    for (u32 i = 0; i < num_layers; ++i) {
        any_settings |= LayerHasSettings(layers[i]);
    }
    if (any_settings) {
        extensions[num_extensions++] = VK_EXT_LAYER_SETTINGS_EXTENSION_NAME;
    }

    const VkApplicationInfo application_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "shadPS4",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "shadPS4 Vulkan",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = available_version,
    };

    for (u32 i = 0; i < num_extensions; ++i) {
        LOG_INFO(Render_Vulkan, "Enabled instance extension: %s", extensions[i]);
    }
    for (u32 i = 0; i < num_layers; ++i) {
        LOG_INFO(Render_Vulkan, "Enabled instance layer: %s", layers[i]);
    }

    // Validation settings
    const VkBool32 enable_core = emu_settings_get_vkvalidation_core_enabled() ? VK_TRUE : VK_FALSE;
    const VkBool32 enable_sync = emu_settings_get_vkvalidation_sync_enabled() ? VK_TRUE : VK_FALSE;
    const VkBool32 enable_gpuav = emu_settings_get_vkvalidation_gpu_enabled() ? VK_TRUE : VK_FALSE;

    // Crash diagnostics settings
    const char* log_path = Common::FS::GetUserPathUTF8(Common::FS::PathType::LogDir);
    const VkBool32 enable_force_barriers = VK_TRUE;

    struct Setting {
        const char* layer;
        const char* name;
        VkLayerSettingTypeEXT type;
        const void* value;
    };
    const Setting settings[] = {
        {VALIDATION_LAYER_NAME, "validate_core", VK_LAYER_SETTING_TYPE_BOOL32_EXT, &enable_core},
        {VALIDATION_LAYER_NAME, "validate_sync", VK_LAYER_SETTING_TYPE_BOOL32_EXT, &enable_sync},
        {VALIDATION_LAYER_NAME, "syncval_submit_time_validation", VK_LAYER_SETTING_TYPE_BOOL32_EXT,
         &enable_sync},
        {VALIDATION_LAYER_NAME, "gpuav_enable", VK_LAYER_SETTING_TYPE_BOOL32_EXT, &enable_gpuav},
        {VALIDATION_LAYER_NAME, "gpuav_descriptor_checks", VK_LAYER_SETTING_TYPE_BOOL32_EXT,
         &enable_gpuav},
        {VALIDATION_LAYER_NAME, "gpuav_buffers_validation", VK_LAYER_SETTING_TYPE_BOOL32_EXT,
         &enable_gpuav},
        {VALIDATION_LAYER_NAME, "gpuav_indirect_draws_buffers", VK_LAYER_SETTING_TYPE_BOOL32_EXT,
         &enable_gpuav},
        {VALIDATION_LAYER_NAME, "gpuav_indirect_dispatches_buffers",
         VK_LAYER_SETTING_TYPE_BOOL32_EXT, &enable_gpuav},
        {VALIDATION_LAYER_NAME, "gpuav_indirect_trace_rays_buffers",
         VK_LAYER_SETTING_TYPE_BOOL32_EXT, &enable_gpuav},
        {VALIDATION_LAYER_NAME, "gpuav_buffer_copies", VK_LAYER_SETTING_TYPE_BOOL32_EXT,
         &enable_gpuav},
        {"lunarg_crash_diagnostic", "output_path", VK_LAYER_SETTING_TYPE_STRING_EXT, &log_path},
        {"lunarg_crash_diagnostic", "sync_after_commands", VK_LAYER_SETTING_TYPE_BOOL32_EXT,
         &enable_force_barriers},
    };
    static constexpr u32 NumSettings = sizeof(settings) / sizeof(settings[0]);
    VkLayerSettingEXT layer_settings[NumSettings];
    for (u32 i = 0; i < NumSettings; ++i) {
        layer_settings[i] = VkLayerSettingEXT{
            .pLayerName = settings[i].layer,
            .pSettingName = settings[i].name,
            .type = settings[i].type,
            .valueCount = 1,
            .pValues = settings[i].value,
        };
    }

    const VkLayerSettingsCreateInfoEXT layer_settings_ci = {
        .sType = VK_STRUCTURE_TYPE_LAYER_SETTINGS_CREATE_INFO_EXT,
        .settingCount = NumSettings,
        .pSettings = layer_settings,
    };
    const VkInstanceCreateInfo instance_ci = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = &layer_settings_ci,
        .pApplicationInfo = &application_info,
        .enabledLayerCount = num_layers,
        .ppEnabledLayerNames = layers,
        .enabledExtensionCount = num_extensions,
        .ppEnabledExtensionNames = extensions,
    };

    VkInstance instance = VK_NULL_HANDLE;
    const VkResult instance_result = vkCreateInstance(&instance_ci, nullptr, &instance);
    ASSERT_MSG(instance_result == VK_SUCCESS, "Failed to create instance: %s",
               ResultName(instance_result));

    ASSERT_MSG(vulkan_symbol_wrapper_load_core_instance_symbols(instance),
               "Failed to load Vulkan's instance entry points");
    vulkan_symbol_wrapper_load_extended_symbols(instance, VK_NULL_HANDLE);
    if (*out_debug_utils) {
        // The debug utils' own, which the wrapper leaves to whoever enables them
        VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(instance,
                                                             vkCreateDebugUtilsMessengerEXT);
        VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(instance,
                                                             vkDestroyDebugUtilsMessengerEXT);
        VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(instance,
                                                             vkSetDebugUtilsObjectNameEXT);
    }
    return instance;
}

VkDebugUtilsMessengerEXT CreateDebugCallback(VkInstance instance) {
    const VkDebugUtilsMessengerCreateInfoEXT msg_ci = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = DebugUtilsCallback,
    };
    VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
    if (!vkCreateDebugUtilsMessengerEXT) {
        LOG_ERROR(Render_Vulkan, "No debug utils for the validation layers' messages");
        return messenger;
    }
    const VkResult result = vkCreateDebugUtilsMessengerEXT(instance, &msg_ci, nullptr, &messenger);
    ASSERT_MSG(result == VK_SUCCESS, "Failed to create debug callback: %s", ResultName(result));
    return messenger;
}

void SetObjectNameV(VkDevice device, VkObjectType type, u64 handle, const char* format,
                    va_list args) {
    if (!vkSetDebugUtilsObjectNameEXT) {
        return;
    }
    char name[256];
    vsnprintf(name, sizeof(name), format, args);
    const VkDebugUtilsObjectNameInfoEXT name_info = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = type,
        .objectHandle = handle,
        .pObjectName = name,
    };
    const VkResult result = vkSetDebugUtilsObjectNameEXT(device, &name_info);
    if (result != VK_SUCCESS) {
        LOG_DEBUG(Render_Vulkan, "Could not set object debug name: %s", ResultName(result));
    }
}

} // namespace Vulkan
