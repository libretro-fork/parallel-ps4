// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <cstring>

#include <formats/raac.h>

#include "ajm.h"
#include "ajm_aac.h"
#include "ajm_result.h"

namespace Libraries::Ajm {

namespace {

class BitWriter {
public:
    explicit BitWriter(std::span<u8> out) : m_out(out) {
        std::fill(m_out.begin(), m_out.end(), u8{0});
    }

    void Put(u32 value, u32 bits) {
        while (bits--) {
            if ((value >> bits) & 1) {
                m_out[m_pos >> 3] |= u8(0x80 >> (m_pos & 7));
            }
            ++m_pos;
        }
    }

    void Align() {
        m_pos = (m_pos + 7) & ~size_t{7};
    }

    size_t Bytes() const {
        return (m_pos + 7) >> 3;
    }

private:
    std::span<u8> m_out;
    size_t m_pos = 0;
};

// The AudioSpecificConfig of a raw stream: AAC-LC at the given rate, with a program config
// element for the layouts that have no channel configuration of their own: 6.1 (C, L/R,
// Ls/Rs, Cs, LFE) and 7.1 (C, L/R, Ls/Rs, Lrs/Rrs, LFE).
size_t MakeRawConfig(std::span<u8> out, u32 sampling_freq_type, u32 channels) {
    BitWriter w(out);
    const u32 config = channels == 0 ? 2 : channels <= 6 ? channels : 0;
    w.Put(2, 5); // AAC LC
    w.Put(sampling_freq_type, 4);
    w.Put(config, 4);
    w.Put(0, 3); // 1024 frames, no core coder, no extension
    if (config == 0) {
        const bool back_pair = channels == 8;
        w.Put(0, 4); // element_instance_tag
        w.Put(1, 2); // object_type: LC
        w.Put(sampling_freq_type, 4);
        w.Put(2, 4);              // front: C, L/R
        w.Put(1, 4);              // side: Ls/Rs
        w.Put(1, 4);              // back: Cs, or Lrs/Rrs
        w.Put(1, 2);              // LFE
        w.Put(0, 3);              // assoc data
        w.Put(0, 4);              // coupling channels
        w.Put(0, 3);              // no mixdowns
        w.Put(0, 1), w.Put(0, 4); // front SCE 0
        w.Put(1, 1), w.Put(0, 4); // front CPE 0
        w.Put(1, 1), w.Put(1, 4); // side CPE 1
        w.Put(back_pair ? 1 : 0, 1);
        w.Put(back_pair ? 2 : 1, 4); // back CPE 2, or SCE 1
        w.Put(0, 4);                 // LFE 0
        w.Align();
        w.Put(0, 8); // comment_field_bytes
    }
    return w.Bytes();
}

} // namespace

AjmAacDecoder::AjmAacDecoder(AjmFormatEncoding format, AjmAacCodecFlags flags, u32 channels)
    : m_format(format), m_flags(flags), m_channels(channels),
      m_skip_frames(True(flags & AjmAacCodecFlags::EnableNondelayOutput) ? 0 : 2) {}

AjmAacDecoder::~AjmAacDecoder() {
    Close();
}

void AjmAacDecoder::Close() {
    if (m_decoder) {
        raac_close(m_decoder);
        m_decoder = nullptr;
    }
    m_asc_size = 0;
}

bool AjmAacDecoder::Open(const u8* asc, size_t asc_size) {
    if (m_decoder && asc_size == m_asc_size && std::memcmp(asc, m_asc.data(), asc_size) == 0) {
        return true;
    }
    Close();
    if (asc_size > m_asc.size()) {
        return false;
    }
    m_decoder = raac_open(asc, asc_size);
    if (!m_decoder) {
        LOG_ERROR(Lib_Ajm, "Unsupported AAC configuration");
        return false;
    }
    // Games take the channels in the WAVE order
    raac_set_channel_order(m_decoder, RAAC_ORDER_WAVE);
    std::memcpy(m_asc.data(), asc, asc_size);
    m_asc_size = asc_size;
    return true;
}

void AjmAacDecoder::Reset() {
    if (m_init_params.config_type == ConfigType::RAW) {
        std::array<u8, 32> asc;
        Open(asc.data(), MakeRawConfig(asc, m_init_params.sampling_freq_type, m_channels));
    }
    // An ADTS stream opens with its first frame's header
    if (m_decoder) {
        raac_reset(m_decoder);
    }
    m_last_frame_bytes = 0;
    m_skip_frames = True(m_flags & AjmAacCodecFlags::EnableNondelayOutput) ? 0 : 2;
}

void AjmAacDecoder::Initialize(const void* buffer, u32 buffer_size) {
    ASSERT(buffer_size == 8);
    m_init_params = *reinterpret_cast<const InitializeParameters*>(buffer);
    Reset();
}

void AjmAacDecoder::GetInfo(void* out_info) const {
    auto* codec_info = reinterpret_cast<AjmSidebandDecM4aacCodecInfo*>(out_info);
    *codec_info = {
        // SBR doubles the frame length
        .heaac = m_decoder && raac_frame_len(m_decoder) == 2048,
    };
}

AjmSidebandFormat AjmAacDecoder::GetFormat() const {
    if (!m_decoder) {
        return {.sample_encoding = m_format};
    }
    const u32 channels = raac_channels(m_decoder);
    const u32 rate = raac_sample_rate(m_decoder);
    return {
        .num_channels = channels,
        .channel_mask = GetChannelMask(channels),
        .sampl_freq = rate,
        .sample_encoding = m_format,
        .bitrate = static_cast<u32>(u64{m_last_frame_bytes} * 8 * rate / raac_frame_len(m_decoder)),
    };
}

u32 AjmAacDecoder::GetMinimumInputSize() const {
    return 0;
}

u32 AjmAacDecoder::GetNextFrameSize(const AjmInstanceGapless& gapless) const {
    if (!m_decoder) {
        return 0;
    }
    const u32 frame = raac_frame_len(m_decoder);
    const auto skip_samples = std::min<u32>(gapless.current.skip_samples, frame);
    const auto samples = gapless.init.total_samples != 0
                             ? std::min<u32>(gapless.current.total_samples, frame - skip_samples)
                             : frame - skip_samples;
    return samples * raac_channels(m_decoder) * GetPCMSize(m_format);
}

DecoderResult AjmAacDecoder::ProcessData(std::span<u8>& input, SparseOutputBuffer& output,
                                         AjmInstanceGapless& gapless) {
    DecoderResult result{};
    const u8* unit = input.data();
    size_t unit_size = input.size();
    size_t frame_bytes = 0;

    if (m_init_params.config_type == ConfigType::ADTS) {
        raac_adts_t header;
        if (input.size() < 7) {
            result.result = ORBIS_AJM_RESULT_PARTIAL_INPUT;
            return result;
        }
        if (raac_adts_parse(input.data(), input.size(), &header) < 0) {
            LOG_ERROR(Lib_Ajm, "No ADTS header");
            result.result = ORBIS_AJM_RESULT_CODEC_ERROR | ORBIS_AJM_RESULT_FATAL;
            return result;
        }
        if (input.size() < header.frame_size) {
            result.result = ORBIS_AJM_RESULT_PARTIAL_INPUT;
            return result;
        }
        if (!Open(header.asc, sizeof(header.asc))) {
            result.result = ORBIS_AJM_RESULT_CODEC_ERROR | ORBIS_AJM_RESULT_FATAL;
            return result;
        }
        unit += header.header_size;
        unit_size = header.frame_size - header.header_size;
        frame_bytes = header.frame_size;
    } else if (!m_decoder) {
        result.result = ORBIS_AJM_RESULT_CODEC_ERROR | ORBIS_AJM_RESULT_FATAL;
        return result;
    }

    const int frame = raac_decode(m_decoder, unit, unit_size);
    if (frame < 0) {
        LOG_ERROR(Lib_Ajm, "AAC frame decode failed");
        result.result = ORBIS_AJM_RESULT_CODEC_ERROR | ORBIS_AJM_RESULT_FATAL;
        return result;
    }
    if (!frame_bytes) {
        frame_bytes = raac_last_size(m_decoder);
    }
    m_last_frame_bytes = static_cast<u32>(frame_bytes);
    result.frames_decoded += 1;
    input = input.subspan(std::min(frame_bytes, input.size()));

    if (m_skip_frames > 0) {
        --m_skip_frames;
        return result;
    }

    u32 skip_samples = 0;
    if (gapless.current.skip_samples > 0) {
        skip_samples = std::min<u32>(frame, gapless.current.skip_samples);
        gapless.current.skip_samples -= skip_samples;
    }
    const u32 max_samples =
        gapless.init.total_samples != 0 ? gapless.current.total_samples : u32(frame);
    const u32 count = std::min<u32>(frame - skip_samples, max_samples);
    const u32 channels = raac_channels(m_decoder);

    // Straight from the decoder into the game's buffer
    size_t written = 0;
    switch (m_format) {
    case AjmFormatEncoding::S16:
        written = output.WriteFrames<s16>(count, channels, [&](size_t first, size_t n, s16* dst) {
            raac_copy_s16(m_decoder, skip_samples + first, n, dst);
        });
        break;
    case AjmFormatEncoding::S32:
        UNREACHABLE_MSG("NOT IMPLEMENTED");
        break;
    case AjmFormatEncoding::Float:
        written =
            output.WriteFrames<float>(count, channels, [&](size_t first, size_t n, float* dst) {
                raac_copy_f32(m_decoder, skip_samples + first, n, dst);
            });
        break;
    default:
        UNREACHABLE();
    }

    result.samples_written = static_cast<u32>(written / channels);
    gapless.current.skipped_samples += frame - result.samples_written;
    if (gapless.init.total_samples != 0) {
        gapless.current.total_samples -= result.samples_written;
    }
    return result;
}

} // namespace Libraries::Ajm
