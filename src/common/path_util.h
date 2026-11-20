// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace Common::FS {

enum class PathType {
    UserDir,          // Where shadPS4 stores its data.
    LogDir,           // Where log files are stored.
    ShaderDir,        // Where shaders are stored.
    TempDataDir,      // Where game temp data is stored.
    GameDataDir,      // Where game data is stored.
    SysModuleDir,     // Where system modules are stored.
    DownloadDir,      // Where downloads/temp files are stored.
    CheatsDir,        // Where cheats are stored.
    PatchesDir,       // Where patches are stored.
    MetaDataDir,      // Where game metadata (e.g. trophies and menu backgrounds) is stored.
    CustomTrophy,     // Where custom files for trophies are stored.
    CustomConfigs,    // Where custom files for different games are stored.
    CacheDir,         // Where pipeline and shader cache is stored.
    FontsDir,         // Where dumped system fonts are stored.
    TrophyDir,        // Where general trophy metadata is stored.
    HomeDir,          // PS4 home directory
    CustomModulesDir, // Where custom modules are stored.
    LicensesDir,      // Where license files are stored.
};

// Sub-directories contained within a user data directory
constexpr auto LOG_DIR = "log";
constexpr auto SHADER_DIR = "shader";
constexpr auto GAMEDATA_DIR = "data";
constexpr auto TEMPDATA_DIR = "temp";
constexpr auto SYSMODULES_DIR = "sys_modules";
constexpr auto DOWNLOAD_DIR = "download";
constexpr auto CHEATS_DIR = "cheats";
constexpr auto PATCHES_DIR = "patches";
constexpr auto METADATA_DIR = "game_data";
constexpr auto CUSTOM_TROPHY = "custom_trophy";
constexpr auto CUSTOM_CONFIGS = "custom_configs";
constexpr auto CACHE_DIR = "cache";
constexpr auto FONTS_DIR = "fonts";
constexpr auto TROPHY_DIR = "trophy";
constexpr auto HOME_DIR = "home";
constexpr auto CUSTOM_MODULES_DIR = "custom_modules";
constexpr auto LICENSES_DIR = "licenses";

// Filenames
constexpr auto LOG_FILE = "shad_log.txt";

/**
 * Converts a filesystem path to a UTF-8 encoded std::string.
 *
 * @param path Filesystem path
 *
 * @returns UTF-8 encoded std::string.
 */
[[nodiscard]] std::string PathToUTF8String(const std::filesystem::path& path);

/**
 * Converts a UTF-8 encoded string to a filesystem path.
 */
[[nodiscard]] std::filesystem::path PathFromUTF8String(std::string_view utf8);

/**
 * Gets a path from a C function that writes a UTF-8 path into a buffer and returns its
 * length, like the directory getters of core/emulator_settings.h. Empty when it doesn't fit.
 */
[[nodiscard]] std::filesystem::path PathFromCString(size_t (*get)(char* s, size_t len));

/**
 * Gets the filesystem path associated with the PathType enum.
 *
 * @param user_path PathType enum
 *
 * @returns The filesystem path associated with the PathType enum.
 */
[[nodiscard]] const std::filesystem::path& GetUserPath(PathType user_path);

/// The directory of @user_path in UTF-8, good for the process's life
[[nodiscard]] const char* GetUserPathUTF8(PathType user_path);

// The libretro core's user directory, set before any path is asked for.
void SetLibretroUserDir(const std::filesystem::path& dir);

} // namespace Common::FS
