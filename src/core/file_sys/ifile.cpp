// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <mutex>
#include <system_error>
#include <unordered_map>

#include <file/file_path.h>
#include <streams/file_stream.h>

#include "common/combiner.h"
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
    static Common::Combiner mutex;
    static std::unordered_map<FileIdentity, u32, FileIdentityHash> inodes;
    static u32 next_inode = 1;

    return mutex.Run([&]() -> std::optional<u32> {
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
    });
}

bool IsZArchiveFile(const std::filesystem::path& path) {
    return path.extension() == ".zar" && Common::FS::IsFile(path);
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
        if (Common::FS::IsFile(full_path)) {
            return full_path;
        }
        return std::nullopt;
    }

    const std::string archive_key = std::to_string(
        std::hash<std::string>{}(std::filesystem::absolute(backend->RootPath()).string()));
    const std::filesystem::path cache_dir =
        Common::FS::GetUserPath(Common::FS::PathType::CacheDir) / "zar_meta" / archive_key;
    const std::filesystem::path dest = cache_dir / std::filesystem::path(rel_path);

    // An extraction no older than the archive is used as it is
    const std::string dest_utf8 = Common::FS::PathToUTF8String(dest);
    int64_t archive_time = 0;
    int64_t dest_time = 0;
    if (path_get_mtime(Common::FS::PathToUTF8String(backend->RootPath()).c_str(), &archive_time) &&
        Common::FS::IsFile(dest) && path_get_mtime(dest_utf8.c_str(), &dest_time) &&
        dest_time >= archive_time) {
        return dest;
    }

    const auto data = backend->ReadFile(rel_path);
    if (!data.has_value()) {
        return std::nullopt;
    }

    Common::FS::CreateDirectories(dest.parent_path());

    if (!filestream_write_file(dest_utf8.c_str(), data->data(), static_cast<s64>(data->size()))) {
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
