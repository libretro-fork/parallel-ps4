// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <formats/zar.h>

#include "core/file_sys/ifile.h"

struct RFILE;

namespace Core::FileSys {

// A ZArchive (.zar) file, read with libretro-common's reader (formats/zar.h) from a memory
// mapping of the whole archive (streams/file_stream.h). Nothing here takes a lock: the mapping
// is read from any thread, and each thread reading the archive has a reader of its own
// (Reader()), since a reader holds the state of the one operation it is doing.
class ZarArchive {
public:
    explicit ZarArchive(const std::filesystem::path& path);
    ~ZarArchive();

    ZarArchive(const ZarArchive&) = delete;
    ZarArchive& operator=(const ZarArchive&) = delete;

    bool IsOpen() const {
        return m_map != nullptr;
    }

    // The file or directory at @rel ('/' separated, without regard to ASCII case).
    bool LookUp(std::string_view rel, zar_file_t& out) const;

    // Up to @size octets of @file from @offset straight into @dst; fewer at its end.
    u64 Read(const zar_file_t& file, u64 offset, void* dst, u64 size) const;

    // The entries of the directory at @rel, in name order; nullopt when it is no directory.
    std::optional<std::vector<DirEntry>> List(std::string_view rel) const;

    // Stat of the entry at @rel: the archive's own times, a guest inode of its own.
    bool Stat(std::string_view rel, const zar_file_t& file, FileStat& out) const;

private:
    zar_t* Reader() const;
    // Drives an operation to its end, answering each range it asks for from the mapping.
    template <typename Step>
    int Run(zar_t* z, Step&& step) const;

    RFILE* m_file{nullptr};
    const u8* m_map{nullptr};
    u64 m_size{0};
    u64 m_serial{0};
    std::optional<Common::FS::FileInfo> m_info;
};

// IFile implementation backed by a file inside a ZArchive (.zar) file.
class ZArchiveFile final : public IFile {
public:
    ZArchiveFile(std::shared_ptr<const ZarArchive> archive, const zar_file_t& file,
                 const FileStat& stat, std::filesystem::path archive_path);
    ~ZArchiveFile() override = default;

    s64 Read(void* dst, u64 size) override;
    s64 Write(const void* src, u64 size) override;
    bool Seek(s64 offset, Common::FS::SeekOrigin origin) override;
    u64 Tell() const override;
    u64 Size() const override;
    bool Flush() override;
    bool IsOpen() const override;

    std::optional<std::filesystem::path> GetMetadataHostPath() const override {
        return m_archive_path;
    }

    void Stat(FileStat& out) override;

    MmapPolicy GetMmapPolicy() const override {
        return MmapPolicy::Copy;
    }

    bool Map(u8* addr, u64 size, u64 offset, u32 raw_prot, const FileMapContext& ctx) override;

    bool IsReadOnly() const override {
        return true;
    }

private:
    std::shared_ptr<const ZarArchive> m_archive;
    zar_file_t m_file;
    FileStat m_stat;
    std::filesystem::path m_archive_path;
    std::atomic<u64> m_position{0};
};

// The entries of a directory, listed when it is opened.
class ZArchiveDirectory final : public IDirectory {
public:
    explicit ZArchiveDirectory(std::vector<DirEntry> entries) : m_entries(std::move(entries)) {}
    ~ZArchiveDirectory() override = default;

    bool Next(DirEntry& out) override;
    void Rewind() override;

private:
    std::vector<DirEntry> m_entries;
    size_t m_index{0};
};

// Backend that maps a mounted namespace onto the contents of a .zar
class ZArchiveBackend final : public IBackend {
public:
    explicit ZArchiveBackend(const std::filesystem::path& archive_path);
    ~ZArchiveBackend() override;

    bool IsOpen() const {
        return m_archive && m_archive->IsOpen();
    }

    bool Exists(std::string_view rel_path) override;
    bool IsDirectory(std::string_view rel_path) override;
    bool Stat(std::string_view rel_path, FileStat& out) override;

    std::unique_ptr<IFile> Open(std::string_view rel_path,
                                Common::FS::FileAccessMode mode) override;
    std::unique_ptr<IDirectory> OpenDir(std::string_view rel_path) override;

    bool IsReadOnly() const override {
        return true;
    }

    std::optional<std::filesystem::path> RootHostPath() const override {
        return std::nullopt;
    }

    [[nodiscard]] std::filesystem::path RootPath() const override {
        return m_archive_path;
    }

    std::optional<std::vector<u8>> ReadFile(std::string_view rel_path) const override;

private:
    std::filesystem::path m_archive_path;
    std::shared_ptr<const ZarArchive> m_archive;
};

} // namespace Core::FileSys
