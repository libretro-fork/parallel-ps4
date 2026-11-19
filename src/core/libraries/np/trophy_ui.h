// SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

#include <filesystem>
#include <string_view>

namespace Libraries::Np::NpTrophy {

/// Makes the trophy sound ready for the core's mix: the custom one in the user's folder, or the
/// built-in one. Called once, before the game runs, so nothing is decoded when a trophy unlocks.
void PreloadTrophySound();

/// @after: shown once the one showing (or queued before it) has had its time, rather than in
/// its place
void AddTrophyToQueue(const std::filesystem::path& trophyIconPath, const std::string& trophyName,
                      const std::string_view& rarity, bool after = false);

}; // namespace Libraries::Np::NpTrophy
