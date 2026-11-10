// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include "core/file_sys/ifile.h"
#include "core/libraries/avplayer/avplayer_data_streamer.h"

namespace Libraries::AvPlayer {

// Reads through a file system backend's handle that has no host path.
class AvPlayerHandleStreamer final : public IDataStreamer {
public:
    explicit AvPlayerHandleStreamer(std::unique_ptr<Core::FileSys::IFile> handle);
    ~AvPlayerHandleStreamer() override;
    bool Init(std::string_view path) override;
    u64 Size() const override {
        return m_file_size;
    }
    u64 ReadAt(u64 offset, u8* dst, u64 size) override;

private:
    std::unique_ptr<Core::FileSys::IFile> m_handle;
    u64 m_file_size{0};
};

} // namespace Libraries::AvPlayer
