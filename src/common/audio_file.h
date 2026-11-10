// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstdlib>
#include <filesystem>
#include <memory>
#include <vector>

#include "common/types.h"

// Whole audio files decoded to 16-bit PCM with libretro-common's decoders (formats/rmp3.h,
// formats/rflac.h, formats/raac.h, formats/rmp4_audio.h), for sound that is not the console's
// own: the music player and the trophy sound.
namespace Common::AudioFile {

struct Pcm {
    struct Free {
        void operator()(s16* p) const {
            std::free(p);
        }
    };
    // Interleaved, malloc'd: a decoder's own buffer is taken over as it is.
    std::unique_ptr<s16[], Free> samples;
    u64 frames = 0;
    u32 channels = 0;
    u32 hz = 0;

    const s16* Data() const {
        return samples.get();
    }
    u64 Frames() const {
        return frames;
    }
};

// The file's bytes, read in one go; empty when it cannot be read.
std::vector<u8> ReadWhole(const std::filesystem::path& path);

// Decodes a whole MP3, FLAC, ADTS AAC or MP4/M4A (AAC) file held in memory, read in place.
// False when it is not one.
bool DecodeMp3(const u8* data, size_t size, Pcm& out);
bool DecodeFlac(const u8* data, size_t size, Pcm& out);
bool DecodeAdts(const u8* data, size_t size, Pcm& out);
bool DecodeMp4(const u8* data, size_t size, Pcm& out);

} // namespace Common::AudioFile
