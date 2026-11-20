// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <memory>

#include <retro_atomic.h>
#include "common/logging/log.h"
#include "core/emulator_settings.h"
#include "core/libraries/audio/audioout.h"
#include "core/libraries/audio/audioout_backend.h"
#include "core/libraries/kernel/threads.h"
#include "core/libraries/kernel/time.h"
#include "libretro/audio_mix.h"

namespace Libraries::AudioOut {

namespace {

constexpr float INV_VOLUME_0DB = 1.0f / 32768.0f;

// The libretro core plays a port by handing it to the frontend: what the game writes is
// converted, with the port's volume, straight into its input of the core's mix
// (libretro/audio_mix.h): to stereo, or a port of eight channels as 7.1 where the mix is
// surround (the frontend takes it), in the frontend's order. The call is paced a buffer's time
// at a time on the game's time, as the device paces it (audioout.cpp, PaceOutput); the mix takes
// one frame of every port's sound (AUDIO_MIX_FRAME_FRAMES) each retro_run, and a port waits
// besides while more than that is queued, on the game's time (Kernel::GameWait): no buffer kept
// ahead, no timeout.
class FrontendPortBackend final : public PortBackend {
public:
    explicit FrontendPortBackend(const PortOut& port)
        : buffer_frames(port.buffer_frames), num_channels(port.format_info.num_channels),
          is_float(port.format_info.is_float),
          wide(port.format_info.num_channels == 8 && audio_mix_channels() == 8),
          order(port.format_info.channel_layout),
          input(wide ? audio_mix_input_open_channels(8) : audio_mix_input_open()) {
        retro_atomic_store_relaxed_int(&game_gain, std::bit_cast<int>(1.0f));
        for (auto& g : channel_gain) {
            retro_atomic_store_relaxed_int(&g, std::bit_cast<int>(1.0f));
        }
        applied.fill(emu_settings_get_volume_slider() * 0.01f);
        if (!input) {
            LOG_ERROR(Lib_AudioOut, "No input left in the core's mix: port type %d is silent",
                      static_cast<int>(port.type));
            return;
        }
        if (num_channels != 1 && num_channels != 2 && num_channels != 8) {
            LOG_ERROR(Lib_AudioOut, "Unsupported channel count %u: port is silent", num_channels);
            audio_mix_input_close(input);
            input = nullptr;
            return;
        }
        LOG_INFO(Lib_AudioOut, "Opened audio port type %d for the frontend (%u ch%s)",
                 static_cast<int>(port.type), num_channels, wide ? ", as 7.1" : "");
    }

    ~FrontendPortBackend() override {
        audio_mix_input_close(input);
    }

    void Output(void* ptr) override {
        if (!input || !ptr) [[unlikely]] {
            return;
        }
        // The call is paced by the game's time (PaceOutput), which keeps a frame and a buffer
        // queued at most. This bound is only for a frontend that stopped taking frames: set
        // past what the pace keeps, so that it never holds a paced port back (a port held, then
        // let go, outputs its late buffers at once: FMOD's output overtakes its mixer)
        Kernel::GameWait(audio_mix_input_room_ec(input), 0, [this] {
            return audio_mix_input_ready(input, buffer_frames, AUDIO_MIX_FRAME_FRAMES * 2) != 0;
        });
        // The gains this buffer goes to: the game's port volume times the Volume option's
        const float slider = emu_settings_get_volume_slider() * 0.01f;
        std::array<float, 8> target;
        if (wide) {
            for (u32 c = 0; c < 8; c++) {
                target[c] = std::bit_cast<float>(
                                retro_atomic_load_relaxed_int(&channel_gain[order[c] & 7])) *
                            slider;
            }
        } else {
            target.fill(std::bit_cast<float>(retro_atomic_load_relaxed_int(&game_gain)) * slider);
        }
        const u32 width = wide ? 8 : 2;
        // A volume that changed is ramped across this buffer from the last one's, as the
        // device does: stepped, it clicks (a fade, a duck)
        const bool ramp = !std::equal(target.begin(), target.begin() + width, applied.begin());
        static constexpr std::array<float, 8> unit{1, 1, 1, 1, 1, 1, 1, 1};
        const std::array<float, 8>& gains = ramp ? unit : target;
        const u32 sample_bytes = is_float ? 4 : 2;
        const u8* src = static_cast<const u8*>(ptr);
        u32 done = 0;
        while (done < buffer_frames) {
            float* dst = nullptr;
            const u32 got = audio_mix_input_reserve(input, &dst, buffer_frames - done);
            if (got == 0) {
                break;
            }
            const void* at = src + static_cast<size_t>(done) * num_channels * sample_bytes;
            if (wide) {
                if (is_float) {
                    audio_mix_convert_f32_wide(dst, static_cast<const float*>(at), got,
                                               order.data(), gains.data());
                } else {
                    audio_mix_convert_s16_wide(dst, static_cast<const s16*>(at), got, order.data(),
                                               gains.data());
                }
            } else if (is_float) {
                audio_mix_convert_f32(dst, static_cast<const float*>(at), got, num_channels,
                                      gains[0]);
            } else {
                audio_mix_convert_s16(dst, static_cast<const s16*>(at), got, num_channels,
                                      gains[0]);
            }
            if (ramp) {
                audio_mix_ramp(dst, got, width, done, buffer_frames, applied.data(), target.data());
            }
            audio_mix_input_commit(input, got);
            done += got;
        }
        applied = target;
    }

    void SetVolume(const std::array<int, 8>& ch_volumes) override {
        float max_channel_gain = 0.0f;
        const u32 channels = std::min(num_channels, 8u);
        for (u32 i = 0; i < channels; i++) {
            max_channel_gain =
                std::max(max_channel_gain, static_cast<float>(ch_volumes[i]) * INV_VOLUME_0DB);
        }
        retro_atomic_store_relaxed_int(&game_gain, std::bit_cast<int>(max_channel_gain));
        for (u32 i = 0; i < channels; i++) {
            retro_atomic_store_relaxed_int(
                &channel_gain[i],
                std::bit_cast<int>(static_cast<float>(ch_volumes[i]) * INV_VOLUME_0DB));
        }
    }

private:
    // The Volume option can change while the game runs.

    const u32 buffer_frames;
    const u32 num_channels;
    const bool is_float;
    // Eight channels kept as 7.1: the mix is surround. The port's channels in the mix's order:
    // channel c of the mix is the port's order[c] (the console has two 7.1 orders)
    const bool wide;
    const std::array<int, 8> order;
    audio_mix_input_t* input;
    // Wide: each of the port's channels' volume as the game set it (its order)
    std::array<retro_atomic_int_t, 8> channel_gain{};
    // The stereo volume: the port's loudest channel's, as the game set it
    retro_atomic_int_t game_gain{};
    // The gains the last buffer ended at (the writers')
    std::array<float, 8> applied{};
    u64 last_volume_check_time{0};
};

} // Anonymous namespace

std::unique_ptr<PortBackend> FrontendAudioOut::Open(PortOut& port) {
    return std::make_unique<FrontendPortBackend>(port);
}

} // namespace Libraries::AudioOut
