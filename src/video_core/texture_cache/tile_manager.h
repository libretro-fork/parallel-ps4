// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/types.h"
#include "common/u64_map.h"
#include "video_core/amdgpu/tiling.h"
#include "video_core/buffer_cache/buffer.h"
#include "video_core/host_shaders/host_spirv.h"

namespace Vulkan {
struct Runtime;
}

namespace VideoCore {

struct ImageInfo;
struct Image;

class TileManager {
public:
    explicit TileManager(const Vulkan::Instance& instance, Vulkan::Scheduler& scheduler,
                         Vulkan::Runtime& runtime, StreamBuffer& stream_buffer);
    ~TileManager();

    TileManager(const TileManager&) = delete;
    TileManager& operator=(const TileManager&) = delete;

    /// Copies @in_image into @out_buffer at @out_offset, tiled as the guest has it. The
    /// offsets of @buffer_copies are moved by where the copy lands.
    void TileImage(Image& in_image, vk::BufferImageCopy* buffer_copies, u32 num_copies,
                   const VideoCore::Buffer* out_buffer, u64 out_offset);

    /// The buffer holding the image at @in_offset of @in_buffer untiled, with the offset at
    /// which it is in *@out_offset: @in_buffer itself for an image that is not tiled.
    const Buffer* DetileImage(const VideoCore::Buffer* in_buffer, u64 in_offset,
                              const ImageInfo& info, u64* out_offset);

private:
    VkPipeline GetTilingPipeline(const ImageInfo& info, bool is_tiler);

private:
    const Vulkan::Instance& instance;
    Vulkan::Scheduler& scheduler;
    Vulkan::Runtime& runtime;
    StreamBuffer& stream_buffer;
    VkDescriptorSetLayout desc_layout{VK_NULL_HANDLE};
    VkPipelineLayout pl_layout{VK_NULL_HANDLE};
    /// Tiling pipelines by their packed key (TilingKey in tile_manager.cpp): VkPipeline values
    u64_map_t tiling_pipelines{};
    /// A new tiling pipeline's module, written here instead of allocated
    u32 spirv_words[HOST_SPIRV_MAX_WORDS];
};

} // namespace VideoCore
