// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/assert.h"
#include "video_core/renderer_vulkan/vk_shader_util.h"

namespace Vulkan {

vk::ShaderModule CompileSPV(const u32* code, size_t num_words, vk::Device device) {
    VkShaderModuleCreateInfo shader_info{};
    shader_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_info.codeSize = num_words * sizeof(u32);
    shader_info.pCode = code;

    VkShaderModule module = VK_NULL_HANDLE;
    const VkResult result =
        vkCreateShaderModule(static_cast<VkDevice>(device), &shader_info, nullptr, &module);
    ASSERT_MSG(result == VK_SUCCESS, "Failed to compile SPIR-V shader: %s", ResultName(result));
    return vk::ShaderModule{module};
}

vk::ShaderModule CompileSPV(std::span<const u32> code, vk::Device device) {
    return CompileSPV(code.data(), code.size(), device);
}

} // namespace Vulkan
