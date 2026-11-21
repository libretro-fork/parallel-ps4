// SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/types.h"
#include "video_core/host_shaders/host_spirv.h"
#include "video_core/renderer_vulkan/vk_common.h"

namespace Vulkan {
class Instance;
class Scheduler;
struct RenderState;
} // namespace Vulkan

namespace VideoCore {

class Image;
class ImageView;
struct ImageInfo;

class BlitHelper {
public:
    explicit BlitHelper(const Vulkan::Instance& instance, Vulkan::Scheduler& scheduler);
    ~BlitHelper();

    BlitHelper(const BlitHelper&) = delete;
    BlitHelper& operator=(const BlitHelper&) = delete;

    void ReinterpretColorAsMsDepth(u32 width, u32 height, u32 num_samples,
                                   VkFormat src_pixel_format, VkFormat dst_pixel_format,
                                   VkImage source, VkImage dest);

    void CopyBetweenMsImages(u32 width, u32 height, u32 num_samples, VkFormat pixel_format,
                             bool src_msaa, VkImage source, VkImage dest);

    /// One plane of a depth buffer, from one of its samples, into a colour image
    void CopyDepthToColor(u32 width, u32 height, u32 dst_samples, VkFormat src_format,
                          VkFormat dst_format, bool src_msaa, bool stencil, bool uint_out,
                          VkColorComponentFlags write_mask, u32 sample, VkImage source,
                          u32 src_layer, VkImage dest, u32 dst_layer);

private:
    void CreateShaders();
    void CreatePipelineLayouts();

    struct MsPipelineKey {
        u32 num_samples;
        VkFormat attachment_format;
        bool src_msaa;

        bool operator==(const MsPipelineKey&) const noexcept = default;
    };
    struct DepthToColorKey {
        u32 dst_samples;
        VkFormat attachment_format;
        u32 sample;
        u32 write_mask;
        bool src_msaa;
        bool stencil;
        bool uint_out;

        bool operator==(const DepthToColorKey&) const noexcept = default;
    };

    /// The pipelines made for one kind of blit, by key: few, so found by a walk
    template <typename Key>
    struct PipelineList {
        struct Entry {
            Key key;
            VkPipeline pipeline;
        };
        Entry* entries{};
        u32 count{};
        u32 room{};

        VkPipeline Find(const Key& key) const {
            for (u32 i = 0; i < count; ++i) {
                if (entries[i].key == key) {
                    return entries[i].pipeline;
                }
            }
            return VK_NULL_HANDLE;
        }
        void Add(const Key& key, VkPipeline pipeline);
        void Destroy(VkDevice device);
    };

    VkPipeline CreateDepthToColorPipeline(const DepthToColorKey& key);
    VkPipeline CreateColorToMSDepthPipeline(const MsPipelineKey& key);
    VkPipeline CreateMsCopyPipeline(const MsPipelineKey& key);

    /// A view of one mip and layer of @image, to be destroyed once the GPU is done with it
    VkImageView TransientView(VkImage image, VkFormat format, VkImageAspectFlags aspect,
                              VkImageUsageFlags usage, u32 layer);

    /// Draws the full-screen triangle with @pipeline, sampling @src_view, into @state
    void Draw(const Vulkan::RenderState& state, VkPipeline pipeline, VkImageView src_view);

    /// A pipeline drawing the full-screen triangle with @frag into @key_samples samples, of
    /// @color_format (or none) and @depth_format (or none)
    VkPipeline CreateBlitPipeline(VkShaderModule frag, u32 num_samples, VkFormat color_format,
                                  VkColorComponentFlags write_mask, VkFormat depth_format);

private:
    const Vulkan::Instance& instance;
    Vulkan::Scheduler& scheduler;
    VkDevice device{VK_NULL_HANDLE};
    VkDescriptorSetLayout single_texture_descriptor_set_layout{VK_NULL_HANDLE};
    VkPipelineLayout single_texture_pl_layout{VK_NULL_HANDLE};
    VkShaderModule fs_tri_vertex{VK_NULL_HANDLE};
    VkShaderModule color_to_ms_depth_frag{VK_NULL_HANDLE};
    VkShaderModule src_msaa_copy_frag{VK_NULL_HANDLE};
    VkShaderModule src_non_msaa_copy_frag{VK_NULL_HANDLE};

    PipelineList<MsPipelineKey> color_to_ms_depth_pl;
    PipelineList<MsPipelineKey> ms_image_copy_pl;
    PipelineList<DepthToColorKey> depth_to_color_pl;
    /// A shader's words, written here instead of allocated
    u32 words[HOST_SPIRV_MAX_WORDS];
};

} // namespace VideoCore
