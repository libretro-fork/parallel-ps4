// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string_view>
#include "core/libraries/avplayer/avplayer.h"
#include "core/libraries/avplayer/avplayer_data_streamer.h"

namespace Libraries::AvPlayer {

// Reads through the game's file replacement callbacks.
class AvPlayerFileStreamer : public IDataStreamer {
public:
    AvPlayerFileStreamer(const AvPlayerFileReplacement& file_replacement);
    ~AvPlayerFileStreamer();

    bool Init(std::string_view path) override;
    u64 Size() const override {
        return m_file_size;
    }
    u64 ReadAt(u64 offset, u8* dst, u64 size) override;

private:
    AvPlayerFileReplacement m_file_replacement;

    int m_fd = -1;
    u64 m_file_size{};
};

} // namespace Libraries::AvPlayer
