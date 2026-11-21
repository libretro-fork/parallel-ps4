// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "video_core/renderer_vulkan/vk_platform.h"

struct guest_import_info;

namespace Vulkan {

class MemoryAllocator;

class Instance {
public:
    explicit Instance(bool validation = false, bool crash_diagnostic = false);
    Instance(s32 physical_device_index, bool enable_validation, bool enable_crash_diagnostic);
    ~Instance();

    /// Gets a compatibility format if the format is not supported.
    [[nodiscard]] VkFormat GetSupportedFormat(VkFormat format, VkFormatFeatureFlags2 flags) const;

    /// Returns the Vulkan instance
    VkInstance GetInstance() const {
        return instance;
    }

    /// Returns the current physical device
    VkPhysicalDevice GetPhysicalDevice() const {
        return physical_device;
    }

    /// Returns the Vulkan device
    VkDevice GetDevice() const {
        return device;
    }

    /// Returns the device memory allocator
    MemoryAllocator* GetAllocator() const {
        return allocator;
    }

    /// Retrieve queue information
    u32 GetGraphicsQueueFamilyIndex() const {
        return queue_family_index;
    }

    VkQueue GetGraphicsQueue() const {
        return graphics_queue;
    }

    /// Returns true if anisotropic filtering is supported
    bool IsAnisotropicFilteringSupported() const {
        return features.samplerAnisotropy;
    }

    /// Returns true if depth bounds testing is supported
    bool IsDepthBoundsSupported() const {
        return features.depthBounds;
    }

    /// Returns true if 16-bit floats are supported in shaders
    bool IsShaderFloat16Supported() const {
        return vk12_features.shaderFloat16;
    }

    /// Returns true if 64-bit floats are supported in shaders
    bool IsShaderFloat64Supported() const {
        return features.shaderFloat64;
    }

    /// Returns true if 64-bit ints are supported in shaders
    bool IsShaderInt64Supported() const {
        return features.shaderInt64;
    }

    /// Returns true if 16-bit ints are supported in shaders
    bool IsShaderInt16Supported() const {
        return features.shaderInt16;
    }

    /// Whether shaders can read and write 8-bit and 16-bit values in storage buffers. Some
    /// mobile GPUs cannot (Adreno 618 with Turnip): the tiling shaders take 32-bit words there.
    bool IsStorageBuffer8BitAccessSupported() const {
        return vk12_features.storageBuffer8BitAccess;
    }
    bool IsStorageBuffer16BitAccessSupported() const {
        return storage_buffer_16bit_access;
    }

    /// Returns true if 8-bit ints are supported in shaders
    bool IsShaderInt8Supported() const {
        return vk12_features.shaderInt8;
    }

    bool IsMaintenance5Supported() const {
        return maintenance_5;
    }

    bool IsMaintenance6Supported() const {
        return maintenance_6;
    }

    /// Returns true if VK_KHR_maintenance8 is supported
    bool IsMaintenance8Supported() const {
        return maintenance_8;
    }

    /// Returns true if VK_EXT_attachment_feedback_loop_layout is supported
    bool IsAttachmentFeedbackLoopLayoutSupported() const {
        return attachment_feedback_loop;
    }

    /// Returns true when VK_EXT_custom_border_color is supported
    bool IsCustomBorderColorSupported() const {
        return custom_border_color;
    }

    /// Returns true when VK_EXT_shader_stencil_export is supported
    bool IsShaderStencilExportSupported() const {
        return shader_stencil_export;
    }

    /// Returns true when VK_EXT_depth_clip_control is supported
    bool IsDepthClipControlSupported() const {
        return depth_clip_control;
    }

    /// Returns true when VK_EXT_depth_clip_enable is supported
    bool IsDepthClipEnableSupported() const {
        return depth_clip_enable;
    }

    /// Returns true when VK_EXT_depth_range_unrestricted is supported
    bool IsDepthRangeUnrestrictedSupported() const {
        return depth_range_unrestricted;
    }

    /// Returns true when VK_EXT_extended_dynamic_state3 is supported
    bool IsExtendedDynamicState3Supported() const {
        return dynamic_state_3;
    }

    /// Returns true when the extendedDynamicState3ColorWriteMask feature of
    /// VK_EXT_extended_dynamic_state3 is supported.
    bool IsDynamicColorWriteMaskSupported() const {
        return dynamic_state_3 && dynamic_state_3_features.extendedDynamicState3ColorWriteMask;
    }

    /// Returns true when VK_EXT_vertex_input_dynamic_state is supported.
    bool IsVertexInputDynamicState() const {
        return vertex_input_dynamic_state;
    }

    /// Returns true when VK_KHR_fragment_shader_barycentric is supported.
    bool IsFragmentShaderBarycentricSupported() const {
        return fragment_shader_barycentric;
    }

    /// Returns true when VK_AMD_shader_explicit_vertex_parameter is supported.
    bool IsAmdShaderExplicitVertexParameterSupported() const {
        return amd_shader_explicit_vertex_parameter;
    }

    /// Returns true when VK_EXT_primitive_topology_list_restart is supported for regular lists.
    bool IsListRestartSupported() const {
        return list_restart && list_restart_features.primitiveTopologyListRestart;
    }

    /// Returns true when VK_EXT_primitive_topology_list_restart is supported for patch lists.
    bool IsPatchListRestartSupported() const {
        return list_restart && list_restart_features.primitiveTopologyPatchListRestart;
    }

    /// Returns true when VK_EXT_provoking_vertex is supported.
    bool IsProvokingVertexSupported() const {
        return provoking_vertex;
    }

    /// Returns true when VK_AMD_shader_image_load_store_lod is supported.
    bool IsImageLoadStoreLodSupported() const {
        return image_load_store_lod;
    }

    /// Returns true when VK_AMD_gcn_shader is supported.
    bool IsAmdGcnShaderSupported() const {
        return amd_gcn_shader;
    }

    /// Returns true when VK_AMD_shader_trinary_minmax is supported.
    bool IsAmdShaderTrinaryMinMaxSupported() const {
        return amd_shader_trinary_minmax;
    }

    /// Returns true when the shaderBufferFloat32AtomicMinMax feature of
    /// VK_EXT_shader_atomic_float2 is supported.
    bool IsShaderAtomicFloatBuffer32MinMaxSupported() const {
        return shader_atomic_float2 &&
               shader_atomic_float2_features.shaderBufferFloat32AtomicMinMax;
    }

    /// Returns true when the shaderImageFloat32AtomicMinMax feature of
    /// VK_EXT_shader_atomic_float2 is supported.
    bool IsShaderAtomicFloatImage32MinMaxSupported() const {
        return shader_atomic_float2 && shader_atomic_float2_features.shaderImageFloat32AtomicMinMax;
    }

    /// Returns true if 64-bit integer atomic operations can be used on buffers
    bool IsBufferInt64AtomicsSupported() const {
        return vk12_features.shaderBufferInt64Atomics;
    }

    /// Returns true if 64-bit integer atomic operations can be used on shared memory
    bool IsSharedInt64AtomicsSupported() const {
        return vk12_features.shaderSharedInt64Atomics;
    }

    /// Returns true if the subgroup size can be set to match guest subgroup size
    bool IsSubgroupSize64Supported() const {
        return vk13_features.subgroupSizeControl && vk13_props.maxSubgroupSize >= 64;
    }

    /// Returns true when VK_KHR_workgroup_memory_explicit_layout is supported.
    bool IsWorkgroupMemoryExplicitLayoutSupported() const {
        return workgroup_memory_explicit_layout &&
               workgroup_memory_explicit_layout_features.workgroupMemoryExplicitLayout16BitAccess;
    }

    /// Returns true if VK_NV_framebuffer_mixed_samples or
    /// VK_AMD_mixed_attachment_samples is supported
    bool IsMixedDepthSamplesSupported() const {
        return nv_framebuffer_mixed_samples || amd_mixed_attachment_samples;
    }

    /// The side of the pixel block over which a pass of @num_samples can be given its own
    /// sample positions (VK_EXT_sample_locations); 0 when it cannot be given any
    u32 SampleLocationGrid(u32 num_samples) const {
        return num_samples == 2 || num_samples == 4 || num_samples == 8
                   ? sample_location_grids[__builtin_ctz(num_samples)]
                   : 0;
    }

    /// Returns true if VK_AMD_mixed_attachment_samples is supported
    bool IsMixedAnySamplesSupported() const {
        return amd_mixed_attachment_samples;
    }

    /// Returns true when geometry shaders are supported by the device
    bool IsGeometryStageSupported() const {
        return features.geometryShader;
    }

    /// Returns true when tessellation is supported by the device
    bool IsTessellationSupported() const {
        return features.tessellationShader;
    }

    /// Returns true when the shaderSubgroupClock feature of
    /// VK_KHR_shader_clock is supported.
    bool IsShaderSubgroupClockSupported() const {
        return shader_clock && shader_clock_features.shaderSubgroupClock;
    }

    /// Returns the vendor ID of the physical device
    u32 GetVendorID() const {
        return properties.vendorID;
    }

    /// Returns the device ID of the physical device
    u32 GetDeviceID() const {
        return properties.deviceID;
    }

    /// Returns the driver ID.
    VkDriverId GetDriverID() const {
        return driver_id;
    }

    /// Returns the current driver version provided in Vulkan-formatted version numbers.
    u32 GetDriverVersion() const {
        return properties.driverVersion;
    }

    /// Returns the current Vulkan API version provided in Vulkan-formatted version numbers.
    u32 ApiVersion() const {
        return properties.apiVersion;
    }

    /// Returns if the device is an integrated GPU.
    bool IsIntegrated() const {
        return properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
    }

    /// Whether the device can export an image's memory - as a descriptor on Linux, an NT
    /// handle on Windows - which is how the libretro core hands the picture to the frontend on
    /// the GPU
    bool IsExternalMemoryExportSupported() const {
        return external_memory_export;
    }

    /// Whether host memory can be imported as device memory (VK_EXT_external_memory_host)
    bool IsExternalMemoryHostSupported() const {
        return external_memory_host;
    }

    /// The alignment of a host pointer and size to import
    VkDeviceSize GetMinImportedHostPointerAlignment() const {
        return min_imported_host_pointer_alignment;
    }

    /// @info filled in to lend [base, base + size) of this process's memory to this device
    /// (buffer_cache/guest_import.h), for buffers of @usage, imported as they are first wanted
    void FillHostImport(guest_import_info* info, void* base, u64 size, u32 usage) const;

    /// Returns the pipeline cache unique identifier, VK_UUID_SIZE bytes
    const u8* GetPipelineCacheUUID() const {
        return properties.pipelineCacheUUID;
    }

    /// Whether the device has sparse buffers at all
    bool IsSparseBindingSupported() const {
        return features.sparseBinding;
    }

    /// Whether a sparse buffer may be used with parts of it left unbound
    bool IsSparseResidencyBufferSupported() const {
        return features.sparseResidencyBuffer;
    }

    /// Returns how much address space all sparse resources of the device share
    VkDeviceSize SparseAddressSpaceSize() const {
        return properties.limits.sparseAddressSpaceSize;
    }

    /// Returns the minimum required alignment for uniforms
    VkDeviceSize UniformMinAlignment() const {
        return properties.limits.minUniformBufferOffsetAlignment;
    }

    /// Returns the minimum required alignment for storage buffers
    VkDeviceSize StorageMinAlignment() const {
        return properties.limits.minStorageBufferOffsetAlignment;
    }

    /// Returns the minimum alignment required for accessing host-mapped device memory
    VkDeviceSize NonCoherentAtomSize() const {
        return properties.limits.nonCoherentAtomSize;
    }

    /// Returns the subgroup size of the selected physical device.
    u32 SubgroupSize() const {
        return vk11_props.subgroupSize;
    }

    /// Returns the maximum size of compute shared memory.
    u32 MaxComputeSharedMemorySize() const {
        return properties.limits.maxComputeSharedMemorySize;
    }

    /// Returns the maximum sampler LOD bias.
    float MaxSamplerLodBias() const {
        return properties.limits.maxSamplerLodBias;
    }

    /// Returns the maximum sampler anisotropy.
    float MaxSamplerAnisotropy() const {
        return properties.limits.maxSamplerAnisotropy;
    }

    /// Returns the maximum number of push descriptors.
    u32 MaxPushDescriptors() const {
        return push_descriptor_props.maxPushDescriptors;
    }

    /// Returns the maximum size of a single VkDeviceMemory
    VkDeviceSize MaxMemoryAllocationSize() const {
        return vk11_props.maxMemoryAllocationSize;
    }

    /// Returns the vulkan 1.2 physical device properties.
    const VkPhysicalDeviceVulkan12Properties& GetVk12Properties() const noexcept {
        return vk12_props;
    }

    /// Returns the memory properties of the physical device.
    const VkPhysicalDeviceMemoryProperties& GetMemoryProperties() const noexcept {
        return memory_properties;
    }

    /// Returns true if shaders can declare the ClipDistance attribute
    bool IsShaderClipDistanceSupported() const {
        return features.shaderClipDistance;
    }

    /// Returns the maximim viewport width.
    u32 GetMaxViewportWidth() const {
        return properties.limits.maxViewportDimensions[0];
    }

    /// Returns the maximum viewport height.
    u32 GetMaxViewportHeight() const {
        return properties.limits.maxViewportDimensions[1];
    }

    /// Returns the maximum render area width.
    u32 GetMaxFramebufferWidth() const {
        return properties.limits.maxFramebufferWidth;
    }

    /// Returns the maximum render area height.
    u32 GetMaxFramebufferHeight() const {
        return properties.limits.maxFramebufferHeight;
    }

    /// Returns the maximum number of samplers that can be allocated at once.
    u32 GetMaxSamplerAllocationCount() const {
        if (driver_id == VK_DRIVER_ID_MESA_KOSMICKRISP) {
            // FIXME: KosmicKrisp has an internal 1024 unique sampler limit before
            // vkCreateSampler starts returning VK_ERROR_OUT_OF_HOST_MEMORY. Work
            // around this for now by reducing the value to 1024.
            return 1024;
        }
        return properties.limits.maxSamplerAllocationCount;
    }

    /// Returns the sample count flags supported by color buffers.
    VkSampleCountFlags GetColorSampleCounts() const {
        return properties.limits.framebufferColorSampleCounts;
    }

    /// Returns the sample count flags supported by depth buffer.
    VkSampleCountFlags GetDepthSampleCounts() const {
        return properties.limits.framebufferDepthSampleCounts &
               properties.limits.framebufferStencilSampleCounts;
    }

    /// Returns true if logic ops are supported by the device.
    bool IsLogicOpSupported() const {
        return features.logicOp;
    }

    /// Returns whether VK_IMAGE_CREATE_BLOCK_TEXEL_VIEW_COMPATIBLE_BIT is supported on compressed
    /// images.
    bool IsBlockTexelViewSupported() const {
        return supports_block_texel_view;
    }

    /// Returns whether VK_IMAGE_CREATE_2D_VIEW_COMPATIBLE_BIT_EXT is supported on 3D images
    bool Is2dViewOf3dSupported() const {
        return image_2d_view_of_3d && image_2d_view_of_3d_features.image2DViewOf3D &&
               image_2d_view_of_3d_features.sampler2DViewOf3D;
    }

    /// Returns whether VK_EXT_image_view_min_lod is supported.
    bool IsImageViewMinLodSupported() const {
        return image_view_min_lod;
    }

    /// Whether storage images can be of the extended formats (R8ui, R16ui, Rg32ui and others)
    bool IsStorageImageExtendedFormatsSupported() const {
        return features.shaderStorageImageExtendedFormats;
    }

    /// Returns whether shaderStorageImageMultisample is supported.
    bool IsMultisampleStorageImageSupported() const {
        return features.shaderStorageImageMultisample;
    }

    /// Returns whether the device can report memory usage.
    bool CanReportMemoryUsage() const {
        return supports_memory_budget;
    }

    /// Returns the amount of memory used.
    [[nodiscard]] u64 GetDeviceMemoryUsage() const;

    /// The driver's account of a device it has lost, into the log (vk_device_fault.h)
    void ReportDeviceLost() const;

    /// Notes an operation about to be recorded into @cmdbuf, for that report: what it is and
    /// numbers that say which. Where the driver keeps checkpoints the report names the last
    /// the GPU reached.
    void Mark(VkCommandBuffer cmdbuf, const char* what, u64 a = 0, u64 b = 0, u64 c = 0,
              u64 d = 0) const;

    /// The same for what is not a command of its own (what the next one works on)
    void Note(const char* what, u64 a = 0, u64 b = 0, u64 c = 0, u64 d = 0) const;

    /// Returns the total memory budget available to the device.
    [[nodiscard]] u64 GetTotalMemoryBudget() const {
        return total_memory_budget;
    }

    /// Determines if a format is supported for a set of feature flags.
    [[nodiscard]] bool IsFormatSupported(VkFormat format, VkFormatFeatureFlags2 flags) const;

private:
    /// Picks the physical device: the frontend's where the picture is handed to it on the GPU,
    /// else the one at @physical_device_index, else the best there is
    void PickPhysicalDevice(s32 physical_device_index);

    /// Creates the logical device opportunistically enabling extensions
    bool CreateDevice();

    /// Whether the device offers the extension @name
    bool HasExtension(const char* name) const;

    /// Collects various information from the device.
    void CollectDeviceParameters();
    void CollectFormatProperties();
    void CollectPhysicalMemoryInfo();
    void CollectImageFormatInfo();
    void CollectToolingInfo() const;

    /// The supported feature flags for @format, which is among those queried
    [[nodiscard]] VkFormatFeatureFlags2 GetFormatFeatureFlags(VkFormat format) const;

    /// Where @format's properties are kept (format_properties), -1 for one not queried
    s32 FormatSlot(VkFormat format) const;

    /// Queries @format's properties, once
    void QueryFormat(VkFormat format);

private:
    VkInstance instance{VK_NULL_HANDLE};
    VkPhysicalDevice physical_device{VK_NULL_HANDLE};
    VkDevice device{VK_NULL_HANDLE};
    VkPhysicalDeviceProperties properties{};
    VkPhysicalDeviceMemoryProperties memory_properties{};
    VkPhysicalDeviceVulkan11Properties vk11_props{};
    VkPhysicalDeviceVulkan12Properties vk12_props{};
    VkPhysicalDeviceVulkan13Properties vk13_props{};
    VkPhysicalDevicePushDescriptorPropertiesKHR push_descriptor_props{};
    VkPhysicalDeviceFeatures features{};
    VkPhysicalDeviceVulkan12Features vk12_features{};
    VkPhysicalDeviceVulkan13Features vk13_features{};
    bool storage_buffer_16bit_access{};
    VkPhysicalDeviceExtendedDynamicState3FeaturesEXT dynamic_state_3_features{};
    VkPhysicalDeviceShaderAtomicFloat2FeaturesEXT shader_atomic_float2_features{};
    VkPhysicalDeviceWorkgroupMemoryExplicitLayoutFeaturesKHR
        workgroup_memory_explicit_layout_features{};
    VkPhysicalDeviceImage2DViewOf3DFeaturesEXT image_2d_view_of_3d_features{};
    VkPhysicalDevicePrimitiveTopologyListRestartFeaturesEXT list_restart_features{};
    VkPhysicalDeviceShaderClockFeaturesKHR shader_clock_features{};
    VkDriverId driver_id{};
    VkDebugUtilsMessengerEXT debug_callback{VK_NULL_HANDLE};
    bool debug_utils{};
    char vendor_name[VK_MAX_DRIVER_NAME_SIZE]{};
    MemoryAllocator* allocator{};
    VkQueue graphics_queue{VK_NULL_HANDLE};
    /// The device's extensions (malloc'd)
    VkExtensionProperties* available_extensions{};
    u32 num_available_extensions{};
    /// The properties of the formats the core uses: core formats by their value, the few of
    /// extensions after them (FormatSlot)
    static constexpr u32 NumCoreFormatSlots = VK_FORMAT_ASTC_12x12_SRGB_BLOCK + 1;
    static constexpr u32 MaxExtensionFormats = 32;
    static constexpr u32 NumFormatSlots = NumCoreFormatSlots + MaxExtensionFormats;
    VkFormatProperties3 format_properties[NumFormatSlots]{};
    bool format_queried[NumFormatSlots]{};
    VkFormat extension_formats[MaxExtensionFormats]{};
    u32 num_extension_formats{};
    u32 queue_family_index{0};
    bool custom_border_color{};
    bool fragment_shader_barycentric{};
    bool amd_shader_explicit_vertex_parameter{};
    bool depth_clip_control{};
    bool depth_clip_enable{};
    bool dynamic_state_3{};
    bool depth_range_unrestricted{};
    bool vertex_input_dynamic_state{};
    bool list_restart{};
    bool provoking_vertex{};
    bool shader_stencil_export{};
    bool image_load_store_lod{};
    bool amd_gcn_shader{};
    bool amd_shader_trinary_minmax{};
    bool nv_framebuffer_mixed_samples{};
    u32 sample_location_grids[4]{};
    bool amd_mixed_attachment_samples{};
    bool shader_atomic_float{};
    bool shader_atomic_float2{};
    bool workgroup_memory_explicit_layout{};
    bool maintenance_5{};
    bool maintenance_6{};
    bool maintenance_8{};
    bool attachment_feedback_loop{};
    bool image_2d_view_of_3d{};
    bool image_view_min_lod{};
    bool shader_clock{};
    bool supports_memory_budget{};
    bool device_fault{};
    bool device_checkpoints{};
    bool external_memory_export{};
    bool external_memory_host{};
    VkDeviceSize min_imported_host_pointer_alignment{};
    bool supports_block_texel_view{};
    u64 total_memory_budget{};
    /// The heaps counted in the memory used (GetDeviceMemoryUsage)
    u32 valid_heaps[VK_MAX_MEMORY_HEAPS]{};
    u32 num_valid_heaps{};
};

} // namespace Vulkan
