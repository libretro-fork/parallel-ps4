// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/assert.h"
#include "common/types.h"
#include "core/emulator_settings.h"
#include "libretro/libretro_bridge.h"
#include "video_core/buffer_cache/guest_import.h"
#include "video_core/overlay/vk_overlay.h"
#include "video_core/renderer_vulkan/liverpool_to_vk.h"
#include "video_core/renderer_vulkan/vk_device_fault.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_memory.h"
#include "video_core/renderer_vulkan/vk_platform.h"

namespace Vulkan {

namespace {

/// Appends @link to the pNext chain whose last next pointer *@tail is, and moves @tail on to it
template <typename T>
void Chain(void**& tail, T& link) {
    *tail = &link;
    tail = &link.pNext;
}

/// The physical devices of @instance (malloc'd), how many into *@out_count
VkPhysicalDevice* EnumeratePhysicalDevices(VkInstance instance, u32* out_count) {
    u32 count = 0;
    VkResult result = vkEnumeratePhysicalDevices(instance, &count, nullptr);
    ASSERT_MSG(result == VK_SUCCESS, "Failed to enumerate physical devices: %s",
               ResultName(result));
    VkPhysicalDevice* const devices =
        static_cast<VkPhysicalDevice*>(malloc((count ? count : 1) * sizeof(VkPhysicalDevice)));
    ASSERT_MSG(devices, "Out of memory enumerating physical devices");
    result = vkEnumeratePhysicalDevices(instance, &count, devices);
    ASSERT_MSG(result == VK_SUCCESS || result == VK_INCOMPLETE,
               "Failed to enumerate physical devices: %s", ResultName(result));
    *out_count = count;
    return devices;
}

/// The device-local memory of the biggest such heap of @physical
VkDeviceSize LargestLocalHeap(VkPhysicalDevice physical) {
    VkPhysicalDeviceMemoryProperties mem;
    vkGetPhysicalDeviceMemoryProperties(physical, &mem);
    VkDeviceSize max = 0;
    for (u32 i = 0; i < mem.memoryHeapCount; i++) {
        const VkMemoryHeap& heap = mem.memoryHeaps[i];
        if ((heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) && heap.size > max) {
            max = heap.size;
        }
    }
    return max;
}

/// Whether @left is a better device to run on than @right: the Vulkan version wanted, then a
/// discrete GPU, then real hardware, then the most memory
bool BetterDevice(VkPhysicalDevice left, VkPhysicalDevice right) {
    VkPhysicalDeviceProperties left_prop;
    VkPhysicalDeviceProperties right_prop;
    vkGetPhysicalDeviceProperties(left, &left_prop);
    vkGetPhysicalDeviceProperties(right, &right_prop);
    const bool left_supports_api = left_prop.apiVersion >= TargetVulkanApiVersion;
    const bool right_supports_api = right_prop.apiVersion >= TargetVulkanApiVersion;
    if (left_supports_api != right_supports_api) {
        return left_supports_api;
    }
    const bool left_is_discrete = left_prop.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
    const bool right_is_discrete = right_prop.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
    if (left_is_discrete != right_is_discrete) {
        return left_is_discrete;
    }
    // Software renderers advertise system memory as device local, so they
    // would win the memory comparison below against real hardware.
    const bool left_is_cpu = left_prop.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU;
    const bool right_is_cpu = right_prop.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU;
    if (left_is_cpu != right_is_cpu) {
        return right_is_cpu;
    }
    return LargestLocalHeap(left) > LargestLocalHeap(right);
}

/// The driver's version as people know it, into @out
void DriverVersionName(VkDriverId driver_id, u32 version, char* out, size_t size) {
    // Extracted from
    // https://github.com/SaschaWillems/vulkan.gpuinfo.org/blob/5dddea46ea1120b0df14eef8f15ff8e318e35462/functions.php#L308-L314
    if (driver_id == VK_DRIVER_ID_NVIDIA_PROPRIETARY) {
        snprintf(out, size, "%u.%u.%u.%u", (version >> 22) & 0x3ff, (version >> 14) & 0x0ff,
                 (version >> 6) & 0x0ff, version & 0x003f);
    } else if (driver_id == VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS) {
        snprintf(out, size, "%u.%u", version >> 14, version & 0x3fff);
    } else {
        snprintf(out, size, "%u.%u.%u", VK_VERSION_MAJOR(version), VK_VERSION_MINOR(version),
                 VK_VERSION_PATCH(version));
    }
}

} // Anonymous namespace

Instance::Instance(bool enable_validation, bool enable_crash_diagnostic)
    : instance{CreateInstance(enable_validation, enable_crash_diagnostic, &debug_utils)} {}

Instance::Instance(s32 physical_device_index, bool enable_validation, bool enable_crash_diagnostic)
    : instance{CreateInstance(enable_validation, enable_crash_diagnostic, &debug_utils)} {
    if (enable_validation && debug_utils) {
        debug_callback = CreateDebugCallback(instance);
    }
    PickPhysicalDevice(physical_device_index);

    u32 count = 0;
    VkResult result =
        vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &count, nullptr);
    if (result == VK_SUCCESS && count != 0) {
        available_extensions =
            static_cast<VkExtensionProperties*>(malloc(count * sizeof(VkExtensionProperties)));
        ASSERT_MSG(available_extensions, "Out of memory querying device extensions");
        result = vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &count,
                                                      available_extensions);
        num_available_extensions = result == VK_SUCCESS || result == VK_INCOMPLETE ? count : 0;
    } else {
        LOG_ERROR(Render_Vulkan, "Could not query supported extensions: %s", ResultName(result));
    }
    CollectFormatProperties();
    vkGetPhysicalDeviceProperties(physical_device, &properties);
    vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);
    CollectDeviceParameters();
    ASSERT_MSG(properties.apiVersion >= TargetVulkanApiVersion,
               "Vulkan %u.%u is required, but only %u.%u is supported by device!",
               VK_VERSION_MAJOR(TargetVulkanApiVersion), VK_VERSION_MINOR(TargetVulkanApiVersion),
               VK_VERSION_MAJOR(properties.apiVersion), VK_VERSION_MINOR(properties.apiVersion));

    CreateDevice();
    CollectPhysicalMemoryInfo();
    CollectImageFormatInfo();
    CollectToolingInfo();
}

Instance::~Instance() {
    if (device != VK_NULL_HANDLE) {
        // Nothing of the overlays' is in use by the device when it goes
        const VkResult result = vkDeviceWaitIdle(device);
        if (result != VK_SUCCESS) {
            LOG_WARNING(Render_Vulkan, "Failed to wait for Vulkan device idle on shutdown: %s",
                        ResultName(result));
        }
        ovl_vk_shutdown();
        delete allocator;
        allocator = nullptr;
        vkDestroyDevice(device, nullptr);
    }
    if (debug_callback != VK_NULL_HANDLE && vkDestroyDebugUtilsMessengerEXT) {
        vkDestroyDebugUtilsMessengerEXT(instance, debug_callback, nullptr);
    }
    free(available_extensions);
    vkDestroyInstance(instance, nullptr);
}

void Instance::PickPhysicalDevice(s32 physical_device_index) {
    u32 num_physical_devices = 0;
    VkPhysicalDevice* const physical_devices =
        EnumeratePhysicalDevices(instance, &num_physical_devices);
    ASSERT_MSG(num_physical_devices > 0, "No physical devices found");
    LOG_INFO(Render_Vulkan, "Found %u physical devices", num_physical_devices);

    // With the picture handed to the frontend on the GPU, the frontend's GPU: an image's
    // memory cannot be handed from one GPU to another (libretro/gpu_frames.cpp).
    u8 frontend_gpu[VK_UUID_SIZE]{};
    if (Libretro::FrontendGpu(frontend_gpu)) {
        for (u32 i = 0; i < num_physical_devices; ++i) {
            VkPhysicalDeviceIDProperties ids = {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES,
            };
            VkPhysicalDeviceProperties2 props = {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
                .pNext = &ids,
            };
            vkGetPhysicalDeviceProperties2(physical_devices[i], &props);
            if (memcmp(ids.deviceUUID, frontend_gpu, VK_UUID_SIZE) == 0) {
                physical_device = physical_devices[i];
                break;
            }
        }
    }
    if (physical_device != VK_NULL_HANDLE) {
        LOG_INFO(Render_Vulkan, "Using the frontend's GPU");
    } else if (physical_device_index < 0) {
        // The best there is; the first of equals, as before
        physical_device = physical_devices[0];
        for (u32 i = 1; i < num_physical_devices; ++i) {
            if (BetterDevice(physical_devices[i], physical_device)) {
                physical_device = physical_devices[i];
            }
        }
    } else {
        ASSERT_MSG(static_cast<u32>(physical_device_index) < num_physical_devices,
                   "Invalid physical device index %d provided when only %u devices exist",
                   physical_device_index, num_physical_devices);
        physical_device = physical_devices[physical_device_index];
    }
    free(physical_devices);
}

bool Instance::HasExtension(const char* name) const {
    for (u32 i = 0; i < num_available_extensions; ++i) {
        if (strcmp(available_extensions[i].extensionName, name) == 0) {
            return true;
        }
    }
    return false;
}

bool Instance::CreateDevice() {
    // What the device has, every feature struct asked for at once
    VkPhysicalDeviceShaderClockFeaturesKHR query_shader_clock = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CLOCK_FEATURES_KHR,
    };
    VkPhysicalDeviceImage2DViewOf3DFeaturesEXT query_2d_view_of_3d = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_2D_VIEW_OF_3D_FEATURES_EXT,
    };
    VkPhysicalDeviceWorkgroupMemoryExplicitLayoutFeaturesKHR query_workgroup_layout = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_WORKGROUP_MEMORY_EXPLICIT_LAYOUT_FEATURES_KHR,
    };
    VkPhysicalDeviceShaderAtomicFloat2FeaturesEXT query_atomic_float2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_2_FEATURES_EXT,
    };
    VkPhysicalDevicePrimitiveTopologyListRestartFeaturesEXT query_list_restart = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRIMITIVE_TOPOLOGY_LIST_RESTART_FEATURES_EXT,
    };
    VkPhysicalDeviceExtendedDynamicState3FeaturesEXT query_dynamic_state_3 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT,
    };
    VkPhysicalDeviceRobustness2FeaturesEXT robustness2_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT,
    };
    VkPhysicalDeviceVulkan13Features query_vk13 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
    };
    VkPhysicalDeviceVulkan12Features query_vk12 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
    };
    VkPhysicalDeviceVulkan11Features vk11_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
    };
    VkPhysicalDeviceFeatures2 query_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
    };
    {
        void** tail = &query_features.pNext;
        Chain(tail, vk11_features);
        Chain(tail, query_vk12);
        Chain(tail, query_vk13);
        Chain(tail, robustness2_features);
        Chain(tail, query_dynamic_state_3);
        Chain(tail, query_list_restart);
        Chain(tail, query_atomic_float2);
        Chain(tail, query_workgroup_layout);
        Chain(tail, query_2d_view_of_3d);
        Chain(tail, query_shader_clock);
    }
    vkGetPhysicalDeviceFeatures2(physical_device, &query_features);
    features = query_features.features;

    VkPhysicalDevicePushDescriptorPropertiesKHR query_push_descriptor = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PUSH_DESCRIPTOR_PROPERTIES_KHR,
    };
    VkPhysicalDeviceVulkan13Properties query_vk13_props = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_PROPERTIES,
        .pNext = &query_push_descriptor,
    };
    VkPhysicalDeviceVulkan12Properties query_vk12_props = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES,
        .pNext = &query_vk13_props,
    };
    VkPhysicalDeviceVulkan11Properties query_vk11_props = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES,
        .pNext = &query_vk12_props,
    };
    VkPhysicalDeviceProperties2 query_properties = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &query_vk11_props,
    };
    vkGetPhysicalDeviceProperties2(physical_device, &query_properties);
    // Kept without the chain they were asked through
    vk11_props = query_vk11_props;
    vk11_props.pNext = nullptr;
    vk12_props = query_vk12_props;
    vk12_props.pNext = nullptr;
    vk13_props = query_vk13_props;
    vk13_props.pNext = nullptr;
    push_descriptor_props = query_push_descriptor;
    push_descriptor_props.pNext = nullptr;
    LOG_INFO(Render_Vulkan, "Physical device subgroup size %u", vk11_props.subgroupSize);

    if (num_available_extensions == 0) {
        LOG_CRITICAL(Render_Vulkan, "No extensions supported by device.");
        return false;
    }

    // Room for the one the libretro core adds below.
    const char* enabled_extensions[48];
    u32 num_enabled_extensions = 0;
    /// Enables an extension the device has; false for one it has not
    struct ExtensionAdder {
        const Instance& self;
        const char** list;
        u32& count;
        u32 capacity;
        bool operator()(const char* extension) const {
            if (self.HasExtension(extension)) {
                LOG_INFO(Render_Vulkan, "Enabling extension: %s", extension);
                ASSERT(count < capacity);
                list[count++] = extension;
                return true;
            }
            LOG_WARNING(Render_Vulkan, "Extension %s unavailable.", extension);
            return false;
        }
    };
    const ExtensionAdder add_extension{
        *this, enabled_extensions, num_enabled_extensions,
        static_cast<u32>(sizeof(enabled_extensions) / sizeof(enabled_extensions[0]))};

    // Required
    ASSERT_MSG(add_extension(VK_KHR_SWAPCHAIN_EXTENSION_NAME),
               "Required Vulkan extension unavailable: %s", VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    ASSERT_MSG(add_extension(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME),
               "Required Vulkan extension unavailable: %s", VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);
    ASSERT_MSG(add_extension(VK_EXT_VERTEX_ATTRIBUTE_DIVISOR_EXTENSION_NAME),
               "Required Vulkan extension unavailable: %s",
               VK_EXT_VERTEX_ATTRIBUTE_DIVISOR_EXTENSION_NAME);
    ASSERT_MSG(add_extension(VK_EXT_ROBUSTNESS_2_EXTENSION_NAME),
               "Required Vulkan extension unavailable: %s", VK_EXT_ROBUSTNESS_2_EXTENSION_NAME);

    ASSERT_MSG(robustness2_features.robustBufferAccess2,
               "Required Vulkan feature unavailable: robustBufferAccess2");
    ASSERT_MSG(robustness2_features.robustImageAccess2,
               "Required Vulkan feature unavailable: robustImageAccess2");
    ASSERT_MSG(robustness2_features.nullDescriptor,
               "Required Vulkan feature unavailable: nullDescriptor");

    // Optional
    maintenance_5 = add_extension(VK_KHR_MAINTENANCE_5_EXTENSION_NAME);
    maintenance_6 = add_extension(VK_KHR_MAINTENANCE_6_EXTENSION_NAME);
    maintenance_8 = add_extension(VK_KHR_MAINTENANCE_8_EXTENSION_NAME);
    attachment_feedback_loop = add_extension(VK_EXT_ATTACHMENT_FEEDBACK_LOOP_LAYOUT_EXTENSION_NAME);
    if (attachment_feedback_loop) {
        attachment_feedback_loop =
            add_extension(VK_EXT_ATTACHMENT_FEEDBACK_LOOP_DYNAMIC_STATE_EXTENSION_NAME);
        if (!attachment_feedback_loop) {
            // We want both extensions so remove the first if the second isn't available
            --num_enabled_extensions;
        }
    }
    depth_range_unrestricted = add_extension(VK_EXT_DEPTH_RANGE_UNRESTRICTED_EXTENSION_NAME);
    dynamic_state_3 = add_extension(VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME);
    if (dynamic_state_3) {
        dynamic_state_3_features = query_dynamic_state_3;
        LOG_INFO(Render_Vulkan, "- extendedDynamicState3ColorWriteMask: %u",
                 dynamic_state_3_features.extendedDynamicState3ColorWriteMask);
    }
    custom_border_color = add_extension(VK_EXT_CUSTOM_BORDER_COLOR_EXTENSION_NAME);
    depth_clip_control = add_extension(VK_EXT_DEPTH_CLIP_CONTROL_EXTENSION_NAME);
    depth_clip_enable = add_extension(VK_EXT_DEPTH_CLIP_ENABLE_EXTENSION_NAME);
    vertex_input_dynamic_state = add_extension(VK_EXT_VERTEX_INPUT_DYNAMIC_STATE_EXTENSION_NAME);
    // Games excessively leave restart enabled with lists when it isn't actually needed, and it
    // slows down KosmicKrisp due to unrolling. Revisit if KosmicKrisp moves this to mesh shaders.
    list_restart = driver_id != VK_DRIVER_ID_MESA_KOSMICKRISP &&
                   add_extension(VK_EXT_PRIMITIVE_TOPOLOGY_LIST_RESTART_EXTENSION_NAME);
    if (list_restart) {
        list_restart_features = query_list_restart;
        LOG_INFO(Render_Vulkan, "- primitiveTopologyListRestart: %u",
                 list_restart_features.primitiveTopologyListRestart);
        LOG_INFO(Render_Vulkan, "- primitiveTopologyPatchListRestart: %u",
                 list_restart_features.primitiveTopologyPatchListRestart);
    }
    amd_shader_explicit_vertex_parameter =
        add_extension(VK_AMD_SHADER_EXPLICIT_VERTEX_PARAMETER_EXTENSION_NAME);
    if (!amd_shader_explicit_vertex_parameter) {
        fragment_shader_barycentric =
            add_extension(VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME);
    }
    provoking_vertex = add_extension(VK_EXT_PROVOKING_VERTEX_EXTENSION_NAME);
    shader_stencil_export = add_extension(VK_EXT_SHADER_STENCIL_EXPORT_EXTENSION_NAME);
    image_load_store_lod = add_extension(VK_AMD_SHADER_IMAGE_LOAD_STORE_LOD_EXTENSION_NAME);
    amd_gcn_shader = add_extension(VK_AMD_GCN_SHADER_EXTENSION_NAME);
    amd_shader_trinary_minmax = add_extension(VK_AMD_SHADER_TRINARY_MINMAX_EXTENSION_NAME);
    nv_framebuffer_mixed_samples = add_extension(VK_NV_FRAMEBUFFER_MIXED_SAMPLES_EXTENSION_NAME);
    if (emu_settings_get_sample_positions() &&
        add_extension(VK_EXT_SAMPLE_LOCATIONS_EXTENSION_NAME)) {
        VkPhysicalDeviceSampleLocationsPropertiesEXT props = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLE_LOCATIONS_PROPERTIES_EXT,
        };
        VkPhysicalDeviceProperties2 props2 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
            .pNext = &props,
        };
        vkGetPhysicalDeviceProperties2(physical_device, &props2);
        // Sixteenths of a pixel, anywhere in the pixel, is what the game's registers hold
        if (props.sampleLocationSubPixelBits >= 4 &&
            props.sampleLocationCoordinateRange[0] <= 0.0f &&
            props.sampleLocationCoordinateRange[1] >= 0.9375f) {
            // The block of pixels the positions can vary over, for 2, 4 and 8 samples
            for (u32 i = 1; i < sizeof(sample_location_grids) / sizeof(u32); ++i) {
                const VkSampleCountFlagBits count = static_cast<VkSampleCountFlagBits>(1u << i);
                if (!(props.sampleLocationSampleCounts & count)) {
                    continue;
                }
                VkMultisamplePropertiesEXT multisample = {
                    .sType = VK_STRUCTURE_TYPE_MULTISAMPLE_PROPERTIES_EXT,
                };
                vkGetPhysicalDeviceMultisamplePropertiesEXT(physical_device, count, &multisample);
                const VkExtent2D grid = multisample.maxSampleLocationGridSize;
                sample_location_grids[i] = Common::Min(Common::Min(grid.width, grid.height), 2u);
            }
        }
        LOG_INFO(Render_Vulkan, "Sample locations: counts 0x%x, grid %ux%u, %u sub-pixel bits",
                 static_cast<u32>(props.sampleLocationSampleCounts),
                 props.maxSampleLocationGridSize.width, props.maxSampleLocationGridSize.height,
                 props.sampleLocationSubPixelBits);
    }
    amd_mixed_attachment_samples = add_extension(VK_AMD_MIXED_ATTACHMENT_SAMPLES_EXTENSION_NAME);
    shader_atomic_float = add_extension(VK_EXT_SHADER_ATOMIC_FLOAT_EXTENSION_NAME);
    shader_atomic_float2 = add_extension(VK_EXT_SHADER_ATOMIC_FLOAT_2_EXTENSION_NAME);
    if (shader_atomic_float2) {
        shader_atomic_float2_features = query_atomic_float2;
        LOG_INFO(Render_Vulkan, "- shaderBufferFloat32AtomicMinMax: %u",
                 shader_atomic_float2_features.shaderBufferFloat32AtomicMinMax);
        LOG_INFO(Render_Vulkan, "- shaderImageFloat32AtomicMinMax: %u",
                 shader_atomic_float2_features.shaderImageFloat32AtomicMinMax);
    }
    workgroup_memory_explicit_layout =
        add_extension(VK_KHR_WORKGROUP_MEMORY_EXPLICIT_LAYOUT_EXTENSION_NAME);
    if (workgroup_memory_explicit_layout) {
        workgroup_memory_explicit_layout_features = query_workgroup_layout;
        LOG_INFO(Render_Vulkan, "- workgroupMemoryExplicitLayout: %u",
                 workgroup_memory_explicit_layout_features.workgroupMemoryExplicitLayout);
        LOG_INFO(Render_Vulkan, "- workgroupMemoryExplicitLayoutScalarBlockLayout: %u",
                 workgroup_memory_explicit_layout_features
                     .workgroupMemoryExplicitLayoutScalarBlockLayout);
        LOG_INFO(
            Render_Vulkan, "- workgroupMemoryExplicitLayout16BitAccess: %u",
            workgroup_memory_explicit_layout_features.workgroupMemoryExplicitLayout16BitAccess);
    }
    image_2d_view_of_3d = add_extension(VK_EXT_IMAGE_2D_VIEW_OF_3D_EXTENSION_NAME);
    if (image_2d_view_of_3d) {
        image_2d_view_of_3d_features = query_2d_view_of_3d;
        LOG_INFO(Render_Vulkan, "- image2DViewOf3D: %u",
                 image_2d_view_of_3d_features.image2DViewOf3D);
        LOG_INFO(Render_Vulkan, "- sampler2DViewOf3D: %u",
                 image_2d_view_of_3d_features.sampler2DViewOf3D);
    }
    image_view_min_lod = add_extension(VK_EXT_IMAGE_VIEW_MIN_LOD_EXTENSION_NAME);
    supports_memory_budget = add_extension(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
    // What the driver has to say of a device it loses (ReportDeviceLost)
    device_fault = add_extension(VK_EXT_DEVICE_FAULT_EXTENSION_NAME);
    // And how far the GPU had got (Mark)
    device_checkpoints = add_extension(VK_NV_DEVICE_DIAGNOSTIC_CHECKPOINTS_EXTENSION_NAME);
#if defined(__linux__)
    // The picture handed to the frontend on the GPU (libretro/gpu_frames.h).
    external_memory_export = add_extension(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME);
#elif defined(_WIN32)
    // The same, by NT handle. Spelled out: its macro comes with Vulkan's Win32 declarations.
    external_memory_export = add_extension("VK_KHR_external_memory_win32");
#endif
    // Game memory the GPU writes a readback straight into (buffer_cache/guest_import.h)
    external_memory_host = add_extension(VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME);
    if (external_memory_host) {
        VkPhysicalDeviceExternalMemoryHostPropertiesEXT host_props = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_HOST_PROPERTIES_EXT,
        };
        VkPhysicalDeviceProperties2 props2 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
            .pNext = &host_props,
        };
        vkGetPhysicalDeviceProperties2(physical_device, &props2);
        min_imported_host_pointer_alignment = host_props.minImportedHostPointerAlignment;
    }
    shader_clock = add_extension(VK_KHR_SHADER_CLOCK_EXTENSION_NAME);
    if (shader_clock) {
        shader_clock_features = query_shader_clock;
        LOG_INFO(Render_Vulkan, "- shaderSubgroupClock: %u",
                 shader_clock_features.shaderSubgroupClock);
    }

    u32 num_families = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &num_families, nullptr);
    if (num_families == 0) {
        LOG_CRITICAL(Render_Vulkan, "Physical device reported no queues.");
        return false;
    }
    VkQueueFamilyProperties* const family_properties = static_cast<VkQueueFamilyProperties*>(
        malloc(num_families * sizeof(VkQueueFamilyProperties)));
    ASSERT_MSG(family_properties, "Out of memory querying queue families");
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &num_families, family_properties);
    bool graphics_queue_found = false;
    for (u32 i = 0; i < num_families; i++) {
        if (family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            queue_family_index = i;
            graphics_queue_found = true;
        }
    }
    free(family_properties);

    if (!graphics_queue_found) {
        LOG_CRITICAL(Render_Vulkan, "Unable to find graphics and/or present queues.");
        return false;
    }

    static constexpr float queue_priority = 1.0f;
    const VkDeviceQueueCreateInfo queue_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = queue_family_index,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority,
    };

    storage_buffer_16bit_access = vk11_features.storageBuffer16BitAccess;
    vk12_features = query_vk12;
    vk12_features.pNext = nullptr;
    vk13_features = query_vk13;
    vk13_features.pNext = nullptr;

    // What is enabled: each struct linked only where its extension is
    VkPhysicalDeviceFeatures2 enable_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .features{
            .robustBufferAccess = features.robustBufferAccess,
            .imageCubeArray = features.imageCubeArray,
            .independentBlend = features.independentBlend,
            .geometryShader = features.geometryShader,
            .tessellationShader = features.tessellationShader,
            .sampleRateShading = features.sampleRateShading,
            .dualSrcBlend = features.dualSrcBlend,
            .logicOp = features.logicOp,
            .multiDrawIndirect = features.multiDrawIndirect,
            .depthClamp = features.depthClamp,
            .depthBiasClamp = features.depthBiasClamp,
            .fillModeNonSolid = features.fillModeNonSolid,
            .depthBounds = features.depthBounds,
            .wideLines = features.wideLines,
            .multiViewport = features.multiViewport,
            .samplerAnisotropy = features.samplerAnisotropy,
            .vertexPipelineStoresAndAtomics = features.vertexPipelineStoresAndAtomics,
            .fragmentStoresAndAtomics = features.fragmentStoresAndAtomics,
            .shaderImageGatherExtended = features.shaderImageGatherExtended,
            .shaderStorageImageExtendedFormats = features.shaderStorageImageExtendedFormats,
            .shaderStorageImageMultisample = features.shaderStorageImageMultisample,
            .shaderClipDistance = features.shaderClipDistance,
            .shaderFloat64 = features.shaderFloat64,
            .shaderInt64 = features.shaderInt64,
            .shaderInt16 = features.shaderInt16,
            .sparseBinding = features.sparseBinding,
            .sparseResidencyBuffer = features.sparseResidencyBuffer,
        },
    };
    VkPhysicalDeviceVulkan11Features enable_vk11 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
        .storageBuffer16BitAccess = vk11_features.storageBuffer16BitAccess,
        .shaderDrawParameters = vk11_features.shaderDrawParameters,
    };
    VkPhysicalDeviceVulkan12Features enable_vk12 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .samplerMirrorClampToEdge = vk12_features.samplerMirrorClampToEdge,
        .drawIndirectCount = vk12_features.drawIndirectCount,
        .storageBuffer8BitAccess = vk12_features.storageBuffer8BitAccess,
        .shaderBufferInt64Atomics = vk12_features.shaderBufferInt64Atomics,
        .shaderSharedInt64Atomics = vk12_features.shaderSharedInt64Atomics,
        .shaderFloat16 = vk12_features.shaderFloat16,
        .shaderInt8 = vk12_features.shaderInt8,
        .scalarBlockLayout = vk12_features.scalarBlockLayout,
        .uniformBufferStandardLayout = vk12_features.uniformBufferStandardLayout,
        .separateDepthStencilLayouts = vk12_features.separateDepthStencilLayouts,
        .hostQueryReset = vk12_features.hostQueryReset,
        .timelineSemaphore = vk12_features.timelineSemaphore,
        .bufferDeviceAddress = vk12_features.bufferDeviceAddress,
        .shaderOutputLayer = vk12_features.shaderOutputLayer,
    };
    VkPhysicalDeviceVulkan13Features enable_vk13 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .robustImageAccess = vk13_features.robustImageAccess,
        .shaderDemoteToHelperInvocation = vk13_features.shaderDemoteToHelperInvocation,
        .subgroupSizeControl = vk13_features.subgroupSizeControl,
        .synchronization2 = vk13_features.synchronization2,
        .dynamicRendering = vk13_features.dynamicRendering,
        .maintenance4 = vk13_features.maintenance4,
    };
    VkPhysicalDeviceCustomBorderColorFeaturesEXT enable_border_color = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_FEATURES_EXT,
        .customBorderColors = VK_TRUE,
        .customBorderColorWithoutFormat = VK_TRUE,
    };
    VkPhysicalDeviceExtendedDynamicState3FeaturesEXT enable_dynamic_state_3 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT,
        .extendedDynamicState3ColorWriteMask =
            dynamic_state_3_features.extendedDynamicState3ColorWriteMask,
    };
    VkPhysicalDeviceDepthClipControlFeaturesEXT enable_clip_control = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_CONTROL_FEATURES_EXT,
        .depthClipControl = VK_TRUE,
    };
    VkPhysicalDeviceDepthClipEnableFeaturesEXT enable_clip_enable = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_ENABLE_FEATURES_EXT,
        .depthClipEnable = VK_TRUE,
    };
    VkPhysicalDeviceRobustness2FeaturesEXT enable_robustness2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT,
        .robustBufferAccess2 = VK_TRUE,
        .robustImageAccess2 = VK_TRUE,
        .nullDescriptor = VK_TRUE,
    };
    VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT enable_vertex_input = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_INPUT_DYNAMIC_STATE_FEATURES_EXT,
        .vertexInputDynamicState = VK_TRUE,
    };
    VkPhysicalDevicePrimitiveTopologyListRestartFeaturesEXT enable_list_restart = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRIMITIVE_TOPOLOGY_LIST_RESTART_FEATURES_EXT,
        .primitiveTopologyListRestart = list_restart_features.primitiveTopologyListRestart,
        .primitiveTopologyPatchListRestart =
            list_restart_features.primitiveTopologyPatchListRestart,
    };
    VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR enable_barycentric = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_KHR,
        .fragmentShaderBarycentric = VK_TRUE,
    };
    VkPhysicalDeviceProvokingVertexFeaturesEXT enable_provoking = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROVOKING_VERTEX_FEATURES_EXT,
        .provokingVertexLast = VK_TRUE,
    };
    VkPhysicalDeviceVertexAttributeDivisorFeatures enable_divisor = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_FEATURES,
        .vertexAttributeInstanceRateDivisor = VK_TRUE,
    };
    VkPhysicalDeviceMaintenance5FeaturesKHR enable_maintenance5 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_5_FEATURES_KHR,
        .maintenance5 = VK_TRUE,
    };
    VkPhysicalDeviceMaintenance6FeaturesKHR enable_maintenance6 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_6_FEATURES_KHR,
        .maintenance6 = VK_TRUE,
    };
    VkPhysicalDeviceMaintenance8FeaturesKHR enable_maintenance8 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_8_FEATURES_KHR,
        .maintenance8 = VK_TRUE,
    };
    VkPhysicalDeviceAttachmentFeedbackLoopLayoutFeaturesEXT enable_feedback_layout = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ATTACHMENT_FEEDBACK_LOOP_LAYOUT_FEATURES_EXT,
        .attachmentFeedbackLoopLayout = VK_TRUE,
    };
    VkPhysicalDeviceAttachmentFeedbackLoopDynamicStateFeaturesEXT enable_feedback_dynamic = {
        .sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ATTACHMENT_FEEDBACK_LOOP_DYNAMIC_STATE_FEATURES_EXT,
        .attachmentFeedbackLoopDynamicState = VK_TRUE,
    };
    VkPhysicalDeviceShaderAtomicFloat2FeaturesEXT enable_atomic_float2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_2_FEATURES_EXT,
        .shaderBufferFloat32AtomicMinMax =
            shader_atomic_float2_features.shaderBufferFloat32AtomicMinMax,
        .shaderImageFloat32AtomicMinMax =
            shader_atomic_float2_features.shaderImageFloat32AtomicMinMax,
    };
    VkPhysicalDeviceWorkgroupMemoryExplicitLayoutFeaturesKHR enable_workgroup_layout = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_WORKGROUP_MEMORY_EXPLICIT_LAYOUT_FEATURES_KHR,
        .workgroupMemoryExplicitLayout =
            workgroup_memory_explicit_layout_features.workgroupMemoryExplicitLayout,
        .workgroupMemoryExplicitLayoutScalarBlockLayout =
            workgroup_memory_explicit_layout_features
                .workgroupMemoryExplicitLayoutScalarBlockLayout,
        .workgroupMemoryExplicitLayout16BitAccess =
            workgroup_memory_explicit_layout_features.workgroupMemoryExplicitLayout16BitAccess,
    };
    VkPhysicalDeviceImage2DViewOf3DFeaturesEXT enable_2d_view_of_3d = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_2D_VIEW_OF_3D_FEATURES_EXT,
        .image2DViewOf3D = image_2d_view_of_3d_features.image2DViewOf3D,
        .sampler2DViewOf3D = image_2d_view_of_3d_features.sampler2DViewOf3D,
    };
    VkPhysicalDeviceImageViewMinLodFeaturesEXT enable_min_lod = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_VIEW_MIN_LOD_FEATURES_EXT,
        .minLod = VK_TRUE,
    };
    VkPhysicalDeviceFaultFeaturesEXT enable_fault = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FAULT_FEATURES_EXT,
        .deviceFault = VK_TRUE,
    };
    VkPhysicalDeviceShaderClockFeaturesKHR enable_shader_clock = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CLOCK_FEATURES_KHR,
        .shaderSubgroupClock = shader_clock_features.shaderSubgroupClock,
    };
    {
        void** tail = &enable_features.pNext;
        Chain(tail, enable_vk11);
        Chain(tail, enable_vk12);
        Chain(tail, enable_vk13);
        if (custom_border_color) {
            Chain(tail, enable_border_color);
        }
        if (dynamic_state_3) {
            Chain(tail, enable_dynamic_state_3);
        }
        if (depth_clip_control) {
            Chain(tail, enable_clip_control);
        }
        if (depth_clip_enable) {
            Chain(tail, enable_clip_enable);
        }
        Chain(tail, enable_robustness2);
        if (vertex_input_dynamic_state) {
            Chain(tail, enable_vertex_input);
        }
        if (list_restart) {
            Chain(tail, enable_list_restart);
        }
        if (fragment_shader_barycentric) {
            Chain(tail, enable_barycentric);
        }
        if (provoking_vertex) {
            Chain(tail, enable_provoking);
        }
        Chain(tail, enable_divisor);
        if (maintenance_5) {
            Chain(tail, enable_maintenance5);
        }
        if (maintenance_6) {
            Chain(tail, enable_maintenance6);
        }
        if (maintenance_8) {
            Chain(tail, enable_maintenance8);
        }
        if (attachment_feedback_loop) {
            Chain(tail, enable_feedback_layout);
            Chain(tail, enable_feedback_dynamic);
        }
        if (shader_atomic_float2) {
            Chain(tail, enable_atomic_float2);
        }
        if (workgroup_memory_explicit_layout) {
            Chain(tail, enable_workgroup_layout);
        }
        if (image_2d_view_of_3d) {
            Chain(tail, enable_2d_view_of_3d);
        }
        if (image_view_min_lod) {
            Chain(tail, enable_min_lod);
        }
        if (device_fault) {
            Chain(tail, enable_fault);
        }
        if (shader_clock) {
            Chain(tail, enable_shader_clock);
        }
    }
    const VkDeviceCreateInfo device_ci = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &enable_features,
        .queueCreateInfoCount = 1u,
        .pQueueCreateInfos = &queue_info,
        .enabledExtensionCount = num_enabled_extensions,
        .ppEnabledExtensionNames = enabled_extensions,
    };

    const VkResult device_result = vkCreateDevice(physical_device, &device_ci, nullptr, &device);
    if (device_result != VK_SUCCESS) {
        LOG_CRITICAL(Render_Vulkan, "Failed to create device: %s", ResultName(device_result));
        device = VK_NULL_HANDLE;
        return false;
    }

    if (!vulkan_symbol_wrapper_load_core_device_symbols(device)) {
        LOG_CRITICAL(Render_Vulkan, "Failed to load Vulkan's device entry points");
        return false;
    }
    vulkan_symbol_wrapper_load_extended_symbols(instance, device);

    vkGetDeviceQueue(device, queue_family_index, 0, &graphics_queue);

    allocator = new MemoryAllocator(physical_device, device, memory_properties, properties.limits,
                                    vk12_features.bufferDeviceAddress, supports_memory_budget);
    return true;
}

void Instance::CollectDeviceParameters() {
    VkPhysicalDeviceDriverProperties driver = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES,
    };
    VkPhysicalDeviceProperties2 props2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &driver,
    };
    vkGetPhysicalDeviceProperties2(physical_device, &props2);

    driver_id = driver.driverID;
    memcpy(vendor_name, driver.driverName, sizeof(vendor_name));
    vendor_name[sizeof(vendor_name) - 1] = '\0';

    char driver_version[32];
    DriverVersionName(driver_id, properties.driverVersion, driver_version, sizeof(driver_version));

    LOG_INFO(Render_Vulkan, "GPU_Vendor: %s", vendor_name);
    LOG_INFO(Render_Vulkan, "GPU_Model: %s", properties.deviceName);
    LOG_INFO(Render_Vulkan, "GPU_Integrated: %s", IsIntegrated() ? "Yes" : "No");
    LOG_INFO(Render_Vulkan, "GPU_Vulkan_Driver: %s %s", vendor_name, driver_version);
    LOG_INFO(Render_Vulkan, "GPU_Vulkan_Version: %u.%u.%u", VK_VERSION_MAJOR(properties.apiVersion),
             VK_VERSION_MINOR(properties.apiVersion), VK_VERSION_PATCH(properties.apiVersion));
    for (u32 i = 0; i < num_available_extensions; ++i) {
        LOG_INFO(Render_Vulkan, "GPU_Vulkan_Extension: %s", available_extensions[i].extensionName);
    }
}

s32 Instance::FormatSlot(VkFormat format) const {
    const u32 value = static_cast<u32>(format);
    if (value < NumCoreFormatSlots) {
        return static_cast<s32>(value);
    }
    for (u32 i = 0; i < num_extension_formats; ++i) {
        if (extension_formats[i] == format) {
            return static_cast<s32>(NumCoreFormatSlots + i);
        }
    }
    return -1;
}

void Instance::QueryFormat(VkFormat format) {
    s32 slot = FormatSlot(format);
    if (slot < 0) {
        ASSERT_MSG(num_extension_formats < MaxExtensionFormats,
                   "More extension formats than are kept (%u)", MaxExtensionFormats);
        extension_formats[num_extension_formats] = format;
        slot = static_cast<s32>(NumCoreFormatSlots + num_extension_formats++);
    }
    if (format_queried[slot]) {
        return;
    }
    VkFormatProperties3& properties3 = format_properties[slot];
    properties3 = VkFormatProperties3{.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_3};
    VkFormatProperties2 properties2 = {
        .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2,
        .pNext = &properties3,
    };
    vkGetPhysicalDeviceFormatProperties2(physical_device, format, &properties2);
    properties3.pNext = nullptr;
    format_queried[slot] = true;
}

void Instance::CollectFormatProperties() {
    for (const LiverpoolToVK::SurfaceFormatInfo& format_info : LiverpoolToVK::SurfaceFormats()) {
        QueryFormat(format_info.vk_format);
    }
    for (const LiverpoolToVK::DepthFormatInfo& format_info : LiverpoolToVK::DepthFormats()) {
        QueryFormat(format_info.vk_format);
    }
    // Other miscellaneous formats, e.g. for color buffers, swizzles, or compatibility
    static constexpr VkFormat misc_formats[] = {
        VK_FORMAT_A2R10G10B10_UNORM_PACK32,
        VK_FORMAT_B8G8R8A8_UNORM,
        VK_FORMAT_B8G8R8A8_SRGB,
        VK_FORMAT_D24_UNORM_S8_UINT,
    };
    for (const VkFormat format : misc_formats) {
        QueryFormat(format);
    }
}

void Instance::CollectPhysicalMemoryInfo() {
    VkPhysicalDeviceMemoryBudgetPropertiesEXT budget = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT,
    };
    VkPhysicalDeviceMemoryProperties2 props = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2,
        .pNext = supports_memory_budget ? &budget : nullptr,
    };
    vkGetPhysicalDeviceMemoryProperties2(physical_device, &props);
    const VkPhysicalDeviceMemoryProperties& memory_props = props.memoryProperties;
    const u32 num_props = memory_props.memoryHeapCount;
    total_memory_budget = 0;
    u64 device_initial_usage = 0;
    for (u32 i = 0; i < num_props; ++i) {
        const bool is_device_local =
            (memory_props.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0;
        if (!IsIntegrated() && !is_device_local) {
            // Ignore non-device local memory on discrete GPUs.
            continue;
        }
        valid_heaps[num_valid_heaps++] = i;
        if (supports_memory_budget) {
            device_initial_usage += budget.heapUsage[i];
            total_memory_budget += budget.heapBudget[i];
            continue;
        }
        // If memory budget is not supported, use the size of the heap as the budget.
        total_memory_budget += memory_props.memoryHeaps[i].size;
    }
    if (!IsIntegrated()) {
        // We reserve some memory for the system.
        const u64 system_memory = Common::Min<u64>(total_memory_budget / 8, 1_GB);
        total_memory_budget -= system_memory;
    } else {
        // Leave at least 8 GB for the system on integrated GPUs.
        const s64 available_memory = static_cast<s64>(total_memory_budget - device_initial_usage);
        total_memory_budget =
            static_cast<u64>(Common::Max<s64>(available_memory - 8_GB, static_cast<s64>(2_GB)));
    }
    LOG_INFO(Render_Vulkan, "GPU memory budget is %llu MiB",
             static_cast<unsigned long long>(total_memory_budget / 1_MB));
}

void Instance::CollectImageFormatInfo() {
    // Check for block texel view support using basic image format info.
    const VkPhysicalDeviceImageFormatInfo2 block_texel_view_info = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
        .format = VK_FORMAT_BC1_RGBA_UNORM_BLOCK,
        .type = VK_IMAGE_TYPE_2D,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT,
        .flags = VK_IMAGE_CREATE_BLOCK_TEXEL_VIEW_COMPATIBLE_BIT,
    };
    VkImageFormatProperties2 block_texel_view_props = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2,
    };
    supports_block_texel_view =
        vkGetPhysicalDeviceImageFormatProperties2(physical_device, &block_texel_view_info,
                                                  &block_texel_view_props) == VK_SUCCESS;
    LOG_INFO(Render_Vulkan, "Block Texel View support: %s",
             supports_block_texel_view ? "Yes" : "No");

    // Check and log format support details.
    for (const LiverpoolToVK::SurfaceFormatInfo& format : LiverpoolToVK::SurfaceFormats()) {
        const VkFormat vk_format = format.vk_format;
        const VkFormatFeatureFlags2 flags = format.flags;
        if ((GetFormatFeatureFlags(vk_format) & flags) != flags &&
            vk_format != VK_FORMAT_UNDEFINED) {
            LOG_WARNING(Render_Vulkan,
                        "Surface format data_format=%u, number_format=%u is not fully supported "
                        "(vk_format=%d, missing features=0x%llx)",
                        static_cast<u32>(format.data_format),
                        static_cast<u32>(format.number_format), static_cast<int>(vk_format),
                        static_cast<unsigned long long>(flags & ~GetFormatFeatureFlags(vk_format)));
        }
    }
    for (const LiverpoolToVK::DepthFormatInfo& format : LiverpoolToVK::DepthFormats()) {
        const VkFormat vk_format = format.vk_format;
        const VkFormatFeatureFlags2 flags = format.flags;
        if ((GetFormatFeatureFlags(vk_format) & flags) != flags &&
            vk_format != VK_FORMAT_UNDEFINED) {
            LOG_WARNING(Render_Vulkan,
                        "Depth format z_format=%u, stencil_format=%u is not fully supported "
                        "(vk_format=%d, missing features=0x%llx)",
                        static_cast<u32>(format.z_format), static_cast<u32>(format.stencil_format),
                        static_cast<int>(vk_format),
                        static_cast<unsigned long long>(flags & ~GetFormatFeatureFlags(vk_format)));
        }
    }
}

void Instance::CollectToolingInfo() const {
    if (driver_id == VK_DRIVER_ID_AMD_PROPRIETARY ||
        driver_id == VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS) {
        // AMD: Causes issues with Reshade.
        // Intel: Causes crash on start.
        return;
    }
    VkPhysicalDeviceToolProperties tools[16];
    u32 num_tools = sizeof(tools) / sizeof(tools[0]);
    for (u32 i = 0; i < num_tools; ++i) {
        tools[i] = VkPhysicalDeviceToolProperties{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TOOL_PROPERTIES,
        };
    }
    const VkResult result = vkGetPhysicalDeviceToolProperties(physical_device, &num_tools, tools);
    if (result != VK_SUCCESS && result != VK_INCOMPLETE) {
        LOG_ERROR(Render_Vulkan, "Could not get Vulkan tool properties: %s", ResultName(result));
        return;
    }
    for (u32 i = 0; i < num_tools; ++i) {
        LOG_INFO(Render_Vulkan, "Attached debugging tool: %s", tools[i].name);
    }
}

void Instance::ReportDeviceLost() const {
    vk_device_fault_report(device, device_fault ? vkGetDeviceFaultInfoEXT : nullptr, graphics_queue,
                           device_checkpoints ? vkGetQueueCheckpointDataNV : nullptr);
}

void Instance::Note(const char* what, u64 a, u64 b, u64 c, u64 d) const {
    vk_device_fault_mark(what, a, b, c, d);
}

void Instance::Mark(VkCommandBuffer cmdbuf, const char* what, u64 a, u64 b, u64 c, u64 d) const {
    const void* const mark = vk_device_fault_mark(what, a, b, c, d);
    if (device_checkpoints) {
        vkCmdSetCheckpointNV(cmdbuf, mark);
    }
}

u64 Instance::GetDeviceMemoryUsage() const {
    VkPhysicalDeviceMemoryBudgetPropertiesEXT memory_budget_props = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT,
    };
    VkPhysicalDeviceMemoryProperties2 props = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2,
        .pNext = &memory_budget_props,
    };
    vkGetPhysicalDeviceMemoryProperties2(physical_device, &props);

    u64 total_usage = 0;
    for (u32 i = 0; i < num_valid_heaps; ++i) {
        total_usage += memory_budget_props.heapUsage[valid_heaps[i]];
    }
    return total_usage;
}

VkFormatFeatureFlags2 Instance::GetFormatFeatureFlags(VkFormat format) const {
    const s32 slot = FormatSlot(format);
    if (slot < 0 || !format_queried[slot]) {
        UNIMPLEMENTED_MSG("Properties of format %d have not been queried.",
                          static_cast<int>(format));
        return 0;
    }
    return format_properties[slot].optimalTilingFeatures | format_properties[slot].bufferFeatures;
}

bool Instance::IsFormatSupported(const VkFormat format, const VkFormatFeatureFlags2 flags) const {
    if (format == VK_FORMAT_UNDEFINED) [[unlikely]] {
        return true;
    }
    return (GetFormatFeatureFlags(format) & flags) == flags;
}

VkFormat Instance::GetSupportedFormat(const VkFormat format,
                                      const VkFormatFeatureFlags2 flags) const {
    if (!IsFormatSupported(format, flags)) [[unlikely]] {
        switch (format) {
        case VK_FORMAT_D16_UNORM_S8_UINT:
            if (IsFormatSupported(VK_FORMAT_D24_UNORM_S8_UINT, flags)) {
                return VK_FORMAT_D24_UNORM_S8_UINT;
            }
            if (IsFormatSupported(VK_FORMAT_D32_SFLOAT_S8_UINT, flags)) {
                return VK_FORMAT_D32_SFLOAT_S8_UINT;
            }
            break;
        case VK_FORMAT_R8_SRGB:
            if (IsFormatSupported(VK_FORMAT_R8_UNORM, flags)) {
                return VK_FORMAT_R8_UNORM;
            }
            break;
        case VK_FORMAT_R8G8_SRGB:
            if (IsFormatSupported(VK_FORMAT_R8G8_UNORM, flags)) {
                return VK_FORMAT_R8G8_UNORM;
            }
            break;
        default:
            break;
        }
    }
    return format;
}

void Instance::FillHostImport(guest_import_info* info, void* base, u64 size, u32 usage) const {
    *info = {};
    info->device = device;
    info->memory = memory_properties;
    info->alignment = min_imported_host_pointer_alignment;
    info->base = static_cast<uint8_t*>(base);
    info->size = size;
    info->usage = usage;
    info->import_on_use = 1;
    info->fns.allocate_memory = vkAllocateMemory;
    info->fns.free_memory = vkFreeMemory;
    info->fns.create_buffer = vkCreateBuffer;
    info->fns.destroy_buffer = vkDestroyBuffer;
    info->fns.get_buffer_memory_requirements = vkGetBufferMemoryRequirements;
    info->fns.bind_buffer_memory = vkBindBufferMemory;
    info->fns.get_memory_host_pointer_properties = vkGetMemoryHostPointerPropertiesEXT;
}

} // namespace Vulkan
