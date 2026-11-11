// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <mutex>
#include <system_error>
#include <unordered_map>

#include <file/file_path.h>
#include <streams/file_stream.h>

#include "common/hash.h"
#include "common/logging/log.h"
#include "common/path_util.h"
#include "core/file_sys/backends/host_fs.h"
#include "core/file_sys/backends/zarchive_fs.h"
#include "core/file_sys/fs.h"
#include "core/file_sys/ifile.h"
#include "libretro/pkg_backend.h"

namespace Core::FileSys {

namespace {

struct FileIdentityHash {
    size_t operator()(const FileIdentity& id) const noexcept {
        u64 seed = static_cast<u64>(id.kind);
        seed = HashCombine(seed, id.host.device);
        seed = HashCombine(seed, id.host.file_hi);
        seed = HashCombine(seed, id.host.file_lo);
        seed = HashCombine(seed, static_cast<u64>(id.node));
        return static_cast<size_t>(seed);
    }
};

} // namespace

std::optional<u32> GetGuestInode(const FileIdentity& identity) {
    static std::mutex mutex;
    static std::unordered_map<FileIdentity, u32, FileIdentityHash> inodes;
    static u32 next_inode = 1;

    std::scoped_lock lk{mutex};
    if (const auto it = inodes.find(identity); it != inodes.end()) {
        return it->second;
    }
    if (next_inode == 0) {
        LOG_ERROR(Kernel_Fs, "Out of guest inode numbers");
        return std::nullopt;
    }
    const u32 inode = next_inode++;
    inodes.emplace(identity, inode);
    return inode;
}

bool IsZArchiveFile(const std::filesystem::path& path) {
    std::error_code ec;
    return path.extension() == ".zar" && std::filesystem::is_regular_file(path, ec) && !ec;
}

std::filesystem::path StripZArchiveExtension(const std::filesystem::path& path) {
    if (path.extension() == ".zar") {
        std::filesystem::path stripped = path;
        stripped.replace_extension();
        return stripped;
    }
    return path;
}

std::unique_ptr<IBackend> OpenGameBackend(const std::filesystem::path& root) {
    const auto resolved = ResolveGameRoot(root);
    if (!resolved.has_value()) {
        return nullptr;
    }

    if (Libretro::IsPkgFile(*resolved)) {
        auto backend = std::make_unique<Libretro::PkgBackend>(*resolved);
        if (!backend->IsOpen()) {
            return nullptr;
        }
        return backend;
    }
    if (IsZArchiveFile(*resolved)) {
        auto backend = std::make_unique<ZArchiveBackend>(*resolved);
        if (!backend->IsOpen()) {
            return nullptr;
        }
        return backend;
    }

    return std::make_unique<HostFsBackend>(*resolved, true);
}

std::optional<Common::FS::FileView> ReadGameFile(const std::filesystem::path& game_root,
                                                 std::string_view rel_path) {
    const auto backend = OpenGameBackend(game_root);
    if (!backend) {
        return std::nullopt;
    }
    return backend->ReadFile(rel_path);
}

std::optional<std::filesystem::path> ResolveGameFilePath(const std::filesystem::path& game_root,
                                                         std::string_view rel_path) {
    const auto backend = OpenGameBackend(game_root);
    if (!backend) {
        return std::nullopt;
    }

    // Directory-backed: just hand back the real path if the file exists.
    if (const auto host_root = backend->RootHostPath()) {
        const std::filesystem::path full_path = *host_root / rel_path;
        std::error_code ec;
        if (std::filesystem::is_regular_file(full_path, ec) && !ec) {
            return full_path;
        }
        return std::nullopt;
    }

    const std::string archive_key = std::to_string(
        std::hash<std::string>{}(std::filesystem::absolute(backend->RootPath()).string()));
    const std::filesystem::path cache_dir =
        Common::FS::GetUserPath(Common::FS::PathType::CacheDir) / "zar_meta" / archive_key;
    const std::filesystem::path dest = cache_dir / std::filesystem::path(rel_path);

    std::error_code ec;
    const auto archive_time = std::filesystem::last_write_time(backend->RootPath(), ec);
    if (!ec && std::filesystem::is_regular_file(dest, ec) && !ec) {
        const auto dest_time = std::filesystem::last_write_time(dest, ec);
        if (!ec && dest_time >= archive_time) {
            return dest;
        }
    }

    const auto data = backend->ReadFile(rel_path);
    if (!data.has_value()) {
        return std::nullopt;
    }

    std::error_code mkdir_ec;
    std::filesystem::create_directories(dest.parent_path(), mkdir_ec);

    if (!filestream_write_file(Common::FS::PathToUTF8String(dest).c_str(), data->data(),
                               static_cast<s64>(data->size()))) {
        LOG_ERROR(Common_Filesystem, "Failed to write the archive extraction: {}", dest.string());
        return std::nullopt;
    }
    return dest;
}

u64 GetGameRootSize(const std::filesystem::path& game_root) {
    if (IsZArchiveFile(game_root)) {
        const s64 size = path_get_size(Common::FS::PathToUTF8String(game_root).c_str());
        return size > 0 ? static_cast<u64>(size) : 0ull;
    }
    return Common::FS::GetDirectorySize(game_root);
}

} // namespace Core::FileSys
