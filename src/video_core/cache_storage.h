// SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/io_file.h"
#include "common/path_util.h"
#include "common/singleton.h"
#include "common/types.h"

#include <atomic>
#include <functional>
#include <string>
#include <vector>

struct sthread;

namespace Storage {

enum class BlobType : u32 {
    ShaderMeta,
    ShaderBinary,
    PipelineKey,
    ShaderProfile,
};

class DataBase {
public:
    static DataBase& Instance() {
        return *Common::Singleton<DataBase>::Instance();
    }

    void Open();
    void Close();
    [[nodiscard]] bool IsOpened() const {
        return opened.load(std::memory_order_acquire);
    }

    /// Hands @data to the writer thread, which writes it from where it lies. Any thread;
    /// does not wait. False when the cache is not open.
    bool Save(BlobType type, std::string name, std::vector<u8>&& data);
    bool Save(BlobType type, std::string name, std::vector<u32>&& data);

    /// The blob, mapped where it lies; nothing when there is none.
    std::optional<Common::FS::FileView> Load(BlobType type, const std::string& name);

    /// Calls @func with each blob of @type, lent until it returns.
    void ForEachBlob(BlobType type, const std::function<void(std::span<const u8> data)>& func);

private:
    static void ProcessIO(void* arg);
    bool Push(BlobType type, std::string&& name, std::vector<u8>&& bytes, std::vector<u32>&& words);

    sthread* io_worker{};
    std::filesystem::path cache_path{};
    std::atomic<bool> opened{};
};

} // namespace Storage
