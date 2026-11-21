// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/elf_info.h"
#include "common/embedded_resources.h"
#include "common/path_util.h"
#include "common/singleton.h"
#include "core/debug_state.h"
#include "core/emulator_settings.h"
#include "core/libraries/system/systemservice.h"
#include "input/controller.h"
#include "video_core/buffer_cache/buffer.h"
#include "video_core/buffer_cache/guest_import.h"
#include "video_core/overlay/ovl.h"
#include "video_core/overlay/vk_overlay.h"
#include "video_core/renderer_vulkan/frame_notes.h"
#include "video_core/renderer_vulkan/vk_memory.h"
#include "video_core/renderer_vulkan/vk_platform.h"
#include "video_core/renderer_vulkan/vk_presenter.h"
#include "video_core/renderer_vulkan/vk_rasterizer.h"
#include "video_core/renderer_vulkan/vk_vertex_capture.h"
#include "video_core/texture_cache/image.h"

#include <features/features_cpu.h>
#include <file/file_path.h>
#include <float.h>
#include <math.h>
#include <string.h>
#include "libretro/libretro_bridge.h"

namespace Vulkan {

/// One colour layer of an image's first level
static constexpr VkImageSubresourceLayers ColorLayers = {
    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
    .mipLevel = 0,
    .baseArrayLayer = 0,
    .layerCount = 1,
};

/// The first level of an image's colour, @layers of it
static constexpr VkImageSubresourceRange ColorRange(u32 layers) {
    return VkImageSubresourceRange{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = layers,
    };
}

/// A barrier moving @image from @old_layout to @new_layout, of the first API's access masks
static VkImageMemoryBarrier ImageBarrier(VkImage image, VkAccessFlags src_access,
                                         VkAccessFlags dst_access, VkImageLayout old_layout,
                                         VkImageLayout new_layout, u32 layers = 1) {
    return VkImageMemoryBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = src_access,
        .dstAccessMask = dst_access,
        .oldLayout = old_layout,
        .newLayout = new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = ColorRange(layers),
    };
}

/// A barrier of the synchronization2 API moving a frame's @image's colour attachment use
static VkImageMemoryBarrier2 FrameBarrier(VkImage image, VkPipelineStageFlags2 src_stage,
                                          VkAccessFlags2 src_access,
                                          VkPipelineStageFlags2 dst_stage,
                                          VkAccessFlags2 dst_access, VkImageLayout old_layout,
                                          VkImageLayout new_layout, u32 layers) {
    return VkImageMemoryBarrier2{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = src_stage,
        .srcAccessMask = src_access,
        .dstStageMask = dst_stage,
        .dstAccessMask = dst_access,
        .oldLayout = old_layout,
        .newLayout = new_layout,
        .image = image,
        .subresourceRange = ColorRange(layers),
    };
}

/// Records @barrier on its own
static void PipelineBarrier2(VkCommandBuffer cmdbuf, const VkImageMemoryBarrier2& barrier) {
    const VkDependencyInfo dependency = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier,
    };
    vkCmdPipelineBarrier2(cmdbuf, &dependency);
}

/// The two corners of a blit's region, [@x0, @x1) by [@y0, @y1)
static void BlitCorners(VkOffset3D* corners, s32 x0, s32 y0, s32 x1, s32 y1) {
    corners[0] = VkOffset3D{x0, y0, 0};
    corners[1] = VkOffset3D{x1, y1, 1};
}

// The first controller to the overlays, as it was last published: the buttons the dialogs
// read, the left stick past a quarter of its throw scrolling
/// One stick @axis of @state to the overlays: @low past a quarter of its throw one way, @high
/// the other
static void StickToOverlay(const Input::State& state, Input::Axis axis, ovl_key low, ovl_key high) {
    const float v = (static_cast<float>(state.axes[static_cast<u32>(axis)]) - 128.0f) / 127.0f;
    const float dead = 0.25f;
    ovl_key_analog_event(low, v < -dead, v < -dead ? Common::Min(-v, 1.0f) : 0.0f);
    ovl_key_analog_event(high, v > dead, v > dead ? Common::Min(v, 1.0f) : 0.0f);
}

static void PadToOverlay(const Input::State& state) {
    using Libraries::Pad::OrbisPadButtonDataOffset;
    struct Key {
        OrbisPadButtonDataOffset button;
        ovl_key key;
    };
    static constexpr Key keys[] = {
        {OrbisPadButtonDataOffset::Cross, OVL_KEY_PAD_FACE_DOWN},
        {OrbisPadButtonDataOffset::Circle, OVL_KEY_PAD_FACE_RIGHT},
        {OrbisPadButtonDataOffset::Square, OVL_KEY_PAD_FACE_LEFT},
        {OrbisPadButtonDataOffset::Triangle, OVL_KEY_PAD_FACE_UP},
        {OrbisPadButtonDataOffset::Up, OVL_KEY_PAD_DPAD_UP},
        {OrbisPadButtonDataOffset::Down, OVL_KEY_PAD_DPAD_DOWN},
        {OrbisPadButtonDataOffset::Left, OVL_KEY_PAD_DPAD_LEFT},
        {OrbisPadButtonDataOffset::Right, OVL_KEY_PAD_DPAD_RIGHT},
        {OrbisPadButtonDataOffset::L1, OVL_KEY_PAD_L1},
        {OrbisPadButtonDataOffset::R1, OVL_KEY_PAD_R1},
        {OrbisPadButtonDataOffset::L2, OVL_KEY_PAD_L2},
        {OrbisPadButtonDataOffset::R2, OVL_KEY_PAD_R2},
        {OrbisPadButtonDataOffset::Options, OVL_KEY_PAD_START},
        {OrbisPadButtonDataOffset::TouchPad, OVL_KEY_PAD_BACK},
    };
    ovl_set_gamepad_connected(state.connected);
    if (!state.connected) {
        return;
    }
    for (const Key& k : keys) {
        ovl_key_event(k.key, True(state.buttonsState & k.button));
    }
    StickToOverlay(state, Input::Axis::LeftX, OVL_KEY_PAD_LSTICK_LEFT, OVL_KEY_PAD_LSTICK_RIGHT);
    StickToOverlay(state, Input::Axis::LeftY, OVL_KEY_PAD_LSTICK_UP, OVL_KEY_PAD_LSTICK_DOWN);
}

// The overlays' frame: the time since the last, and on a frame of the game's own its frame rate
// over its last 60 frames, which the FPS counter shows
static void NewOverlayFrame(u32 width, u32 height, bool is_reusing_frame) {
    static constexpr u32 FrameRateFrames = 60;
    static retro_time_t last_frame_time;
    static retro_time_t last_game_frame_time;
    static float game_frame_seconds[FrameRateFrames];
    static float game_frame_seconds_sum = 0.0f;
    static u32 game_frame_index = 0;
    const retro_time_t now = cpu_features_get_time_usec();
    const float delta =
        last_frame_time != 0
            ? Common::Max(static_cast<float>(now - last_frame_time) / 1000000.0f, FLT_MIN)
            : 1.0f / 60.0f;
    last_frame_time = now;
    if (!is_reusing_frame) {
        const float game_delta =
            last_game_frame_time != 0
                ? Common::Max(static_cast<float>(now - last_game_frame_time) / 1000000.0f, FLT_MIN)
                : 1.0f / 60.0f;
        last_game_frame_time = now;
        DebugState.FrameDeltaTime = game_delta;
        float& slot = game_frame_seconds[game_frame_index];
        game_frame_seconds_sum += game_delta - slot;
        slot = game_delta;
        game_frame_index = (game_frame_index + 1) % FrameRateFrames;
        DebugState.Framerate =
            game_frame_seconds_sum > 0.0f
                ? 1.0f / (game_frame_seconds_sum / static_cast<float>(FrameRateFrames))
                : FLT_MAX;
    }
    ovl_vk_new_frame(width, height, delta, DebugState.Framerate);
}

/// Where a frame of @frame_width by @frame_height goes in an area of @area_width by
/// @area_height: as big as fits, its shape kept, in the middle
static VkRect2D FitImage(s32 frame_width, s32 frame_height, s32 area_width, s32 area_height) {
    const float frame_aspect = static_cast<float>(frame_width) / frame_height;
    const float area_aspect = static_cast<float>(area_width) / area_height;

    u32 dst_width = area_width;
    u32 dst_height = area_height;

    if (frame_aspect > area_aspect) {
        dst_height = static_cast<s32>(area_width / frame_aspect);
    } else {
        dst_width = static_cast<s32>(area_height * frame_aspect);
    }

    const s32 offset_x = (area_width - static_cast<s32>(dst_width)) / 2;
    const s32 offset_y = (area_height - static_cast<s32>(dst_height)) / 2;

    return VkRect2D{{offset_x, offset_y}, {dst_width, dst_height}};
}

// A finished frame read back for the frontend, as XRGB8888. A frame of ten bits a channel is
// narrowed to it first, on the GPU, into an image of the readback's own: the one place the
// picture loses its last two bits, and only for a frontend that cannot share the images.
// With HDR Output the frame is read back whole, ten bits a channel, through an image of
// libretro's XRGB2101010 order (A2R10G10B10) where the device can blit to one, the blit
// putting R and B in place; else as it is, and Libretro::PushFrame puts them in place.
// The frame goes straight into the shared memory where it can (Presenter::AcquireFrameSlot);
// the buffer is made only for a frame that cannot.
struct FrameReadback {
    const Instance* instance{};
    VideoCore::Buffer* buffer{};
    u32 width{};
    u32 height{};
    VkImage narrow{VK_NULL_HANDLE};
    MemoryAllocation narrow_allocation{};
    // The pixels read back are in libretro's order as they are
    bool in_order{true};
    // Taken for a frame, and given back once the frame has been handed over (on the thread the
    // scheduler runs its priority operations on)
    retro_atomic_int_t taken;

    /// Made for frames of @width_ by @height_ of @format
    void Make(const Instance& instance_, const u32 width_, const u32 height_,
              const VkFormat format) {
        instance = &instance_;
        width = width_;
        height = height_;
        in_order = true;
        VkFormat narrow_format = VK_FORMAT_B8G8R8A8_UNORM;
        if (emu_settings_get_hdr_allowed()) {
            narrow_format = VK_FORMAT_A2R10G10B10_UNORM_PACK32;
            in_order = format != VK_FORMAT_A2B10G10R10_UNORM_PACK32;
            if (in_order ||
                !instance_.IsFormatSupported(static_cast<VkFormat>(narrow_format),
                                             VK_FORMAT_FEATURE_2_BLIT_DST_BIT |
                                                 VK_FORMAT_FEATURE_2_TRANSFER_SRC_BIT)) {
                return;
            }
        } else if (format == VK_FORMAT_B8G8R8A8_UNORM) {
            return;
        }
        const VkImageCreateInfo image_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = narrow_format,
            .extent = {width, height, 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        };
        if (instance_.GetAllocator()->CreateImage(image_info, false, narrow, narrow_allocation) !=
            VK_SUCCESS) {
            LOG_CRITICAL(Render_Vulkan, "No image to read a frame back through");
            UNREACHABLE();
        }
        in_order = true;
    }

    /// What it was made with, gone; free of the GPU, as no frame has it
    void Unmake() {
        delete buffer;
        buffer = nullptr;
        if (narrow != VK_NULL_HANDLE) {
            instance->GetAllocator()->DestroyImage(narrow, narrow_allocation);
            narrow = VK_NULL_HANDLE;
        }
        width = 0;
        height = 0;
    }

    /// The buffer to read back into when the shared memory cannot be written to directly
    VideoCore::Buffer& Buffer() {
        if (!buffer) {
            buffer = new VideoCore::Buffer(*instance, 0,
                                           static_cast<u64>(width) * static_cast<u64>(height) * 4,
                                           VideoCore::MemoryType::HostCached);
        }
        return *buffer;
    }
};

// The libretro core reads every frame back, and a buffer made for each one cost more than the
// copy it was made for: at 3840x2160 the device ran out of memory. The readbacks are kept and
// used again, one for each frame that can be in flight and a few over. They are never
// destroyed: they would outlive the device they belong to.
static constexpr u32 NumFrameReadbacks = OutputImages::MaxCount + 2;
static FrameReadback frame_readbacks[NumFrameReadbacks];

/// A readback for a frame of @width by @height of @format: one made for that size where one is
/// free, else a free one made for it. Free ones of another size are from before the picture
/// changed size, and what they hold goes.
static FrameReadback* TakeFrameReadback(const Instance& instance, const u32 width, const u32 height,
                                        const VkFormat format) {
    FrameReadback* found = nullptr;
    FrameReadback* spare = nullptr;
    for (FrameReadback& readback : frame_readbacks) {
        if (!retro_atomic_cas_int(&readback.taken, 0, 1)) {
            continue;
        }
        const bool same_size = readback.width == width && readback.height == height;
        if (!found && same_size) {
            found = &readback;
            continue;
        }
        if (!same_size && readback.width != 0) {
            readback.Unmake();
        }
        if (!found && !spare) {
            spare = &readback;
            continue;
        }
        retro_atomic_store_release_int(&readback.taken, 0);
    }
    if (found) {
        if (spare) {
            retro_atomic_store_release_int(&spare->taken, 0);
        }
        return found;
    }
    ASSERT_MSG(spare, "More frames in flight than readbacks for them");
    if (spare->width != 0) {
        spare->Unmake();
    }
    spare->Make(instance, width, height, format);
    return spare;
}

/// Gives @readback back once its frame has been handed over
static void ReturnFrameReadback(FrameReadback* readback) {
    retro_atomic_store_release_int(&readback->taken, 0);
}

static void CopyImageToReadback(VkCommandBuffer cmdbuf, VkImage image, VkImageLayout layout,
                                const FrameReadback& readback, VkBuffer target) {
    const VkBufferImageCopy copy_region = {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = ColorLayers,
        .imageOffset = {0, 0, 0},
        .imageExtent = {readback.width, readback.height, 1},
    };
    vkCmdCopyImageToBuffer(cmdbuf, image, layout, target, 1, &copy_region);
}

// The ten-bit frame narrowed to the readback's XRGB8888 image, rounded to the nearest step
// by the blit, and that copied to its buffer
static void NarrowToReadback(VkCommandBuffer cmdbuf, VkImage image, const FrameReadback& readback,
                             VkBuffer target) {
    const VkImageMemoryBarrier to_dst =
        ImageBarrier(readback.narrow, VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                     VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                         0, nullptr, 0, nullptr, 1, &to_dst);
    VkImageBlit blit = {
        .srcSubresource = ColorLayers,
        .dstSubresource = ColorLayers,
    };
    BlitCorners(blit.srcOffsets, 0, 0, static_cast<s32>(readback.width),
                static_cast<s32>(readback.height));
    BlitCorners(blit.dstOffsets, 0, 0, static_cast<s32>(readback.width),
                static_cast<s32>(readback.height));
    vkCmdBlitImage(cmdbuf, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback.narrow,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_NEAREST);
    const VkImageMemoryBarrier to_src =
        ImageBarrier(readback.narrow, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                         0, nullptr, 0, nullptr, 1, &to_src);
    CopyImageToReadback(cmdbuf, readback.narrow, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback,
                        target);
}

// The rectangle of the image's pixels [x, x + width) by [y, y + height) covers, rounded to
// whole pixels and kept inside @extent, into *@out; false when nothing of it is
static bool ClampRect(float x, float y, float width, float height, const VkExtent2D extent,
                      VkRect2D* out) {
    const s32 left = Common::Max<s32>(static_cast<s32>(lroundf(x)), 0);
    const s32 top = Common::Max<s32>(static_cast<s32>(lroundf(y)), 0);
    const s32 right =
        Common::Min<s32>(static_cast<s32>(lroundf(x + width)), static_cast<s32>(extent.width));
    const s32 bottom =
        Common::Min<s32>(static_cast<s32>(lroundf(y + height)), static_cast<s32>(extent.height));
    if (right <= left || bottom <= top) {
        return false;
    }
    *out = VkRect2D{{left, top}, {static_cast<u32>(right - left), static_cast<u32>(bottom - top)}};
    return true;
}

// The game's picture, the frame's image, into @image at @rect (none: nowhere), the rest of it
// black: copied where it is the frame's size, stretched by a blit where it is not. @image is in
// TransferDstOptimal and the frame's image in TransferSrcOptimal; after, @image is ready to be
// drawn over and the frame's image to be sampled again.
static void CopyGameToImage(VkCommandBuffer cmdbuf, const Frame& frame, VkImage image,
                            const VkExtent2D extent, const VkRect2D* rect) {
    const VkImageSubresourceRange range = ColorRange(1);
    const bool covers = rect && rect->offset.x == 0 && rect->offset.y == 0 &&
                        rect->extent.width == extent.width && rect->extent.height == extent.height;
    if (!covers) {
        // Black around the picture, as the window's background was
        VkClearColorValue black;
        black.float32[0] = 0.0f;
        black.float32[1] = 0.0f;
        black.float32[2] = 0.0f;
        black.float32[3] = 1.0f;
        vkCmdClearColorImage(cmdbuf, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &black, 1,
                             &range);
        if (rect) {
            const VkImageMemoryBarrier cleared = ImageBarrier(
                image, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                                 &cleared);
        }
    }
    if (rect && frame.width == rect->extent.width && frame.height == rect->extent.height) {
        const VkImageCopy copy = {
            .srcSubresource = ColorLayers,
            .srcOffset = {0, 0, 0},
            .dstSubresource = ColorLayers,
            .dstOffset = {rect->offset.x, rect->offset.y, 0},
            .extent = {frame.width, frame.height, 1},
        };
        vkCmdCopyImage(cmdbuf, frame.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    } else if (rect) {
        VkImageBlit blit = {
            .srcSubresource = ColorLayers,
            .dstSubresource = ColorLayers,
        };
        BlitCorners(blit.srcOffsets, 0, 0, static_cast<s32>(frame.width),
                    static_cast<s32>(frame.height));
        BlitCorners(blit.dstOffsets, rect->offset.x, rect->offset.y,
                    rect->offset.x + static_cast<s32>(rect->extent.width),
                    rect->offset.y + static_cast<s32>(rect->extent.height));
        vkCmdBlitImage(cmdbuf, frame.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);
    }
    const VkImageMemoryBarrier barriers[] = {
        ImageBarrier(image, VK_ACCESS_TRANSFER_WRITE_BIT,
                     VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
        ImageBarrier(frame.image, VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT,
                     VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
    };
    vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 2, barriers);
}

static void InitOverlays(const Instance& instance, VkFormat format, u32 frame_count) {
    size_t font_size = 0;
    const u8* const font =
        Common::GetEmbeddedResource("src/resources/fonts/NotoSans-Regular.ttf", &font_size);
    ovl_vk_info_t info{};
    info.get_device_proc = vkGetDeviceProcAddr;
    info.device = instance.GetDevice();
    vkGetPhysicalDeviceMemoryProperties(instance.GetPhysicalDevice(), &info.memory_properties);
    info.image_count = frame_count;
    info.surface_format = format;
    info.font = font;
    info.font_size = font_size;
    // The fonts after it, tried in turn for what it lacks: Arabic, Thai, symbols, then the
    // CJK collection's face for the console's language
    static constexpr const char* fallbacks[] = {
        "src/resources/fonts/NotoSansArabic-Regular.ttf",
        "src/resources/fonts/NotoSansThai-Regular.ttf",
        "src/resources/fonts/NotoSansSymbols2-Regular.ttf",
        "src/resources/fonts/NotoSansCJK-Regular.ttc",
    };
    static constexpr u32 NumFallbacks = sizeof(fallbacks) / sizeof(fallbacks[0]);
    ovl_face_desc_t faces[NumFallbacks]{};
    for (u32 i = 0; i < NumFallbacks; i++) {
        size_t size = 0;
        faces[i].data = Common::GetEmbeddedResource(fallbacks[i], &size);
        faces[i].size = size;
    }
    // The collection's faces: Japanese, Korean, Simplified and Traditional Chinese; the
    // console's languages 9, 11 and 10 are the last three, any other reads the Japanese
    switch (emu_settings_get_console_language()) {
    case 9:
        faces[3].index = 1;
        break;
    case 11:
        faces[3].index = 2;
        break;
    case 10:
        faces[3].index = 3;
        break;
    default:
        break;
    }
    info.fallback_faces = faces;
    info.fallback_count = NumFallbacks;
    // The splash screen, read where the game's files lent it
    size_t splash_size = 0;
    info.splash = Common::ElfInfo::Instance().GetSplashData(&splash_size);
    info.splash_size = splash_size;
    // Without them the game still runs: no FPS counter, notifications or trophy popups
    if (!font) {
        LOG_ERROR(Render_Vulkan, "The UI font is not among the core's resources: the "
                                 "overlays are off (a clean build puts it back)");
    } else if (!ovl_vk_init(&info)) {
        LOG_ERROR(Render_Vulkan, "The overlays could not be made (%s): they are off",
                  ovl_vk_error());
    }
}

Presenter::Presenter(AmdGpu::Liverpool* liverpool_)
    : liverpool{liverpool_},
      instance{emu_settings_get_gpu_id(), emu_settings_get_vkvalidation_enabled(),
               emu_settings_get_vkcrash_diagnostic_enabled()},
      draw_scheduler{instance}, present_scheduler{instance}, flip_scheduler{instance},
      output_images{instance}, runtime{instance, draw_scheduler},
      rasterizer{new Rasterizer(instance, draw_scheduler, runtime, liverpool)},
      texture_cache{rasterizer->GetTextureCache()} {
    // The images at the Resolution option's size; with Resolution: Default they are made at
    // the game's, once it has said it (Present)
    if (!Libretro::GameSizePending()) {
        output_images.Create(emu_settings_get_window_width(), emu_settings_get_window_height(),
                             present_scheduler);
    }
    num_present_frames =
        output_images.Made() ? output_images.Count() : OutputImages::OffscreenCount;
    const VkDevice device = instance.GetDevice();

    // Create presentation frames.
    const VkFenceCreateInfo fence_ci = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };
    for (u32 i = 0; i < num_present_frames; i++) {
        Frame& frame = present_frames[i];
        frame.id = static_cast<u8>(i);
        const VkResult result = vkCreateFence(device, &fence_ci, nullptr, &frame.present_done);
        ASSERT_MSG(result == VK_SUCCESS, "Failed to create present done fence: %s",
                   ResultName(result));
        free_queue.TryEmplace(&frame);
    }

    pp_pass.Create(device, output_images.Format());
    // One of the overlays' frames for each of the presenter's, as many as can be in flight
    InitOverlays(instance, output_images.Format(), num_present_frames);
}

Presenter::~Presenter() {
    draw_scheduler.Finish();
    present_scheduler.Finish();
    flip_scheduler.Finish();
    // Output images made before the last ones go now the GPU is done with them
    present_scheduler.PopPendingOperations();
    vkResetCommandBuffer(draw_scheduler.CommandBuffer(), 0);
    vkResetCommandBuffer(present_scheduler.CommandBuffer(), 0);
    vkResetCommandBuffer(flip_scheduler.CommandBuffer(), 0);

    for (FrameSlotImport& frame_slot_import : frame_slot_imports) {
        guest_import_destroy(frame_slot_import.import);
    }

    const VkDevice device = instance.GetDevice();
    for (u32 i = 0; i < num_present_frames; i++) {
        Frame& frame = present_frames[i];
        instance.GetAllocator()->DestroyImage(frame.image, frame.allocation);
        vkDestroyImageView(device, frame.image_view, nullptr);
        vkDestroyFence(device, frame.present_done, nullptr);
    }
    delete rasterizer;
}

Presenter::FrameSlotTarget Presenter::AcquireFrameSlot(u32 width, u32 height) {
    if (!instance.IsExternalMemoryHostSupported()) {
        return {};
    }
    u32 slot = 0;
    u8* const base = Libretro::AcquireFrameSlot(slot);
    if (!base) {
        return {};
    }
    // The slot's memory lent to the GPU, as much of it as the frame takes: made again only for
    // a bigger frame. Nothing the GPU does is still writing the slot: it is free.
    FrameSlotImport& slot_import = frame_slot_imports[slot];
    const u64 bytes = static_cast<u64>(width) * height * 4;
    if (bytes > slot_import.bytes) {
        guest_import_destroy(slot_import.import);
        const u64 alignment =
            Common::Max<u64>(static_cast<u64>(instance.GetMinImportedHostPointerAlignment()), 1);
        const u64 lent = (bytes + alignment - 1) / alignment * alignment;
        slot_import.import = nullptr;
        slot_import.bytes = bytes;
        // Not past the slot, into the next one or the end of the memory
        if (lent <= Libretro::FRAME_BYTES) {
            guest_import_info_t info;
            instance.FillHostImport(&info, base, lent, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
            slot_import.import = guest_import_create(&info);
        }
    }
    VkDeviceSize offset = 0;
    const VkBuffer buffer = guest_import_buffer(slot_import.import, 0, bytes, &offset);
    if (buffer == VK_NULL_HANDLE) {
        Libretro::ReleaseFrameSlot(slot);
        return {};
    }
    return {buffer, slot};
}

bool Presenter::IsVideoOutSurface(const AmdGpu::ColorBuffer& color_buffer) const {
    const VAddr address = color_buffer.Address();
    for (const VAddr vo_address : vo_buffers_addr) {
        if (vo_address == address) {
            return true;
        }
    }
    return false;
}

void Presenter::RecreateFrame(Frame* frame, u32 width, u32 height) {
    const VkDevice device = instance.GetDevice();
    if (frame->image_view) {
        vkDestroyImageView(device, frame->image_view, nullptr);
    }
    if (frame->image) {
        instance.GetAllocator()->DestroyImage(frame->image, frame->allocation);
    }

    const VkFormat format = output_images.Format();
    const VkImageCreateInfo image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = {width, height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                 VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
    };

    VkResult result =
        instance.GetAllocator()->CreateImage(image_info, true, frame->image, frame->allocation);
    if (result != VK_SUCCESS) [[unlikely]] {
        LOG_CRITICAL(Render_Vulkan, "Failed allocating texture with error %s", ResultName(result));
        UNREACHABLE();
    }
    SetObjectName(instance.GetDevice(), frame->image, "Frame image #%d", frame->id);

    const VkImageViewCreateInfo view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = frame->image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .subresourceRange = ColorRange(1),
    };
    result = vkCreateImageView(device, &view_info, nullptr, &frame->image_view);
    ASSERT_MSG(result == VK_SUCCESS, "Failed to create frame image view: %s", ResultName(result));
    frame->width = width;
    frame->height = height;
}

/// Waits for @frame's last presentation to be done with it
static void WaitPresentDone(const Instance& instance, const Frame& frame) {
    const VkDevice device = instance.GetDevice();
    while (true) {
        const VkResult result = vkWaitForFences(device, 1, &frame.present_done, VK_FALSE, ~0ULL);
        if (result == VK_SUCCESS) {
            return;
        }
        // Waited out: again
        if (result == VK_TIMEOUT) {
            continue;
        }
        if (result == VK_ERROR_DEVICE_LOST) {
            instance.ReportDeviceLost();
        }
        ASSERT_MSG(result != VK_ERROR_DEVICE_LOST, "Device lost during waiting for a frame");
    }
}

Frame* Presenter::PrepareLastFrame() {
    if (last_submit_frame == nullptr) {
        return nullptr;
    }

    Frame* frame = last_submit_frame;
    WaitPresentDone(instance, *frame);

    Scheduler& scheduler = flip_scheduler;
    scheduler.EndRendering();
    const VkCommandBuffer cmdbuf = scheduler.CommandBuffer();

    PipelineBarrier2(cmdbuf,
                     FrameBarrier(frame->image, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                  VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
                                  VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                  VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                                  VK_REMAINING_ARRAY_LAYERS));

    // Flush frame creation commands.
    frame->ready_semaphore = static_cast<VkSemaphore>(scheduler.GetWorkSemaphore()->Handle());
    frame->ready_tick = scheduler.CurrentTick();
    SubmitInfo info{};
    scheduler.Flush(info);
    return frame;
}

static VkFormat GetFrameViewFormat(const Libraries::VideoOut::PixelFormat format) {
    switch (format) {
    case Libraries::VideoOut::PixelFormat::A8B8G8R8Srgb:
        return VK_FORMAT_R8G8B8A8_SRGB;
    case Libraries::VideoOut::PixelFormat::A8R8G8B8Srgb:
        return VK_FORMAT_B8G8R8A8_SRGB;
    case Libraries::VideoOut::PixelFormat::A2R10G10B10:
    case Libraries::VideoOut::PixelFormat::A2R10G10B10Srgb:
    case Libraries::VideoOut::PixelFormat::A2R10G10B10Bt2020Pq:
        return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
    default:
        break;
    }
    UNREACHABLE_MSG("Unknown format=%u", static_cast<u32>(format));
    return {};
}

/// The folder a frame's notes are written to with Dump Shaders on: next to the shaders, in one
/// of their own so that they can be sent without them
static const char* FrameNotesDir() {
    static char dir[PATH_MAX_LENGTH];
    if (!dir[0]) {
        fill_pathname_join_special(dir,
                                   Common::FS::GetUserPathUTF8(Common::FS::PathType::ShaderDir),
                                   "frames", sizeof(dir));
        path_mkdir(dir);
    }
    return dir;
}

Frame* Presenter::PrepareFrame(const Libraries::VideoOut::BufferAttributeGroup& attribute,
                               VAddr cpu_address, bool game_hdr) {
    // A frame of the game's is done: a vertex capture counts them
    VertexCapture::OnFlip();
    rasterizer->OnFlip();
    // With Dump Shaders on, what is recorded for the GPU during a frame is written out
    // every so many frames, next to the shaders
    if (emu_settings_get_dump_shaders()) {
        frame_notes_flip(FrameNotesDir());
    }

    VideoCore::TextureCache::ImageDesc desc{attribute, cpu_address};
    const VideoCore::ImageId image_id = texture_cache.FindImage(desc);
    texture_cache.UpdateImage(image_id);

    Frame* frame = GetRenderFrame();

    draw_scheduler.EndRendering();
    const VkCommandBuffer cmdbuf = draw_scheduler.CommandBuffer();
    PipelineBarrier2(
        cmdbuf, FrameBarrier(frame->image, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                             VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
                             VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                             VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_REMAINING_ARRAY_LAYERS));

    VideoCore::ImageViewInfo view_info{};
    view_info.format = GetFrameViewFormat(attribute.attrib.pixel_format);
    // Exclude alpha from output frame to avoid blending with UI.
    view_info.mapping.a = AmdGpu::CompSwizzle::One;

    VideoCore::Image& image = texture_cache.GetImage(image_id);
    const VkImageView image_view = image.FindView(view_info).Handle();
    // Its size on the GPU, which Render Resolution may make smaller than the guest's
    const VkExtent2D image_size = {image.HostSize().width, image.HostSize().height};
    expected_ratio = static_cast<float>(image_size.width) / static_cast<float>(image_size.height);

    runtime.Transit(&image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);
    runtime.FlushBarriers();

    // Vulkan has no sRGB variant of the 10-bit format, so an A2R10G10B10Srgb buffer reaches
    // the post process pass still sRGB encoded and has to be decoded there instead.
    pp_settings.srgb_input =
        attribute.attrib.pixel_format == Libraries::VideoOut::PixelFormat::A2R10G10B10Srgb;
    // HDR Output: a game's own HDR (PQ in a ten-bit buffer) goes through as it is; any other
    // picture is turned into PQ, its white at the frontend's paper white
    const bool hdr_output = IsHDRSupported();
    pp_settings.hdr = hdr_output && game_hdr;
    pp_settings.pq_white =
        hdr_output ? static_cast<float>(emu_settings_get_hdr_paper_white()) / 10000.0f : 0.0f;
    pp_pass.Render(cmdbuf, image_view, image_size, *frame, pp_settings);

    DebugState.game_resolution = {image_size.width, image_size.height};
    DebugState.output_resolution = {frame->width, frame->height};

    // Flush frame creation commands.
    frame->ready_semaphore = static_cast<VkSemaphore>(draw_scheduler.GetWorkSemaphore()->Handle());
    frame->ready_tick = draw_scheduler.CurrentTick();
    SubmitInfo info{};
    draw_scheduler.Flush(info);
    return frame;
}

Frame* Presenter::PrepareBlankFrame(bool present_thread) {
    // Request a free presentation frame.
    Frame* frame = GetRenderFrame();

    Scheduler& scheduler = present_thread ? present_scheduler : draw_scheduler;
    scheduler.EndRendering();

    const VkCommandBuffer cmdbuf = scheduler.CommandBuffer();

    const VkRenderingAttachmentInfo attachment = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = frame->image_view,
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    };
    const VkRenderingInfo rendering_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea =
            {
                .extent = {frame->width, frame->height},
            },
        .layerCount = 1,
        .colorAttachmentCount = 1u,
        .pColorAttachments = &attachment,
    };

    PipelineBarrier2(cmdbuf,
                     FrameBarrier(frame->image, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                  VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
                                  VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                  VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1));

    vkCmdBeginRendering(cmdbuf, &rendering_info);
    vkCmdEndRendering(cmdbuf);

    PipelineBarrier2(
        cmdbuf, FrameBarrier(frame->image, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                             VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                             VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 1));

    // Flush frame creation commands.
    frame->ready_semaphore = static_cast<VkSemaphore>(scheduler.GetWorkSemaphore()->Handle());
    frame->ready_tick = scheduler.CurrentTick();
    SubmitInfo info{};
    scheduler.Flush(info);
    return frame;
}

void Presenter::Present(Frame* frame, bool is_reusing_frame, bool is_game_frame) {
    (void)is_game_frame;
    // Resolution: Default - the picture at the game's size, once it has said it
    if (u32 game_width, game_height; Libretro::TakeGameSize(game_width, game_height)) {
        output_images.Create(game_width, game_height, present_scheduler);
    }
    // A game that never says its size is shown at the Resolution option's
    if (!output_images.Made()) {
        output_images.Create(emu_settings_get_window_width(), emu_settings_get_window_height(),
                             present_scheduler);
    }

    output_images.Acquire();

    // Reset fence for queue submission.
    const VkResult reset_result = vkResetFences(instance.GetDevice(), 1, &frame->present_done);
    ASSERT_MSG(reset_result == VK_SUCCESS, "Unexpected error resetting present done fence: %s",
               ResultName(reset_result));

    {
        // The pad, for the overlays' dialogs
        Input::GameControllers& controllers =
            *Common::Singleton<Input::GameControllers>::Instance();
        const Input::State pad = controllers[0]->ReadState();
        PadToOverlay(pad);
    }
    const VkExtent2D extent = output_images.Extent();
    NewOverlayFrame(extent.width, extent.height, is_reusing_frame);

    const VkImage output_image = output_images.Image();
    const VkImageView output_image_view = output_images.ImageView();

    Scheduler& scheduler = present_scheduler;
    const VkCommandBuffer cmdbuf = scheduler.CommandBuffer();
    // The frame for the frontend, when it does not draw from the image itself, and the slot of
    // the shared memory it is read back into where it goes there directly
    FrameReadback* frame_readback = nullptr;
    FrameSlotTarget frame_slot{};

    if (emu_settings_get_vkhost_markers()) {
        const VkDebugUtilsLabelEXT label = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
            .pLabelName = "Present",
        };
        vkCmdBeginDebugUtilsLabelEXT(cmdbuf, &label);
    }

    {
        // The game's picture is copied into the image, and the overlays drawn over it; or, while
        // the game shows its splash screen and it is decoded, the splash drawn in its place.
        // The frame image goes back to being sampled after (PrepareLastFrame).
        const bool splash_visible = Libraries::SystemService::IsSplashVisible();
        if (!splash_visible) {
            ovl_vk_splash_done();
        }
        const bool direct = !splash_visible || !ovl_vk_splash_ready();
        const VkImageMemoryBarrier pre_barriers[] = {
            ImageBarrier(output_image, VK_ACCESS_NONE,
                         direct ? VK_ACCESS_TRANSFER_WRITE_BIT
                                : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                         VK_IMAGE_LAYOUT_UNDEFINED,
                         direct ? VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
                                : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                         VK_REMAINING_ARRAY_LAYERS),
            ImageBarrier(frame->image, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                         direct ? VK_ACCESS_TRANSFER_READ_BIT : VK_ACCESS_SHADER_READ_BIT,
                         VK_IMAGE_LAYOUT_GENERAL,
                         direct ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
                                : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         VK_REMAINING_ARRAY_LAYERS),
        };

        bool read_back = false;

        vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 0, nullptr, 0, nullptr, 2, pre_barriers);
        const s32 area_width = static_cast<s32>(extent.width);
        const s32 area_height = static_cast<s32>(extent.height);
        SetExpectedGameSize(area_width, area_height);
        if (direct) {
            // Where the game's picture goes, in the image's pixels
            const VkRect2D fit = FitImage(frame->width, frame->height, area_width, area_height);
            VkRect2D rect;
            const bool any =
                ClampRect(static_cast<float>(fit.offset.x), static_cast<float>(fit.offset.y),
                          static_cast<float>(fit.extent.width),
                          static_cast<float>(fit.extent.height), extent, &rect);
            CopyGameToImage(cmdbuf, *frame, output_image, extent, any ? &rect : nullptr);
        } else {
            ovl_vk_render_splash(cmdbuf, output_image_view, extent);
        }
        ovl_vk_render(cmdbuf, output_image_view, extent);

        // Every frame is read back for the frontend, after the overlays are drawn,
        // into a buffer that is used again - unless the frontend draws from the image itself.
        const bool read_back_frame = !Libretro::GpuFramesActive();
        if (read_back_frame) {
            frame_readback =
                TakeFrameReadback(instance, extent.width, extent.height, output_images.Format());
            FrameReadback& readback = *frame_readback;

            const VkImageMemoryBarrier to_transfer =
                ImageBarrier(output_image, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                             VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                             VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_REMAINING_ARRAY_LAYERS);
            vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0,
                                 nullptr, 0, nullptr, 1, &to_transfer);
            // Straight into a slot of the shared memory where one is free and the device can
            // write there, else into the readback's buffer and copied there once it is done
            frame_slot = readback.in_order ? AcquireFrameSlot(extent.width, extent.height)
                                           : FrameSlotTarget{};
            const VkBuffer target =
                frame_slot.buffer ? frame_slot.buffer : readback.Buffer().Handle();
            if (readback.narrow) {
                NarrowToReadback(cmdbuf, output_image, readback, target);
            } else {
                CopyImageToReadback(cmdbuf, output_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                    readback, target);
            }
            read_back = true;
        }

        // What the frontend is told an image it draws from is in (libretro/gpu_frames.cpp,
        // Import)
        const VkImageMemoryBarrier post_barrier = ImageBarrier(
            output_image,
            read_back ? VK_ACCESS_TRANSFER_READ_BIT : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_NONE,
            read_back ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
                      : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            Libretro::GpuFramesActive() ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                        : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_REMAINING_ARRAY_LAYERS);
        vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                             VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0,
                             nullptr, 0, nullptr, 1, &post_barrier);
    }
    if (emu_settings_get_vkhost_markers()) {
        vkCmdEndDebugUtilsLabelEXT(cmdbuf);
    }

    // Flush vulkan commands.
    if (Libretro::GpuFramesActive()) {
        // The frame is the image: published once the GPU has drawn it.
        scheduler.DeferPriorityOperation(
            [image = output_images.Index(), generation = output_images.Generation(),
             width = extent.width,
             height = extent.height]() { Libretro::PushImage(image, generation, width, height); });
    }
    if (frame_readback && frame_slot.buffer) {
        // In the shared memory already: published once the GPU has written it
        scheduler.DeferPriorityOperation([readback = frame_readback, slot = frame_slot.slot]() {
            Libretro::PublishFrameSlot(slot, readback->width, readback->height);
            ReturnFrameReadback(readback);
        });
    } else if (frame_readback) {
        scheduler.DeferPriorityOperation([readback = frame_readback]() {
            const u64 size = static_cast<u64>(readback->width) * readback->height * 4;
            VideoCore::Buffer& buffer = readback->Buffer();
            buffer.Invalidate(0, size);
            if (buffer.mapped_data && buffer.size_bytes >= size) {
                Libretro::PushFrame(buffer.mapped_data, readback->width, readback->height,
                                    readback->in_order);
            }
            ReturnFrameReadback(readback);
        });
    }

    SubmitInfo info{};
    info.AddWait(frame->ready_semaphore, frame->ready_tick);
    info.AddSignal(frame->present_done);
    scheduler.Flush(info);

    // Free the frame for reuse
    if (!is_reusing_frame) {
        last_submit_frame = frame;
        free_queue.TryEmplace(frame);
    }
}

Frame* Presenter::GetRenderFrame() {
    // Wait for free presentation frames
    Frame* frame = free_queue.PopWait();

    // Wait for the presentation to be finished so all frame resources are free
    WaitPresentDone(instance, *frame);

    if (frame->width != expected_frame_width || frame->height != expected_frame_height) {
        RecreateFrame(frame, expected_frame_width, expected_frame_height);
    }

    return frame;
}

void Presenter::SetExpectedGameSize(s32 width, s32 height) {
    const float ratio = (float)width / (float)height;

    expected_frame_height = height;
    expected_frame_width = width;
    if (ratio > expected_ratio) {
        expected_frame_width = static_cast<s32>(height * expected_ratio);
    } else {
        expected_frame_height = static_cast<s32>(width / expected_ratio);
    }
}

} // namespace Vulkan
