// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <string.h>

#include <compat/strl.h>
#include <file/file_path.h>
#include <retro_miscellaneous.h>
#include <streams/file_stream.h>

#include "common/io_file.h"
#include "common/logging/log.h"
#include "common/path_util.h"
#include "common/scope_exit.h"
#include "common/types.h"
#include "core/file_sys/ifile.h"

namespace Common::FS {

namespace fs = std::filesystem;

namespace {

constexpr size_t PathTypeCount = static_cast<size_t>(PathType::LicensesDir) + 1;

// The libretro core keeps everything under the frontend's system directory (system/shadPS4),
// set before the first path is asked for (libretro/host.cpp)
char libretro_user_dir[PATH_MAX_LENGTH];

struct UserPathTable {
    char utf8[PathTypeCount][PATH_MAX_LENGTH];
    fs::path paths[PathTypeCount];
};

constexpr const char* SubDirs[PathTypeCount] = {
    "",           LOG_DIR,    SHADER_DIR,  TEMPDATA_DIR, GAMEDATA_DIR,       SYSMODULES_DIR,
    DOWNLOAD_DIR, CHEATS_DIR, PATCHES_DIR, METADATA_DIR, CUSTOM_TROPHY,      CUSTOM_CONFIGS,
    CACHE_DIR,    FONTS_DIR,  TROPHY_DIR,  HOME_DIR,     CUSTOM_MODULES_DIR, LICENSES_DIR,
};

void SetTableEntry(UserPathTable& table, size_t type, const char* utf8) {
    strlcpy(table.utf8[type], utf8, sizeof(table.utf8[type]));
    table.paths[type] = PathFromUTF8String(table.utf8[type]);
}

UserPathTable* MakeUserPaths() {
    UserPathTable* const table = new UserPathTable;
    const char* const user_dir = libretro_user_dir;
    for (size_t type = 0; type < PathTypeCount; ++type) {
        char dir[PATH_MAX_LENGTH];
        if (SubDirs[type][0]) {
            fill_pathname_join_special(dir, user_dir, SubDirs[type], sizeof(dir));
        } else {
            strlcpy(dir, user_dir, sizeof(dir));
        }
        path_mkdir(dir);
        SetTableEntry(*table, type, dir);
    }

    // Written once, and again only when the text has changed
    static constexpr char notice[] =
        "++++++++++++++++++++++++++++++++\n+ Custom Trophy Images / Sound "
        "+\n++++++++++++++++++++++++++++++++\n\nYou can add custom images to the "
        "trophies.\n*We recommend a square resolution image, for example 200x200, 500x500, "
        "the same size as the height and width.\nIn this folder ('user\\custom_trophy'), "
        "add the files with the following "
        "names:\n\nbronze.png\nsilver.png\ngold.png\nplatinum.png\n\nYou can add a custom "
        "sound for trophy notifications.\n*By default, no audio is played unless it is in "
        "this folder and you are using the QT version.\nIn this folder "
        "('user\\custom_trophy'), add the files with the following names:\n\ntrophy.mp3";
    char notice_path[PATH_MAX_LENGTH];
    fill_pathname_join_special(notice_path,
                               table->utf8[static_cast<size_t>(PathType::CustomTrophy)],
                               "Notice.txt", sizeof(notice_path));
    if (!filestream_matches_buf(notice_path, notice, sizeof(notice) - 1)) {
        filestream_write_file(notice_path, notice, static_cast<s64>(sizeof(notice) - 1));
    }
    return table;
}

// Made when first asked for, not when the library is loaded: the frontend's system directory
// is only known once the core is running. Kept for the process's life.
UserPathTable& UserPaths() {
    static UserPathTable* const table = MakeUserPaths();
    return *table;
}

} // Anonymous namespace

void SetLibretroUserDir(const std::filesystem::path& dir) {
    strlcpy(libretro_user_dir, PathToUTF8String(dir).c_str(), sizeof(libretro_user_dir));
}

// The longest path the host takes: UTF-16 code units on Windows, UTF-8 ones elsewhere
#ifdef _WIN32
constexpr size_t MaxPathUnits = 260;
#else
constexpr size_t MaxPathUnits = 1024;
#endif

bool ValidatePath(const fs::path& path) {
    if (path.empty()) {
        LOG_ERROR(Common_Filesystem, "Input path is empty, path=%s",
                  (PathToUTF8String(path)).c_str());
        return false;
    }

#ifdef _WIN32
    if (path.u16string().size() >= MaxPathUnits) {
        LOG_ERROR(Common_Filesystem, "Input path is too long, path=%s",
                  (PathToUTF8String(path)).c_str());
        return false;
    }
#else
    if (path.u8string().size() >= MaxPathUnits) {
        LOG_ERROR(Common_Filesystem, "Input path is too long, path=%s",
                  (PathToUTF8String(path)).c_str());
        return false;
    }
#endif

    return true;
}

std::string PathToUTF8String(const std::filesystem::path& path) {
    const auto u8_string = path.u8string();
    return std::string{u8_string.begin(), u8_string.end()};
}

std::filesystem::path PathFromUTF8String(std::string_view utf8) {
    return std::filesystem::path(
        std::u8string_view(reinterpret_cast<const char8_t*>(utf8.data()), utf8.size()));
}

std::filesystem::path PathFromCString(size_t (*get)(char* s, size_t len)) {
    char buf[4096];
    const size_t len = get(buf, sizeof(buf));
    if (len >= sizeof(buf)) {
        return {};
    }
    return PathFromUTF8String(std::string_view(buf, len));
}

const fs::path& GetUserPath(PathType shad_path) {
    return UserPaths().paths[static_cast<size_t>(shad_path)];
}

const char* GetUserPathUTF8(PathType shad_path) {
    return UserPaths().utf8[static_cast<size_t>(shad_path)];
}

std::string GetUserPathString(PathType shad_path) {
    return GetUserPathUTF8(shad_path);
}

void SetUserPath(PathType shad_path, const fs::path& new_path) {
    if (!Common::FS::IsDirectory(new_path)) {
        LOG_ERROR(Common_Filesystem, "Filesystem object at new_path=%s is not a directory",
                  (PathToUTF8String(new_path)).c_str());
        return;
    }

    SetTableEntry(UserPaths(), static_cast<size_t>(shad_path), PathToUTF8String(new_path).c_str());
}

std::optional<fs::path> FindGameByID(const fs::path& dir, const std::string& game_id,
                                     int max_depth) {
    const auto boot_path_for = [](const fs::path& root) -> std::optional<fs::path> {
        std::error_code ec;
        if (Common::FS::IsDirectory(root) && !ec) {
            if (!Common::FS::Exists(root / "sce_sys" / "param.sfo")) {
                return std::nullopt;
            }
            if (auto eboot_path = root / "eboot.bin"; Common::FS::Exists(eboot_path)) {
                return eboot_path;
            }
            return std::nullopt;
        }
        if (Core::FileSys::IsZArchiveFile(root) &&
            Core::FileSys::ReadGameFile(root, "sce_sys/param.sfo").has_value()) {
            return root;
        }
        return std::nullopt;
    };

    bool is_different_game_root = false;

    // Check if this is a valid game root
    if (auto found = boot_path_for(dir)) {
        if (dir.filename() == game_id) { // Check if this is the game we're looking for
            return found;
        } else { // if not, don't waste time looking through game data folders in a different game
            is_different_game_root = true;
        }
    }

    if (is_different_game_root) {
        return std::nullopt;
    } else {
        if (auto found = boot_path_for(dir / game_id)) {
            return found;
        }
        if (auto found = boot_path_for(dir / (game_id + ".zar"))) {
            return found;
        }
    }

    if (max_depth <= 0) {
        return std::nullopt;
    }

    // Recursively search subdirectories
    std::optional<fs::path> found;
    ForEachEntry(dir, [&](std::string_view name, bool is_dir) {
        if (is_dir && !found) {
            found = FindGameByID(dir / PathFromUTF8String(name), game_id, max_depth - 1);
        }
    });
    return found;
}

} // namespace Common::FS
