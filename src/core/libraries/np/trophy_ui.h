// SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

#include "imgui/imgui_layer.h"
#include "imgui/imgui_texture.h"

namespace Libraries::Np::NpTrophy {

class TrophyUI final : public ImGui::Layer {
public:
    TrophyUI(const std::filesystem::path& trophyIconPath, const std::string& trophyName,
             const std::string_view& rarity);
    ~TrophyUI() override;

    void Finish();

    void Draw() override;

private:
    std::string trophy_name;
    std::string_view trophy_type;
    ImGui::RefCountedTexture trophy_icon;
    ImGui::RefCountedTexture trophy_type_icon;
};

struct TrophyInfo {
    std::filesystem::path trophy_icon_path;
    std::string trophy_name;
    std::string_view trophy_type;
};

/// Makes the trophy sound ready for the core's mix: the custom one in the user's folder, or the
/// built-in one. Called once, before the game runs, so nothing is decoded when a trophy unlocks.
void PreloadTrophySound();

void AddTrophyToQueue(const std::filesystem::path& trophyIconPath, const std::string& trophyName,
                      const std::string_view& rarity);

}; // namespace Libraries::Np::NpTrophy
