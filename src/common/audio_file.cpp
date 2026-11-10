// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>

#include <formats/rflac.h>
#include <formats/rmp3.h>

#include "common/audio_file.h"
#include "common/io_file.h"

namespace Common::AudioFile {

std::vector<u8> ReadWhole(const std::filesystem::path& path) {
    std::vector<u8> bytes;
    Common::FS::IOFile file(path, Common::FS::FileAccessMode::Read);
    if (!file.IsOpen()) {
        return bytes;
    }
    bytes.resize(file.GetSize());
    if (file.ReadRaw<u8>(bytes.data(), bytes.size()) != bytes.size()) {
        bytes.clear();
    }
    return bytes;
}

bool DecodeMp3(const u8* data, size_t size, Pcm& out) {
    rmp3 mp3;
    if (!rmp3_init_memory(&mp3, data, size) || mp3.channels == 0 || mp3.sampleRate == 0) {
        return false;
    }
    out.channels = mp3.channels;
    out.hz = mp3.sampleRate;
    // Room for the length at 128 kbit/s, grown when the stream is longer: the frames are read
    // straight into it.
    u64 room = std::max<u64>(static_cast<u64>(size) * out.hz / 16000, 4096);
    u64 frames = 0;
    for (;;) {
        out.samples.resize(static_cast<size_t>(room * out.channels));
        const u64 got =
            rmp3_read_s16(&mp3, room - frames, out.samples.data() + frames * out.channels);
        frames += got;
        if (frames < room) {
            break;
        }
        room *= 2;
    }
    rmp3_uninit(&mp3);
    out.samples.resize(static_cast<size_t>(frames * out.channels));
    return frames > 0;
}

bool DecodeFlac(const u8* data, size_t size, Pcm& out) {
    rflac_t* flac = rflac_new();
    if (!flac) {
        return false;
    }
    rflac_set_in(flac, data, size);
    rflac_set_eof(flac);
    // The header first, with no output set: it gives the geometry and, usually, the length.
    while (!rflac_format(flac)) {
        if (rflac_process(flac, nullptr, nullptr) != RFLAC_PROCESS_NEXT) {
            rflac_free(flac);
            return false;
        }
    }
    const rflac_format_t* format = rflac_format(flac);
    out.channels = format->channels;
    out.hz = format->sample_rate;
    if (out.channels == 0 || out.hz == 0) {
        rflac_free(flac);
        return false;
    }
    const u64 total = rflac_total_frames(flac);
    u64 room = total ? total : std::max<u64>(static_cast<u64>(size) / (2 * out.channels), 4096);
    u64 frames = 0;
    int result = RFLAC_PROCESS_NEXT;
    unsigned idle = 0; // calls that gave nothing with room left: a stream that cannot go on
    while (result == RFLAC_PROCESS_NEXT && idle < 4) {
        if (frames == room) {
            room *= 2;
        }
        out.samples.resize(static_cast<size_t>(room * out.channels));
        rflac_set_out_s16(flac, out.samples.data() + frames * out.channels,
                          static_cast<size_t>(room - frames));
        size_t wrote = 0;
        result = rflac_process(flac, nullptr, &wrote);
        frames += wrote;
        idle = wrote ? 0 : idle + 1;
    }
    rflac_free(flac);
    out.samples.resize(static_cast<size_t>(frames * out.channels));
    return result != RFLAC_PROCESS_ERROR && frames > 0;
}

} // namespace Common::AudioFile
