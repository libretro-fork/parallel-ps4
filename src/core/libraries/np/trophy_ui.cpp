// SPDX-FileCopyrightText: Copyright 2025-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>
#include <fmt/format.h>
#include <formats/rmp3.h>

#define MINIMP3_IMPLEMENTATION

#include "common/embedded_resources.h"
#include "common/io_file.h"
#include "common/logging/formatter.h"
#include "common/logging/log.h"
#include "common/path_util.h"
#include "core/emulator_settings.h"
#include "core/libraries/np/trophy_ui.h"
#include "libretro/audio_mix.h"
#include "video_core/overlay/ovl_layers.h"

namespace fs = std::filesystem;
namespace Libraries::Np::NpTrophy {

// The trophy sound, as the core's mix plays it: stereo at 48 kHz in the mix's format, made once
// when the emulator starts and borrowed by the mix each time a trophy is unlocked.
static std::vector<float> trophy_sound;

// 16-bit PCM in a RIFF/WAVE file, 1 or 2 channels: where its samples are, read in place.
static bool ParseWav(const u8* data, size_t size, const s16** samples, u64* frames, u32* channels,
                     u32* hz) {
    if (size < 12 || std::memcmp(data, "RIFF", 4) != 0 || std::memcmp(data + 8, "WAVE", 4) != 0) {
        return false;
    }
    const auto le16 = [](const u8* p) { return static_cast<u32>(p[0] | (p[1] << 8)); };
    const auto le32 = [](const u8* p) {
        return static_cast<u32>(p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<u32>(p[3]) << 24));
    };
    bool have_fmt = false;
    for (size_t at = 12; at + 8 <= size;) {
        const u32 chunk = le32(data + at + 4);
        const u8* body = data + at + 8;
        if (chunk > size - at - 8) {
            return false;
        }
        if (std::memcmp(data + at, "fmt ", 4) == 0 && chunk >= 16) {
            if (le16(body) != 1 || le16(body + 14) != 16) {
                return false; // only 16-bit PCM
            }
            *channels = le16(body + 2);
            *hz = le32(body + 4);
            have_fmt = *channels == 1 || *channels == 2;
        } else if (std::memcmp(data + at, "data", 4) == 0 && have_fmt && *hz > 0) {
            *samples = reinterpret_cast<const s16*>(body);
            *frames = chunk / (2 * *channels);
            return true;
        }
        at += 8 + chunk + (chunk & 1);
    }
    return false;
}

// A 16-bit file (a WAV) or a decoder's float output, resampled into the sound once
template <typename Sample>
static void ResampleTrophySound(const Sample* samples, u64 frames, u32 channels, u32 hz) {
    audio_mix_resampler_t r;
    audio_mix_resampler_init(&r, hz, channels);
    trophy_sound.resize(static_cast<size_t>(((frames * 48000) / hz + 1) * 2));
    u64 pos = 0;
    const unsigned room = static_cast<unsigned>(trophy_sound.size() / 2);
    unsigned out;
    if constexpr (std::is_same_v<Sample, float>) {
        out = audio_mix_resample_f32(&r, samples, frames, &pos, trophy_sound.data(), room, 1.0f);
    } else {
        out = audio_mix_resample(&r, samples, frames, &pos, trophy_sound.data(), room, 1.0f);
    }
    trophy_sound.resize(static_cast<size_t>(out) * 2);
}

// An MP3 decoded to float, as rmp3 makes it, so that the sound is rounded to 16 bits once, in
// the mix, and not first here as well
static bool DecodeMp3F32(const u8* data, size_t size, std::vector<float>& pcm, u32* channels,
                         u32* hz) {
    rmp3 mp3;
    if (!rmp3_init_memory(&mp3, data, size)) {
        return false;
    }
    *channels = mp3.channels;
    *hz = mp3.sampleRate;
    bool ok = (*channels == 1 || *channels == 2) && *hz > 0;
    if (ok) {
        // Short: the length at 128 kbit/s, grown while the stream goes on
        u64 room = mp3.totalFrames ? mp3.totalFrames + 1
                                   : std::max<u64>(static_cast<u64>(size) * *hz / 16000, 4096);
        u64 frames = 0;
        for (;;) {
            pcm.resize(static_cast<size_t>(room * *channels));
            frames += rmp3_read_f32(&mp3, room - frames, pcm.data() + frames * *channels);
            if (frames < room) {
                break;
            }
            room *= 2;
        }
        pcm.resize(static_cast<size_t>(frames * *channels));
        ok = frames > 0;
    }
    rmp3_uninit(&mp3);
    return ok;
}

static void PreloadTrophySoundInto();

void PreloadTrophySound() {
    PreloadTrophySoundInto();
    // Lent to the overlay, which plays it as a trophy shows
    ovl_trophy_set_sound(trophy_sound.data(), static_cast<unsigned>(trophy_sound.size() / 2));
}

static void PreloadTrophySoundInto() {
    trophy_sound.clear();
    const auto dir = Common::FS::GetUserPath(Common::FS::PathType::CustomTrophy);
    const auto mp3_path = dir / "trophy.mp3";
    const auto wav_path = dir / "trophy.wav";
    if (Common::FS::Exists(mp3_path)) {
        const auto bytes = Common::FS::FileView::Map(mp3_path);
        std::vector<float> pcm;
        u32 channels = 0;
        u32 hz = 0;
        if (bytes && DecodeMp3F32(bytes->data(), bytes->size(), pcm, &channels, &hz)) {
            ResampleTrophySound(pcm.data(), pcm.size() / channels, channels, hz);
        } else {
            LOG_ERROR(Lib_NpTrophy, "Could not decode the custom trophy sound {}",
                      fmt::UTF(mp3_path.u8string()));
        }
        return;
    }
    const s16* samples = nullptr;
    u64 frames = 0;
    u32 channels = 0;
    u32 hz = 0;
    if (Common::FS::Exists(wav_path)) {
        const auto bytes = Common::FS::FileView::Map(wav_path);
        if (bytes && ParseWav(bytes->data(), bytes->size(), &samples, &frames, &channels, &hz)) {
            ResampleTrophySound(samples, frames, channels, hz);
        } else {
            LOG_ERROR(Lib_NpTrophy, "The custom trophy sound {} is not 16-bit PCM",
                      fmt::UTF(wav_path.u8string()));
        }
        return;
    }
    const auto file = Common::GetEmbeddedResource("src/resources/trophy.wav");
    if (ParseWav(file.data(), file.size(), &samples, &frames, &channels, &hz)) {
        ResampleTrophySound(samples, frames, channels, hz);
    }
}
void AddTrophyToQueue(const std::filesystem::path& trophyIconPath, const std::string& trophyName,
                      const std::string_view& rarity, const bool after) {
    if (emu_settings_get_trophy_popup_disabled()) {
        return;
    }
    // The trophy's icon, mapped where it lies; the type's, the user's own in the custom trophy
    // folder or the core's
    std::optional<Common::FS::FileView> icon;
    if (Common::FS::Exists(trophyIconPath)) {
        icon = Common::FS::FileView::Map(trophyIconPath);
    }
    if (!icon) {
        LOG_ERROR(Lib_NpTrophy, "Couldnt load trophy icon at {}",
                  fmt::UTF(trophyIconPath.u8string()));
    }
    const char* type_name = rarity == "P"   ? "platinum.png"
                            : rarity == "G" ? "gold.png"
                            : rarity == "S" ? "silver.png"
                            : rarity == "B" ? "bronze.png"
                                            : nullptr;
    std::optional<Common::FS::FileView> type_icon;
    if (type_name) {
        const auto custom = Common::FS::GetUserPath(Common::FS::PathType::CustomTrophy) / type_name;
        if (Common::FS::Exists(custom)) {
            type_icon = Common::FS::FileView::Map(custom);
        } else {
            // Embedded in the core: lent, not copied
            type_icon = Common::FS::FileView::Borrow(
                Common::GetEmbeddedResource(std::string("src/resources/") + type_name));
        }
    }
    // Decoded here, on the game's thread, before the overlay shows it
    ovl_trophy_queue_after(trophyName.c_str(), icon ? icon->data() : nullptr,
                           icon ? icon->size() : 0, type_icon ? type_icon->data() : nullptr,
                           type_icon ? type_icon->size() : 0, after ? 1 : 0);
}

} // namespace Libraries::Np::NpTrophy
