// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <cstdint>

#include "common/enum.h"
#include "common/types.h"
#include "core/libraries/ajm/ajm.h"
#include "core/libraries/ajm/ajm_batch.h"

#include <memory>
#include <optional>
#include <tuple>

namespace Libraries::Ajm {

u8 GetPCMSize(AjmFormatEncoding format);

class SparseOutputBuffer {
public:
    SparseOutputBuffer(std::span<std::span<u8>> chunks)
        : m_chunks(chunks), m_current(m_chunks.begin()) {}

    template <class T>
    size_t Write(std::span<T> pcm) {
        size_t samples_written = 0;
        while (!pcm.empty() && !IsEmpty()) {
            auto size = std::min(pcm.size() * sizeof(T), m_current->size());
            std::memcpy(m_current->data(), pcm.data(), size);
            const auto nsamples = size / sizeof(T);
            samples_written += nsamples;
            pcm = pcm.subspan(nsamples);
            *m_current = m_current->subspan(size);
            if (m_current->empty()) {
                ++m_current;
            }
        }
        if (!pcm.empty()) {
            LOG_ERROR(Lib_Ajm, "Could not write {} samples", pcm.size());
        }
        return samples_written;
    }

    // Writes count frames of channels samples each straight into the output memory:
    // fill(first, n, dst) produces frames [first, first + n) at dst. Only a frame that
    // straddles two chunks, or a chunk the samples would sit unaligned in, goes through
    // a frame-sized buffer. Returns the samples written.
    template <class T, class Fill>
    size_t WriteFrames(size_t count, u32 channels, Fill&& fill) {
        constexpr u32 MaxChannels = 8;
        const size_t frame_bytes = channels * sizeof(T);
        size_t done = 0;
        size_t samples = 0;
        while (done < count && !IsEmpty() && channels <= MaxChannels) {
            const size_t whole = m_current->size() / frame_bytes;
            if (whole == 0 ||
                reinterpret_cast<std::uintptr_t>(m_current->data()) % alignof(T) != 0) {
                std::array<T, MaxChannels> frame;
                fill(done, 1, frame.data());
                samples += Write(std::span<T>(frame.data(), channels));
                ++done;
                continue;
            }
            const size_t n = std::min(whole, count - done);
            fill(done, n, reinterpret_cast<T*>(m_current->data()));
            *m_current = m_current->subspan(n * frame_bytes);
            if (m_current->empty()) {
                ++m_current;
            }
            done += n;
            samples += n * channels;
        }
        if (done < count) {
            LOG_ERROR(Lib_Ajm, "Could not write {} frames", count - done);
        }
        return samples;
    }

    bool IsEmpty() const {
        return m_current == m_chunks.end();
    }

    size_t Size() const {
        size_t result = 0;
        for (auto it = m_current; it != m_chunks.end(); ++it) {
            result += it->size();
        }
        return result;
    }

private:
    std::span<std::span<u8>> m_chunks;
    std::span<std::span<u8>>::iterator m_current;
};

struct AjmInstanceGapless {
    AjmSidebandGaplessDecode init{};
    AjmSidebandGaplessDecode current{};

    bool IsEnd() const {
        return init.total_samples != 0 && current.total_samples == 0;
    }

    void Reset() {
        current.total_samples = init.total_samples;
        current.skip_samples = init.skip_samples;
        current.skipped_samples = 0;
    }
};

struct DecoderResult {
    s32 result = 0;
    s32 internal_result = 0;
    u32 frames_decoded = 0;
    u32 samples_written = 0;
    bool is_reset = false;
};

class AjmCodec {
public:
    virtual ~AjmCodec() = default;

    virtual void Initialize(const void* buffer, u32 buffer_size) = 0;
    virtual void Reset() = 0;
    virtual void GetInfo(void* out_info) const = 0;
    virtual AjmSidebandFormat GetFormat() const = 0;
    virtual u32 GetMinimumInputSize() const = 0;
    virtual u32 GetNextFrameSize(const AjmInstanceGapless& gapless) const = 0;
    virtual DecoderResult ProcessData(std::span<u8>& input, SparseOutputBuffer& output,
                                      AjmInstanceGapless& gapless) = 0;
};

class AjmInstance {
public:
    AjmInstance(AjmCodecType codec_type, AjmInstanceFlags flags);

    void ExecuteJob(AjmJob& job);

private:
    bool HasEnoughSpace(const SparseOutputBuffer& output) const;
    void Reset();

    AjmInstanceFlags m_flags{};
    AjmSidebandFormat m_format{};
    AjmInstanceGapless m_gapless{};
    AjmSidebandResampleParameters m_resample_parameters{};
    u32 m_total_samples{};
    std::unique_ptr<AjmCodec> m_codec;
};

} // namespace Libraries::Ajm
