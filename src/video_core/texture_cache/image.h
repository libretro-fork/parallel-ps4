// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/combiner.h"
#include "common/enum.h"
#include "common/incremental_id.h"
#include "common/lru_cache.h"
#include "common/small_vector.h"
#include "common/target_frames.h"
#include "common/types.h"
#include "video_core/renderer_vulkan/vk_common.h"
#include "video_core/texture_cache/host_compatibility.h"
#include "video_core/texture_cache/image_info.h"
#include "video_core/texture_cache/image_view.h"

namespace Vulkan {
class Instance;
class Runtime;
} // namespace Vulkan

struct vk_heap_span;

namespace Vulkan {
class MemoryAllocator;
}

namespace VideoCore {

enum ImageFlagBits : u32 {
    Empty = 0,
    MaybeCpuDirty = 1 << 0, ///< The page this image is in was touched before the image address
    CpuDirty = 1 << 1,      ///< Contents have been modified from the CPU
    GpuDirty = 1 << 2, ///< Image contents have been modified from the GPU (valid data in buffer
                       ///< cache)
    Dirty = MaybeCpuDirty | CpuDirty | GpuDirty,
    GpuModified = 1 << 3, ///< Contents have been modified from the GPU
    Registered = 1 << 6,  ///< True when the image is registered
};
DECLARE_ENUM_FLAG_OPERATORS(ImageFlagBits)

struct UniqueImage {
    explicit UniqueImage() = default;
    explicit UniqueImage(vk::Device device, Vulkan::MemoryAllocator* allocator)
        : device{device}, allocator{allocator} {}
    ~UniqueImage();

    UniqueImage(const UniqueImage&) = delete;
    UniqueImage& operator=(const UniqueImage&) = delete;

    UniqueImage(UniqueImage&& other) noexcept
        : device{other.device}, allocator{other.allocator}, allocation{other.allocation},
          image{other.image}, image_ci{other.image_ci}, size_bytes{other.size_bytes} {
        other.Release();
    }
    UniqueImage& operator=(UniqueImage&& other) noexcept {
        if (this != &other) {
            // The image this one held goes first, not left behind with its memory
            Destroy();
            device = other.device;
            image = other.image;
            allocator = other.allocator;
            allocation = other.allocation;
            image_ci = other.image_ci;
            size_bytes = other.size_bytes;
            other.Release();
        }
        return *this;
    }

    /// Forgets the image without destroying it: it has been moved elsewhere
    void Release() {
        allocator = nullptr;
        allocation = nullptr;
        image = vk::Image{};
        size_bytes = 0;
    }

    void Create(const vk::ImageCreateInfo& image_ci);

    void Destroy();

    operator vk::Image() const {
        return image;
    }

    operator bool() const {
        return image;
    }

public:
    vk::Device device{};
    Vulkan::MemoryAllocator* allocator{};
    vk_heap_span* allocation{};
    vk::Image image{};
    vk::ImageCreateInfo image_ci{};
    vk::DeviceSize size_bytes{};
};

struct Image : public Common::LRUNode<> {
    explicit Image(const Vulkan::Instance& instance, Vulkan::Runtime& runtime,
                   Common::SlotVector<ImageView>& slot_image_views, const ImageInfo& info,
                   u32 scale_num = 1, u32 scale_den = 1);
    ~Image();

    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;

    Image(Image&&) = delete;
    Image& operator=(Image&&) = delete;

    bool Overlaps(VAddr addr, size_t size) const noexcept {
        return info.guest_address < (addr + size) && addr < (info.guest_address + info.guest_size);
    }

    vk::Image GetImage() const {
        return backing->image.image;
    }

    bool IsUntracked() {
        return track_addr == 0 || track_addr_end == 0;
    }

    // The libretro core's Resolution below the game's: a render target of the game's frame
    // size is made scale_num / scale_den of it on the GPU (HostSize), while info keeps what
    // the game sees. Such an image is never uploaded from or downloaded to guest memory.
    bool IsScaled() const {
        return scale_num != scale_den;
    }

    Extent3D HostSize() const {
        if (!IsScaled()) {
            return info.size;
        }
        u32 width = static_cast<u32>(u64(info.size.width) * scale_num / scale_den);
        u32 height = static_cast<u32>(u64(info.size.height) * scale_num / scale_den);
        width = width ? width : 1;
        height = height ? height : 1;
        return Extent3D{width, height, info.size.depth};
    }

    bool HasGpuValidData() const {
        return True(flags & ImageFlagBits::GpuModified) && False(flags & ImageFlagBits::Dirty);
    }

    bool CanBeDownloaded() {
        return HasGpuValidData() && info.num_samples == 1 && !info.props.has_stencil &&
               IsVulkanFormatCompatible(info.pixel_format, backing->image.image_ci.format);
    }

    void AssociateDepth(ImageId depth_image_id, u64 depth_image_uid) {
        depth_id = depth_image_id;
        depth_uid = depth_image_uid;
    }

    void DisassociateDepth() {
        depth_id = {};
        depth_uid = {};
    }

    vk::DeviceSize SizeBytes() const {
        vk::DeviceSize sum = 0;
        for (size_t i = 0; i < backing_images.size(); ++i) {
            sum += backing_images[i].image.size_bytes;
        }
        return sum;
    }

    ImageView& FindView(const ImageViewInfo& view_info, bool ensure_guest_samples = true);

    using Barriers = Common::SmallVector<vk::ImageMemoryBarrier2, 32>;
    void GetBarriers(Barriers& out_barriers, vk::ImageLayout dst_layout, vk::AccessFlags2 dst_mask,
                     vk::PipelineStageFlags2 dst_stage,
                     const SubresourceRange* subres_range = nullptr);

public:
    Vulkan::Runtime* runtime;
    Common::SlotVector<ImageView>* slot_image_views;
    // The image's tracking changes one at a time here (the fault handler's thread and the GPU
    // thread both change it)
    Common::Combiner combiner;
    ImageInfo info;
    u32 scale_num = 1;
    u32 scale_den = 1;
    vk::ImageAspectFlags aspect_mask = vk::ImageAspectFlagBits::eColor;
    vk::SampleCountFlags supported_samples = vk::SampleCountFlagBits::e1;
    ImageFlagBits flags = ImageFlagBits::Dirty;
    VAddr track_addr = 0;
    VAddr track_addr_end = 0;
    ImageId depth_id{};
    /// The sample positions a depth buffer was last drawn with, when they were the game's own:
    /// side of the pixel block (0: Vulkan's positions), samples, and x, y in sixteenths
    u8 sample_locs_grid{};
    u8 sample_locs_samples{};
    u8 sample_locs[64]{};
    u64 depth_uid{};

    vk::ImageUsageFlags usage_flags;
    vk::FormatFeatureFlags2 format_features;
    struct State {
        vk::PipelineStageFlags2 pl_stage = vk::PipelineStageFlagBits2::eAllCommands;
        vk::AccessFlags2 access_mask = vk::AccessFlagBits2::eNone;
        vk::ImageLayout layout = vk::ImageLayout::eUndefined;
    };
    struct BackingImage {
        BackingImage() = default;
        ~BackingImage() {
            delete[] subresource_states;
        }
        BackingImage(const BackingImage&) = delete;
        BackingImage& operator=(const BackingImage&) = delete;
        BackingImage(BackingImage&& other) noexcept
            : image{static_cast<UniqueImage&&>(other.image)}, state{other.state},
              subresource_states{other.subresource_states},
              subresource_capacity{other.subresource_capacity},
              num_subresource_states{other.num_subresource_states},
              image_view_infos{
                  static_cast<Common::SmallVector<ImageViewInfo, 2>&&>(other.image_view_infos)},
              image_view_ids{
                  static_cast<Common::SmallVector<ImageViewId, 2>&&>(other.image_view_ids)},
              num_samples{other.num_samples} {
            other.subresource_states = nullptr;
            other.subresource_capacity = 0;
            other.num_subresource_states = 0;
        }
        BackingImage& operator=(BackingImage&& other) noexcept {
            if (this != &other) {
                image = static_cast<UniqueImage&&>(other.image);
                state = other.state;
                delete[] subresource_states;
                subresource_states = other.subresource_states;
                subresource_capacity = other.subresource_capacity;
                num_subresource_states = other.num_subresource_states;
                other.subresource_states = nullptr;
                other.subresource_capacity = 0;
                other.num_subresource_states = 0;
                image_view_infos =
                    static_cast<Common::SmallVector<ImageViewInfo, 2>&&>(other.image_view_infos);
                image_view_ids =
                    static_cast<Common::SmallVector<ImageViewId, 2>&&>(other.image_view_ids);
                num_samples = other.num_samples;
            }
            return *this;
        }

        UniqueImage image;
        State state;
        /// Each mip's and layer's state, while they are not all in one: made on the first
        /// partial transition, kept for the image's life, in use while num_subresource_states
        /// is not 0
        State* subresource_states{};
        u32 subresource_capacity{};
        u32 num_subresource_states{};
        Common::SmallVector<ImageViewInfo, 2> image_view_infos;
        Common::SmallVector<ImageViewId, 2> image_view_ids;
        u32 num_samples;
    };
    Common::SmallVector<BackingImage, 2> backing_images;
    BackingImage* backing{};
    u64* memory_usage_counter{};
    u64 image_uid{};
    u64 lru_id{};
    u64 tick_accessed_last{};
    /// The game's frames it was last drawn into (asynchronous shader compilation)
    target_frames_t drawn_frames{};
    u64 cpu_hash{};
    u64 gpu_hash{};

    struct {
        u32 texture : 1;
        u32 storage : 1;
        u32 render_target : 1;
        u32 depth_target : 1;
        u32 vo_surface : 1;
    } usage{};

    struct {
        u32 is_bound : 1;
        u32 is_target : 1;
        u32 needs_rebind : 1;
        u32 force_general : 1;
    } binding{};

private:
    static Common::IncrementalIdProvider<u64> global_image_uid;
};

} // namespace VideoCore
