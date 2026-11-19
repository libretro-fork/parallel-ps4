// SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/io_file.h"
#include "common/path_util.h"
#include "common/singleton.h"
#include "common/types.h"

#include <functional>
#include <string>
#include <vector>
#include <retro_atomic.h>

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

    /// Hands @data to the writer thread, which writes it from where it lies. Any thread;
    /// does not wait. False when the cache is not open.
    bool Save(BlobType type, std::string name, std::vector<u8>&& data);
    bool Save(BlobType type, std::string name, std::vector<u32>&& data);

    /// The blob, mapped where it lies; nothing when there is none.
    std::optional<Common::FS::FileView> Load(BlobType type, const std::string& name);

    /// @hook (nullptr: none) called by Close before it closes; it lives until it is cleared.
    void SetCloseHook(const CloseHook* hook);

    /// Calls @func with each blob of @type, lent until it returns.
    void ForEachBlob(BlobType type, const std::function<void(std::span<const u8> data)>& func);

private:
    static void ProcessIO(void* arg);
    bool Push(BlobType type, std::string&& name, std::vector<u8>&& bytes, std::vector<u32>&& words);

    sthread* io_worker{};
    std::filesystem::path cache_path{};
    retro_atomic_int_t opened{};
    retro_atomic_ptr_t close_hook{};
};

} // namespace Storage
