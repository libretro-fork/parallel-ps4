// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// A PKG as a game's root, as a .zar is one: the emulator's file system reads the game,
// its update or its additional content straight out of the PKG (pkg_reader.h).

#pragma once

#include "common/combiner.h"
#include <filesystem>
#include <memory>
#include <mutex>

#include "core/file_sys/ifile.h"
#include "pkg_reader.h"

namespace Libretro {

class PkgFile final : public Core::FileSys::IFile {
public:
    PkgFile(std::shared_ptr<PkgReader> reader, const PkgReader::Node* node);

    s64 Read(void* dst, u64 size) override;
    s64 Write(const void* src, u64 size) override;
    bool Seek(s64 offset, Common::FS::SeekOrigin origin) override;
    u64 Tell() const override;
    u64 Size() const override;
    bool Flush() override;
    bool IsOpen() const override;

    std::optional<std::filesystem::path> GetMetadataHostPath() const override {
        return reader->Path();
    }

    void Stat(Core::FileSys::FileStat& out) override;

    Core::FileSys::MmapPolicy GetMmapPolicy() const override {
        return Core::FileSys::MmapPolicy::Copy;
    }

    bool Map(u8* addr, u64 size, u64 offset, u32 raw_prot,
             const Core::FileSys::FileMapContext& ctx) override;

    bool IsReadOnly() const override {
        return true;
    }

private:
    std::shared_ptr<PkgReader> reader;
    const PkgReader::Node* node;
    mutable Common::Combiner position_mutex;
    u64 position = 0;
};

class PkgDirectory final : public Core::FileSys::IDirectory {
public:
    PkgDirectory(std::shared_ptr<PkgReader> reader, std::string rel, const PkgReader::Node* node);

    bool Next(Core::FileSys::DirEntry& out) override;
    void Rewind() override;

private:
    std::shared_ptr<PkgReader> reader;
    std::string rel;
    const PkgReader::Node* node;
    std::size_t index = 0;
};

class PkgBackend final : public Core::FileSys::IBackend {
public:
    explicit PkgBackend(const std::filesystem::path& pkg_path);

    bool IsOpen() const {
        return reader != nullptr;
    }

    bool Exists(std::string_view rel_path) override;
    bool IsDirectory(std::string_view rel_path) override;
    std::unique_ptr<Core::FileSys::IFile> Open(std::string_view rel_path,
                                               Common::FS::FileAccessMode mode) override;
    std::unique_ptr<Core::FileSys::IDirectory> OpenDir(std::string_view rel_path) override;
    bool Stat(std::string_view rel_path, Core::FileSys::FileStat& out) override;

    bool IsReadOnly() const override {
        return true;
    }

    std::filesystem::path RootPath() const override {
        return pkg_path;
    }

    std::optional<Common::FS::FileView> ReadFile(std::string_view rel_path) const override;

private:
    std::filesystem::path pkg_path;
    std::shared_ptr<PkgReader> reader;
};

// True for a regular file named *.pkg.
bool IsPkgFile(const std::filesystem::path& path);

} // namespace Libretro
