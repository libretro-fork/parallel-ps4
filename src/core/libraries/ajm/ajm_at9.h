// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/enum.h"
#include "common/types.h"
#include "core/libraries/ajm/ajm_instance.h"

struct ratrac9;

#include <span>
#include <vector>

namespace Libraries::Ajm {

enum AjmAt9CodecFlags : u32 {
    ParseRiffHeader = 1 << 0,
    NonInterleavedOutput = 1 << 8,
};
DECLARE_ENUM_FLAG_OPERATORS(AjmAt9CodecFlags)

struct AjmSidebandDecAt9CodecInfo {
    u32 super_frame_size;
    u32 frames_in_super_frame;
    u32 next_frame_size;
    u32 frame_samples;
};

struct AjmAt9Decoder final : AjmCodec {
    explicit AjmAt9Decoder(AjmFormatEncoding format, AjmAt9CodecFlags flags, u32 channels);
    ~AjmAt9Decoder() override;

    void Reset() override;
    void Initialize(const void* buffer, u32 buffer_size) override;
    void GetInfo(void* out_info) const override;
    AjmSidebandFormat GetFormat() const override;
    u32 GetMinimumInputSize() const override;
    u32 GetNextFrameSize(const AjmInstanceGapless& gapless) const override;
    DecoderResult ProcessData(std::span<u8>& input, SparseOutputBuffer& output,
                              AjmInstanceGapless& gapless) override;

private:
    template <class T>
    size_t WriteOutputSamples(SparseOutputBuffer& output, u32 skipped_samples, u32 max_samples) {
        std::span<T> pcm_data{reinterpret_cast<T*>(m_pcm_buffer.data()),
                              m_pcm_buffer.size() / sizeof(T)};
        pcm_data = pcm_data.subspan(skipped_samples * m_channels);
        const auto pcm_size = std::min(u32(pcm_data.size()), max_samples);
        return output.Write(pcm_data.subspan(0, pcm_size));
    }

    bool ParseRIFFHeader(std::span<u8>& input, AjmInstanceGapless& gapless);

    const AjmFormatEncoding m_format;
    const AjmAt9CodecFlags m_flags;

    bool m_is_initialized{};
    // libretro-common's ATRAC9 decoder
    ratrac9* m_decoder{};
    u8 m_config_data[ORBIS_AT9_CONFIG_DATA_SIZE]{};
    u32 m_superframe_bytes_remain{};
    u32 m_num_frames{};
    u32 m_channels{};
    u32 m_sample_rate{};
    u32 m_frame_samples{};
    u32 m_superframe_bytes{};
    u32 m_frames_in_superframe{};
    std::vector<u8> m_pcm_buffer;
};

} // namespace Libraries::Ajm
