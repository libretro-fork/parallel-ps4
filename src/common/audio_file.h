// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <filesystem>
#include <vector>

#include "common/types.h"

// Whole audio files decoded to 16-bit PCM with libretro-common's decoders (formats/rmp3.h,
// formats/rflac.h), for sound that is not the console's own: the music player and the trophy
// sound.
namespace Common::AudioFile {

struct Pcm {
    std::vector<s16> samples; // interleaved
    u32 channels = 0;
    u32 hz = 0;

    u64 Frames() const {
        return channels ? samples.size() / channels : 0;
    }
};

// The file's bytes, read in one go; empty when it cannot be read.
std::vector<u8> ReadWhole(const std::filesystem::path& path);

// Decodes a whole MP3 or FLAC file held in memory, read in place. False when it is not one.
bool DecodeMp3(const u8* data, size_t size, Pcm& out);
bool DecodeFlac(const u8* data, size_t size, Pcm& out);

} // namespace Common::AudioFile
