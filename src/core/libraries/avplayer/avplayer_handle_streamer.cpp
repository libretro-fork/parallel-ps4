// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>

#include "core/libraries/avplayer/avplayer_handle_streamer.h"

namespace Libraries::AvPlayer {

AvPlayerHandleStreamer::AvPlayerHandleStreamer(std::unique_ptr<Core::FileSys::IFile> handle)
    : m_handle(std::move(handle)) {}

AvPlayerHandleStreamer::~AvPlayerHandleStreamer() = default;

bool AvPlayerHandleStreamer::Init(std::string_view /*path*/) {
    if (!m_handle || !m_handle->IsOpen()) {
        return false;
    }
    m_file_size = m_handle->Size();
    return true;
}

u64 AvPlayerHandleStreamer::ReadAt(u64 offset, u8* dst, u64 size) {
    if (offset >= m_file_size) {
        return 0;
    }
    m_handle->Seek(static_cast<s64>(offset), Common::FS::SeekOrigin::SetOrigin);
    const s64 got = m_handle->Read(dst, std::min(size, m_file_size - offset));
    return got > 0 ? static_cast<u64>(got) : 0;
}

} // namespace Libraries::AvPlayer
