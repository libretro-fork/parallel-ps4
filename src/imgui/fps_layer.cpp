// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cmath>

#include <imgui.h>

#include "core/debug_state.h"
#include "core/emulator_settings.h"
#include "imgui/fps_layer.h"
#include "imgui/imgui_layer.h"
#include "imgui_internal.h"

namespace ImGui::FpsCounter {

namespace {

class FpsLayer final : public ImGui::Layer {
public:
    void Draw() override {
        // The core option, changed while the game runs, then F10
        const bool option = emu_settings_get_show_fps_counter();
        if (option != last_option) {
            last_option = option;
            shown = option;
        }
        if (IsKeyPressed(ImGuiKey_F10, false)) {
            shown = !shown;
        }
        if (!shown) {
            return;
        }
        if (Begin("Video Info", nullptr,
                  ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDecoration |
                      ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDocking |
                      ImGuiWindowFlags_NoInputs)) {
            SetWindowPos({GetIO().DisplaySize.x - GetCurrentWindowRead()->SizeFull.x, 0.0f});
            const float rate = DebugState.Framerate;
            const ImVec4 colour = rate < 10.0f   ? ImVec4(1.0f, 0.0f, 0.0f, 1.0f)  // red
                                  : rate < 20.0f ? ImVec4(1.0f, 0.5f, 0.0f, 1.0f)  // orange
                                                 : ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // white
            PushStyleColor(ImGuiCol_Text, colour);
            Text("%d FPS (%.1f ms)", static_cast<int>(std::round(rate)), 1000.0f / rate);
            PopStyleColor();
        }
        End();
    }

    bool ShouldKeepDrawing() override {
        return false;
    }

    bool shown{};
    bool last_option{};
};

FpsLayer layer;

} // namespace

void Register() {
    layer.last_option = layer.shown = emu_settings_get_show_fps_counter();
    ImGui::Layer::AddLayer(&layer);
}

void Unregister() {
    ImGui::Layer::RemoveLayer(&layer);
}

} // namespace ImGui::FpsCounter
