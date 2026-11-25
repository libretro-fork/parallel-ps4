// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/bounded_threadsafe_queue.h"
#include "core/emulator_settings.h"
#include "core/libraries/videoout/buffer.h"
#include "libretro/libretro_bridge.h"
#include "video_core/renderer_vulkan/host_passes/pp_pass.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_output_images.h"
#include "video_core/renderer_vulkan/vk_runtime.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/texture_cache/texture_cache.h"

struct vk_heap_span;
struct guest_import;

namespace AmdGpu {
struct Liverpool;
}

namespace Vulkan {

struct Frame {
    u32 width;
    u32 height;
    vk_heap_span* allocation;
    VkImage image;
    VkImageView image_view;
    VkFence present_done;
    VkSemaphore ready_semaphore;
    u64 ready_tick;
    bool is_hdr{false};
    u8 id{};
};

enum SchedulerType {
    Draw,
    Present,
    CpuFlip,
};

class Rasterizer;

class Presenter {
public:
    explicit Presenter(AmdGpu::Liverpool* liverpool);
    ~Presenter();

    HostPasses::PostProcessingPass::Settings& GetPPSettingsRef() {
        return pp_settings;
    }

    Rasterizer& GetRasterizer() const {
        return *rasterizer;
    }

    // HDR Output (libretro core option), with a frontend that takes HDR10: the game is told
    // the display shows HDR
    bool IsHDRSupported() const {
        return emu_settings_get_hdr_allowed();
    }

    VideoCore::Image& RegisterVideoOutSurface(const BufferAttributeGroup& attribute,
                                              VAddr cpu_address) {
        vo_buffers_addr.emplace_back(cpu_address);
        // Resolution: Default hands over the game's own size
        Libretro::SetGameSize(attribute.attrib.width, attribute.attrib.height);
        VideoCore::TextureCache::ImageDesc desc{attribute, cpu_address};
        const VideoCore::ImageId image_id = texture_cache.FindImage(desc);
        VideoCore::Image& image = texture_cache.GetImage(image_id);
        image.usage.vo_surface = 1u;
        return image;
    }

    bool IsVideoOutSurface(const AmdGpu::ColorBuffer& color_buffer) const;

    /// @game_hdr: the game set its output to HDR (Bt2020PQ): its buffer holds PQ already
    Frame* PrepareFrame(const BufferAttributeGroup& attribute, VAddr cpu_address, bool game_hdr);

    Frame* PrepareBlankFrame(bool present_thread);

    void Present(Frame* frame, bool is_reusing_frame = false, bool is_game_frame = true);
    Frame* PrepareLastFrame();

private:
    Frame* GetRenderFrame();

    void RecreateFrame(Frame* frame, u32 width, u32 height);

    void SetExpectedGameSize(s32 width, s32 height);

    /// A slot of the shared memory to read the frame back into directly, lent to the GPU
    /// (Libretro::AcquireFrameSlot); no buffer when none is free or the device cannot write
    /// there
    struct FrameSlotTarget {
        VkBuffer buffer{VK_NULL_HANDLE};
        u32 slot{};
    };
    FrameSlotTarget AcquireFrameSlot(u32 width, u32 height);

private:
    float expected_ratio{1920.0 / 1080.0f};
    u32 expected_frame_width{1920};
    u32 expected_frame_height{1080};

    Instance instance;
    HostPasses::PostProcessingPass::Settings pp_settings{};
    HostPasses::PostProcessingPass pp_pass;
    AmdGpu::Liverpool* liverpool;
    Scheduler draw_scheduler;
    Scheduler present_scheduler;
    Scheduler flip_scheduler;
    OutputImages output_images;
    Runtime runtime;
    Rasterizer* rasterizer;
    VideoCore::TextureCache& texture_cache;
    Frame present_frames[OutputImages::MaxCount]{};
    u32 num_present_frames{};
    // The frames free to draw into: the output images' count at most, far below the
    // capacity
    Common::MPMCQueue<Frame*, 64> free_queue;
    Frame* last_submit_frame{};
    Common::SmallVector<VAddr, 16> vo_buffers_addr;
    // Each slot of the shared memory lent to the GPU, and how much of it
    struct FrameSlotImport {
        ::guest_import* import{};
        u64 bytes{};
    };
    FrameSlotImport frame_slot_imports[Libretro::FRAME_SLOTS]{};
};

} // namespace Vulkan
