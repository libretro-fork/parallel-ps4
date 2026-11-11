// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstring>
#include <limits>
#include <vector>

#include <file/file_path.h>
#include <retro_dirent.h>
#include <streams/file_stream.h>
#include <vfs/vfs.h>

#include "common/alignment.h"
#include "common/assert.h"
#include "common/error.h"
#include "common/io_file.h"
#include "common/logging/log.h"
#include "common/path_util.h"

#ifdef _WIN32
#include "common/ntapi.h"

#include <io.h>
#include <share.h>
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#ifdef _MSC_VER
#define fileno _fileno
#endif

namespace Common::FS {

namespace fs = std::filesystem;

namespace {

#ifdef _WIN32

std::optional<FileInfo> GetFileInfoFromHandle(HANDLE handle) {
    BY_HANDLE_FILE_INFORMATION stat{};
    FILE_BASIC_INFO basic{};
    if (!GetFileInformationByHandle(handle, &stat) ||
        !GetFileInformationByHandleEx(handle, FileBasicInfo, &basic, sizeof(basic))) {
        return std::nullopt;
    }
    FileInfo info{};
    FILE_ID_INFO id{};
    if (GetFileInformationByHandleEx(handle, FileIdInfo, &id, sizeof(id))) {
        info.identity.device = id.VolumeSerialNumber;
        std::memcpy(&info.identity.file_lo, id.FileId.Identifier, sizeof(u64));
        std::memcpy(&info.identity.file_hi, id.FileId.Identifier + sizeof(u64), sizeof(u64));
    } else {
        info.identity.device = stat.dwVolumeSerialNumber;
        info.identity.file_lo = (u64{stat.nFileIndexHigh} << 32) | stat.nFileIndexLow;
    }
    info.size = (u64{stat.nFileSizeHigh} << 32) | stat.nFileSizeLow;
    info.is_directory = (stat.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    const auto set_time = [](const LARGE_INTEGER& time, s64& seconds, s64& nanoseconds) {
        const u64 ticks = static_cast<u64>(time.QuadPart);
        seconds = static_cast<s64>(ticks / 10'000'000) - 11'644'473'600;
        nanoseconds = static_cast<s64>(ticks % 10'000'000) * 100;
    };
    set_time(basic.LastWriteTime, info.mtime_sec, info.mtime_nsec);
    set_time(basic.LastAccessTime, info.atime_sec, info.atime_nsec);
    set_time(basic.ChangeTime, info.ctime_sec, info.ctime_nsec);
    return info;
}

#else

FileInfo GetFileInfoFromStat(const struct stat& stat) {
    FileInfo info{};
    info.identity.device = static_cast<u64>(stat.st_dev);
    info.identity.file_lo = static_cast<u64>(stat.st_ino);
    info.size = static_cast<u64>(stat.st_size);
    info.is_directory = S_ISDIR(stat.st_mode);
#ifdef __APPLE__
    info.mtime_sec = stat.st_mtimespec.tv_sec;
    info.mtime_nsec = stat.st_mtimespec.tv_nsec;
    info.atime_sec = stat.st_atimespec.tv_sec;
    info.atime_nsec = stat.st_atimespec.tv_nsec;
    info.ctime_sec = stat.st_ctimespec.tv_sec;
    info.ctime_nsec = stat.st_ctimespec.tv_nsec;
#else
    info.mtime_sec = stat.st_mtim.tv_sec;
    info.mtime_nsec = stat.st_mtim.tv_nsec;
    info.atime_sec = stat.st_atim.tv_sec;
    info.atime_nsec = stat.st_atim.tv_nsec;
    info.ctime_sec = stat.st_ctim.tv_sec;
    info.ctime_nsec = stat.st_ctim.tv_nsec;
#endif
    return info;
}

#endif

[[nodiscard]] constexpr int ToSeekOrigin(SeekOrigin origin) {
    switch (origin) {
    case SeekOrigin::SetOrigin:
        return RETRO_VFS_SEEK_POSITION_START;
    case SeekOrigin::CurrentPosition:
        return RETRO_VFS_SEEK_POSITION_CURRENT;
    case SeekOrigin::End:
        return RETRO_VFS_SEEK_POSITION_END;
    default:
        UNREACHABLE_MSG("Impossible SeekOrigin {}", static_cast<u32>(origin));
    }
}

[[nodiscard]] constexpr unsigned ToHints(FileHint hint) {
    switch (hint) {
    case FileHint::Mapped:
        return RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS;
    case FileHint::Sequential:
        return RETRO_VFS_FILE_ACCESS_HINT_SEQUENTIAL_BULK;
    default:
        return RETRO_VFS_FILE_ACCESS_HINT_NONE;
    }
}

/// The OS descriptor under a stream of the VFS's own (the core installs no frontend VFS in the
/// emulator's process), or -1.
int Descriptor(RFILE* file) {
    const libretro_vfs_implementation_file* handle = filestream_get_vfs_handle(file);
    if (!handle) {
        return -1;
    }
    if (handle->fd >= 0) {
        return handle->fd;
    }
    return handle->fp ? fileno(handle->fp) : -1;
}

} // Anonymous namespace

IOFile::IOFile() = default;

IOFile::IOFile(const fs::path& path, FileAccessMode mode, FileHint hint) {
    Open(path, mode, hint);
}

IOFile::~IOFile() {
    Close();
}

IOFile::IOFile(IOFile&& other) noexcept {
    std::swap(file_path, other.file_path);
    std::swap(file_access_mode, other.file_access_mode);
    std::swap(file, other.file);
    std::swap(file_mapping, other.file_mapping);
}

IOFile& IOFile::operator=(IOFile&& other) noexcept {
    std::swap(file_path, other.file_path);
    std::swap(file_access_mode, other.file_access_mode);
    std::swap(file, other.file);
    std::swap(file_mapping, other.file_mapping);
    return *this;
}

int IOFile::Open(const fs::path& path, FileAccessMode mode, FileHint hint) {
    Close();

    file_path = path;
    file_access_mode = mode;

    const std::string utf8 = PathToUTF8String(path);
    const unsigned hints = ToHints(hint);
    errno = 0;
    switch (mode) {
    case FileAccessMode::Read:
        file = filestream_open(utf8.c_str(), RETRO_VFS_FILE_ACCESS_READ, hints);
        break;
    case FileAccessMode::Write:
    case FileAccessMode::ReadWrite:
        file = filestream_open(
            utf8.c_str(), RETRO_VFS_FILE_ACCESS_READ_WRITE | RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING,
            hints);
        break;
    case FileAccessMode::Append:
    case FileAccessMode::ReadAppend:
        // The VFS has no append mode: the file is opened keeping its contents, created first
        // when missing, and every write goes to its end (WriteBytes).
        file = filestream_open(
            utf8.c_str(), RETRO_VFS_FILE_ACCESS_READ_WRITE | RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING,
            hints);
        if (!file && !filestream_exists(utf8.c_str())) {
            file = filestream_open(utf8.c_str(), RETRO_VFS_FILE_ACCESS_READ_WRITE, hints);
        }
        break;
    case FileAccessMode::Create:
        file = filestream_open(utf8.c_str(), RETRO_VFS_FILE_ACCESS_WRITE, hints);
        break;
    default:
        UNREACHABLE_MSG("Impossible FileAccessMode {}", static_cast<u32>(mode));
    }

    if (!IsOpen()) {
        const int result = errno ? errno : ENOENT;
        const auto ec = std::error_code{result, std::generic_category()};
        LOG_ERROR(Common_Filesystem, "Failed to open the file at path={}, error_message={}", utf8,
                  ec.message());
        return result;
    }
    return 0;
}

void IOFile::Close() {
    if (!IsOpen()) {
        return;
    }

#ifdef _WIN64
    if (file_mapping && file_access_mode == FileAccessMode::ReadWrite) {
        CloseHandle(std::bit_cast<HANDLE>(file_mapping));
    }
#endif
    file_mapping = 0;

    if (filestream_close(file) != 0) {
        LOG_ERROR(Common_Filesystem, "Failed to close the file at path={}",
                  PathToUTF8String(file_path));
    }
    file = nullptr;
}

void IOFile::Unlink() {
    if (!IsOpen()) {
        return;
    }

    // Mark the file for deletion
#ifdef _WIN64
    FILE_DISPOSITION_INFORMATION disposition;
    IO_STATUS_BLOCK iosb;

    HANDLE hfile = reinterpret_cast<HANDLE>(_get_osfhandle(Descriptor(file)));

    disposition.DeleteFile = TRUE;
    NtSetInformationFile(hfile, &iosb, &disposition, sizeof(disposition),
                         FileDispositionInformation);
#else
    if (filestream_delete(PathToUTF8String(file_path).c_str()) != 0) {
        LOG_ERROR(Common_Filesystem, "Failed to unlink the file at path={}",
                  PathToUTF8String(file_path));
    }
#endif
}

uintptr_t IOFile::GetFileMapping() {
    if (file_mapping) {
        return file_mapping;
    }
    const int fd = Descriptor(file);
    ASSERT_MSG(fd >= 0, "No descriptor under {}", PathToUTF8String(file_path));
#ifdef _WIN64
    HANDLE hfile = reinterpret_cast<HANDLE>(_get_osfhandle(fd));
    HANDLE mapping = nullptr;

    if (file_access_mode == FileAccessMode::ReadWrite) {
        mapping = CreateFileMapping2(hfile, NULL, FILE_MAP_WRITE, PAGE_READWRITE, SEC_COMMIT, 0,
                                     NULL, NULL, 0);
    } else {
        mapping = hfile;
    }

    file_mapping = std::bit_cast<uintptr_t>(mapping);
    ASSERT_MSG(file_mapping, "{}", Common::GetLastErrorMsg());
    return file_mapping;
#else
    file_mapping = static_cast<uintptr_t>(fd);
    return file_mapping;
#endif
}

size_t IOFile::ReadBytes(void* data, size_t size) const {
    if (!IsOpen() || size == 0) {
        return 0;
    }
    const s64 read = filestream_read(file, data, static_cast<s64>(size));
    if (read < 0) {
        LOG_ERROR(Common_Filesystem, "Failed to read the file at path={}",
                  PathToUTF8String(file_path));
        return 0;
    }
    return static_cast<size_t>(read);
}

size_t IOFile::WriteBytes(const void* data, size_t size) const {
    if (!IsOpen() || size == 0) {
        return 0;
    }
    if (True(file_access_mode & FileAccessMode::Append)) {
        filestream_seek(file, 0, RETRO_VFS_SEEK_POSITION_END);
    }
    const s64 written = filestream_write(file, data, static_cast<s64>(size));
    if (written < 0) {
        LOG_ERROR(Common_Filesystem, "Failed to write to the file at path={}",
                  PathToUTF8String(file_path));
        return 0;
    }
    return static_cast<size_t>(written);
}

std::span<const u8> IOFile::Mapped() const {
    if (!IsOpen()) {
        return {};
    }
    s64 length = 0;
    const u8* base = filestream_get_mapped_ptr(file, &length);
    if (!base) {
        return {};
    }
    return {base, static_cast<size_t>(length)};
}

s64 IOFile::View(u64 len, const u8** ptr) const {
    if (!IsOpen()) {
        return -1;
    }
    return filestream_view(file, static_cast<s64>(len), ptr);
}

void IOFile::Prefetch(u64 offset, u64 len) const {
    if (IsOpen()) {
        filestream_prefetch(file, offset, len);
    }
}

bool IOFile::Flush() const {
    if (!IsOpen()) {
        return false;
    }
    const bool flushed = filestream_flush(file) == 0;
    if (!flushed) {
        LOG_ERROR(Common_Filesystem, "Failed to flush the file at path={}",
                  PathToUTF8String(file_path));
    }
    return flushed;
}

bool IOFile::Commit() const {
    if (!IsOpen()) {
        return false;
    }
    bool committed = filestream_flush(file) == 0;
    if (committed) {
        const int fd = Descriptor(file);
#ifdef _WIN32
        committed = fd >= 0 && _commit(fd) == 0;
#else
        committed = fd >= 0 && fsync(fd) == 0;
#endif
    }
    if (!committed) {
        LOG_ERROR(Common_Filesystem, "Failed to commit the file at path={}",
                  PathToUTF8String(file_path));
    }
    return committed;
}

bool IOFile::SetSize(u64 size) const {
    if (!IsOpen()) {
        return false;
    }
    const bool resized = filestream_truncate(file, static_cast<s64>(size)) == 0;
    if (!resized) {
        LOG_ERROR(Common_Filesystem, "Failed to resize the file at path={}, size={}",
                  PathToUTF8String(file_path), size);
    }
    return resized;
}

std::optional<FileInfo> GetFileInfo(const std::filesystem::path& path) {
#ifdef _WIN32
    const HANDLE handle =
        CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        return std::nullopt;
    }
    const auto info = GetFileInfoFromHandle(handle);
    CloseHandle(handle);
    return info;
#else
    struct stat stat{};
    if (::stat(path.c_str(), &stat) != 0) {
        return std::nullopt;
    }
    return GetFileInfoFromStat(stat);
#endif
}

std::optional<FileInfo> IOFile::GetInfo() const {
    if (!IsOpen()) {
        return std::nullopt;
    }
#ifdef _WIN32
    const int fd = Descriptor(file);
    if (fd < 0) {
        return std::nullopt;
    }
    const auto handle = reinterpret_cast<HANDLE>(_get_osfhandle(fd));
    if (handle == INVALID_HANDLE_VALUE) {
        return std::nullopt;
    }
    return GetFileInfoFromHandle(handle);
#else
    struct stat stat{};
    const int fd = Descriptor(file);
    if (fd < 0 || ::fstat(fd, &stat) != 0) {
        return std::nullopt;
    }
    return GetFileInfoFromStat(stat);
#endif
}

u64 IOFile::GetSize() const {
    if (!IsOpen()) {
        return 0;
    }
    const s64 size = filestream_get_size(file);
    return size < 0 ? 0 : static_cast<u64>(size);
}

bool IOFile::Seek(s64 offset, SeekOrigin origin) const {
    if (!IsOpen()) {
        return false;
    }
    const bool sought = filestream_seek(file, offset, ToSeekOrigin(origin)) >= 0;
    if (!sought) {
        LOG_ERROR(Common_Filesystem, "Failed to seek the file at path={}, offset={}, origin={}",
                  PathToUTF8String(file_path), offset, static_cast<u32>(origin));
    }
    return sought;
}

s64 IOFile::Tell() const {
    if (!IsOpen()) {
        return 0;
    }
    return filestream_tell(file);
}

FileView::FileView(FileView&& other) noexcept
    : file{std::move(other.file)}, owned{std::move(other.owned)}, bytes{other.bytes} {
    // A moved vector keeps its buffer, so a span into it stays good
    other.bytes = {};
}

FileView& FileView::operator=(FileView&& other) noexcept {
    file = std::move(other.file);
    owned = std::move(other.owned);
    bytes = other.bytes;
    other.bytes = {};
    return *this;
}

std::optional<FileView> FileView::Map(const fs::path& path) {
    // A missing file is an answer, not an error to log
    const std::string utf8 = PathToUTF8String(path);
    if (!path_is_valid(utf8.c_str())) {
        return std::nullopt;
    }
    FileView view;
    if (view.file.Open(path, FileAccessMode::Read, FileHint::Mapped) != 0) {
        return std::nullopt;
    }
    // An empty file has nothing to map
    if (view.file.GetSize() != 0) {
        view.bytes = view.file.Mapped();
        if (view.bytes.empty()) {
            LOG_ERROR(Common_Filesystem, "Failed to map the file at path={}",
                      PathToUTF8String(path));
            return std::nullopt;
        }
    }
    return view;
}

bool CopyFile(const fs::path& from, const fs::path& to) {
    retro_vfs_copy_handle* copy = filestream_copy_begin(
        PathToUTF8String(from).c_str(), PathToUTF8String(to).c_str(), RETRO_VFS_COPY_OVERWRITE);
    if (!copy) {
        return false;
    }
    s64 done = 0;
    s64 total = 0;
    while (filestream_copy_step(copy, std::numeric_limits<s64>::max(), &done, &total) ==
           RETRO_VFS_COPY_RUNNING) {
    }
    return filestream_copy_close(copy) == 0;
}

bool Exists(const fs::path& path) {
    return path_is_valid(PathToUTF8String(path).c_str());
}

bool IsDirectory(const fs::path& path) {
    return path_is_directory(PathToUTF8String(path).c_str());
}

bool CreateDirectories(const fs::path& path) {
    return path_mkdir(PathToUTF8String(path).c_str());
}

bool RemoveAll(const fs::path& path) {
    return path_remove_tree(PathToUTF8String(path).c_str());
}

bool CopyAll(const fs::path& from, const fs::path& to) {
    return path_copy_tree(PathToUTF8String(from).c_str(), PathToUTF8String(to).c_str());
}

bool Rename(const fs::path& from, const fs::path& to) {
    return filestream_rename(PathToUTF8String(from).c_str(), PathToUTF8String(to).c_str()) == 0;
}

bool ForEachEntry(const fs::path& dir,
                  const std::function<void(std::string_view name, bool is_dir)>& func) {
    RDIR* rdir = retro_opendir_include_hidden(PathToUTF8String(dir).c_str(), true);
    if (!rdir) {
        return false;
    }
    while (retro_readdir(rdir)) {
        const std::string_view name = retro_dirent_get_name(rdir);
        if (name == "." || name == "..") {
            continue;
        }
        func(name, retro_dirent_is_dir(rdir, nullptr));
    }
    retro_closedir(rdir);
    return true;
}

u64 GetDirectorySize(const fs::path& path) {
    u64 total = 0;
    ForEachEntry(path, [&](std::string_view name, bool is_dir) {
        const fs::path entry = path / PathFromUTF8String(name);
        if (is_dir) {
            total += GetDirectorySize(entry);
        } else if (const s64 size = path_get_size(PathToUTF8String(entry).c_str()); size > 0) {
            total += static_cast<u64>(size);
        }
    });
    return total;
}

} // namespace Common::FS
