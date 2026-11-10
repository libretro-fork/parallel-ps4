// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string_view>

#include "common/types.h"

namespace Libraries::AvPlayer {

// A media file the demuxer reads by offset, for files that are not mapped.
class IDataStreamer {
public:
    virtual ~IDataStreamer() = default;
    virtual bool Init(std::string_view path) = 0;
    virtual u64 Size() const = 0;
    // Bytes read into dst; fewer at the end of the file or on an error.
    virtual u64 ReadAt(u64 offset, u8* dst, u64 size) = 0;
};

} // namespace Libraries::AvPlayer
