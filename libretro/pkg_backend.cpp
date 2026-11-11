// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstring>
#include <functional>
#include <map>
#include <system_error>

#include "common/combiner.h"
#include "common/io_file.h"
#include "common/logging/log.h"
#include "common/path_util.h"
#include "pkg_backend.h"

namespace Libretro {

namespace {

// One reader per PKG while anything uses it: a PKG is mounted more than once (the game,
// a look at an additional content's param.sfo, its mount), and opening one reads its PFS.
std::shared_ptr<PkgReader> SharedReader(const std::filesystem::path& path) {
    static Common::Combiner mutex;
    static std::map<std::filesystem::path, std::weak_ptr<PkgReader>> readers;
    return mutex.Run([&]() -> std::shared_ptr<PkgReader> {
        if (auto reader = readers[path].lock()) {
            return reader;
        }
        auto reader = std::make_shared<PkgReader>();
        std::string error;
        const auto keys = PkgKeysFile(Common::FS::GetUserPath(Common::FS::PathType::UserDir));
        if (!reader->Open(path, keys, error)) {
            LOG_ERROR(Loader, "PKG {} cannot be read: {}", path.string(), error);
            return nullptr;
        }
        LOG_INFO(Loader, "PKG {}: {} files, read in place", path.string(), reader->Files().size());
        readers[path] = reader;
        return reader;
    });
}

// As the .zar backend does: the times are the PKG's, and the inode is the guest inode of
// the entry, which stays the same for the life of the process. The same for a path's stat
// and an open file's fstat, which fails without an inode.
bool NodeStat(const std::filesystem::path& pkg_path, const PkgReader::Node* node,
              Core::FileSys::FileStat& out) {
    out = {};
    const auto info = Common::FS::GetFileInfo(pkg_path);
    if (!info) {
        return false;
    }
    // The reader's nodes do not move once the PKG is open, so a node's address names it.
    const auto inode = Core::FileSys::GetGuestInode(Core::FileSys::FileIdentity{
        .kind = Core::FileSys::FileIdentity::Kind::ArchiveEntry,
        .host = info->identity,
        .node = static_cast<u32>(std::hash<const void*>{}(node)),
    });
    if (!inode || *inode == 0) {
        return false;
    }
    out = {
        .size = node->directory ? 0 : node->size,
        .inode = *inode,
        .mtime_sec = info->mtime_sec,
        .mtime_nsec = info->mtime_nsec,
        .atime_sec = info->atime_sec,
        .atime_nsec = info->atime_nsec,
        .ctime_sec = info->ctime_sec,
        .ctime_nsec = info->ctime_nsec,
        .is_directory = node->directory,
    };
    return true;
}

} // namespace

bool IsPkgFile(const std::filesystem::path& path) {
    std::error_code ec;
    return path.extension() == ".pkg" && Common::FS::IsFile(path) && !ec;
}

PkgFile::PkgFile(std::shared_ptr<PkgReader> reader_, const PkgReader::Node* node_)
    : reader(std::move(reader_)), node(node_) {}

s64 PkgFile::Read(void* dst, u64 size) {
    return position_mutex.Run([&]() -> s64 {
        const u64 read = reader->Read(*node, position, size, dst);
        position += read;
        return static_cast<s64>(read);
    });
}

s64 PkgFile::Write(const void*, u64) {
    return -1;
}

bool PkgFile::Seek(s64 offset, Common::FS::SeekOrigin origin) {
    return position_mutex.Run([&]() -> bool {
        s64 base = 0;
        if (origin == Common::FS::SeekOrigin::CurrentPosition) {
            base = static_cast<s64>(position);
        } else if (origin == Common::FS::SeekOrigin::End) {
            base = static_cast<s64>(node->size);
        }
        if (base + offset < 0) {
            return false;
        }
        position = static_cast<u64>(base + offset);
        return true;
    });
}

u64 PkgFile::Tell() const {
    return position_mutex.Run([&]() -> u64 {
        return position;
    });
}

u64 PkgFile::Size() const {
    return node->size;
}

bool PkgFile::Flush() {
    return true;
}

bool PkgFile::IsOpen() const {
    return reader && node;
}

void PkgFile::Stat(Core::FileSys::FileStat& out) {
    NodeStat(reader->Path(), node, out);
}

bool PkgFile::Map(u8* addr, u64 size, u64 offset, u32 raw_prot,
                  const Core::FileSys::FileMapContext& ctx) {
    ctx.map_anonymous(addr, size);
    const u64 read = reader->Read(*node, offset, size, addr);
    if (read < size) {
        std::memset(addr + read, 0, size - read);
    }
    ctx.protect(addr, size, raw_prot);
    return true;
}

PkgDirectory::PkgDirectory(std::shared_ptr<PkgReader> reader_, std::string rel_,
                           const PkgReader::Node* node_)
    : reader(std::move(reader_)), rel(std::move(rel_)), node(node_) {}

bool PkgDirectory::Next(Core::FileSys::DirEntry& out) {
    if (index >= node->children.size()) {
        return false;
    }
    const std::string& name = node->children[index++];
    out.name = name;
    const auto* child = reader->Find(rel.empty() ? name : rel + "/" + name);
    out.is_directory = child && child->directory;
    out.size = child && !child->directory ? child->size : 0;
    return true;
}

void PkgDirectory::Rewind() {
    index = 0;
}

PkgBackend::PkgBackend(const std::filesystem::path& path)
    : pkg_path(path), reader(SharedReader(path)) {}

bool PkgBackend::Exists(std::string_view rel_path) {
    return reader && reader->Find(rel_path);
}

bool PkgBackend::IsDirectory(std::string_view rel_path) {
    const auto* node = reader ? reader->Find(rel_path) : nullptr;
    return node && node->directory;
}

std::unique_ptr<Core::FileSys::IFile> PkgBackend::Open(std::string_view rel_path,
                                                       Common::FS::FileAccessMode mode) {
    if (!reader || mode != Common::FS::FileAccessMode::Read) {
        return nullptr;
    }
    const auto* node = reader->Find(rel_path);
    if (!node || node->directory) {
        return nullptr;
    }
    return std::make_unique<PkgFile>(reader, node);
}

std::unique_ptr<Core::FileSys::IDirectory> PkgBackend::OpenDir(std::string_view rel_path) {
    const auto* node = reader ? reader->Find(rel_path) : nullptr;
    if (!node || !node->directory) {
        return nullptr;
    }
    std::string rel{rel_path};
    while (!rel.empty() && rel.front() == '/') {
        rel.erase(0, 1);
    }
    while (!rel.empty() && rel.back() == '/') {
        rel.pop_back();
    }
    return std::make_unique<PkgDirectory>(reader, rel, node);
}

bool PkgBackend::Stat(std::string_view rel_path, Core::FileSys::FileStat& out) {
    out = {};
    const auto* node = reader ? reader->Find(rel_path) : nullptr;
    return node && NodeStat(pkg_path, node, out);
}

std::optional<Common::FS::FileView> PkgBackend::ReadFile(std::string_view rel_path) const {
    const auto* node = reader ? reader->Find(rel_path) : nullptr;
    if (!node || node->directory) {
        return std::nullopt;
    }
    std::vector<u8> data(node->size);
    // Decrypted and inflated into its own buffer, which the view keeps
    if (reader->Read(*node, 0, data.size(), data.data()) != data.size()) {
        return std::nullopt;
    }
    return Common::FS::FileView{std::move(data)};
}

} // namespace Libretro
