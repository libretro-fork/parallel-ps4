// SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/types.h"
#include "video_core/renderer_vulkan/vk_common.h"

namespace Vulkan {
class Instance;
class Scheduler;
} // namespace Vulkan

namespace VideoCore {

class Image;
class ImageView;
struct ImageInfo;

class BlitHelper {
    static constexpr size_t MaxMsPipelines = 6;

public:
    explicit BlitHelper(const Vulkan::Instance& instance, Vulkan::Scheduler& scheduler);
    ~BlitHelper();

    void ReinterpretColorAsMsDepth(u32 width, u32 height, u32 num_samples,
                                   vk::Format src_pixel_format, vk::Format dst_pixel_format,
                                   vk::Image source, vk::Image dest);

    void CopyBetweenMsImages(u32 width, u32 height, u32 num_samples, vk::Format pixel_format,
                             bool src_msaa, vk::Image source, vk::Image dest);

    /// One plane of a depth buffer, from one of its samples, into a colour image
    void CopyDepthToColor(u32 width, u32 height, u32 dst_samples, vk::Format src_format,
                          vk::Format dst_format, bool src_msaa, bool stencil, bool uint_out,
                          u32 sample, vk::Image source, u32 src_layer, vk::Image dest,
                          u32 dst_layer);

private:
    void CreateShaders();
    void CreatePipelineLayouts();

    struct MsPipelineKey {
        u32 num_samples;
        vk::Format attachment_format;
        bool src_msaa;

        auto operator<=>(const MsPipelineKey&) const noexcept = default;
    };
    struct DepthToColorKey {
        u32 dst_samples;
        vk::Format attachment_format;
        u32 sample;
        bool src_msaa;
        bool stencil;
        bool uint_out;

        auto operator<=>(const DepthToColorKey&) const noexcept = default;
    };
    void CreateDepthToColorPipeline(const DepthToColorKey& key);
    void CreateColorToMSDepthPipeline(const MsPipelineKey& key);
    void CreateMsCopyPipeline(const MsPipelineKey& key);

private:
    const Vulkan::Instance& instance;
    Vulkan::Scheduler& scheduler;
    vk::UniqueDescriptorSetLayout single_texture_descriptor_set_layout;
    vk::UniquePipelineLayout single_texture_pl_layout;
    vk::ShaderModule fs_tri_vertex;
    vk::ShaderModule color_to_ms_depth_frag;
    vk::ShaderModule src_msaa_copy_frag;
    vk::ShaderModule src_non_msaa_copy_frag;

    using MsPipeline = std::pair<MsPipelineKey, vk::UniquePipeline>;
    std::vector<MsPipeline> color_to_ms_depth_pl;
    std::vector<MsPipeline> ms_image_copy_pl;
    std::vector<std::pair<DepthToColorKey, vk::UniquePipeline>> depth_to_color_pl;
};

} // namespace VideoCore
