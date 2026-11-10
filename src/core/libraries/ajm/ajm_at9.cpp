// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ajm_result.h"
#include "common/assert.h"
#include "core/libraries/ajm/ajm_at9.h"

#include <formats/ratrac9.h>

namespace Libraries::Ajm {

struct ChunkHeader {
    u32 tag;
    u32 length;
};
static_assert(sizeof(ChunkHeader) == 8);

struct SampleData {
    u32 sample_length;
    u32 encoder_delay;
    u32 encoder_delay2;
};
static_assert(sizeof(SampleData) == 12);

struct RIFFHeader {
    u32 riff;
    u32 size;
    u32 wave;
};
static_assert(sizeof(RIFFHeader) == 12);

AjmAt9Decoder::AjmAt9Decoder(AjmFormatEncoding format, AjmAt9CodecFlags flags, u32)
    : m_format(format), m_flags(flags) {}

AjmAt9Decoder::~AjmAt9Decoder() {
    ratrac9_close(m_decoder);
}

void AjmAt9Decoder::Reset() {
    // Back to the start of the stream; the decoder's memory is kept
    if (m_decoder) {
        ratrac9_reset(m_decoder);
    }
    m_num_frames = 0;
    m_superframe_bytes_remain = m_superframe_bytes;
}

void AjmAt9Decoder::Initialize(const void* buffer, u32 buffer_size) {
    ASSERT_MSG(buffer_size == sizeof(AjmDecAt9InitializeParameters),
               "Incorrect At9 initialization buffer size {}", buffer_size);
    const auto params = reinterpret_cast<const AjmDecAt9InitializeParameters*>(buffer);
    std::memcpy(m_config_data, params->config_data, ORBIS_AT9_CONFIG_DATA_SIZE);
    ratrac9_close(m_decoder);
    m_decoder = ratrac9_open(m_config_data);
    m_is_initialized = m_decoder != nullptr;
    if (!m_decoder) {
        LOG_ERROR(Lib_Ajm, "Invalid ATRAC9 configuration {:02x}{:02x}{:02x}{:02x}",
                  m_config_data[0], m_config_data[1], m_config_data[2], m_config_data[3]);
        return;
    }
    m_channels = ratrac9_channels(m_decoder);
    m_sample_rate = ratrac9_sample_rate(m_decoder);
    m_frame_samples = ratrac9_frame_samples(m_decoder);
    m_superframe_bytes = ratrac9_superframe_bytes(m_decoder);
    m_frames_in_superframe = ratrac9_frames_per_superframe(m_decoder);
    AjmAt9Decoder::Reset();
    m_pcm_buffer.resize(m_frame_samples * m_channels * GetPCMSize(m_format), 0);
}

void AjmAt9Decoder::GetInfo(void* out_info) const {
    auto* info = reinterpret_cast<AjmSidebandDecAt9CodecInfo*>(out_info);
    info->super_frame_size = m_superframe_bytes;
    info->frames_in_super_frame = m_frames_in_superframe;
    info->next_frame_size = m_superframe_bytes_remain;
    info->frame_samples = m_frame_samples;
}

bool AjmAt9Decoder::ParseRIFFHeader(std::span<u8>& in_buf, AjmInstanceGapless& gapless) {
    if (in_buf.size() < sizeof(RIFFHeader) + sizeof(ChunkHeader)) {
        return false;
    }

    auto* header = reinterpret_cast<const RIFFHeader*>(in_buf.data());
    if (header->riff != 'FFIR' || header->wave != 'EVAW') {
        return false;
    }
    in_buf = in_buf.subspan(sizeof(RIFFHeader));

    bool result = false;

    while (in_buf.size() >= sizeof(ChunkHeader)) {
        auto* chunk = reinterpret_cast<const ChunkHeader*>(in_buf.data());
        in_buf = in_buf.subspan(sizeof(ChunkHeader));

        if (chunk->tag == 'atad') {
            break;
        }

        if (in_buf.size() < chunk->length) {
            return false;
        }

        switch (chunk->tag) {
        case ' tmf': {
            // WAVE_FORMAT_EXTENSIBLE with the ATRAC9 sub-format, holding the config bytes
            AjmDecAt9InitializeParameters init_params = {};
            if (!ratrac9_parse_wave_fmt(in_buf.data(), chunk->length, init_params.config_data)) {
                return false;
            }
            Initialize(&init_params, sizeof(init_params));
            result = true;
            break;
        }
        case 'tcaf': {
            if (chunk->length != sizeof(SampleData)) {
                return false;
            }
            auto* samples = reinterpret_cast<const SampleData*>(in_buf.data());
            gapless.init.total_samples = samples->sample_length;
            gapless.init.skip_samples = samples->encoder_delay;
            gapless.Reset();
            break;
        }
        default:
            break;
        }

        const size_t advance_len = std::min(in_buf.size(), size_t((chunk->length + 1) & ~1));
        in_buf = in_buf.subspan(advance_len);
    }

    return result;
}

u32 AjmAt9Decoder::GetMinimumInputSize() const {
    return m_superframe_bytes_remain;
}

DecoderResult AjmAt9Decoder::ProcessData(std::span<u8>& in_buf, SparseOutputBuffer& output,
                                         AjmInstanceGapless& gapless) {
    DecoderResult result{};
    const bool is_riff = True(m_flags & AjmAt9CodecFlags::ParseRiffHeader) &&
                         in_buf.size() >= sizeof(RIFFHeader) &&
                         *reinterpret_cast<const u32*>(in_buf.data()) == 'FFIR';
    if (is_riff) {
        if (!ParseRIFFHeader(in_buf, gapless)) {
            LOG_ERROR(Lib_Ajm, "Failed to parse ATRAC9 RIFF header");
            result.result = ORBIS_AJM_RESULT_CODEC_ERROR | ORBIS_AJM_RESULT_FATAL;
            return result;
        }
        result.is_reset = true;
    }

    if (!m_is_initialized) {
        result.result = ORBIS_AJM_RESULT_NOT_INITIALIZED;
        return result;
    }

    if (in_buf.empty()) {
        return result;
    }

    const bool planar = True(m_flags & AjmAt9CodecFlags::NonInterleavedOutput);
    int ret = 0;
    size_t bytes_used = 0;
    switch (m_format) {
    case AjmFormatEncoding::S16:
        ret = ratrac9_decode_s16(m_decoder, in_buf.data(), in_buf.size(),
                                 reinterpret_cast<s16*>(m_pcm_buffer.data()), planar, &bytes_used);
        break;
    case AjmFormatEncoding::S32:
        ret = ratrac9_decode_s32(m_decoder, in_buf.data(), in_buf.size(),
                                 reinterpret_cast<s32*>(m_pcm_buffer.data()), planar, &bytes_used);
        break;
    case AjmFormatEncoding::Float:
        ret =
            ratrac9_decode_f32(m_decoder, in_buf.data(), in_buf.size(),
                               reinterpret_cast<float*>(m_pcm_buffer.data()), planar, &bytes_used);
        break;
    default:
        UNREACHABLE();
    }
    if (ret != 0) {
        LOG_ERROR(Lib_Ajm, "ATRAC9 decode failed ret = {}", ret);
        result.result = ORBIS_AJM_RESULT_CODEC_ERROR | ORBIS_AJM_RESULT_FATAL;
        result.internal_result = ret;
        return result;
    }
    // A frame cut short counts the bytes it would have had: never step past the input
    const auto consume = [&](size_t used) {
        used = std::min(used, in_buf.size());
        in_buf = in_buf.subspan(used);
        m_superframe_bytes_remain -= std::min<u32>(m_superframe_bytes_remain, u32(used));
    };

    result.frames_decoded += 1;
    consume(bytes_used);

    u32 skip_samples = 0;
    if (gapless.current.skip_samples > 0) {
        skip_samples = std::min(u16(m_frame_samples), gapless.current.skip_samples);
        gapless.current.skip_samples -= skip_samples;
    }

    const auto max_pcm = gapless.init.total_samples != 0
                             ? gapless.current.total_samples * m_channels
                             : std::numeric_limits<u32>::max();

    size_t pcm_written = 0;
    switch (m_format) {
    case AjmFormatEncoding::S16:
        pcm_written = WriteOutputSamples<s16>(output, skip_samples, max_pcm);
        break;
    case AjmFormatEncoding::S32:
        pcm_written = WriteOutputSamples<s32>(output, skip_samples, max_pcm);
        break;
    case AjmFormatEncoding::Float:
        pcm_written = WriteOutputSamples<float>(output, skip_samples, max_pcm);
        break;
    default:
        UNREACHABLE();
    }

    result.samples_written = pcm_written / m_channels;
    gapless.current.skipped_samples += m_frame_samples - result.samples_written;
    if (gapless.init.total_samples != 0) {
        gapless.current.total_samples -= result.samples_written;
    }

    m_num_frames += 1;
    if ((m_num_frames % m_frames_in_superframe) == 0) {
        if (m_superframe_bytes_remain) {
            consume(m_superframe_bytes_remain);
        }
        m_superframe_bytes_remain = m_superframe_bytes;
        m_num_frames = 0;
    } else if (gapless.IsEnd()) {
        // Drain the remaining superframe, into the frame buffer whose samples are not used
        while ((m_num_frames % m_frames_in_superframe) != 0) {
            ret = ratrac9_decode_s16(m_decoder, in_buf.data(), in_buf.size(),
                                     reinterpret_cast<s16*>(m_pcm_buffer.data()), planar,
                                     &bytes_used);
            if (ret == 0) {
                consume(bytes_used);
            }
            result.frames_decoded += 1;
            m_num_frames += 1;
        }
        consume(m_superframe_bytes_remain);
        m_superframe_bytes_remain = m_superframe_bytes;
        m_num_frames = 0;
    }

    return result;
}

AjmSidebandFormat AjmAt9Decoder::GetFormat() const {
    return AjmSidebandFormat{
        .num_channels = m_channels,
        .channel_mask = GetChannelMask(m_channels),
        .sampl_freq = m_sample_rate,
        .sample_encoding = m_format,
        .bitrate = m_frames_in_superframe && m_frame_samples
                       ? u32((u64(m_sample_rate) * m_superframe_bytes * 8) /
                             (m_frames_in_superframe * m_frame_samples))
                       : 0,
        .reserved = 0,
    };
}

u32 AjmAt9Decoder::GetNextFrameSize(const AjmInstanceGapless& gapless) const {
    const auto skip_samples = std::min<u32>(gapless.current.skip_samples, m_frame_samples);
    const auto samples =
        gapless.init.total_samples != 0
            ? std::min<u32>(gapless.current.total_samples, m_frame_samples - skip_samples)
            : m_frame_samples - skip_samples;
    return samples * m_channels * GetPCMSize(m_format);
}

} // namespace Libraries::Ajm
