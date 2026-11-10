// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstring>

#include "vdecsw_impl.h"

#include "common/alignment.h"
#include "common/assert.h"
#include "common/logging/log.h"
#include "common/thread.h"
#include "video_utils.h"
#include "videodec_error.h"

namespace Libraries::Vdecsw {

VdecDecoder::VdecDecoder(const OrbisVdecswDecoderConfigInfo& config_info,
                         const OrbisVdecswDecoderMemoryInfo& memory_info)
    : m_decoder(config_info.codec_type != OrbisVdecswCodecType::Avc),
      m_is_avc(config_info.codec_type == OrbisVdecswCodecType::Avc) {
    m_worker_thread =
        std::jthread([this](std::stop_token stop_token) { WorkerLoop(std::move(stop_token)); });
}

VdecDecoder::~VdecDecoder() {
    m_worker_thread.request_stop();
    m_input_cv.notify_all();
    m_output_cv.notify_all();
    m_worker_thread.join();
}

s32 VdecDecoder::SetDecodeInput(const OrbisVdecswInputData& input_data) {
    if (!input_data.au_data) {
        LOG_ERROR(Lib_Vdecsw, "ORBIS_VDECSW_ERROR_ACCESS_UNIT_POINTER");
        return ORBIS_VDECSW_ERROR_ACCESS_UNIT_POINTER;
    }
    if (input_data.au_size == 0) {
        LOG_ERROR(Lib_Vdecsw, "ORBIS_VDECSW_ERROR_ACCESS_UNIT_SIZE");
        return ORBIS_VDECSW_ERROR_ACCESS_UNIT_SIZE;
    }

    // The access unit data is copied because the game is allowed to reuse the
    // buffer as soon as this call returns.
    VdecSwCommand command{};
    command.type = VdecSwCommand::Type::Input;
    command.input = input_data;
    command.au_data =
        std::vector((u8*)input_data.au_data, (u8*)input_data.au_data + input_data.au_size);

    {
        std::scoped_lock lock{m_mutex};
        m_queue.push_back(std::move(command));
        ++m_unsynced_inputs;
    }
    m_input_cv.notify_one();
    return ORBIS_OK;
}

s32 VdecDecoder::SyncDecodeInput(OrbisVdecswInputResult& input_result) {
    std::unique_lock lock{m_mutex};
    m_input_cv.wait(lock, [&] {
        return !m_completed_inputs.empty() || m_unsynced_inputs == 0 ||
               m_worker_thread.get_stop_token().stop_requested();
    });

    if (m_completed_inputs.empty()) {
        LOG_ERROR(Lib_Vdecsw, "ORBIS_VDECSW_ERROR_INPUT_QUEUE_EMPTY");
        return ORBIS_VDECSW_ERROR_INPUT_QUEUE_EMPTY;
    }

    const CompletedInput entry = std::move(m_completed_inputs.front());
    m_completed_inputs.pop_front();
    --m_unsynced_inputs;

    input_result.decoded_au = entry.au_data;
    input_result.output_frame_count = entry.output_frame_count;
    input_result.reserved0 = 0;
    return entry.result;
}

s32 VdecDecoder::TrySyncDecodeInput(OrbisVdecswInputResult& input_result) {
    std::scoped_lock lock{m_mutex};
    if (m_completed_inputs.empty()) {
        if (m_unsynced_inputs == 0) {
            LOG_ERROR(Lib_Vdecsw, "ORBIS_VDECSW_ERROR_INPUT_QUEUE_EMPTY");
            return ORBIS_VDECSW_ERROR_INPUT_QUEUE_EMPTY;
        }
        LOG_TRACE(Lib_Vdecsw, "ORBIS_VDECSW_ERROR_DECODE_PENDING");
        return ORBIS_VDECSW_ERROR_DECODE_PENDING;
    }

    const CompletedInput entry = std::move(m_completed_inputs.front());
    m_completed_inputs.pop_front();
    --m_unsynced_inputs;

    input_result.decoded_au = entry.au_data;
    input_result.output_frame_count = entry.output_frame_count;
    input_result.reserved0 = 0;
    return entry.result;
}

s32 VdecDecoder::SetDecodeOutput(const OrbisVdecswFrameBuffer& frame_buffer) {
    if (!frame_buffer.frame_buffer) {
        LOG_ERROR(Lib_Vdecsw, "ORBIS_VDECSW_ERROR_FRAME_BUFFER_POINTER");
        return ORBIS_VDECSW_ERROR_FRAME_BUFFER_POINTER;
    }
    if (frame_buffer.frame_buffer_size == 0) {
        LOG_ERROR(Lib_Vdecsw, "ORBIS_VDECSW_ERROR_FRAME_BUFFER_SIZE");
        return ORBIS_VDECSW_ERROR_FRAME_BUFFER_SIZE;
    }

    std::scoped_lock lock{m_mutex};
    if (m_pending_output.has_value()) {
        LOG_ERROR(Lib_Vdecsw, "ORBIS_VDECSW_ERROR_OUTPUT_BUFFER_FULL");
        return ORBIS_VDECSW_ERROR_OUTPUT_BUFFER_FULL;
    }

    m_pending_output = frame_buffer;
    return ORBIS_OK;
}

s32 VdecDecoder::SyncDecodeOutput(OrbisVdecswOutputInfo& output_info) {
    output_info.is_valid = false;
    output_info.is_last_frame = false;
    output_info.is_error_frame = false;
    output_info.is_discarded_frame = false;
    output_info.picture_count = 0;

    std::unique_lock lock{m_mutex};
    if (!m_pending_output.has_value()) {
        LOG_ERROR(Lib_Vdecsw, "ORBIS_VDECSW_ERROR_OUTPUT_BUFFER_EMPTY");
        return ORBIS_VDECSW_ERROR_OUTPUT_BUFFER_EMPTY;
    }

    const OrbisVdecswFrameBuffer frame_buffer = *m_pending_output;
    m_pending_output.reset();

    m_output_cv.wait(lock, [&] {
        return !m_frame_queue.empty() || m_finalized ||
               m_worker_thread.get_stop_token().stop_requested();
    });

    if (m_frame_queue.empty()) {
        // The sequence was finalized and all frames have been returned.
        output_info.is_last_frame = m_finalized;
        return ORBIS_OK;
    }

    DecodedFrame frame = std::move(m_frame_queue.front());
    m_frame_queue.pop_front();
    const bool is_last_frame = m_finalized && m_frame_queue.empty();
    lock.unlock();

    return WriteFrame(output_info, frame, frame_buffer, is_last_frame);
}

s32 VdecDecoder::TrySyncDecodeOutput(OrbisVdecswOutputInfo& output_info) {
    output_info.is_valid = false;
    output_info.is_last_frame = false;
    output_info.is_error_frame = false;
    output_info.is_discarded_frame = false;
    output_info.picture_count = 0;

    std::unique_lock lock{m_mutex};
    if (!m_pending_output.has_value()) {
        LOG_ERROR(Lib_Vdecsw, "ORBIS_VDECSW_ERROR_OUTPUT_BUFFER_EMPTY");
        return ORBIS_VDECSW_ERROR_OUTPUT_BUFFER_EMPTY;
    }

    if (m_frame_queue.empty()) {
        if (m_finalized) {
            output_info.is_last_frame = true;
            m_pending_output.reset();
            return ORBIS_OK;
        }
        LOG_TRACE(Lib_Vdecsw, "ORBIS_VDECSW_ERROR_DECODE_PENDING");
        return ORBIS_VDECSW_ERROR_DECODE_PENDING;
    }

    DecodedFrame frame = std::move(m_frame_queue.front());
    m_frame_queue.pop_front();
    const bool is_last_frame = m_finalized && m_frame_queue.empty();
    const OrbisVdecswFrameBuffer frame_buffer = *m_pending_output;
    m_pending_output.reset();
    lock.unlock();

    return WriteFrame(output_info, frame, frame_buffer, is_last_frame);
}

s32 VdecDecoder::FinalizeDecodeSequence() {
    VdecSwCommand command{};
    command.type = VdecSwCommand::Type::Flush;
    {
        std::scoped_lock lock{m_mutex};
        m_queue.push_back(std::move(command));
    }
    m_input_cv.notify_one();
    return ORBIS_OK;
}

s32 VdecDecoder::Reset() {
    VdecSwCommand command{};
    command.type = VdecSwCommand::Type::Reset;
    {
        std::scoped_lock lock{m_mutex};
        m_pending_output.reset();
        m_queue.push_back(std::move(command));
    }
    m_input_cv.notify_one();
    return ORBIS_OK;
}

void VdecDecoder::WorkerLoop(std::stop_token stop_token) {
    Common::SetCurrentThreadName("shadPS4:VdecswWorker");

    std::unique_lock lock{m_mutex};
    while (!stop_token.stop_requested()) {
        m_input_cv.wait(lock, stop_token, [&] { return !m_queue.empty(); });
        if (stop_token.stop_requested() && m_queue.empty()) {
            break;
        }

        VdecSwCommand command = std::move(m_queue.front());
        m_queue.pop_front();
        lock.unlock();

        switch (command.type) {
        case VdecSwCommand::Type::Input:
            ProcessInput(command);
            break;
        case VdecSwCommand::Type::Flush:
            ProcessFlush();
            break;
        case VdecSwCommand::Type::Reset:
            ProcessReset();
            break;
        default:
            break;
        }

        lock.lock();
    }
}

void VdecDecoder::ProcessInput(VdecSwCommand& command) {
    if (!m_decoder.IsOpen()) {
        CompleteInput(command.input.au_data, 0, ORBIS_VDECSW_ERROR_API_FAIL);
        return;
    }
    const Videodec::PictureMeta meta{
        .pts = command.input.pts_data,
        .dts = command.input.dts_data,
        .attached = command.input.attached_data,
    };
    const int ret = m_decoder.Decode(command.au_data.data(), command.au_data.size(), meta);
    if (ret < 0) {
        LOG_ERROR(Lib_Vdecsw, "Error decoding the access unit: {}", ret);
        CompleteInput(command.input.au_data, 0, ORBIS_VDECSW_ERROR_API_FAIL);
        return;
    }
    CompleteInput(command.input.au_data, ret ? QueueFrame() : 0, ORBIS_OK);
}

void VdecDecoder::ProcessFlush() {
    // The pictures still held for display order
    while (m_decoder.IsOpen() && m_decoder.Drain()) {
        QueueFrame();
    }

    {
        std::scoped_lock lock{m_mutex};
        m_finalized = true;
    }
    m_output_cv.notify_all();
}

void VdecDecoder::ProcessReset() {
    m_decoder.Reset();

    std::scoped_lock lock{m_mutex};
    m_unsynced_inputs -= (s32)m_completed_inputs.size();
    m_completed_inputs.clear();
    ClearFrameQueue();
    m_finalized = false;
}

void VdecDecoder::CompleteInput(void* au_data, u32 frame_count, s32 result) {
    {
        std::scoped_lock lock{m_mutex};
        m_completed_inputs.push_back(CompletedInput{au_data, frame_count, result});
    }
    m_input_cv.notify_all();
}

u32 VdecDecoder::QueueFrame() {
    DecodedFrame frame;
    {
        std::scoped_lock lock{m_mutex};
        if (!m_free_frames.empty()) {
            frame.nv12 = std::move(m_free_frames.back());
            m_free_frames.pop_back();
        }
    }
    frame.nv12.resize(m_decoder.Nv12Size());
    if (!m_decoder.WriteNV12(frame.nv12.data(), frame.nv12.size())) {
        return 0;
    }
    frame.width = m_decoder.Width();
    frame.height = m_decoder.Height();
    frame.meta = m_decoder.Meta();
    {
        std::scoped_lock lock{m_mutex};
        m_frame_queue.push_back(std::move(frame));
    }
    m_output_cv.notify_all();
    return 1;
}

s32 VdecDecoder::WriteFrame(OrbisVdecswOutputInfo& output_info, DecodedFrame& frame,
                            const OrbisVdecswFrameBuffer& frame_buffer, bool is_last_frame) {
    const u64 info_size =
        m_is_avc ? sizeof(OrbisVdecswAvcPictureInfo) : sizeof(OrbisVdecswHevcPictureInfo);
    const bool fits = frame_buffer.frame_buffer_size >= info_size &&
                      frame.nv12.size() <= frame_buffer.frame_buffer_size - info_size;
    if (fits) {
        std::memcpy(frame_buffer.frame_buffer, frame.nv12.data(), frame.nv12.size());
    }
    {
        std::scoped_lock lock{m_mutex};
        m_free_frames.push_back(std::move(frame.nv12));
    }
    if (!fits) {
        LOG_ERROR(Lib_Vdecsw, "The {}x{} picture does not fit the frame buffer", frame.width,
                  frame.height);
        return ORBIS_VDECSW_ERROR_API_FAIL;
    }

    const auto width = Common::AlignUp<u32>(frame.width, 16);
    const auto pitch = Common::AlignUp<u32>(frame.width, 64);
    const auto height = Common::AlignUp<u32>(frame.height, 16);

    output_info.is_valid = true;
    output_info.is_last_frame = is_last_frame;
    output_info.picture_count = 1; // TODO: 2 pictures for interlaced video
    output_info.codec_type = m_is_avc ? OrbisVdecswCodecType::Avc : OrbisVdecswCodecType::Hevc;
    output_info.frame_width = width;
    output_info.frame_pitch = pitch;
    output_info.frame_height = height;
    output_info.frame_buffer = frame_buffer.frame_buffer;
    output_info.frame_buffer_size = (pitch * height * 3) / 2;

    // Only set frameFormat and framePitchInBytes if the game uses the newer struct version.
    if (output_info.this_size == sizeof(OrbisVdecswOutputInfo)) {
        output_info.frame_format = 0;
        output_info.frame_pitch_in_bytes = pitch;
    }

    if (m_is_avc) {
        auto& picture_info = *(OrbisVdecswAvcPictureInfo*)((u8*)output_info.frame_buffer +
                                                           output_info.frame_buffer_size);

        picture_info = {};
        picture_info.is_valid = true;

        picture_info.pts_data = frame.meta.pts;
        picture_info.dts_data = frame.meta.dts;
        picture_info.attached_data = frame.meta.attached;

        picture_info.frame_crop_top_offset = 0;
        picture_info.frame_crop_left_offset = 0;
        picture_info.frame_crop_right_offset = pitch - frame.width;
        picture_info.frame_crop_bottom_offset = height - frame.height;
    } else {
        auto& picture_info = *(OrbisVdecswHevcPictureInfo*)((u8*)output_info.frame_buffer +
                                                            output_info.frame_buffer_size);

        picture_info = {};
        picture_info.is_valid = true;

        picture_info.pts_data = frame.meta.pts;
        picture_info.dts_data = frame.meta.dts;
        picture_info.attached_data = frame.meta.attached;

        picture_info.pic_width_in_luma_samples = frame.width;
        picture_info.pic_height_in_luma_samples = frame.height;

        picture_info.frame_crop_top_offset = 0;
        picture_info.frame_crop_left_offset = 0;
        picture_info.frame_crop_right_offset = pitch - frame.width;
        picture_info.frame_crop_bottom_offset = height - frame.height;
    }

    return ORBIS_OK;
}

void VdecDecoder::ClearFrameQueue() {
    while (!m_frame_queue.empty()) {
        m_free_frames.push_back(std::move(m_frame_queue.front().nv12));
        m_frame_queue.pop_front();
    }
}

} // namespace Libraries::Vdecsw
