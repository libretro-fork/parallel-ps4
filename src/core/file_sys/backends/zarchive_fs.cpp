// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <array>
#include <cstring>
#include <string>

#include <encodings/xxh3.h>
#include <streams/file_stream.h>

#include "common/logging/log.h"
#include "core/file_sys/backends/zarchive_fs.h"

namespace Core::FileSys {

namespace {

// The archive's paths have no leading slash, and must end in a terminator for the reader.
std::string NormalizeRel(std::string_view rel) {
    while (!rel.empty() && rel.front() == '/') {
        rel.remove_prefix(1);
    }
    return std::string(rel);
}

// An entry's place in the guest's inode table: its path, as the reader matches it (without
// regard to ASCII case), hashed.
u64 EntryNode(std::string_view rel) {
    std::string key(rel);
    std::transform(key.begin(), key.end(), key.begin(), [](char c) {
        return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c == '\\' ? '/' : c;
    });
    return xxh3_64(key.data(), key.size(), 0);
}

std::atomic<u64> next_serial{1};

} // namespace

ZarArchive::ZarArchive(const std::filesystem::path& path) {
    const auto u8path = path.u8string();
    m_file =
        filestream_open(reinterpret_cast<const char*>(u8path.c_str()), RETRO_VFS_FILE_ACCESS_READ,
                        RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS);
    if (!m_file) {
        LOG_ERROR(Kernel_Fs, "Failed to open ZArchive: {}", path.string());
        return;
    }
    s64 size = 0;
    const u8* map = filestream_get_mapped_ptr(m_file, &size);
    if (!map || size <= 0) {
        LOG_ERROR(Kernel_Fs, "Failed to map ZArchive: {}", path.string());
        return;
    }
    m_size = static_cast<u64>(size);
    m_serial = next_serial.fetch_add(1, std::memory_order_relaxed);
    m_info = Common::FS::GetFileInfo(path);
    m_map = map;
    // The archive is opened once here, to refuse a file that is not one.
    if (!Reader()) {
        LOG_ERROR(Kernel_Fs, "Not a ZArchive: {}", path.string());
        m_map = nullptr;
    }
}

ZarArchive::~ZarArchive() {
    if (m_file) {
        filestream_close(m_file);
    }
}

template <typename Step>
int ZarArchive::Run(zar_t* z, Step&& step) const {
    for (;;) {
        zar_request_t req;
        const int status = step(req);
        if (status != ZAR_PENDING) {
            return status;
        }
        const size_t n = req.offset < m_size
                             ? static_cast<size_t>(std::min<u64>(req.length, m_size - req.offset))
                             : 0;
        std::memcpy(req.dst, m_map + req.offset, n);
        if (zar_feed(z, n) != ZAR_OK) {
            return ZAR_ERROR_DATA;
        }
    }
}

zar_t* ZarArchive::Reader() const {
    // A few archives at a time per thread; one made for another archive is reused.
    struct Slot {
        u64 serial = 0;
        zar_t* reader = nullptr;
        ~Slot() {
            zar_free(reader);
        }
    };
    thread_local std::array<Slot, 4> slots;
    thread_local unsigned next = 0;
    for (auto& slot : slots) {
        if (slot.serial == m_serial && slot.reader) {
            return slot.reader;
        }
    }
    Slot& slot = slots[next++ % slots.size()];
    zar_free(slot.reader);
    slot.serial = 0;
    slot.reader = zar_new(m_size);
    if (!slot.reader) {
        return nullptr;
    }
    zar_t* z = slot.reader;
    if (Run(z, [z](zar_request_t& req) { return zar_open_step(z, &req); }) != ZAR_OK) {
        zar_free(slot.reader);
        slot.reader = nullptr;
        return nullptr;
    }
    slot.serial = m_serial;
    return z;
}

bool ZarArchive::LookUp(std::string_view rel, zar_file_t& out) const {
    zar_t* z = Reader();
    if (!z) {
        return false;
    }
    const std::string path = NormalizeRel(rel);
    if (zar_lookup_begin(z, path.c_str()) != ZAR_OK) {
        return false;
    }
    return Run(z, [&](zar_request_t& req) { return zar_lookup_step(z, &req, &out); }) == ZAR_OK;
}

u64 ZarArchive::Read(const zar_file_t& file, u64 offset, void* dst, u64 size) const {
    zar_t* z = Reader();
    if (!z || zar_read_begin(z, &file, offset, dst, static_cast<size_t>(size)) != ZAR_OK) {
        return 0;
    }
    Run(z, [z](zar_request_t& req) { return zar_read_step(z, &req); });
    return zar_read_progress(z);
}

std::optional<std::vector<DirEntry>> ZarArchive::List(std::string_view rel) const {
    zar_t* z = Reader();
    if (!z) {
        return std::nullopt;
    }
    const std::string path = NormalizeRel(rel);
    if (zar_dir_begin(z, path.c_str()) != ZAR_OK) {
        return std::nullopt;
    }
    std::vector<DirEntry> entries;
    for (;;) {
        zar_dirent_t ent;
        const int status = Run(z, [&](zar_request_t& req) { return zar_dir_step(z, &req, &ent); });
        if (status == ZAR_END) {
            return entries;
        }
        if (status != ZAR_OK) {
            return std::nullopt;
        }
        entries.push_back(DirEntry{
            .name = std::string(ent.name, ent.len),
            .is_directory = ent.is_dir != 0,
            .size = ent.is_dir ? 0 : ent.file.size,
        });
    }
}

bool ZarArchive::Stat(std::string_view rel, const zar_file_t& file, FileStat& out) const {
    out = {};
    if (!m_info) {
        return false;
    }
    const auto inode = GetGuestInode(FileIdentity{.kind = FileIdentity::Kind::ArchiveEntry,
                                                  .host = m_info->identity,
                                                  .node = EntryNode(NormalizeRel(rel))});
    if (!inode || *inode == 0) {
        return false;
    }
    out = {
        .size = file.dir ? 0 : file.size,
        .inode = *inode,
        .mtime_sec = m_info->mtime_sec,
        .mtime_nsec = m_info->mtime_nsec,
        .atime_sec = m_info->atime_sec,
        .atime_nsec = m_info->atime_nsec,
        .ctime_sec = m_info->ctime_sec,
        .ctime_nsec = m_info->ctime_nsec,
        .is_directory = file.dir != 0,
    };
    return true;
}

ZArchiveFile::ZArchiveFile(std::shared_ptr<const ZarArchive> archive, const zar_file_t& file,
                           const FileStat& stat, std::filesystem::path archive_path)
    : m_archive(std::move(archive)), m_file(file), m_stat(stat),
      m_archive_path(std::move(archive_path)) {}

s64 ZArchiveFile::Read(void* dst, u64 size) {
    if (!IsOpen() || size == 0) {
        return 0;
    }
    const u64 pos = m_position.load(std::memory_order_relaxed);
    if (pos >= m_file.size) {
        return 0;
    }
    const u64 read = m_archive->Read(m_file, pos, dst, std::min(size, m_file.size - pos));
    m_position.store(pos + read, std::memory_order_relaxed);
    return static_cast<s64>(read);
}

s64 ZArchiveFile::Write(const void* /*src*/, u64 /*size*/) {
    // ZArchive is read-only
    return -1;
}

bool ZArchiveFile::Seek(s64 offset, Common::FS::SeekOrigin origin) {
    s64 base = 0;
    switch (origin) {
    case Common::FS::SeekOrigin::SetOrigin:
        base = 0;
        break;
    case Common::FS::SeekOrigin::CurrentPosition:
        base = static_cast<s64>(m_position.load(std::memory_order_relaxed));
        break;
    case Common::FS::SeekOrigin::End:
        base = static_cast<s64>(m_file.size);
        break;
    }
    const s64 target = base + offset;
    if (target < 0) {
        return false;
    }
    m_position.store(static_cast<u64>(target), std::memory_order_relaxed);
    return true;
}

u64 ZArchiveFile::Tell() const {
    return m_position.load(std::memory_order_relaxed);
}

u64 ZArchiveFile::Size() const {
    return m_file.size;
}

bool ZArchiveFile::Flush() {
    // Read-only backend, nothing to flush.
    return true;
}

bool ZArchiveFile::IsOpen() const {
    return m_archive && m_archive->IsOpen();
}

void ZArchiveFile::Stat(FileStat& out) {
    out = m_stat;
}

bool ZArchiveFile::Map(u8* addr, u64 size, u64 offset, u32 raw_prot, const FileMapContext& ctx) {
    if (!IsOpen()) {
        return false;
    }
    // Reserve writable anonymous pages, read the file straight into them, zero what lies past
    // its end, then drop to the requested protection.
    ctx.map_anonymous(addr, size);
    const u64 read = offset < m_file.size ? m_archive->Read(m_file, offset, addr, size) : 0;
    if (read < size) {
        std::memset(addr + read, 0, size - read);
    }
    ctx.protect(addr, size, raw_prot);
    return true;
}

bool ZArchiveDirectory::Next(DirEntry& out) {
    if (m_index >= m_entries.size()) {
        return false;
    }
    out = m_entries[m_index++];
    return true;
}

void ZArchiveDirectory::Rewind() {
    m_index = 0;
}

ZArchiveBackend::ZArchiveBackend(const std::filesystem::path& archive_path)
    : m_archive_path(archive_path), m_archive(std::make_shared<const ZarArchive>(archive_path)) {}

ZArchiveBackend::~ZArchiveBackend() = default;

bool ZArchiveBackend::Exists(std::string_view rel_path) {
    zar_file_t file;
    return IsOpen() && m_archive->LookUp(rel_path, file);
}

bool ZArchiveBackend::IsDirectory(std::string_view rel_path) {
    zar_file_t file;
    return IsOpen() && m_archive->LookUp(rel_path, file) && file.dir;
}

bool ZArchiveBackend::Stat(std::string_view rel_path, FileStat& out) {
    zar_file_t file;
    return IsOpen() && m_archive->LookUp(rel_path, file) && m_archive->Stat(rel_path, file, out);
}

std::unique_ptr<IFile> ZArchiveBackend::Open(std::string_view rel_path,
                                             Common::FS::FileAccessMode mode) {
    // ZArchive is read-only, any write/create/append mode is rejected.
    if (mode != Common::FS::FileAccessMode::Read || !IsOpen()) {
        return nullptr;
    }
    zar_file_t file;
    if (!m_archive->LookUp(rel_path, file) || file.dir) {
        return nullptr;
    }
    FileStat stat{};
    m_archive->Stat(rel_path, file, stat);
    return std::make_unique<ZArchiveFile>(m_archive, file, stat, m_archive_path);
}

std::unique_ptr<IDirectory> ZArchiveBackend::OpenDir(std::string_view rel_path) {
    if (!IsOpen()) {
        return nullptr;
    }
    auto entries = m_archive->List(rel_path);
    if (!entries) {
        return nullptr;
    }
    return std::make_unique<ZArchiveDirectory>(std::move(*entries));
}

std::optional<std::vector<u8>> ZArchiveBackend::ReadFile(std::string_view rel_path) const {
    zar_file_t file;
    if (!IsOpen() || !m_archive->LookUp(rel_path, file) || file.dir) {
        return std::nullopt;
    }
    std::vector<u8> data(file.size);
    if (m_archive->Read(file, 0, data.data(), data.size()) != data.size()) {
        LOG_ERROR(Common_Filesystem, "Short read from ZArchive entry: {}", rel_path);
        return std::nullopt;
    }
    return data;
}

} // namespace Core::FileSys
