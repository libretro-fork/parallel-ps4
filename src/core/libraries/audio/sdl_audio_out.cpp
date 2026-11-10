// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <atomic>
#include <cstring>
#include <memory>
#include <thread>
#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_hints.h>

#include "common/logging/log.h"
#include "core/emulator_settings.h"
#include "core/libraries/audio/audioout.h"
#include "core/libraries/audio/audioout_backend.h"
#include "core/libraries/kernel/threads.h"
#include "libretro/libretro_bridge.h"

// SIMD support detection
#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#define HAS_SSE2
#endif

#define SDL_INVALID_AUDIODEVICEID 0

namespace Libraries::AudioOut {

// Volume constants
constexpr float VOLUME_0DB = 32768.0f; // 1 << 15
constexpr float INV_VOLUME_0DB = 1.0f / VOLUME_0DB;
constexpr float VOLUME_EPSILON = 0.001f;
// Timing constants
constexpr u64 VOLUME_CHECK_INTERVAL_US = 50000; // Check every 50ms
// Memory alignment for SIMD
constexpr size_t AUDIO_BUFFER_ALIGNMENT = 32;

// Channel positions
enum ChannelPos : u8 {
    FL = 0,
    FR = 1,
    FC = 2,
    LF = 3,
    SL = 4,
    SR = 5,
    BL = 6,
    BR = 7,
    STD_SL = 6,
    STD_SR = 7,
    STD_BL = 4,
    STD_BR = 5
};

class SDLPortBackend : public PortBackend {
public:
    explicit SDLPortBackend(const PortOut& port)
        : frame_size(port.format_info.FrameSize()), buffer_frames(port.buffer_frames),
          sample_rate(port.sample_rate), num_channels(port.format_info.num_channels),
          is_float(port.format_info.is_float), is_std(port.format_info.is_std),
          channel_layout(port.format_info.channel_layout) {

        if (!Initialize(port.type)) {
            LOG_ERROR(Lib_AudioOut, "Failed to initialize SDL audio backend");
        }
    }

    ~SDLPortBackend() override {
        Cleanup();
    }

    void Output(void* ptr) override {
        if (!stream || !internal_buffer || !convert) [[unlikely]] {
            return;
        }

        if (ptr == nullptr) [[unlikely]] {
            return;
        }

        UpdateVolumeIfChanged();
        const u64 current_time = Kernel::sceKernelGetProcessTime();
        convert(ptr, internal_buffer, buffer_frames, nullptr);
        WaitForFrontend();

        if ((output_count++ & 0xF) == 0) { // Check every 16 outputs
            ManageAudioQueue();
        }

        if (!SDL_PutAudioStreamData(stream, internal_buffer, internal_buffer_size)) [[unlikely]] {
            LOG_ERROR(Lib_AudioOut, "Failed to output to SDL audio stream: {}", SDL_GetError());
        }

        last_output_time.store(current_time, std::memory_order_release);
    }

    void SetVolume(const std::array<int, 8>& ch_volumes) override {
        if (!stream) [[unlikely]] {
            return;
        }

        float max_channel_gain = 0.0f;
        const u32 channels_to_check = std::min(num_channels, 8u);

        for (u32 i = 0; i < channels_to_check; i++) {
            const float channel_gain = static_cast<float>(ch_volumes[i]) * INV_VOLUME_0DB;
            max_channel_gain = std::max(max_channel_gain, channel_gain);
        }
        game_gain.store(max_channel_gain, std::memory_order_release);

        const float slider_gain = EmulatorSettings.GetVolumeSlider() * 0.01f; // Faster than /100.0f
        const float total_gain = max_channel_gain * slider_gain;

        const float current = current_gain.load(std::memory_order_acquire);
        if (std::abs(total_gain - current) < VOLUME_EPSILON) {
            return;
        }

        // Apply volume change
        if (SDL_SetAudioStreamGain(stream, total_gain)) {
            current_gain.store(total_gain, std::memory_order_release);
            LOG_DEBUG(Lib_AudioOut,
                      "Set combined audio gain to {:.3f} (channel: {:.3f}, slider: {:.3f})",
                      total_gain, max_channel_gain, slider_gain);
        } else {
            LOG_ERROR(Lib_AudioOut, "Failed to set audio stream gain: {}", SDL_GetError());
        }
    }

    u64 GetLastOutputTime() const {
        return last_output_time.load(std::memory_order_acquire);
    }

private:
    bool Initialize(OrbisAudioOutPort type) {
        // Allocate aligned internal buffer for SIMD operations
        internal_buffer_size = buffer_frames * sizeof(float) * num_channels;

#ifdef _WIN32
        internal_buffer = _aligned_malloc(internal_buffer_size, AUDIO_BUFFER_ALIGNMENT);
#else
        if (posix_memalign(&internal_buffer, AUDIO_BUFFER_ALIGNMENT, internal_buffer_size) != 0) {
            internal_buffer = nullptr;
        }
#endif

        if (!internal_buffer) {
            LOG_ERROR(Lib_AudioOut, "Failed to allocate aligned audio buffer of size {}",
                      internal_buffer_size);
            return false;
        }

        // Initialize current gain
        current_gain.store(EmulatorSettings.GetVolumeSlider() * 0.01f, std::memory_order_relaxed);

        if (!OpenDevice(type)) {
            FreeAlignedBuffer();
            return false;
        }

        if (!SelectConverter()) {
            FreeAlignedBuffer();
            return false;
        }

        CalculateQueueThreshold();
        return true;
    }

    void Cleanup() {
        Libretro::RemoveAudioStream(stream);
        if (stream) {
            SDL_DestroyAudioStream(stream);
            stream = nullptr;
        }
        FreeAlignedBuffer();
    }

    void FreeAlignedBuffer() {
        if (internal_buffer) {
#ifdef _WIN32
            _aligned_free(internal_buffer);
#else
            free(internal_buffer);
#endif
            internal_buffer = nullptr;
        }
    }

    void UpdateVolumeIfChanged() {
        const u64 current_time = Kernel::sceKernelGetProcessTime();

        if (current_time - last_volume_check_time < VOLUME_CHECK_INTERVAL_US) {
            return;
        }

        last_volume_check_time = current_time;

        const float config_volume =
            EmulatorSettings.GetVolumeSlider() * 0.01f * game_gain.load(std::memory_order_acquire);
        const float stored_gain = current_gain.load(std::memory_order_acquire);

        // Only update if the difference is significant
        if (std::abs(config_volume - stored_gain) > VOLUME_EPSILON) {
            if (SDL_SetAudioStreamGain(stream, config_volume)) {
                current_gain.store(config_volume, std::memory_order_release);
                LOG_DEBUG(Lib_AudioOut, "Updated audio gain to {:.3f}", config_volume);
            } else {
                LOG_ERROR(Lib_AudioOut, "Failed to set audio stream gain: {}", SDL_GetError());
            }
        }
    }

    // The libretro core: the port is paced by what the frontend takes, as a device would pace
    // it, not by the clock. The frontend takes a frame's worth per run at its own rate, a
    // little off the game's; against the clock the difference piled up as latency until the
    // queue was thrown away. The port waits while more than about 32 ms of its sound is still
    // queued (the ring the core reads from holds as much again). Not for ever: a frontend
    // that has stopped taking sound must not stop the game's audio thread with it.
    void WaitForFrontend() {
        const int limit = static_cast<int>((sample_rate * 32 / 1000) * sizeof(float) *
                                           (ps4_downmix ? 2 : num_channels));
        for (int waited_ms = 0; waited_ms < 250; ++waited_ms) {
            if (SDL_GetAudioStreamQueued(stream) <= limit) {
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    void ManageAudioQueue() {
        const auto queued = SDL_GetAudioStreamQueued(stream);

        if (queued >= queue_threshold) [[unlikely]] {
            LOG_DEBUG(Lib_AudioOut, "Clearing backed up audio queue ({} >= {})", queued,
                      queue_threshold);
            SDL_ClearAudioStream(stream);
            CalculateQueueThreshold();
        }
    }

    // The libretro core: the port's stream is bound to no device. SDL converts what the game
    // writes to the frontend's format (16-bit stereo at 48 kHz) and the core takes it out a
    // frame's worth per retro_run, mixing the ports.
    bool OpenDevice(OrbisAudioOutPort type) {
        ps4_downmix = num_channels >= 6;
        if (ps4_downmix) {
            internal_buffer_size = buffer_frames * sizeof(float) * 2;
        }
        const SDL_AudioSpec source = {
            .format = SDL_AUDIO_F32LE,
            .channels = ps4_downmix ? 2 : static_cast<int>(num_channels),
            .freq = static_cast<int>(sample_rate),
        };
        const SDL_AudioSpec target = {
            .format = SDL_AUDIO_S16LE,
            .channels = 2,
            .freq = Libretro::AUDIO_SAMPLE_RATE,
        };
        stream = SDL_CreateAudioStream(&source, &target);
        if (!stream) {
            LOG_ERROR(Lib_AudioOut, "Failed to create SDL audio stream: {}", SDL_GetError());
            return false;
        }
        if (!ps4_downmix && !ConfigureChannelMap()) {
            SDL_DestroyAudioStream(stream);
            stream = nullptr;
            return false;
        }
        SDL_SetAudioStreamGain(stream, current_gain.load(std::memory_order_relaxed));
        Libretro::AddAudioStream(stream);
        LOG_INFO(Lib_AudioOut, "Opened audio port type {} for the frontend ({} Hz, {} ch)",
                 static_cast<int>(type), sample_rate, num_channels);
        return true;
    }

    bool ConfigureChannelMap() {
        if (num_channels == 0) {
            return true;
        }

        std::vector<int> channel_map(num_channels);

        if (is_std && num_channels == 8) {
            // Standard 8CH layout requires remapping
            channel_map = {FL, FR, FC, LF, STD_SL, STD_SR, STD_BL, STD_BR};
        } else {
            std::copy_n(channel_layout.begin(), num_channels, channel_map.begin());
        }

        if (!SDL_SetAudioStreamInputChannelMap(stream, channel_map.data(), num_channels)) {
            LOG_ERROR(Lib_AudioOut, "Failed to set channel map: {}", SDL_GetError());
            return false;
        }

        return true;
    }

    static constexpr float DOWNMIX_FRONT = 1.0f;
    static constexpr float DOWNMIX_CENTER = 0.7071f;
    static constexpr float DOWNMIX_SURROUND = 0.7071f;

    static void DownmixS16_8CHToStereoPS4(const void* src, void* dst, u32 frames, const float*) {
        constexpr float INV = 1.0f / 32768.0f;
        const s16* s = static_cast<const s16*>(src);
        float* d = static_cast<float*>(dst);
        for (u32 i = 0; i < frames; i++) {
            const u32 o = i << 3;
            const float center = DOWNMIX_CENTER * s[o + FC];
            d[i * 2 + 0] =
                (DOWNMIX_FRONT * s[o + FL] + center + DOWNMIX_SURROUND * (s[o + 4] + s[o + 6])) *
                INV;
            d[i * 2 + 1] =
                (DOWNMIX_FRONT * s[o + FR] + center + DOWNMIX_SURROUND * (s[o + 5] + s[o + 7])) *
                INV;
        }
    }
    static void DownmixF32_8CHToStereoPS4(const void* src, void* dst, u32 frames, const float*) {
        const float* s = static_cast<const float*>(src);
        float* d = static_cast<float*>(dst);
        for (u32 i = 0; i < frames; i++) {
            const u32 o = i << 3;
            const float center = DOWNMIX_CENTER * s[o + FC];
            d[i * 2 + 0] =
                DOWNMIX_FRONT * s[o + FL] + center + DOWNMIX_SURROUND * (s[o + 4] + s[o + 6]);
            d[i * 2 + 1] =
                DOWNMIX_FRONT * s[o + FR] + center + DOWNMIX_SURROUND * (s[o + 5] + s[o + 7]);
        }
    }

    bool SelectConverter() {
        if (ps4_downmix) {
            convert = is_float ? &DownmixF32_8CHToStereoPS4 : &DownmixS16_8CHToStereoPS4;
            return true;
        }

        if (is_float) {
            switch (num_channels) {
            case 1:
                convert = &ConvertF32Mono;
                break;
            case 2:
                convert = &ConvertF32Stereo;
                break;
            case 8:
                convert = is_std ? &ConvertF32Std8CH : &ConvertF32_8CH;
                break;
            default:
                LOG_ERROR(Lib_AudioOut, "Unsupported float channel count: {}", num_channels);
                return false;
            }
        } else {
            switch (num_channels) {
            case 1:
                convert = &ConvertS16Mono;
                break;
            case 2:
#if defined(HAS_SSE2)
                convert = &ConvertS16StereoSIMD;
#else
                convert = &ConvertS16Stereo;
#endif
                break;
            case 8:
#if defined(HAS_SSE2)
                convert = &ConvertS16_8CH_SIMD;
#else
                convert = &ConvertS16_8CH;
#endif
                break;
            default:
                LOG_ERROR(Lib_AudioOut, "Unsupported S16 channel count: {}", num_channels);
                return false;
            }
        }

        return true;
    }

    void CalculateQueueThreshold() {
        if (!stream) {
            return;
        }
        // No device to ask: a quarter of a second of what the game writes. The port waits for
        // the frontend well before that (WaitForFrontend), so this is only reached when the
        // frontend has stopped taking sound.
        queue_threshold = (sample_rate / 4) * sizeof(float) * (ps4_downmix ? 2 : num_channels);
    }

    using ConverterFunc = void (*)(const void* src, void* dst, u32 frames, const float* volumes);

    static void ConvertS16Mono(const void* src, void* dst, u32 frames, const float*) {
        const s16* s = static_cast<const s16*>(src);
        float* d = static_cast<float*>(dst);

        for (u32 i = 0; i < frames; i++) {
            d[i] = s[i] * INV_VOLUME_0DB;
        }
    }

    static void ConvertS16Stereo(const void* src, void* dst, u32 frames, const float*) {
        const s16* s = static_cast<const s16*>(src);
        float* d = static_cast<float*>(dst);

        const u32 num_samples = frames << 1; // * 2
        for (u32 i = 0; i < num_samples; i++) {
            d[i] = s[i] * INV_VOLUME_0DB;
        }
    }

#ifdef HAS_SSE2
    static void ConvertS16StereoSIMD(const void* src, void* dst, u32 frames, const float*) {
        const s16* s = static_cast<const s16*>(src);
        float* d = static_cast<float*>(dst);

        const __m128 scale = _mm_set1_ps(INV_VOLUME_0DB);
        const u32 num_samples = frames << 1;
        u32 i = 0;

        // Process 8 samples at a time (4 stereo frames)
        for (; i + 8 <= num_samples; i += 8) {
            // Load 8 s16 values
            __m128i s16_vals = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&s[i]));

            // Convert to 32-bit integers
            __m128i s32_lo = _mm_cvtepi16_epi32(s16_vals);
            __m128i s32_hi = _mm_cvtepi16_epi32(_mm_srli_si128(s16_vals, 8));

            // Convert to float and scale
            __m128 f_lo = _mm_mul_ps(_mm_cvtepi32_ps(s32_lo), scale);
            __m128 f_hi = _mm_mul_ps(_mm_cvtepi32_ps(s32_hi), scale);

            // Store results
            _mm_storeu_ps(&d[i], f_lo);
            _mm_storeu_ps(&d[i + 4], f_hi);
        }

        // Handle remaining samples
        for (; i < num_samples; i++) {
            d[i] = s[i] * INV_VOLUME_0DB;
        }
    }
#endif

    static void ConvertS16_8CH(const void* src, void* dst, u32 frames, const float*) {
        const s16* s = static_cast<const s16*>(src);
        float* d = static_cast<float*>(dst);

        const u32 num_samples = frames << 3; // * 8
        for (u32 i = 0; i < num_samples; i++) {
            d[i] = s[i] * INV_VOLUME_0DB;
        }
    }

#ifdef HAS_SSE2
    static void ConvertS16_8CH_SIMD(const void* src, void* dst, u32 frames, const float*) {
        const s16* s = static_cast<const s16*>(src);
        float* d = static_cast<float*>(dst);

        const __m128 scale = _mm_set1_ps(INV_VOLUME_0DB);
        const u32 num_samples = frames << 3;
        u32 i = 0;

        // Process 8 samples at a time
        for (; i + 8 <= num_samples; i += 8) {
            __m128i s16_vals = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&s[i]));
            __m128i s32_lo = _mm_cvtepi16_epi32(s16_vals);
            __m128i s32_hi = _mm_cvtepi16_epi32(_mm_srli_si128(s16_vals, 8));
            __m128 f_lo = _mm_mul_ps(_mm_cvtepi32_ps(s32_lo), scale);
            __m128 f_hi = _mm_mul_ps(_mm_cvtepi32_ps(s32_hi), scale);
            _mm_storeu_ps(&d[i], f_lo);
            _mm_storeu_ps(&d[i + 4], f_hi);
        }

        for (; i < num_samples; i++) {
            d[i] = s[i] * INV_VOLUME_0DB;
        }
    }
#endif

    static void ConvertF32Mono(const void* src, void* dst, u32 frames, const float*) {
        std::memcpy(dst, src, frames * sizeof(float));
    }

    static void ConvertF32Stereo(const void* src, void* dst, u32 frames, const float*) {
        std::memcpy(dst, src, frames * 2 * sizeof(float));
    }

    static void ConvertF32_8CH(const void* src, void* dst, u32 frames, const float*) {
        std::memcpy(dst, src, frames * 8 * sizeof(float));
    }

    static void ConvertF32Std8CH(const void* src, void* dst, u32 frames, const float*) {
        const float* s = static_cast<const float*>(src);
        float* d = static_cast<float*>(dst);

        // Channel remapping for standard 8CH layout
        for (u32 i = 0; i < frames; i++) {
            const u32 offset = i << 3; // * 8

            d[offset + FL] = s[offset + FL];
            d[offset + FR] = s[offset + FR];
            d[offset + FC] = s[offset + FC];
            d[offset + LF] = s[offset + LF];
            d[offset + SL] = s[offset + STD_SL];
            d[offset + SR] = s[offset + STD_SR];
            d[offset + BL] = s[offset + STD_BL];
            d[offset + BR] = s[offset + STD_BR];
        }
    }

    // Audio format parameters
    const u32 frame_size;
    const u32 buffer_frames;
    const u32 sample_rate;
    const u32 num_channels;
    const bool is_float;
    const bool is_std;
    // Set when the output device is stereo and we fold 5.1/7.1 ourselves.
    bool ps4_downmix{false};
    const std::array<int, 8> channel_layout;

    alignas(64) std::atomic<u64> last_output_time{0};
    u64 last_volume_check_time{0};
    u32 output_count{0};

    // Buffers
    u32 internal_buffer_size{0};
    void* internal_buffer{nullptr};

    // Converter function pointer
    ConverterFunc convert{nullptr};

    // Volume management
    alignas(64) std::atomic<float> current_gain{1.0f};
    std::atomic<float> game_gain{1.0f};

    // SDL audio stream
    SDL_AudioStream* stream{nullptr};
    u32 queue_threshold{0};
};

std::unique_ptr<PortBackend> SDLAudioOut::Open(PortOut& port) {
    return std::make_unique<SDLPortBackend>(port);
}

} // namespace Libraries::AudioOut
