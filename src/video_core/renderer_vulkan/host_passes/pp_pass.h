//  SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
//  SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/types.h"
#include "video_core/renderer_vulkan/vk_common.h"

namespace Vulkan {
struct Frame;
}

namespace Vulkan::HostPasses {

class PostProcessingPass {
public:
    struct Settings {
        float gamma = 1.0f;
        u32 hdr = 0;
        u32 srgb_input = 0;
        // Above 0: the picture goes out as PQ Rec.2020, SDR white at this much of 10000 nits
        float pq_white = 0.0f;
    };

    PostProcessingPass() = default;
    ~PostProcessingPass();

    PostProcessingPass(const PostProcessingPass&) = delete;
    PostProcessingPass& operator=(const PostProcessingPass&) = delete;

    void Create(VkDevice device, VkFormat surface_format);

    /// Draws @input, @input_size, into @output's image with @settings
    void Render(VkCommandBuffer cmdbuf, VkImageView input, VkExtent2D input_size, Frame& output,
                Settings settings);

private:
    VkDevice device{VK_NULL_HANDLE};
    VkPipeline pipeline{VK_NULL_HANDLE};
    VkPipelineLayout pipeline_layout{VK_NULL_HANDLE};
    VkDescriptorSetLayout desc_set_layout{VK_NULL_HANDLE};
    VkSampler sampler{VK_NULL_HANDLE};
};

} // namespace Vulkan::HostPasses
