// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "common/concepts.h"
#include "common/types.h"
#include "enum.h"

// libretro-common's file stream (streams/file_stream.h)
struct RFILE;

namespace Common::FS {

enum class FileAccessMode {
    /// Opens an existing file for reading.
    Read = 1 << 0,
    /// Opens an existing file for writing, keeping its contents.
    Write = 1 << 1,
    /// Opens an existing file for reading and writing, keeping its contents.
    ReadWrite = Read | Write,
    /// Opens a file for appending, creating it if it does not exist: every write goes to the end.
    Append = 1 << 2,
    /// As Append, and the file can be read.
    ReadAppend = Read | Append,
    /// Creates a file, or empties an existing one, for writing.
    Create = 1 << 3,
};
DECLARE_ENUM_FLAG_OPERATORS(FileAccessMode);

/// How a file will be used, so the file stream can serve it the cheapest way.
enum class FileHint {
    None,
    /// Read where it lies: a read-only file is memory mapped, and Mapped() and View() lend its
    /// bytes in place instead of copying them out.
    Mapped,
    /// No stdio buffer between the caller and the file, reading or writing: each call goes to
    /// the descriptor, and what is written is in the file at once. For big pieces, or a file
    /// shared with other handles; small pieces are better off buffered.
    Sequential,
};

enum class SeekOrigin : u32 {
    SetOrigin,       // Seeks from the start of the file.
    CurrentPosition, // Seeks from the current file pointer position.
    End,             // Seeks from the end of the file.
};

struct FileIdentity {
    u64 device{};
    u64 file_hi{};
    u64 file_lo{};

    bool operator==(const FileIdentity&) const = default;
};

struct FileInfo {
    FileIdentity identity{};
    u64 size{};
    s64 mtime_sec{};
    s64 mtime_nsec{};
    s64 atime_sec{};
    s64 atime_nsec{};
    s64 ctime_sec{};
    s64 ctime_nsec{};
    bool is_directory{};
};

[[nodiscard]] std::optional<FileInfo> GetFileInfo(const std::filesystem::path& path);

/// A file, read and written through libretro-common's file stream (RFILE).
class IOFile final {
public:
    IOFile();

    explicit IOFile(const std::filesystem::path& path, FileAccessMode mode,
                    FileHint hint = FileHint::None);
    explicit IOFile(const std::string& path, FileAccessMode mode, FileHint hint = FileHint::None)
        : IOFile(std::filesystem::path(path), mode, hint) {}
    explicit IOFile(std::string_view path, FileAccessMode mode, FileHint hint = FileHint::None)
        : IOFile(std::filesystem::path(path), mode, hint) {}

    ~IOFile();

    IOFile(const IOFile&) = delete;
    IOFile& operator=(const IOFile&) = delete;

    IOFile(IOFile&& other) noexcept;
    IOFile& operator=(IOFile&& other) noexcept;

    std::filesystem::path GetPath() const {
        return file_path;
    }

    FileAccessMode GetAccessMode() const {
        return file_access_mode;
    }

    bool IsOpen() const {
        return file != nullptr;
    }

    bool IsWriteOnly() const {
        return file_access_mode == FileAccessMode::Append ||
               file_access_mode == FileAccessMode::Write;
    }

    /// The OS handle a guest mapping of the file is made from: a file mapping or file handle
    /// on Windows, a descriptor elsewhere.
    uintptr_t GetFileMapping();

    /// 0 on success, else an errno value.
    int Open(const std::filesystem::path& path, FileAccessMode mode,
             FileHint hint = FileHint::None);
    void Close();

    void Unlink();

    bool Flush() const;
    bool Commit() const;

    bool SetSize(u64 size) const;
    u64 GetSize() const;
    [[nodiscard]] std::optional<FileInfo> GetInfo() const;

    bool Seek(s64 offset, SeekOrigin origin = SeekOrigin::SetOrigin) const;
    s64 Tell() const;

    /// The whole file, lent in place, when it was opened with FileHint::Mapped and could be
    /// mapped; empty otherwise. Valid until the file is closed.
    std::span<const u8> Mapped() const;

    /// Lends up to @len bytes at the file position in place (from the mapping or the stream's
    /// read-ahead) and moves past them, as a read would. Returns the bytes lent, 0 at the end,
    /// -1 when nothing can be lent there: read instead. Valid until the next call on the file.
    s64 View(u64 len, const u8** ptr) const;

    /// Asks the OS to start reading [offset, offset + len) ahead of use. Returns at once.
    void Prefetch(u64 offset, u64 len) const;

    template <typename T>
    size_t Read(T& data) const {
        if constexpr (IsContiguousContainer<T>) {
            using ContiguousType = typename T::value_type;
            static_assert(std::is_trivially_copyable_v<ContiguousType>,
                          "Data type must be trivially copyable.");
            return ReadSpan<ContiguousType>(data);
        } else {
            return ReadObject(data) ? 1 : 0;
        }
    }

    template <typename T>
    size_t Write(const T& data) const {
        if constexpr (IsContiguousContainer<T>) {
            using ContiguousType = typename T::value_type;
            static_assert(std::is_trivially_copyable_v<ContiguousType>,
                          "Data type must be trivially copyable.");
            return WriteSpan<ContiguousType>(data);
        } else {
            static_assert(std::is_trivially_copyable_v<T>, "Data type must be trivially copyable.");
            return WriteObject(data) ? 1 : 0;
        }
    }

    template <typename T>
    size_t ReadSpan(std::span<T> data) const {
        static_assert(std::is_trivially_copyable_v<T>, "Data type must be trivially copyable.");
        return ReadRaw<T>(data.data(), data.size());
    }

    /// Reads @count elements of T into @data; returns the whole elements read.
    template <typename T>
    size_t ReadRaw(void* data, size_t count) const {
        return ReadBytes(data, count * sizeof(T)) / sizeof(T);
    }

    template <typename T>
    size_t WriteSpan(std::span<const T> data) const {
        static_assert(std::is_trivially_copyable_v<T>, "Data type must be trivially copyable.");
        return WriteRaw<T>(data.data(), data.size());
    }

    template <typename T>
    bool ReadObject(T& object) const {
        static_assert(std::is_trivially_copyable_v<T>, "Data type must be trivially copyable.");
        static_assert(!std::is_pointer_v<T>, "T must not be a pointer to an object.");
        return ReadBytes(&object, sizeof(T)) == sizeof(T);
    }

    /// Writes @count elements of T from @data; returns the whole elements written.
    template <typename T>
    size_t WriteRaw(const void* data, size_t count) const {
        return WriteBytes(data, count * sizeof(T)) / sizeof(T);
    }

    template <typename T>
    bool WriteObject(const T& object) const {
        static_assert(std::is_trivially_copyable_v<T>, "Data type must be trivially copyable.");
        static_assert(!std::is_pointer_v<T>, "T must not be a pointer to an object.");
        return WriteBytes(&object, sizeof(T)) == sizeof(T);
    }

    size_t WriteString(std::span<const char> string) const {
        return WriteSpan(string);
    }

    /// Writes @data to a new file at @path in one go; returns the elements written.
    static size_t WriteBytes(const std::filesystem::path& path, const auto& data) {
        IOFile out(path, FileAccessMode::Create, FileHint::Sequential);
        return out.Write(data);
    }

    RFILE* Handle() const {
        return file;
    }

private:
    size_t ReadBytes(void* data, size_t size) const;
    size_t WriteBytes(const void* data, size_t size) const;

    RFILE* file = nullptr;
    std::filesystem::path file_path;
    FileAccessMode file_access_mode{};
    uintptr_t file_mapping = 0;
};

/// A file's bytes, lent where they lie: a read-only mapping of a host file, or, for a source
/// that cannot lend (an entry decompressed out of an archive), the buffer it was decoded into.
class FileView {
public:
    FileView() = default;
    /// Bytes with no file behind them, kept by the view.
    explicit FileView(std::vector<u8>&& decoded) : owned{std::move(decoded)}, bytes{owned} {}

    FileView(FileView&& other) noexcept;
    FileView& operator=(FileView&& other) noexcept;
    FileView(const FileView&) = delete;
    FileView& operator=(const FileView&) = delete;

    /// Maps the file at @path; nothing when it cannot be opened or mapped.
    static std::optional<FileView> Map(const std::filesystem::path& path);

    /// Lends @bytes, which must outlive the view (embedded resources, data kept for the session).
    static FileView Borrow(std::span<const u8> bytes) {
        FileView view;
        view.bytes = bytes;
        return view;
    }

    std::span<const u8> Bytes() const {
        return bytes;
    }
    const u8* data() const {
        return bytes.data();
    }
    size_t size() const {
        return bytes.size();
    }
    bool empty() const {
        return bytes.empty();
    }
    operator std::span<const u8>() const {
        return bytes;
    }

private:
    IOFile file;
    std::vector<u8> owned;
    std::span<const u8> bytes;
};

// Files and directories through libretro-common (file/file_path.h, retro_dirent.h)

/// True when there is something at @path.
bool Exists(const std::filesystem::path& path);
bool IsDirectory(const std::filesystem::path& path);
/// True when @path is there and is not a directory.
bool IsFile(const std::filesystem::path& path);
/// Makes the directory @path and any of its parents that are missing; true when it is there.
bool CreateDirectories(const std::filesystem::path& path);
/// The size of the file @path in bytes, or -1.
s64 FileSize(const std::filesystem::path& path);
/// True when the directory @dir holds nothing (files whose names start with a dot not counted).
bool IsEmptyDirectory(const std::filesystem::path& dir);
/// The bytes free for the caller on the volume holding @path, or -1.
s64 FreeSpace(const std::filesystem::path& path);
/// Removes the file @path; true when it was removed.
bool Remove(const std::filesystem::path& path);
/// Removes @path: a file, or a directory with everything in it, without following links.
/// True when nothing is left there, including when there was nothing to begin with.
bool RemoveAll(const std::filesystem::path& path);
/// Copies the file or directory @from to @to, making directories as needed and replacing
/// files already there. Each file is cloned where the file system can.
bool CopyAll(const std::filesystem::path& from, const std::filesystem::path& to);
/// Moves @from to @to, replacing a file at @to.
bool Rename(const std::filesystem::path& from, const std::filesystem::path& to);
/// Calls @func with the name of each entry of the directory @dir, hidden ones included and
/// "." and ".." left out, and whether it is a directory. False when @dir cannot be listed.
bool ForEachEntry(const std::filesystem::path& dir,
                  const std::function<void(std::string_view name, bool is_dir)>& func);

u64 GetDirectorySize(const std::filesystem::path& path);

} // namespace Common::FS
