// SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <retro_atomic.h>
#include <retro_miscellaneous.h>

#include "common/concepts.h"
#include "common/io_file.h"
#include "common/singleton.h"
#include "common/types.h"

#include "video_core/blob_pack.h"

struct sthread;

namespace Storage {

enum class BlobType : u32 {
    ShaderMeta,
    ShaderBinary,
    PipelineKey,
    ShaderProfile,
    DriverCache, // the driver's own pipeline cache (vkGetPipelineCacheData)
};

/// What Close calls first, while the cache still takes blobs: the last word of one that keeps
/// what it saves in memory until then
struct CloseHook {
    void (*func)(void* arg);
    void* arg;
};

class DataBase {
public:
    static DataBase& Instance() {
        return *Common::Singleton<DataBase>::Instance();
    }

    void Open();
    void Close();
    /// Removes every blob of the game's cache. Before anything is saved: not while the
    /// writer has blobs to write.
    void Clear();
    [[nodiscard]] bool IsOpened() const {
        return (retro_atomic_load_acquire_int(&opened) != 0);
    }

    /// The longest blob name, its terminator not counted
    static constexpr size_t MaxNameLength = 63;

    /// Hands the @size bytes at @data, made with malloc, to the writer thread, which writes
    /// them from where they lie and frees them. Any thread; does not wait. False when the
    /// cache is not open (@data is freed then too).
    bool Save(BlobType type, const char* name, void* data, size_t size);
    /// The same with a copy of @data, which stays the caller's
    bool SaveCopy(BlobType type, const char* name, const void* data, size_t size);

    /// The blob where it lies, as the cache was when it opened; empty when there is none.
    /// Lent from the pack's mapping, which outlives Close (a check on the compiler thread may
    /// still read one as the cache closes) and goes at the next Open. The driver's cache is
    /// lent from a mapping of its own file, kept until the next Load of it.
    Common::Span<const u8> Load(BlobType type, const char* name);

    /// @hook (nullptr: none) called by Close before it closes; it lives until it is cleared.
    void SetCloseHook(const CloseHook* hook);

    /// Calls @func with @user and each blob of @type the cache had when it opened, lent until
    /// it returns.
    void ForEachBlob(BlobType type, void (*func)(void* user, Common::Span<const u8> data),
                     void* user);

private:
    static void ProcessIO(void* arg);
    /// Maps the pack and indexes it: a new one when there is none (or it is not one); written
    /// again with its live records only when it ends in a record cut short or holds many dead
    void OpenPack();
    void ReleasePack();
    bool WriteNewPack() const;

    sthread* io_worker{};
    char cache_path[PATH_MAX_LENGTH]{};
    /// Every blob but the driver's cache, in one file (blob_pack.h): appended to by the writer,
    /// read through the mapping made as the cache opened
    char pack_path[PATH_MAX_LENGTH]{};
    Common::FS::IOFile pack_file{};
    /// The driver's cache as last loaded, mapped
    Common::FS::IOFile driver_file{};
    blob_pack_index_t pack_index{};
    retro_atomic_int_t opened{};
    retro_atomic_ptr_t close_hook{};
};

} // namespace Storage
