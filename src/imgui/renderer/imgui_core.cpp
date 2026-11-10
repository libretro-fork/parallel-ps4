// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cfloat>
#include <chrono>
#include <cstdint>
#include <imgui.h>

#include "common/path_util.h"
#include "core/debug_state.h"
#include "core/devtools/layer.h"
#include "core/emulator_settings.h"
#include "font_data.h"
#include "font_stack.h"
#include "frontend_window.h"
#include "imgui/imgui_layer.h"
#include "imgui_core.h"
#include "imgui_impl_vulkan.h"
#include "imgui_internal.h"
#include "texture_manager.h"
#include "video_core/renderer_vulkan/vk_presenter.h"

static void CheckVkResult(const vk::Result err) {
    LOG_ERROR(ImGui, "Vulkan error {}", vk::to_string(err));
}

static std::vector<ImGui::Layer*> layers;

// Update layers before rendering to allow layer changes to be applied during rendering.
// Using deque to keep the order of changes in case a Layer is removed then added again between
// frames.
static std::deque<std::pair<bool, ImGui::Layer*>> change_layers{};
static std::mutex change_layers_mutex{};

static ImGuiID dock_id;

// ImGui's platform in the core: no window of the system's and no input of its own (the overlay
// has no mouse or keyboard; the game's pad reads stay with the game), so all it is given is the
// size of the picture it is drawn on and the time between frames.
static const Frontend::Window* platform_window = nullptr;
static std::chrono::steady_clock::time_point last_frame_time{};
static std::chrono::steady_clock::time_point last_game_frame_time{};
static std::array<float, 60> game_frame_seconds{};
static float game_frame_seconds_sum = 0.0f;
static std::size_t game_frame_index = 0;
static std::atomic<std::uint32_t> force_gamepad_input_capture_count{0};

namespace ImGui {

namespace Core {

void AcquireGamepadInputCapture() {
    force_gamepad_input_capture_count.fetch_add(1, std::memory_order_relaxed);
}

void ReleaseGamepadInputCapture() {
    std::uint32_t expected = force_gamepad_input_capture_count.load(std::memory_order_relaxed);
    while (expected != 0) {
        if (force_gamepad_input_capture_count.compare_exchange_weak(
                expected, expected - 1, std::memory_order_relaxed, std::memory_order_relaxed)) {
            return;
        }
    }
    LOG_WARNING(ImGui, "ReleaseGamepadInputCapture called with no active capture");
}

bool IsGamepadInputCaptured() {
    return force_gamepad_input_capture_count.load(std::memory_order_relaxed) > 0;
}

void Initialize(const ::Vulkan::Instance& instance, const Frontend::Window& window,
                const u32 image_count, vk::Format surface_format,
                const vk::AllocationCallbacks* allocator) {

    const auto config_path = GetUserPath(Common::FS::PathType::UserDir) / "imgui.ini";
    const auto log_path = GetUserPath(Common::FS::PathType::LogDir) / "imgui_log.txt";

    CreateContext();
    ImGuiIO& io = GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.DisplaySize = ImVec2((float)window.GetWidth(), (float)window.GetHeight());
    PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f); // Makes the window edges rounded

    auto path = config_path.u8string();
    char* config_file_buf = new char[path.size() + 1]();
    std::memcpy(config_file_buf, path.c_str(), path.size());
    io.IniFilename = config_file_buf;

    path = log_path.u8string();
    char* log_file_buf = new char[path.size() + 1]();
    std::memcpy(log_file_buf, path.c_str(), path.size());
    io.LogFilename = log_file_buf;

    ImFontConfig font_cfg{};
    font_cfg.OversampleH = 2;
    font_cfg.OversampleV = 1;
    io.Fonts->Flags |= ImFontAtlasFlags_NoPowerOfTwoHeight;
    const int console_language = emu_settings_get_console_language();
    io.FontDefault = FontStack::AddPrimaryUiFont(io.Fonts, 32.0f, console_language, font_cfg, true);

    io.Fonts->AddFontFromMemoryCompressedTTF(imgui_font_proggyvector_regular_compressed_data,
                                             imgui_font_proggyvector_regular_compressed_size,
                                             32.0f);

    // Avoid exploding atlas size on Metal/MoltenVK when CJK fallback is enabled.
    FontStack::AddPrimaryUiFont(io.Fonts, 128.0f, console_language, font_cfg, false);

    // Big Picture
    FontStack::AddPrimaryUiFont(ImGui::GetIO().Fonts, 64.0f, emu_settings_get_console_language(),
                                font_cfg, true);

    // Let the atlas size grow to the largest image the device can create,
    // floored to the power of two the packer requires.
    const u32 max_dim = instance.GetPhysicalDevice().getProperties().limits.maxImageDimension2D;
    const int atlas_max = static_cast<int>(std::bit_floor(std::max<u32>(max_dim, 512u)));
    io.Fonts->TexMaxWidth = atlas_max;
    io.Fonts->TexMaxHeight = atlas_max;

    io.FontGlobalScale = 0.5f;

    StyleColorsDark();

    ::Core::Devtools::Layer::SetupSettings();
    platform_window = &window;
    io.BackendPlatformName = "shadps4_libretro";

    const Vulkan::InitInfo vk_info{
        .instance = instance.GetInstance(),
        .physical_device = instance.GetPhysicalDevice(),
        .device = instance.GetDevice(),
        .queue_family = instance.GetPresentQueueFamilyIndex(),
        .queue = instance.GetPresentQueue(),
        .image_count = image_count,
        .min_allocation_size = 1024 * 1024,
        .pipeline_rendering_create_info{
            .colorAttachmentCount = 1,
            .pColorAttachmentFormats = &surface_format,
        },
        .allocator = allocator,
        .check_vk_result_fn = &CheckVkResult,
    };
    Vulkan::Init(vk_info);

    TextureManager::StartWorker();

    char label[32];
    ImFormatString(label, IM_ARRAYSIZE(label), "WindowOverViewport_%08X", GetMainViewport()->ID);
    dock_id = ImHashStr(label);

    std::at_quick_exit([] { SaveIniSettingsToDisk(GetIO().IniFilename); });
}

void OnSurfaceFormatChange(vk::Format surface_format) {
    Vulkan::OnSurfaceFormatChange(surface_format);
}

void Shutdown(const vk::Device& device) {
    auto result = device.waitIdle();
    if (result != vk::Result::eSuccess) {
        LOG_WARNING(ImGui, "Failed to wait for Vulkan device idle on shutdown: {}",
                    vk::to_string(result));
    }

    TextureManager::StopWorker();

    const ImGuiIO& io = GetIO();
    const auto ini_filename = (void*)io.IniFilename;
    const auto log_filename = (void*)io.LogFilename;

    Vulkan::Shutdown();
    platform_window = nullptr;
    GetIO().BackendPlatformName = nullptr;
    DestroyContext();

    delete[] (char*)ini_filename;
    delete[] (char*)log_filename;
}

// Each frame: the picture's size, the time step, and, on a frame of
// the game's own, its frame time and rate for the overlay.
static void PlatformNewFrame(bool is_reusing_frame) {
    using namespace std::chrono;
    ImGuiIO& io = GetIO();
    io.DisplaySize = ImVec2(static_cast<float>(platform_window->GetWidth()),
                            static_cast<float>(platform_window->GetHeight()));

    const auto now = steady_clock::now();
    const auto seconds = [](steady_clock::duration d) {
        return std::max(duration<float>(d).count(), FLT_MIN);
    };
    io.DeltaTime = last_frame_time.time_since_epoch().count() != 0 ? seconds(now - last_frame_time)
                                                                   : 1.0f / 60.0f;
    last_frame_time = now;

    if (!is_reusing_frame) {
        const float delta = last_game_frame_time.time_since_epoch().count() != 0
                                ? seconds(now - last_game_frame_time)
                                : 1.0f / 60.0f;
        last_game_frame_time = now;
        DebugState.FrameDeltaTime = delta;

        float& slot = game_frame_seconds[game_frame_index];
        game_frame_seconds_sum += delta - slot;
        slot = delta;
        game_frame_index = (game_frame_index + 1) % game_frame_seconds.size();
        DebugState.Framerate =
            game_frame_seconds_sum > 0.0f
                ? 1.0f / (game_frame_seconds_sum / static_cast<float>(game_frame_seconds.size()))
                : FLT_MAX;
    }
}

ImGuiID NewFrame(bool is_reusing_frame) {
    {
        std::scoped_lock lock{change_layers_mutex};
        while (!change_layers.empty()) {
            const auto [to_be_added, layer] = change_layers.front();
            if (to_be_added) {
                layers.push_back(layer);
            } else {
                const auto [begin, end] = std::ranges::remove(layers, layer);
                layers.erase(begin, end);
            }
            change_layers.pop_front();
        }
    }

    PlatformNewFrame(is_reusing_frame);
    ImGui::NewFrame();
    SetKeyOwner(ImGuiKey_GamepadFaceUp, ImHashStr("shadps4/pad"));

    ImGuiWindowFlags flags =
        ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_AutoHideTabBar;
    if (!DebugState.IsShowingDebugMenuBar()) {
        flags |= ImGuiDockNodeFlags_NoTabBar;
    }
    ImGuiID dockId = DockSpaceOverViewport(0, GetMainViewport(), flags);

    for (auto* layer : layers) {
        layer->Draw();
    }

    return dockId;
}

void Render(const vk::CommandBuffer& cmdbuf, const vk::ImageView& image_view,
            const vk::Extent2D& extent) {
    ImGui::Render();
    ImDrawData* draw_data = GetDrawData();
    // Font atlas uploads are recorded ahead of the render pass that samples them.
    Vulkan::UpdateTextures(*draw_data, cmdbuf);
    if (draw_data->CmdListsCount == 0) {
        return;
    }

    if (emu_settings_get_vkhost_markers()) {
        cmdbuf.beginDebugUtilsLabelEXT(vk::DebugUtilsLabelEXT{
            .pLabelName = "ImGui Render",
        });
    }

    vk::RenderingAttachmentInfo color_attachments[1]{
        {
            .imageView = image_view,
            .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
        },
    };
    vk::RenderingInfo render_info{};
    render_info.renderArea = vk::Rect2D{
        .offset = {0, 0},
        .extent = extent,
    };
    render_info.layerCount = 1;
    render_info.colorAttachmentCount = 1;
    render_info.pColorAttachments = color_attachments;
    cmdbuf.beginRendering(render_info);
    Vulkan::RenderDrawData(*draw_data, cmdbuf);
    cmdbuf.endRendering();
    if (emu_settings_get_vkhost_markers()) {
        cmdbuf.endDebugUtilsLabelEXT();
    }
}

bool MustKeepDrawing() {
    return std::ranges::any_of(layers, [](Layer* layer) { return layer->ShouldKeepDrawing(); }) ||
           change_layers.size() > 1;
}

} // namespace Core

void Layer::AddLayer(Layer* layer) {
    std::scoped_lock lock{change_layers_mutex};
    change_layers.emplace_back(true, layer);
}

void Layer::RemoveLayer(Layer* layer) {
    std::scoped_lock lock{change_layers_mutex};
    change_layers.emplace_back(false, layer);
}

} // namespace ImGui
