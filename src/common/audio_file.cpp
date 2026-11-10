// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>

#include <formats/raac.h>
#include <formats/rflac.h>
#include <formats/rmp3.h>
#include <formats/rmp4_audio.h>

#include "common/audio_file.h"

namespace Common::AudioFile {

namespace {
// Room for `frames` frames, the decoded ones kept; realloc can usually grow in place.
bool Reserve(Pcm& out, u64 frames) {
    void* p = std::realloc(out.samples.get(), static_cast<size_t>(frames * out.channels) * 2);
    if (!p) {
        return false;
    }
    out.samples.release();
    out.samples.reset(static_cast<s16*>(p));
    return true;
}

// Gives back what was reserved past the decoded frames.
bool Finish(Pcm& out) {
    if (out.frames == 0) {
        out.samples.reset();
        return false;
    }
    Reserve(out, out.frames);
    return true;
}
} // namespace

bool DecodeMp3(const u8* data, size_t size, Pcm& out) {
    rmp3 mp3;
    if (!rmp3_init_memory(&mp3, data, size) || mp3.channels == 0 || mp3.sampleRate == 0) {
        return false;
    }
    out.channels = mp3.channels;
    out.hz = mp3.sampleRate;
    out.frames = 0;
    // The tag frame gives the length and rmp3 trims the encoder's delay and padding, so the
    // track is gapless. Without it, room for the length at 128 kbit/s, grown when the stream is
    // longer. The frames are read straight into it.
    u64 room = mp3.totalFrames ? mp3.totalFrames + 1
                               : std::max<u64>(static_cast<u64>(size) * out.hz / 16000, 4096);
    for (;;) {
        if (!Reserve(out, room)) {
            break;
        }
        out.frames +=
            rmp3_read_s16(&mp3, room - out.frames, out.samples.get() + out.frames * out.channels);
        if (out.frames < room) {
            break;
        }
        room *= 2;
    }
    rmp3_uninit(&mp3);
    return Finish(out);
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
    out.frames = 0;
    if (out.channels == 0 || out.hz == 0) {
        rflac_free(flac);
        return false;
    }
    const u64 total = rflac_total_frames(flac);
    u64 room = total ? total : std::max<u64>(static_cast<u64>(size) / (2 * out.channels), 4096);
    int result = RFLAC_PROCESS_NEXT;
    unsigned idle = 0; // calls that gave nothing with room left: a stream that cannot go on
    bool reserved = Reserve(out, room);
    while (reserved && result == RFLAC_PROCESS_NEXT && idle < 4) {
        if (out.frames == room) {
            room *= 2;
            if (!(reserved = Reserve(out, room))) {
                break;
            }
        }
        rflac_set_out_s16(flac, out.samples.get() + out.frames * out.channels,
                          static_cast<size_t>(room - out.frames));
        size_t wrote = 0;
        result = rflac_process(flac, nullptr, &wrote);
        out.frames += wrote;
        idle = wrote ? 0 : idle + 1;
    }
    rflac_free(flac);
    return Finish(out) && result != RFLAC_PROCESS_ERROR;
}

bool DecodeAdts(const u8* data, size_t size, Pcm& out) {
    raac_adts_t header;
    if (raac_adts_parse(data, size, &header) < 0) {
        return false;
    }
    raac_t* aac = raac_open(header.asc, sizeof(header.asc));
    if (!aac) {
        return false;
    }
    out.channels = raac_channels(aac);
    out.hz = raac_sample_rate(aac);
    out.frames = 0;
    const u64 frame_len = raac_frame_len(aac);
    // Room for the length at 128 kbit/s, grown when the stream is longer
    u64 room = std::max<u64>(static_cast<u64>(size) * out.hz / 16000, 4 * frame_len);
    bool ok = Reserve(out, room);
    size_t pos = 0;
    while (ok && pos + 7 <= size && raac_adts_parse(data + pos, size - pos, &header) == 0 &&
           header.frame_size <= size - pos) {
        // Each raw_data_block of the frame is an access unit, CRC words between them
        size_t at = pos + header.header_size;
        const size_t end = pos + header.frame_size;
        for (unsigned block = 0; block < header.raw_blocks && at < end; ++block) {
            if (out.frames + frame_len > room) {
                room *= 2;
                if (!(ok = Reserve(out, room))) {
                    break;
                }
            }
            const int got = raac_decode_s16(aac, data + at, end - at,
                                            out.samples.get() + out.frames * out.channels);
            if (got > 0) {
                out.frames += static_cast<u64>(got);
            }
            const size_t used = raac_last_size(aac);
            if (got < 0 || used == 0) {
                break;
            }
            at += used + (header.header_size == 9 && block + 1 < header.raw_blocks ? 2 : 0);
        }
        pos = end;
    }
    raac_close(aac);
    return Finish(out);
}

bool DecodeMp4(const u8* data, size_t size, Pcm& out) {
    int16_t* pcm = nullptr;
    size_t frames = 0;
    unsigned rate = 0;
    unsigned channels = 0;
    if (!rmp4_audio_decode(data, size, 0, &pcm, &frames, &rate, &channels)) {
        return false;
    }
    // rmp4's buffer is kept as it is
    out.samples.reset(pcm);
    out.frames = frames;
    out.channels = channels;
    out.hz = rate;
    return frames > 0 && channels > 0 && rate > 0;
}

} // namespace Common::AudioFile
