// SPDX-FileCopyrightText: Copyright 2025-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <retro_atomic.h>
#include "common/combiner.h"
#include "common/io_file.h"
#include "common/string_util.h"
#include "core/file_sys/devices/base_device.h"
#include "core/file_sys/directories/base_directory.h"
#include "core/file_sys/ifile.h"

namespace Core::FileSys {

/// Builds the path of an overlay that sits next to a game
[[nodiscard]] std::filesystem::path OverlayPath(const std::filesystem::path& base,
                                                std::string_view suffix);

/// Inverse of OverlayPath.
[[nodiscard]] std::optional<std::filesystem::path> BaseGameFromOverlay(
    const std::filesystem::path& path);

[[nodiscard]] std::optional<std::filesystem::path> ResolveGameRoot(
    const std::filesystem::path& root);

#ifdef _WIN64
inline constexpr bool NeedsCaseInsensitiveSearch = false;
#else
inline constexpr bool NeedsCaseInsensitiveSearch = true;
#endif

class MntPoints {
public:
    static bool ignore_game_patches;
    // The libretro core runs a game straight from its PKG: the update's PKG it found beside
    // the game's, laid over /app0 in the place of a "-UPDATE" folder.
    static std::filesystem::path libretro_update;
    // And the additional content's PKGs beside it, in addition to the addcont folder.
    static std::vector<std::filesystem::path> libretro_addcont;
    struct MntPair {
        std::filesystem::path host_path;
        std::string mount; // e.g /app0
        bool read_only;
        std::vector<std::shared_ptr<IBackend>> backends;
        // The overlays over the mount, looked up once as it is made rather than at every path:
        // the update ("-UPDATE", else "-patch", or the PKG's update) and "-mods", and whether
        // there is one at all
        std::filesystem::path patch_root{};
        std::filesystem::path mods_root{};
        bool has_patch{false};
        bool has_mods{false};
    };

    enum class HostPathType {
        Default, // Prioritizes Mod, then patch, then base
        Base,
        Patch,
        Mod
    };

    explicit MntPoints() = default;
    ~MntPoints() = default;

    std::shared_ptr<IBackend> CreateBackend(const std::filesystem::path& host_path, bool read_only);

    void Mount(const std::filesystem::path& host_folder, const std::string& guest_folder,
               bool read_only = false);
    void Unmount(const std::string& guest_folder);
    void UnmountAll();

    std::filesystem::path GetHostPath(std::string_view guest_directory,
                                      bool* is_read_only = nullptr,
                                      HostPathType host_path = HostPathType::Default);
    using IterateDirectoryCallback =
        std::function<void(const std::filesystem::path& host_path, bool is_file)>;
    void IterateDirectory(std::string_view guest_directory,
                          const IterateDirectoryCallback& callback);

    /// Returns true if the guest path exists in any backend of the
    /// mount stack. Mirrors fs::exists() on the resolved host path.
    bool Exists(std::string_view guest_path);

    /// Returns true if the guest path resolves to a directory in any
    /// backend of the mount stack.
    bool IsDirectory(std::string_view guest_path);
    /// What the guest path is in the mount's stack - the first backend that has it decides -
    /// from one look per backend, where Exists and IsDirectory each take one
    PathKind Kind(std::string_view guest_path);
    bool Stat(std::string_view guest_path, FileStat& out);

    /// Opens the guest path through the mount's backend stack. Returns
    /// nullptr when the path does not exist or the caller requested
    /// writable access on a read-only mount.
    std::unique_ptr<IFile> Open(std::string_view guest_path, bool writable = false);
    // Open with an explicit host access mode.
    std::unique_ptr<IFile> Open(std::string_view guest_path, Common::FS::FileAccessMode mode);

    /// Opens a directory through the mount's backend stack.
    std::unique_ptr<IDirectory> OpenDir(std::string_view guest_path);

    /// open + read the whole file as bytes. Returns nullopt
    /// when the file does not exist or is unreadable.
    /// The whole file, lent where it lies (mapped), or decoded out of an archive.
    std::optional<Common::FS::FileView> ReadFile(std::string_view guest_path);

    /// Normalizes and validates a guest path (resolves dot and dot-dot components).
    static std::optional<std::string> SanitizeGuestPath(std::string_view path);

    const MntPair* GetMountFromHostPath(const std::string& host_path) {
        return m_mutex.Run([&]() -> const MntPair* {
            const auto it = std::ranges::find_if(m_mnt_pairs, [&](const MntPair& mount) {
                return host_path.starts_with(Common::U8stringToString(mount.host_path.u8string()));
            });
            return it == m_mnt_pairs.end() ? nullptr : &*it;
        });
    }

    const MntPair* GetMount(std::string_view guest_path) {
        std::string normalized;
        if (guest_path.find("..") != std::string_view::npos ||
            guest_path.find("//") != std::string_view::npos) {
            normalized = std::filesystem::path(guest_path).lexically_normal().generic_string();
            guest_path = normalized;
        }
        return m_mutex.Run([&]() -> const MntPair* {
            const auto it = std::ranges::find_if(m_mnt_pairs, [&](const auto& mount) {
                // The mount itself, or a path under it: its name and then a slash, so as not to
                // match only part of the mount's name - compared in place, nothing built
                const std::string_view name{mount.mount};
                return guest_path == name ||
                       (guest_path.size() > name.size() && guest_path.starts_with(name) &&
                        guest_path[name.size()] == '/');
            });
            if (it == m_mnt_pairs.end()) {
                return nullptr;
            }
            return &*it;
        });
    }

private:
    std::vector<MntPair> m_mnt_pairs;
    std::vector<std::filesystem::path> path_parts;
    std::unordered_map<std::filesystem::path, std::filesystem::path> path_cache;
    Common::Combiner m_mutex; // what it guards changes one call at a time here
};

enum class FileType {
    Regular, // standard file
    Directory,
    Device,
    Socket,
    Equeue
};

struct File {
    retro_atomic_int_t is_opened{};
    retro_atomic_int_t type{static_cast<int>(FileType::Regular)};
    std::filesystem::path m_host_name;
    std::string m_guest_name;
    std::unique_ptr<IFile> handle;
    Common::Combiner m_mutex; // what it guards changes one call at a time here
    std::shared_ptr<Directories::BaseDirectory> directory; // only valid for type == Directory
    std::shared_ptr<Devices::BaseDevice> device;           // only valid for type == Device

    bool IsBackendOpen() const {
        return handle && handle->IsOpen();
    }

    s64 Read(void* dst, u64 size) {
        return handle ? handle->Read(dst, size) : -1;
    }

    s64 ReadAt(void* dst, u64 size, u64 offset) {
        return handle ? handle->ReadAt(dst, size, offset) : -1;
    }

    s64 Write(const void* src, u64 size) {
        return handle ? handle->Write(src, size) : -1;
    }

    bool Seek(s64 offset, Common::FS::SeekOrigin origin = Common::FS::SeekOrigin::SetOrigin) {
        return handle ? handle->Seek(offset, origin) : false;
    }

    s64 Tell() const {
        return handle ? static_cast<s64>(handle->Tell()) : -1;
    }

    u64 GetSize() const {
        return handle ? handle->Size() : 0;
    }

    bool Flush() {
        return handle ? handle->Flush() : false;
    }

    bool IsReadOnly() const {
        return handle ? handle->IsReadOnly() : false;
    }

    bool IsWriteOnly() const {
        return handle ? handle->IsWriteOnly() : false;
    }

    Common::FS::IOFile* GetHostFile() const {
        return handle ? handle->GetHostFile() : nullptr;
    }

    bool ReadsInPlace() const {
        return handle && handle->ReadsInPlace();
    }
};

class HandleTable {
public:
    HandleTable() {
        // Room for more descriptors than a game keeps open, so that neither list grows mid-game
        m_files.reserve(1024);
        m_free.reserve(1024);
    }
    virtual ~HandleTable() = default;

    int CreateHandle();
    void DeleteHandle(int d);
    File* GetFile(int d);
    File* GetSocket(int d);
    std::vector<int> GetSocketHandles();
    File* GetFile(const std::filesystem::path& host_name);
    int GetFileDescriptor(File* file);

    void CreateStdHandles();
    void FlushAll();

private:
    std::vector<File*> m_files;
    // Closed files, emptied and kept for the next open: a game opening and closing files as it
    // streams its levels gets one back instead of a new one
    std::vector<File*> m_free;
    Common::Combiner m_mutex; // what it guards changes one call at a time here
};

} // namespace Core::FileSys
