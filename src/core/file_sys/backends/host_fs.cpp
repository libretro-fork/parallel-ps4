// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <system_error>

#include <retro_dirent.h>

#include "common/path_util.h"
#include "common/string_util.h"
#include "core/file_sys/backends/host_fs.h"
#include "core/file_sys/fs.h"

namespace Core::FileSys {

namespace {

FileStat MakeFileStat(const Common::FS::FileInfo& info, u32 inode) {
    return {
        .size = info.size,
        .inode = inode,
        .mtime_sec = info.mtime_sec,
        .mtime_nsec = info.mtime_nsec,
        .atime_sec = info.atime_sec,
        .atime_nsec = info.atime_nsec,
        .ctime_sec = info.ctime_sec,
        .ctime_nsec = info.ctime_nsec,
        .is_directory = info.is_directory,
    };
}

} // namespace

// Guest code expects POSIX sharing: opening a file already open for writing must not fail. A file
// of a read-only mount (the game's own) cannot change underneath, so it is mapped and read from
// where it lies.
HostFile::HostFile(std::filesystem::path host_path, Common::FS::FileAccessMode mode, bool read_only)
    : m_path(std::move(host_path)),
      m_file(m_path, mode,
             read_only && mode == Common::FS::FileAccessMode::Read ? Common::FS::FileHint::Mapped
                                                                   : Common::FS::FileHint::None),
      m_read_only(read_only) {
    if (const auto info = m_file.GetInfo()) {
        m_inode = GetGuestInode(FileIdentity{.host = info->identity}).value_or(0);
    }
}

s64 HostFile::Read(void* dst, u64 size) {
    if (!m_file.IsOpen()) {
        return -1;
    }
    return static_cast<s64>(m_file.ReadRaw<u8>(dst, size));
}

s64 HostFile::Write(const void* src, u64 size) {
    if (!m_file.IsOpen() || m_read_only) {
        return -1;
    }
    const s64 written = static_cast<s64>(m_file.WriteRaw<u8>(src, size));
    // Visible at once to every other handle on the file, as on the console
    m_file.Flush();
    return written;
}

bool HostFile::Seek(s64 offset, Common::FS::SeekOrigin origin) {
    if (!m_file.IsOpen()) {
        return false;
    }
    return m_file.Seek(offset, origin);
}

u64 HostFile::Tell() const {
    if (!m_file.IsOpen()) {
        return 0;
    }
    return static_cast<u64>(m_file.Tell());
}

u64 HostFile::Size() const {
    if (!m_file.IsOpen()) {
        return 0;
    }
    return m_file.GetSize();
}

bool HostFile::Flush() {
    if (!m_file.IsOpen()) {
        return false;
    }
    if (m_read_only) {
        return true;
    }
    return m_file.Commit();
}

bool HostFile::IsOpen() const {
    return m_file.IsOpen();
}

void HostFile::Stat(FileStat& out) {
    m_file.Flush();
    const auto info = m_file.GetInfo();
    out = info ? MakeFileStat(*info, m_inode) : FileStat{};
}

HostDirectory::HostDirectory(const std::filesystem::path& root)
    : m_root(Common::FS::PathToUTF8String(root)) {
    m_dir = retro_opendir_include_hidden(m_root.c_str(), true);
}

HostDirectory::~HostDirectory() {
    if (m_dir) {
        retro_closedir(m_dir);
    }
}

bool HostDirectory::Next(DirEntry& out) {
    while (m_dir && retro_readdir(m_dir)) {
        const std::string_view name = retro_dirent_get_name(m_dir);
        if (name == "." || name == "..") {
            continue;
        }
        out.name = name;
        out.is_directory = retro_dirent_is_dir(m_dir, nullptr);
        // The size from what the listing already has (the find data on Windows)
        int64_t size = 0;
        out.size = (out.is_directory || !retro_dirent_stat(m_dir, &size, nullptr))
                       ? 0
                       : static_cast<u64>(size);
        return true;
    }
    return false;
}

void HostDirectory::Rewind() {
    if (m_dir) {
        retro_closedir(m_dir);
    }
    m_dir = retro_opendir_include_hidden(m_root.c_str(), true);
}

HostFsBackend::HostFsBackend(std::filesystem::path root, bool read_only)
    : m_root(std::move(root)), m_read_only(read_only) {}

std::filesystem::path HostFsBackend::Resolve(std::string_view rel_path) const {
    if (rel_path.empty()) {
        return m_root;
    }
    std::filesystem::path direct = m_root;
    direct /= rel_path;
    if constexpr (!NeedsCaseInsensitiveSearch) {
        return direct;
    } else {
        if (Common::FS::Exists(direct)) {
            return direct;
        }
        return ResolveCaseInsensitive(rel_path).value_or(std::move(direct));
    }
}

std::optional<std::filesystem::path> HostFsBackend::ResolveCaseInsensitive(
    std::string_view rel_path) const {
    return m_case_cache_mutex.Run([&]() -> std::optional<std::filesystem::path> {
        std::filesystem::path current = m_root;
        std::string prefix;

        for (const auto& part : std::filesystem::path(rel_path)) {
            const std::string part_str = part.string();
            if (part_str.empty() || part_str == ".") {
                continue;
            }
            if (!prefix.empty()) {
                prefix += '/';
            }
            prefix += Common::ToLower(part_str);

            if (const auto it = m_case_cache.find(prefix); it != m_case_cache.end()) {
                current = it->second;
                continue;
            }

            if (std::filesystem::path candidate = current / part_str;
                Common::FS::Exists(candidate)) {
                current = std::move(candidate);
                m_case_cache.emplace(prefix, current);
                continue;
            }

            const std::string part_low = Common::ToLower(part_str);
            std::string match;
            Common::FS::ForEachEntry(current, [&](std::string_view name, bool) {
                if (match.empty() && Common::ToLower(std::string{name}) == part_low) {
                    match = name;
                }
            });
            if (match.empty()) {
                return std::nullopt;
            }
            current /= Common::FS::PathFromUTF8String(match);
            m_case_cache.emplace(prefix, current);
        }
        return current;
    });
}

bool HostFsBackend::Exists(std::string_view rel_path) {
    return Common::FS::Exists(Resolve(rel_path));
}

bool HostFsBackend::IsDirectory(std::string_view rel_path) {
    return Common::FS::IsDirectory(Resolve(rel_path));
}

bool HostFsBackend::Stat(std::string_view rel_path, FileStat& out) {
    const auto info = Common::FS::GetFileInfo(Resolve(rel_path));
    if (!info) {
        return false;
    }
    const auto inode = GetGuestInode(FileIdentity{.host = info->identity});
    if (!inode) {
        return false;
    }
    out = MakeFileStat(*info, *inode);
    return true;
}

std::unique_ptr<IFile> HostFsBackend::Open(std::string_view rel_path,
                                           Common::FS::FileAccessMode mode) {
    const bool writable = mode != Common::FS::FileAccessMode::Read;
    if (writable && m_read_only) {
        return nullptr;
    }
    const auto path = Resolve(rel_path);
    if (mode != Common::FS::FileAccessMode::Create && !Common::FS::IsFile(path)) {
        return nullptr;
    }
    auto handle = std::make_unique<HostFile>(path, mode, m_read_only || !writable);
    if (!handle->IsOpen()) {
        return nullptr;
    }
    return handle;
}

std::unique_ptr<IDirectory> HostFsBackend::OpenDir(std::string_view rel_path) {
    const auto path = Resolve(rel_path);
    if (!Common::FS::IsDirectory(path)) {
        return nullptr;
    }
    return std::make_unique<HostDirectory>(path);
}

std::optional<Common::FS::FileView> HostFsBackend::ReadFile(std::string_view rel_path) const {
    return Common::FS::FileView::Map(Resolve(rel_path));
}

} // namespace Core::FileSys
