// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include "video_core/renderer_vulkan/liverpool_to_vk.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/texture_cache/sampler.h"

namespace VideoCore {

Sampler::Sampler(const Vulkan::Instance& instance, const AmdGpu::Sampler& sampler,
                 const AmdGpu::BorderColorBuffer border_color_base, const bool is_depth) {
    using namespace Vulkan;
    const bool anisotropy_enable = instance.IsAnisotropicFilteringSupported() &&
                                   (AmdGpu::IsAnisoFilter(sampler.xy_mag_filter) ||
                                    AmdGpu::IsAnisoFilter(sampler.xy_min_filter));
    const float max_anisotropy =
        anisotropy_enable ? std::clamp(sampler.MaxAniso(), 1.0f, instance.MaxSamplerAnisotropy())
                          : 1.0f;
    auto border_color =
        static_cast<VkBorderColor>(LiverpoolToVK::BorderColor(sampler.border_color_type));
    if (border_color == VK_BORDER_COLOR_FLOAT_CUSTOM_EXT &&
        !instance.IsCustomBorderColorSupported()) {
        LOG_WARNING(Render_Vulkan, "Custom border color is not supported, falling back to black");
        border_color = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    }

    VkSamplerCustomBorderColorCreateInfoEXT custom_color{};
    custom_color.sType = VK_STRUCTURE_TYPE_SAMPLER_CUSTOM_BORDER_COLOR_CREATE_INFO_EXT;
    if (border_color == VK_BORDER_COLOR_FLOAT_CUSTOM_EXT) {
        const auto border_color_index = sampler.border_color_ptr.Value();
        const auto border_color_buffer = border_color_base.Address<std::array<float, 4>*>();
        const auto& custom_border_color_array = border_color_buffer[border_color_index];
        for (u32 i = 0; i < 4; ++i) {
            custom_color.customBorderColor.float32[i] = custom_border_color_array[i];
        }
        custom_color.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    }

    VkSamplerCreateInfo sampler_ci{};
    sampler_ci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_ci.pNext = border_color == VK_BORDER_COLOR_FLOAT_CUSTOM_EXT ? &custom_color : nullptr;
    sampler_ci.magFilter = static_cast<VkFilter>(LiverpoolToVK::Filter(sampler.xy_mag_filter));
    sampler_ci.minFilter = static_cast<VkFilter>(LiverpoolToVK::Filter(sampler.xy_min_filter));
    sampler_ci.mipmapMode =
        static_cast<VkSamplerMipmapMode>(LiverpoolToVK::MipFilter(sampler.mip_filter));
    sampler_ci.addressModeU =
        static_cast<VkSamplerAddressMode>(LiverpoolToVK::ClampMode(sampler.clamp_x));
    sampler_ci.addressModeV =
        static_cast<VkSamplerAddressMode>(LiverpoolToVK::ClampMode(sampler.clamp_y));
    sampler_ci.addressModeW =
        static_cast<VkSamplerAddressMode>(LiverpoolToVK::ClampMode(sampler.clamp_z));
    sampler_ci.mipLodBias = std::min(sampler.LodBias(), instance.MaxSamplerLodBias());
    sampler_ci.anisotropyEnable = anisotropy_enable;
    sampler_ci.maxAnisotropy = max_anisotropy;
    // GCN compares per instruction; a plain read of a compare sampler is undefined in Vulkan.
    sampler_ci.compareEnable = is_depth;
    sampler_ci.compareOp =
        static_cast<VkCompareOp>(LiverpoolToVK::DepthCompare(sampler.depth_compare_func));
    sampler_ci.minLod = sampler.MinLod();
    sampler_ci.maxLod = sampler.MaxLod();
    sampler_ci.borderColor = border_color;
    sampler_ci.unnormalizedCoordinates = VK_FALSE; // Handled in shader due to Vulkan limitations.

    device = static_cast<VkDevice>(instance.GetDevice());
    const VkResult result = vkCreateSampler(device, &sampler_ci, nullptr, &handle);
    ASSERT_MSG(result == VK_SUCCESS, "Failed to create sampler: %s",
               vk::to_string(static_cast<vk::Result>(result)).c_str());
}

Sampler::~Sampler() {
    if (handle != VK_NULL_HANDLE) {
        vkDestroySampler(device, handle, nullptr);
    }
}

Sampler::Sampler(Sampler&& other) noexcept
    : Common::LRUNode<>(std::move(other)), hash{other.hash}, device{other.device},
      handle{other.handle} {
    other.handle = VK_NULL_HANDLE;
}

Sampler& Sampler::operator=(Sampler&& other) noexcept {
    if (this != &other) {
        if (handle != VK_NULL_HANDLE) {
            vkDestroySampler(device, handle, nullptr);
        }
        Common::LRUNode<>::operator=(std::move(other));
        hash = other.hash;
        device = other.device;
        handle = other.handle;
        other.handle = VK_NULL_HANDLE;
    }
    return *this;
}

} // namespace VideoCore
