// SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/io_file.h"
#include "common/path_util.h"
#include "common/singleton.h"
#include "common/types.h"

#include <functional>
#include <thread>
#include <vector>

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
        return opened;
    }

    bool Save(BlobType type, const std::string& name, std::vector<u8>&& data);
    bool Save(BlobType type, const std::string& name, std::vector<u32>&& data);

    /// The blob, mapped where it lies; nothing when there is none.
    std::optional<Common::FS::FileView> Load(BlobType type, const std::string& name);

    /// Calls @func with each blob of @type, lent until it returns.
    void ForEachBlob(BlobType type, const std::function<void(std::span<const u8> data)>& func);

private:
    std::jthread io_worker{};
    std::filesystem::path cache_path{};
    bool opened{};
};

} // namespace Storage
