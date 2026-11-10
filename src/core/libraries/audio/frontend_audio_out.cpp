// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <memory>

#include "common/logging/log.h"
#include "core/emulator_settings.h"
#include "core/libraries/audio/audioout.h"
#include "core/libraries/audio/audioout_backend.h"
#include "core/libraries/kernel/threads.h"
#include "libretro/audio_mix.h"

namespace Libraries::AudioOut {

namespace {

constexpr float INV_VOLUME_0DB = 1.0f / 32768.0f;
constexpr u64 VOLUME_CHECK_INTERVAL_US = 50000;
// A frontend that has stopped taking sound must not stop the game's audio thread with it.
constexpr s64 FRONTEND_WAIT_US = 250000;

// The libretro core plays a port by handing it to the frontend: what the game writes is
// converted to 16-bit stereo, with the port's volume, straight into its input of the core's
// mix (libretro/audio_mix.h). The port is paced by what the frontend takes, as a device would
// pace it: it waits while more than about 32 ms of its sound is still queued.
class FrontendPortBackend final : public PortBackend {
public:
    explicit FrontendPortBackend(const PortOut& port)
        : buffer_frames(port.buffer_frames), num_channels(port.format_info.num_channels),
          is_float(port.format_info.is_float), input(audio_mix_input_open()) {
        gain.store(emu_settings_get_volume_slider() * 0.01f, std::memory_order_relaxed);
        if (!input) {
            LOG_ERROR(Lib_AudioOut, "No input left in the core's mix: port type {} is silent",
                      static_cast<int>(port.type));
            return;
        }
        if (num_channels != 1 && num_channels != 2 && num_channels != 8) {
            LOG_ERROR(Lib_AudioOut, "Unsupported channel count {}: port is silent", num_channels);
            audio_mix_input_close(input);
            input = nullptr;
            return;
        }
        LOG_INFO(Lib_AudioOut, "Opened audio port type {} for the frontend ({} ch)",
                 static_cast<int>(port.type), num_channels);
    }

    ~FrontendPortBackend() override {
        audio_mix_input_close(input);
    }

    void Output(void* ptr) override {
        if (!input || !ptr) [[unlikely]] {
            return;
        }
        UpdateVolumeIfChanged();
        if (!audio_mix_input_wait(input, buffer_frames, FRONTEND_WAIT_US)) {
            return; // the frontend is not taking sound: the buffer is dropped
        }
        const float g = gain.load(std::memory_order_relaxed);
        const u32 sample_bytes = is_float ? 4 : 2;
        const u8* src = static_cast<const u8*>(ptr);
        u32 done = 0;
        while (done < buffer_frames) {
            s16* dst = nullptr;
            const u32 got = audio_mix_input_reserve(input, &dst, buffer_frames - done);
            if (got == 0) {
                break;
            }
            const void* at = src + static_cast<size_t>(done) * num_channels * sample_bytes;
            if (is_float) {
                audio_mix_convert_f32(dst, static_cast<const float*>(at), got, num_channels, g);
            } else {
                audio_mix_convert_s16(dst, static_cast<const s16*>(at), got, num_channels, g);
            }
            audio_mix_input_commit(input, got);
            done += got;
        }
    }

    void SetVolume(const std::array<int, 8>& ch_volumes) override {
        float max_channel_gain = 0.0f;
        const u32 channels = std::min(num_channels, 8u);
        for (u32 i = 0; i < channels; i++) {
            max_channel_gain =
                std::max(max_channel_gain, static_cast<float>(ch_volumes[i]) * INV_VOLUME_0DB);
        }
        game_gain.store(max_channel_gain, std::memory_order_relaxed);
        gain.store(max_channel_gain * emu_settings_get_volume_slider() * 0.01f,
                   std::memory_order_relaxed);
    }

private:
    // The Volume option can change while the game runs.
    void UpdateVolumeIfChanged() {
        const u64 now = Kernel::sceKernelGetProcessTime();
        if (now - last_volume_check_time < VOLUME_CHECK_INTERVAL_US) {
            return;
        }
        last_volume_check_time = now;
        gain.store(emu_settings_get_volume_slider() * 0.01f *
                       game_gain.load(std::memory_order_relaxed),
                   std::memory_order_relaxed);
    }

    const u32 buffer_frames;
    const u32 num_channels;
    const bool is_float;
    audio_mix_input_t* input;
    std::atomic<float> gain{1.0f};
    std::atomic<float> game_gain{1.0f};
    u64 last_volume_check_time{0};
};

} // Anonymous namespace

std::unique_ptr<PortBackend> FrontendAudioOut::Open(PortOut& port) {
    return std::make_unique<FrontendPortBackend>(port);
}

} // namespace Libraries::AudioOut
