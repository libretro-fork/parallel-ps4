// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/alignment.h"
#include "common/logging/log.h"
#include "videodec2_impl.h"
#include "videodec_error.h"

namespace Libraries::Videodec2 {

VdecDecoder::VdecDecoder(const OrbisVideodec2DecoderConfigInfo& config_info,
                         const OrbisVideodec2DecoderMemoryInfo& memory_info)
    : m_decoder(config_info.codec_type != OrbisVideodec2CodecType::Avc) {}

VdecDecoder::~VdecDecoder() = default;

s32 VdecDecoder::Decode(const OrbisVideodec2InputData& input_data,
                        OrbisVideodec2FrameBuffer& frame_buffer,
                        OrbisVideodec2OutputInfo& output_info) {
    frame_buffer.is_accepted = false;
    output_info.is_valid = false;
    output_info.picture_count = 0;

    if (!input_data.au_data) {
        LOG_ERROR(Lib_Vdec2, "Invalid access unit data pointer");
        return ORBIS_VIDEODEC2_ERROR_ACCESS_UNIT_POINTER;
    }
    if (input_data.au_size == 0) {
        LOG_ERROR(Lib_Vdec2, "Invalid access unit size");
        return ORBIS_VIDEODEC2_ERROR_ACCESS_UNIT_SIZE;
    }
    if (!m_decoder.IsOpen()) {
        return ORBIS_VIDEODEC2_ERROR_API_FAIL;
    }

    const Videodec::PictureMeta meta{
        .pts = input_data.pts_data,
        .dts = input_data.dts_data,
        .attached = input_data.attached_data,
    };
    const int ret =
        m_decoder.Decode(static_cast<const u8*>(input_data.au_data), input_data.au_size, meta);
    if (ret < 0) {
        LOG_ERROR(Lib_Vdec2, "Error decoding the access unit: {}", ret);
        return ORBIS_VIDEODEC2_ERROR_API_FAIL;
    }
    return ret ? Output(frame_buffer, output_info) : ORBIS_OK;
}

s32 VdecDecoder::Flush(OrbisVideodec2FrameBuffer& frame_buffer,
                       OrbisVideodec2OutputInfo& output_info) {
    frame_buffer.is_accepted = false;
    output_info.is_valid = false;
    output_info.picture_count = 0;
    if (!m_decoder.IsOpen() || !m_decoder.Drain()) {
        return ORBIS_OK;
    }
    return Output(frame_buffer, output_info);
}

s32 VdecDecoder::Reset() {
    m_decoder.Reset();
    return ORBIS_OK;
}

s32 VdecDecoder::Output(OrbisVideodec2FrameBuffer& frame_buffer,
                        OrbisVideodec2OutputInfo& output_info) {
    const bool is_avc = !m_decoder.IsHevc();
    const u64 info_size =
        is_avc ? sizeof(OrbisVideodec2AvcPictureInfo) : sizeof(OrbisVideodec2HevcPictureInfo);
    if (frame_buffer.frame_buffer_size < info_size ||
        !m_decoder.WriteNV12(static_cast<u8*>(frame_buffer.frame_buffer),
                             frame_buffer.frame_buffer_size - info_size)) {
        LOG_ERROR(Lib_Vdec2, "The {}x{} picture does not fit the frame buffer", m_decoder.Width(),
                  m_decoder.Height());
        return ORBIS_VIDEODEC2_ERROR_API_FAIL;
    }
    frame_buffer.is_accepted = true;

    const auto width = Common::AlignUp<u32>(m_decoder.Width(), 16);
    const auto pitch = m_decoder.Pitch();
    const auto height = m_decoder.AlignedHeight();
    const auto& meta = m_decoder.Meta();

    output_info.is_valid = true;
    output_info.is_error_frame = false;
    output_info.picture_count = 1; // TODO: 2 pictures for interlaced video
    output_info.codec_type = is_avc ? OrbisVideodec2CodecType::Avc : OrbisVideodec2CodecType::Hevc;
    output_info.frame_width = width;
    output_info.frame_pitch = pitch;
    output_info.frame_height = height;
    output_info.frame_buffer = frame_buffer.frame_buffer;
    output_info.frame_buffer_size = m_decoder.Nv12Size();

    // Only set frameFormat and framePitchInBytes if the game uses the newer struct version.
    if (output_info.this_size == sizeof(OrbisVideodec2OutputInfo)) {
        output_info.frame_format = 0;
        output_info.frame_pitch_in_bytes = pitch;
    }

    u8* info = static_cast<u8*>(output_info.frame_buffer) + output_info.frame_buffer_size;
    const auto fill = [&](auto& picture_info) {
        picture_info = {};
        picture_info.is_valid = true;
        picture_info.pts_data = meta.pts;
        picture_info.dts_data = meta.dts;
        picture_info.attached_data = meta.attached;
        picture_info.frame_crop_top_offset = 0;
        picture_info.frame_crop_left_offset = 0;
        picture_info.frame_crop_right_offset = pitch - m_decoder.Width();
        picture_info.frame_crop_bottom_offset = height - m_decoder.Height();
    };
    if (is_avc) {
        fill(*reinterpret_cast<OrbisVideodec2AvcPictureInfo*>(info));
    } else {
        fill(*reinterpret_cast<OrbisVideodec2HevcPictureInfo*>(info));
    }
    return ORBIS_OK;
}

} // namespace Libraries::Videodec2
