// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>

#include "core/libraries/avplayer/avplayer_file_streamer.h"

namespace Libraries::AvPlayer {

AvPlayerFileStreamer::AvPlayerFileStreamer(const AvPlayerFileReplacement& file_replacement)
    : m_file_replacement(file_replacement) {}

AvPlayerFileStreamer::~AvPlayerFileStreamer() {
    if (m_file_replacement.close != nullptr && m_fd >= 0) {
        m_file_replacement.close(m_file_replacement.object_ptr);
    }
}

bool AvPlayerFileStreamer::Init(std::string_view path) {
    const auto ptr = m_file_replacement.object_ptr;
    m_fd = m_file_replacement.open(ptr, path.data());
    if (m_fd < 0) {
        return false;
    }
    m_file_size = m_file_replacement.size(ptr);
    return true;
}

u64 AvPlayerFileStreamer::ReadAt(u64 offset, u8* dst, u64 size) {
    u64 done = 0;
    while (done < size && offset + done < m_file_size) {
        // The callback takes a 32-bit length
        const u32 chunk = static_cast<u32>(
            std::min<u64>({size - done, m_file_size - offset - done, u64{1} << 30}));
        const s32 got = m_file_replacement.read_offset(m_file_replacement.object_ptr, dst + done,
                                                       offset + done, chunk);
        if (got <= 0) {
            break;
        }
        done += static_cast<u64>(got);
    }
    return done;
}

} // namespace Libraries::AvPlayer
