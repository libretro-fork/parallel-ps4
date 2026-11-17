// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/combiner.h"
#include "common/io_file.h"
#include "common/path_util.h"
#include "common/singleton.h"
#include "core/debug_state.h"
#include "core/emulator_settings.h"
#include "core/libraries/system/systemservice.h"
#include "frontend_window.h"
#include "imgui/renderer/imgui_core.h"
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

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <csetjmp>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <memory>
#include <span>
#include <sstream>
#include <system_error>
#include <vector>
#include "libretro/libretro_bridge.h"

namespace Vulkan {

bool CanBlitToSwapchain(const vk::PhysicalDevice physical_device, vk::Format format) {
    const vk::FormatProperties props{physical_device.getFormatProperties(format)};
    return static_cast<bool>(props.optimalTilingFeatures & vk::FormatFeatureFlagBits::eBlitDst);
}

[[nodiscard]] vk::ImageSubresourceLayers MakeImageSubresourceLayers() {
    return vk::ImageSubresourceLayers{
        .aspectMask = vk::ImageAspectFlagBits::eColor,
        .mipLevel = 0,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };
}

[[nodiscard]] vk::ImageBlit MakeImageBlit(s32 frame_width, s32 frame_height, s32 dst_width,
                                          s32 dst_height, s32 offset_x, s32 offset_y) {
    return vk::ImageBlit{
        .srcSubresource = MakeImageSubresourceLayers(),
        .srcOffsets =
            std::array{
                vk::Offset3D{
                    .x = 0,
                    .y = 0,
                    .z = 0,
                },
                vk::Offset3D{
                    .x = frame_width,
                    .y = frame_height,
                    .z = 1,
                },
            },
        .dstSubresource = MakeImageSubresourceLayers(),
        .dstOffsets =
            std::array{
                vk::Offset3D{
                    .x = offset_x,
                    .y = offset_y,
                    .z = 0,
                },
                vk::Offset3D{
                    .x = offset_x + dst_width,
                    .y = offset_y + dst_height,
                    .z = 1,
                },
            },
    };
}

[[nodiscard]] vk::ImageBlit MakeImageBlitStretch(s32 frame_width, s32 frame_height,
                                                 s32 swapchain_width, s32 swapchain_height) {
    return MakeImageBlit(frame_width, frame_height, swapchain_width, swapchain_height, 0, 0);
}

// The overlays' frame: the time since the last, and on a frame of the game's own its frame rate
// over its last 60 frames, which the FPS counter shows
// The first controller to the overlays, as ImGui had it (imgui_core.cpp's PadToImGui): the
// buttons the dialogs read, the left stick past a quarter of its throw scrolling
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
    const auto stick = [&](Input::Axis axis, ovl_key low, ovl_key high) {
        const float v =
            (static_cast<float>(state.axes[std::to_underlying(axis)]) - 128.0f) / 127.0f;
        const float dead = 0.25f;
        ovl_key_analog_event(low, v < -dead, v < -dead ? std::min(-v, 1.0f) : 0.0f);
        ovl_key_analog_event(high, v > dead, v > dead ? std::min(v, 1.0f) : 0.0f);
    };
    stick(Input::Axis::LeftX, OVL_KEY_PAD_LSTICK_LEFT, OVL_KEY_PAD_LSTICK_RIGHT);
    stick(Input::Axis::LeftY, OVL_KEY_PAD_LSTICK_UP, OVL_KEY_PAD_LSTICK_DOWN);
}

static void NewOverlayFrame(u32 width, u32 height, bool is_reusing_frame) {
    using namespace std::chrono;
    static steady_clock::time_point last_frame_time{};
    static steady_clock::time_point last_game_frame_time{};
    static std::array<float, 60> game_frame_seconds{};
    static float game_frame_seconds_sum = 0.0f;
    static std::size_t game_frame_index = 0;
    const auto now = steady_clock::now();
    const auto seconds = [](steady_clock::duration d) {
        return std::max(duration<float>(d).count(), FLT_MIN);
    };
    const float delta = last_frame_time.time_since_epoch().count() != 0
                            ? seconds(now - last_frame_time)
                            : 1.0f / 60.0f;
    last_frame_time = now;
    if (!is_reusing_frame) {
        const float game_delta = last_game_frame_time.time_since_epoch().count() != 0
                                     ? seconds(now - last_game_frame_time)
                                     : 1.0f / 60.0f;
        last_game_frame_time = now;
        DebugState.FrameDeltaTime = game_delta;
        float& slot = game_frame_seconds[game_frame_index];
        game_frame_seconds_sum += game_delta - slot;
        slot = game_delta;
        game_frame_index = (game_frame_index + 1) % game_frame_seconds.size();
        DebugState.Framerate =
            game_frame_seconds_sum > 0.0f
                ? 1.0f / (game_frame_seconds_sum / static_cast<float>(game_frame_seconds.size()))
                : FLT_MAX;
    }
    ovl_vk_new_frame(width, height, delta, DebugState.Framerate);
}

static vk::Rect2D FitImage(s32 frame_width, s32 frame_height, s32 swapchain_width,
                           s32 swapchain_height) {
    float frame_aspect = static_cast<float>(frame_width) / frame_height;
    float swapchain_aspect = static_cast<float>(swapchain_width) / swapchain_height;

    u32 dst_width = swapchain_width;
    u32 dst_height = swapchain_height;

    if (frame_aspect > swapchain_aspect) {
        dst_height = static_cast<s32>(swapchain_width / frame_aspect);
    } else {
        dst_width = static_cast<s32>(swapchain_height * frame_aspect);
    }

    const s32 offset_x = (swapchain_width - dst_width) / 2;
    const s32 offset_y = (swapchain_height - dst_height) / 2;

    return vk::Rect2D{{offset_x, offset_y}, {dst_width, dst_height}};
}

[[nodiscard]] vk::ImageBlit MakeImageBlitFit(s32 frame_width, s32 frame_height, s32 swapchain_width,
                                             s32 swapchain_height) {
    const auto& dst_rect = FitImage(frame_width, frame_height, swapchain_width, swapchain_height);

    return MakeImageBlit(frame_width, frame_height, dst_rect.extent.width, dst_rect.extent.height,
                         dst_rect.offset.x, dst_rect.offset.y);
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
    const Instance* instance;
    std::optional<VideoCore::Buffer> buffer;
    u32 width{};
    u32 height{};
    vk::Image narrow{};
    MemoryAllocation narrow_allocation{};
    // The pixels read back are in libretro's order as they are
    bool in_order{true};

    FrameReadback(const Instance& instance_, const u32 width_, const u32 height_,
                  const vk::Format format)
        : instance{&instance_}, width{width_}, height{height_} {
        vk::Format narrow_format = vk::Format::eB8G8R8A8Unorm;
        if (emu_settings_get_hdr_allowed()) {
            narrow_format = vk::Format::eA2R10G10B10UnormPack32;
            in_order = format != vk::Format::eA2B10G10R10UnormPack32;
            if (in_order || !instance_.IsFormatSupported(
                                narrow_format, vk::FormatFeatureFlagBits2::eBlitDst |
                                                   vk::FormatFeatureFlagBits2::eTransferSrc)) {
                return;
            }
        } else if (format == vk::Format::eB8G8R8A8Unorm) {
            return;
        }
        const vk::ImageCreateInfo image_info = {
            .imageType = vk::ImageType::e2D,
            .format = narrow_format,
            .extent = {width, height, 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = vk::SampleCountFlagBits::e1,
            .tiling = vk::ImageTiling::eOptimal,
            .usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc,
        };
        if (instance_.GetAllocator()->CreateImage(image_info, false, narrow, narrow_allocation) !=
            vk::Result::eSuccess) {
            LOG_CRITICAL(Render_Vulkan, "No image to read a frame back through");
            UNREACHABLE();
        }
        in_order = true;
    }

    /// The buffer to read back into when the shared memory cannot be written to directly
    VideoCore::Buffer& Buffer() {
        if (!buffer) {
            buffer.emplace(*instance, 0, static_cast<u64>(width) * static_cast<u64>(height) * 4,
                           VideoCore::MemoryType::HostCached);
        }
        return *buffer;
    }
};

// The libretro core reads every frame back, and a buffer made for each one cost more than the
// copy it was made for: at 3840x2160 the device ran out of memory. The buffers are kept and
// used again. The list is never destroyed: it would outlive the device it belongs to.
static Common::Combiner frame_readbacks_combiner; // the list changes one call at a time
static std::vector<std::shared_ptr<FrameReadback>>& FrameReadbacks() {
    static auto* const readbacks = new std::vector<std::shared_ptr<FrameReadback>>;
    return *readbacks;
}

static std::shared_ptr<FrameReadback> TakeFrameReadback(const Instance& instance, const u32 width,
                                                        const u32 height, const vk::Format format) {
    auto kept = frame_readbacks_combiner.Run([&]() -> std::shared_ptr<FrameReadback> {
        auto& readbacks = FrameReadbacks();
        for (auto it = readbacks.begin(); it != readbacks.end(); ++it) {
            if ((*it)->width == width && (*it)->height == height) {
                auto readback = std::move(*it);
                readbacks.erase(it);
                return readback;
            }
        }
        return nullptr;
    });
    if (kept) {
        return kept;
    }
    return std::make_shared<FrameReadback>(instance, width, height, format);
}

// Called where a frame's buffer used to be destroyed, once its frame has been handed over.
// Buffers of another size are from before the picture changed size, and go.
static void ReturnFrameReadback(std::shared_ptr<FrameReadback> readback) {
    static constexpr std::size_t MaxKept = 4;
    std::vector<std::shared_ptr<FrameReadback>> dropped;
    frame_readbacks_combiner.Run([&] {
        auto& readbacks = FrameReadbacks();
        std::erase_if(readbacks, [&](auto& kept) {
            if (kept->width == readback->width && kept->height == readback->height) {
                return false;
            }
            dropped.push_back(std::move(kept));
            return true;
        });
        if (readbacks.size() < MaxKept) {
            readbacks.push_back(std::move(readback));
        }
    });
}

static void CopyImageToReadback(const vk::CommandBuffer& cmdbuf, const vk::Image image,
                                const vk::ImageLayout layout, FrameReadback& readback,
                                const vk::Buffer target) {
    const vk::BufferImageCopy copy_region = {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource =
            {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        .imageOffset = {0, 0, 0},
        .imageExtent = {readback.width, readback.height, 1},
    };
    cmdbuf.copyImageToBuffer(image, layout, target, copy_region);
}

// The ten-bit frame narrowed to the readback's XRGB8888 image, rounded to the nearest step
// by the blit, and that copied to its buffer
static void NarrowToReadback(const vk::CommandBuffer& cmdbuf, const vk::Image image,
                             FrameReadback& readback, const vk::Buffer target) {
    const vk::ImageSubresourceRange range = {
        .aspectMask = vk::ImageAspectFlagBits::eColor,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };
    const vk::ImageMemoryBarrier to_dst = {
        .srcAccessMask = vk::AccessFlagBits::eTransferRead,
        .dstAccessMask = vk::AccessFlagBits::eTransferWrite,
        .oldLayout = vk::ImageLayout::eUndefined,
        .newLayout = vk::ImageLayout::eTransferDstOptimal,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = readback.narrow,
        .subresourceRange = range,
    };
    cmdbuf.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                           vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, to_dst);
    const vk::ImageSubresourceLayers layers = {
        .aspectMask = vk::ImageAspectFlagBits::eColor,
        .mipLevel = 0,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };
    const std::array<vk::Offset3D, 2> corners = {
        vk::Offset3D{0, 0, 0},
        vk::Offset3D{static_cast<s32>(readback.width), static_cast<s32>(readback.height), 1},
    };
    const vk::ImageBlit blit = {
        .srcSubresource = layers,
        .srcOffsets = corners,
        .dstSubresource = layers,
        .dstOffsets = corners,
    };
    cmdbuf.blitImage(image, vk::ImageLayout::eTransferSrcOptimal, readback.narrow,
                     vk::ImageLayout::eTransferDstOptimal, blit, vk::Filter::eNearest);
    const vk::ImageMemoryBarrier to_src = {
        .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
        .dstAccessMask = vk::AccessFlagBits::eTransferRead,
        .oldLayout = vk::ImageLayout::eTransferDstOptimal,
        .newLayout = vk::ImageLayout::eTransferSrcOptimal,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = readback.narrow,
        .subresourceRange = range,
    };
    cmdbuf.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                           vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, to_src);
    CopyImageToReadback(cmdbuf, readback.narrow, vk::ImageLayout::eTransferSrcOptimal, readback,
                        target);
}

// The rectangle of the image's pixels [x, x + width) by [y, y + height) covers, rounded to
// whole pixels and kept inside @extent; none when nothing of it is
static std::optional<vk::Rect2D> ClampRect(float x, float y, float width, float height,
                                           const vk::Extent2D extent) {
    const s32 left = std::max<s32>(static_cast<s32>(std::lround(x)), 0);
    const s32 top = std::max<s32>(static_cast<s32>(std::lround(y)), 0);
    const s32 right =
        std::min<s32>(static_cast<s32>(std::lround(x + width)), static_cast<s32>(extent.width));
    const s32 bottom =
        std::min<s32>(static_cast<s32>(std::lround(y + height)), static_cast<s32>(extent.height));
    if (right <= left || bottom <= top) {
        return std::nullopt;
    }
    return vk::Rect2D{{left, top},
                      {static_cast<u32>(right - left), static_cast<u32>(bottom - top)}};
}

// The game's picture, the frame's image, into @image at @rect, the rest of it black: copied
// where it is the frame's size, stretched by a blit where it is not. @image is in
// TransferDstOptimal and the frame's image in TransferSrcOptimal; after, @image is ready to be
// drawn over and the frame's image to be sampled again.
static void CopyGameToImage(const vk::CommandBuffer& cmdbuf, const Frame& frame,
                            const vk::Image image, const vk::Extent2D extent,
                            const std::optional<vk::Rect2D>& rect) {
    const vk::ImageSubresourceRange range = {
        .aspectMask = vk::ImageAspectFlagBits::eColor,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };
    const vk::ImageSubresourceLayers layers = {
        .aspectMask = vk::ImageAspectFlagBits::eColor,
        .mipLevel = 0,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };
    const bool covers = rect && rect->offset.x == 0 && rect->offset.y == 0 &&
                        rect->extent.width == extent.width && rect->extent.height == extent.height;
    if (!covers) {
        // Black around the picture, as the window's background was
        const vk::ClearColorValue black{std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}};
        cmdbuf.clearColorImage(image, vk::ImageLayout::eTransferDstOptimal, black, range);
        if (rect) {
            const vk::ImageMemoryBarrier cleared = {
                .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
                .dstAccessMask = vk::AccessFlagBits::eTransferWrite,
                .oldLayout = vk::ImageLayout::eTransferDstOptimal,
                .newLayout = vk::ImageLayout::eTransferDstOptimal,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = image,
                .subresourceRange = range,
            };
            cmdbuf.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                   vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, cleared);
        }
    }
    if (rect && frame.width == rect->extent.width && frame.height == rect->extent.height) {
        const vk::ImageCopy copy = {
            .srcSubresource = layers,
            .srcOffset = {0, 0, 0},
            .dstSubresource = layers,
            .dstOffset = {rect->offset.x, rect->offset.y, 0},
            .extent = {frame.width, frame.height, 1},
        };
        cmdbuf.copyImage(frame.image, vk::ImageLayout::eTransferSrcOptimal, image,
                         vk::ImageLayout::eTransferDstOptimal, copy);
    } else if (rect) {
        const vk::ImageBlit blit = {
            .srcSubresource = layers,
            .srcOffsets =
                std::array<vk::Offset3D, 2>{
                    vk::Offset3D{0, 0, 0},
                    vk::Offset3D{static_cast<s32>(frame.width), static_cast<s32>(frame.height), 1}},
            .dstSubresource = layers,
            .dstOffsets =
                std::array<vk::Offset3D, 2>{
                    vk::Offset3D{rect->offset.x, rect->offset.y, 0},
                    vk::Offset3D{rect->offset.x + static_cast<s32>(rect->extent.width),
                                 rect->offset.y + static_cast<s32>(rect->extent.height), 1}},
        };
        cmdbuf.blitImage(frame.image, vk::ImageLayout::eTransferSrcOptimal, image,
                         vk::ImageLayout::eTransferDstOptimal, blit, vk::Filter::eLinear);
    }
    const std::array barriers = {
        vk::ImageMemoryBarrier{
            .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
            .dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead |
                             vk::AccessFlagBits::eColorAttachmentWrite,
            .oldLayout = vk::ImageLayout::eTransferDstOptimal,
            .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image,
            .subresourceRange = range,
        },
        vk::ImageMemoryBarrier{
            .srcAccessMask = vk::AccessFlagBits::eTransferRead,
            .dstAccessMask = vk::AccessFlagBits::eShaderRead,
            .oldLayout = vk::ImageLayout::eTransferSrcOptimal,
            .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = frame.image,
            .subresourceRange = range,
        },
    };
    cmdbuf.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                           vk::PipelineStageFlagBits::eColorAttachmentOutput |
                               vk::PipelineStageFlagBits::eFragmentShader,
                           {}, {}, {}, barriers);
}

Presenter::Presenter(Frontend::Window& window_, AmdGpu::Liverpool* liverpool_)
    : window{window_}, liverpool{liverpool_},
      instance{emu_settings_get_gpu_id(), emu_settings_get_vkvalidation_enabled(),
               emu_settings_get_vkcrash_diagnostic_enabled()},
      draw_scheduler{instance}, present_scheduler{instance}, flip_scheduler{instance},
      swapchain{instance, window}, runtime{instance, draw_scheduler},
      rasterizer{std::make_unique<Rasterizer>(instance, draw_scheduler, runtime, liverpool)},
      texture_cache{rasterizer->GetTextureCache()} {
    const u32 num_images = swapchain.GetImageCount();
    const vk::Device device = instance.GetDevice();

    // Create presentation frames.
    present_frames.resize(num_images);
    for (u32 i = 0; i < num_images; i++) {
        Frame& frame = present_frames[i];
        frame.id = i;
        auto fence = Check<"create present done fence">(
            device.createFence({.flags = vk::FenceCreateFlagBits::eSignaled}));
        frame.present_done = fence;
        free_queue.TryEmplace(&frame);
    }

    pp_pass.Create(device, swapchain.GetSurfaceFormat().format);
}

Presenter::~Presenter() {
    draw_scheduler.Finish();
    present_scheduler.Finish();
    flip_scheduler.Finish();
    Check(draw_scheduler.CommandBuffer().reset());
    Check(present_scheduler.CommandBuffer().reset());
    Check(flip_scheduler.CommandBuffer().reset());

    for (auto& frame_slot_import : frame_slot_imports) {
        guest_import_destroy(frame_slot_import.import);
    }

    const vk::Device device = instance.GetDevice();
    for (auto& frame : present_frames) {
        instance.GetAllocator()->DestroyImage(frame.image, frame.allocation);
        device.destroyImageView(frame.image_view);
        device.destroyFence(frame.present_done);
    }
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
    auto& slot_import = frame_slot_imports[slot];
    const u64 bytes = static_cast<u64>(width) * height * 4;
    if (bytes > slot_import.bytes) {
        guest_import_destroy(slot_import.import);
        const u64 alignment = std::max<u64>(instance.GetMinImportedHostPointerAlignment(), 1);
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
    return {vk::Buffer{buffer}, slot};
}

bool Presenter::IsVideoOutSurface(const AmdGpu::ColorBuffer& color_buffer) const {
    return std::ranges::find(vo_buffers_addr, color_buffer.Address()) != vo_buffers_addr.cend();
}

void Presenter::RecreateFrame(Frame* frame, u32 width, u32 height) {
    const vk::Device device = instance.GetDevice();
    if (frame->image_view) {
        device.destroyImageView(frame->image_view);
    }
    if (frame->image) {
        instance.GetAllocator()->DestroyImage(frame->image, frame->allocation);
    }

    const vk::Format format = swapchain.GetSurfaceFormat().format;
    const vk::ImageCreateInfo image_info = {
        .flags = vk::ImageCreateFlagBits::eMutableFormat,
        .imageType = vk::ImageType::e2D,
        .format = format,
        .extent = {width, height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = vk::SampleCountFlagBits::e1,
        .usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst |
                 vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eSampled,
    };

    const vk::Result result =
        instance.GetAllocator()->CreateImage(image_info, true, frame->image, frame->allocation);
    if (result != vk::Result::eSuccess) [[unlikely]] {
        LOG_CRITICAL(Render_Vulkan, "Failed allocating texture with error {}",
                     vk::to_string(result));
        UNREACHABLE();
    }
    SetObjectName(device, frame->image, "Frame image #{}", frame->id);

    const vk::ImageViewCreateInfo view_info = {
        .image = frame->image,
        .viewType = vk::ImageViewType::e2D,
        .format = format,
        .subresourceRange{
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };
    auto view = Check<"create frame image view">(device.createImageView(view_info));
    frame->image_view = view;
    frame->width = width;
    frame->height = height;
}

Frame* Presenter::PrepareLastFrame() {
    if (last_submit_frame == nullptr) {
        return nullptr;
    }

    Frame* frame = last_submit_frame;

    while (true) {
        vk::Result result = instance.GetDevice().waitForFences(frame->present_done, false,
                                                               std::numeric_limits<u64>::max());
        if (result == vk::Result::eSuccess) {
            break;
        }
        if (result == vk::Result::eTimeout) {
            continue;
        }
        if (result == vk::Result::eErrorDeviceLost) {
            instance.ReportDeviceLost();
        }
        ASSERT_MSG(result != vk::Result::eErrorDeviceLost,
                   "Device lost during waiting for a frame");
    }

    auto& scheduler = flip_scheduler;
    scheduler.EndRendering();
    const auto cmdbuf = scheduler.CommandBuffer();

    const auto frame_subresources = vk::ImageSubresourceRange{
        .aspectMask = vk::ImageAspectFlagBits::eColor,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = VK_REMAINING_ARRAY_LAYERS,
    };

    const auto pre_barrier =
        vk::ImageMemoryBarrier2{.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                                .srcAccessMask = vk::AccessFlagBits2::eColorAttachmentRead,
                                .dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                                .dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
                                .oldLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                                .newLayout = vk::ImageLayout::eGeneral,
                                .image = frame->image,
                                .subresourceRange{frame_subresources}};

    cmdbuf.pipelineBarrier2(vk::DependencyInfo{
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &pre_barrier,
    });

    // Flush frame creation commands.
    frame->ready_semaphore = scheduler.GetWorkSemaphore()->Handle();
    frame->ready_tick = scheduler.CurrentTick();
    SubmitInfo info{};
    scheduler.Flush(info);
    return frame;
}

static vk::Format GetFrameViewFormat(const Libraries::VideoOut::PixelFormat format) {
    switch (format) {
    case Libraries::VideoOut::PixelFormat::A8B8G8R8Srgb:
        return vk::Format::eR8G8B8A8Srgb;
    case Libraries::VideoOut::PixelFormat::A8R8G8B8Srgb:
        return vk::Format::eB8G8R8A8Srgb;
    case Libraries::VideoOut::PixelFormat::A2R10G10B10:
    case Libraries::VideoOut::PixelFormat::A2R10G10B10Srgb:
    case Libraries::VideoOut::PixelFormat::A2R10G10B10Bt2020Pq:
        return vk::Format::eA2R10G10B10UnormPack32;
    default:
        break;
    }
    UNREACHABLE_MSG("Unknown format={}", static_cast<u32>(format));
    return {};
}

Frame* Presenter::PrepareFrame(const Libraries::VideoOut::BufferAttributeGroup& attribute,
                               VAddr cpu_address, bool game_hdr) {
    // A frame of the game's is done: a vertex capture counts them
    VertexCapture::OnFlip();
    // With Dump Shaders on, what is recorded for the GPU during a frame is written out
    // every so many frames, next to the shaders
    if (emu_settings_get_dump_shaders()) {
        // In a folder of their own, so that they can be sent without the shaders
        static const std::string dir = [] {
            const auto path = Common::FS::GetUserPath(Common::FS::PathType::ShaderDir) / "frames";
            if (!Common::FS::Exists(path)) {
                Common::FS::CreateDirectories(path);
            }
            return path.string();
        }();
        frame_notes_flip(dir.c_str());
    }

    auto desc = VideoCore::TextureCache::ImageDesc{attribute, cpu_address};
    const auto image_id = texture_cache.FindImage(desc);
    texture_cache.UpdateImage(image_id);

    Frame* frame = GetRenderFrame();

    const auto frame_subresources = vk::ImageSubresourceRange{
        .aspectMask = vk::ImageAspectFlagBits::eColor,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = VK_REMAINING_ARRAY_LAYERS,
    };

    const auto pre_barrier = vk::ImageMemoryBarrier2{
        .srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        .srcAccessMask = vk::AccessFlagBits2::eColorAttachmentRead,
        .dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        .dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
        .oldLayout = vk::ImageLayout::eUndefined,
        .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .image = frame->image,
        .subresourceRange{frame_subresources},
    };

    draw_scheduler.EndRendering();
    const auto cmdbuf = draw_scheduler.CommandBuffer();
    cmdbuf.pipelineBarrier2(vk::DependencyInfo{
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &pre_barrier,
    });

    VideoCore::ImageViewInfo view_info{};
    view_info.format = GetFrameViewFormat(attribute.attrib.pixel_format);
    // Exclude alpha from output frame to avoid blending with UI.
    view_info.mapping.a = AmdGpu::CompSwizzle::One;

    auto& image = texture_cache.GetImage(image_id);
    auto image_view = *image.FindView(view_info).image_view;
    // Its size on the GPU, which Render Resolution may make smaller than the guest's
    const vk::Extent2D image_size = {image.HostSize().width, image.HostSize().height};
    expected_ratio = static_cast<float>(image_size.width) / static_cast<float>(image_size.height);

    runtime.Transit(&image, vk::ImageLayout::eShaderReadOnlyOptimal,
                    vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderRead);
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
    frame->ready_semaphore = draw_scheduler.GetWorkSemaphore()->Handle();
    frame->ready_tick = draw_scheduler.CurrentTick();
    SubmitInfo info{};
    draw_scheduler.Flush(info);
    return frame;
}

Frame* Presenter::PrepareBlankFrame(bool present_thread) {
    // Request a free presentation frame.
    Frame* frame = GetRenderFrame();

    auto& scheduler = present_thread ? present_scheduler : draw_scheduler;
    scheduler.EndRendering();

    const auto cmdbuf = scheduler.CommandBuffer();

    constexpr vk::ImageSubresourceRange simple_subresource = {
        .aspectMask = vk::ImageAspectFlagBits::eColor,
        .levelCount = 1,
        .layerCount = 1,
    };
    const auto pre_barrier = vk::ImageMemoryBarrier2{
        .srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        .srcAccessMask = vk::AccessFlagBits2::eColorAttachmentRead,
        .dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        .dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
        .oldLayout = vk::ImageLayout::eUndefined,
        .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .image = frame->image,
        .subresourceRange = simple_subresource,
    };

    const auto post_barrier = vk::ImageMemoryBarrier2{
        .srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        .srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
        .dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader,
        .dstAccessMask = vk::AccessFlagBits2::eShaderRead,
        .oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .newLayout = vk::ImageLayout::eGeneral,
        .image = frame->image,
        .subresourceRange = simple_subresource,
    };

    const vk::RenderingAttachmentInfo attachment = {
        .imageView = frame->image_view,
        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eStore,
    };
    const vk::RenderingInfo rendering_info = {
        .renderArea =
            {
                .extent = {frame->width, frame->height},
            },
        .layerCount = 1,
        .colorAttachmentCount = 1u,
        .pColorAttachments = &attachment,
    };

    cmdbuf.pipelineBarrier2(vk::DependencyInfo{
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &pre_barrier,
    });

    cmdbuf.beginRendering(rendering_info);
    cmdbuf.endRendering();

    cmdbuf.pipelineBarrier2(vk::DependencyInfo{
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &post_barrier,
    });

    // Flush frame creation commands.
    frame->ready_semaphore = scheduler.GetWorkSemaphore()->Handle();
    frame->ready_tick = scheduler.CurrentTick();
    SubmitInfo info{};
    scheduler.Flush(info);
    return frame;
}

void Presenter::Present(Frame* frame, bool is_reusing_frame, bool is_game_frame) {
    // Free the frame for reuse
    const auto free_frame = [&] {
        if (!is_reusing_frame) {
            last_submit_frame = frame;
            free_queue.TryEmplace(frame);
        }
    };

    // Resolution: Default - the picture at the game's size, once it has said it
    if (u32 game_width, game_height; Libretro::TakeGameSize(game_width, game_height)) {
        window.SetSize(game_width, game_height);
        swapchain.Recreate(game_width, game_height);
    }
    // Recreate the swapchain if the window was resized.
    if (window.GetWidth() != swapchain.GetWidth() || window.GetHeight() != swapchain.GetHeight()) {
        swapchain.Recreate(window.GetWidth(), window.GetHeight());
    }

    swapchain.AcquireNextImage();

    // Reset fence for queue submission.
    const auto reset_result = instance.GetDevice().resetFences(frame->present_done);
    ASSERT_MSG(reset_result == vk::Result::eSuccess,
               "Unexpected error resetting present done fence: {}", vk::to_string(reset_result));

    {
        // The pad, once, for ImGui's dialogs and the overlays'
        auto& controllers = *Common::Singleton<Input::GameControllers>::Instance();
        const Input::State pad = controllers[0]->ReadState();
        ImGui::Core::NewFrame(pad);
        PadToOverlay(pad);
    }
    NewOverlayFrame(window.GetWidth(), window.GetHeight(), is_reusing_frame);

    const vk::Image swapchain_image = swapchain.Image();
    const vk::ImageView swapchain_image_view = swapchain.ImageView();

    auto& scheduler = present_scheduler;
    const auto cmdbuf = scheduler.CommandBuffer();
    // The frame for the frontend, when it does not draw from the image itself, and the slot of
    // the shared memory it is read back into where it goes there directly
    std::shared_ptr<FrameReadback> frame_readback;
    FrameSlotTarget frame_slot{};

    if (emu_settings_get_vkhost_markers()) {
        cmdbuf.beginDebugUtilsLabelEXT(vk::DebugUtilsLabelEXT{
            .pLabelName = "Present",
        });
    }

    {
        const vk::Extent2D extent = swapchain.GetExtent();
        // The game's picture is copied into the image, and the overlays drawn over it; or, while
        // the game shows its splash screen and it is decoded, the splash drawn in its place.
        // The frame image goes back to being sampled after (PrepareLastFrame).
        const bool splash_visible = Libraries::SystemService::IsSplashVisible();
        if (!splash_visible) {
            ovl_vk_splash_done();
        }
        const bool direct = !splash_visible || !ovl_vk_splash_ready();
        const std::array pre_barriers{
            vk::ImageMemoryBarrier{
                .srcAccessMask = vk::AccessFlagBits::eNone,
                .dstAccessMask = direct ? vk::AccessFlagBits::eTransferWrite
                                        : vk::AccessFlagBits::eColorAttachmentWrite,
                .oldLayout = vk::ImageLayout::eUndefined,
                .newLayout = direct ? vk::ImageLayout::eTransferDstOptimal
                                    : vk::ImageLayout::eColorAttachmentOptimal,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = swapchain_image,
                .subresourceRange{
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = VK_REMAINING_ARRAY_LAYERS,
                },
            },
            vk::ImageMemoryBarrier{
                .srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
                .dstAccessMask =
                    direct ? vk::AccessFlagBits::eTransferRead : vk::AccessFlagBits::eShaderRead,
                .oldLayout = vk::ImageLayout::eGeneral,
                .newLayout = direct ? vk::ImageLayout::eTransferSrcOptimal
                                    : vk::ImageLayout::eShaderReadOnlyOptimal,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = frame->image,
                .subresourceRange{
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = VK_REMAINING_ARRAY_LAYERS,
                },
            },
        };

        bool read_back = false;

        cmdbuf.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput,
                               vk::PipelineStageFlagBits::eColorAttachmentOutput |
                                   vk::PipelineStageFlagBits::eFragmentShader |
                                   vk::PipelineStageFlagBits::eTransfer,
                               {}, {}, {}, pre_barriers);
        const s32 area_width = static_cast<s32>(window.GetWidth());
        const s32 area_height = static_cast<s32>(window.GetHeight());
        SetExpectedGameSize(area_width, area_height);
        if (direct) {
            // Where the game's picture goes, in the image's pixels
            const auto fit = FitImage(frame->width, frame->height, area_width, area_height);
            CopyGameToImage(cmdbuf, *frame, swapchain_image, extent,
                            ClampRect(static_cast<float>(fit.offset.x),
                                      static_cast<float>(fit.offset.y),
                                      static_cast<float>(fit.extent.width),
                                      static_cast<float>(fit.extent.height), extent));
        } else {
            ovl_vk_render_splash(static_cast<VkCommandBuffer>(cmdbuf),
                                 static_cast<VkImageView>(swapchain_image_view), extent);
        }
        ImGui::Core::Render(cmdbuf, swapchain_image_view, swapchain.GetExtent());
        ovl_vk_render(static_cast<VkCommandBuffer>(cmdbuf),
                      static_cast<VkImageView>(swapchain_image_view), swapchain.GetExtent());

        // Every frame is read back for the frontend, after the overlays are drawn,
        // into a buffer that is used again - unless the frontend draws from the image itself.
        const bool read_back_frame = !Libretro::GpuFramesActive();
        if (read_back_frame) {
            frame_readback = TakeFrameReadback(instance, extent.width, extent.height,
                                               swapchain.GetSurfaceFormat().format);
            auto& readback = *frame_readback;

            const vk::ImageMemoryBarrier to_transfer{
                .srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
                .dstAccessMask = vk::AccessFlagBits::eTransferRead,
                .oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
                .newLayout = vk::ImageLayout::eTransferSrcOptimal,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = swapchain_image,
                .subresourceRange{
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = VK_REMAINING_ARRAY_LAYERS,
                },
            };

            cmdbuf.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                   vk::PipelineStageFlagBits::eTransfer,
                                   vk::DependencyFlagBits::eByRegion, {}, {}, to_transfer);
            // Straight into a slot of the shared memory where one is free and the device can
            // write there, else into the readback's buffer and copied there once it is done
            frame_slot = readback.in_order ? AcquireFrameSlot(extent.width, extent.height)
                                           : FrameSlotTarget{};
            const vk::Buffer target =
                frame_slot.buffer ? frame_slot.buffer : readback.Buffer().Handle();
            if (readback.narrow) {
                NarrowToReadback(cmdbuf, swapchain_image, readback, target);
            } else {
                CopyImageToReadback(cmdbuf, swapchain_image, vk::ImageLayout::eTransferSrcOptimal,
                                    readback, target);
            }
            read_back = true;
        }

        const vk::AccessFlags post_src_access_mask =
            read_back ? vk::AccessFlagBits::eTransferRead
                      : vk::AccessFlagBits::eColorAttachmentWrite;
        const vk::ImageLayout post_old_layout = read_back
                                                    ? vk::ImageLayout::eTransferSrcOptimal
                                                    : vk::ImageLayout::eColorAttachmentOptimal;
        const vk::ImageMemoryBarrier post_barrier{
            .srcAccessMask = post_src_access_mask,
            .dstAccessMask = vk::AccessFlagBits::eNone,
            .oldLayout = post_old_layout,
            // What the frontend is told an image it draws from is in
            // (libretro/gpu_frames.cpp, Import).
            .newLayout = Libretro::GpuFramesActive() ? vk::ImageLayout::eShaderReadOnlyOptimal
                                                     : vk::ImageLayout::ePresentSrcKHR,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = swapchain_image,
            .subresourceRange{
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = VK_REMAINING_ARRAY_LAYERS,
            },
        };
        cmdbuf.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands,
                               vk::PipelineStageFlagBits::eAllCommands,
                               vk::DependencyFlagBits::eByRegion, {}, {}, post_barrier);
    }
    if (emu_settings_get_vkhost_markers()) {
        cmdbuf.endDebugUtilsLabelEXT();
    }

    // Flush vulkan commands.
    if (Libretro::GpuFramesActive()) {
        // The frame is the image: published once the GPU has drawn it.
        const vk::Extent2D shown = swapchain.GetExtent();
        scheduler.DeferPriorityOperation(
            [image = swapchain.GetImageIndex(), generation = swapchain.GetImageGeneration(),
             width = shown.width,
             height = shown.height]() { Libretro::PushImage(image, generation, width, height); });
    }
    if (frame_readback && frame_slot.buffer) {
        // In the shared memory already: published once the GPU has written it
        scheduler.DeferPriorityOperation(
            [readback = std::move(frame_readback), slot = frame_slot.slot]() {
                Libretro::PublishFrameSlot(slot, readback->width, readback->height);
                ReturnFrameReadback(readback);
            });
    } else if (frame_readback) {
        scheduler.DeferPriorityOperation([readback = std::move(frame_readback)]() {
            const u64 size = static_cast<u64>(readback->width) * readback->height * 4;
            auto& buffer = readback->Buffer();
            buffer.Invalidate(0, size);
            if (buffer.mapped_data.size() >= size) {
                Libretro::PushFrame(buffer.mapped_data.data(), readback->width, readback->height,
                                    readback->in_order);
            }
            ReturnFrameReadback(readback);
        });
    }

    SubmitInfo info{};
    info.AddWait(frame->ready_semaphore, frame->ready_tick);
    info.AddSignal(frame->present_done);
    scheduler.Flush(info);

    swapchain.Present();

    free_frame();
}

Frame* Presenter::GetRenderFrame() {
    // Wait for free presentation frames
    Frame* frame = free_queue.PopWait();

    const vk::Device device = instance.GetDevice();
    vk::Result result{};

    const auto wait = [&]() {
        result = device.waitForFences(frame->present_done, false, std::numeric_limits<u64>::max());
        return result;
    };

    // Wait for the presentation to be finished so all frame resources are free
    while (wait() != vk::Result::eSuccess) {
        if (result == vk::Result::eErrorDeviceLost) {
            instance.ReportDeviceLost();
        }
        ASSERT_MSG(result != vk::Result::eErrorDeviceLost,
                   "Device lost during waiting for a frame");
        // Retry if the waiting times out
        if (result == vk::Result::eTimeout) {
            continue;
        }
    }

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
