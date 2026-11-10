// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <unordered_map>
#include <utility>

#include "common/hash.h"
#include "common/types.h"
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
    struct TilingKey {
        AmdGpu::TileMode tile_mode;
        u32 num_bits;
        u32 num_samples;
        bool is_tiler;

        bool operator==(const TilingKey&) const = default;

        struct Hash {
            size_t operator()(const TilingKey& key) const {
                // The four fields packed into two words, then mixed
                const u64 a = (static_cast<u64>(key.tile_mode) << 32) | key.num_bits;
                const u64 b = (static_cast<u64>(key.num_samples) << 1) | (key.is_tiler ? 1 : 0);
                return static_cast<size_t>(HashCombine(HashCombine(u64{0}, a), b));
            }
        };
    };

public:
    explicit TileManager(const Vulkan::Instance& instance, Vulkan::Scheduler& scheduler,
                         Vulkan::Runtime& runtime, StreamBuffer& stream_buffer);
    ~TileManager();

    void TileImage(Image& in_image, std::span<vk::BufferImageCopy> buffer_copies,
                   const VideoCore::Buffer* out_buffer, u64 out_offset);

    std::pair<const Buffer*, u64> DetileImage(const VideoCore::Buffer* in_buffer, u64 in_offset,
                                              const ImageInfo& info);

private:
    vk::Pipeline GetTilingPipeline(const ImageInfo& info, bool is_tiler);

private:
    const Vulkan::Instance& instance;
    Vulkan::Scheduler& scheduler;
    Vulkan::Runtime& runtime;
    StreamBuffer& stream_buffer;
    vk::UniqueDescriptorSetLayout desc_layout;
    vk::UniquePipelineLayout pl_layout;
    std::unordered_map<TilingKey, vk::UniquePipeline, TilingKey::Hash> tiling_pipelines;
    /// A new tiling pipeline's module, written here instead of allocated
    std::array<u32, HOST_SPIRV_MAX_WORDS> spirv_words;
};

} // namespace VideoCore
