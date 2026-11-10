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

namespace {
// Waits on `ec` until `ready` holds.
template <class F>
void WaitFor(retro_eventcount_t* ec, F&& ready) {
    while (!ready()) {
        const int key = retro_eventcount_prepare_wait(ec);
        if (ready()) {
            retro_eventcount_cancel_wait(ec);
            return;
        }
        retro_eventcount_commit_wait(ec, key);
    }
}
} // namespace

VdecDecoder::VdecDecoder(const OrbisVdecswDecoderConfigInfo& config_info,
                         const OrbisVdecswDecoderMemoryInfo& memory_info)
    : m_decoder(config_info.codec_type != OrbisVdecswCodecType::Avc),
      m_is_avc(config_info.codec_type == OrbisVdecswCodecType::Avc) {
    for (u32 i = 0; i < CommandSlots; ++i) {
        m_commands[i].seq.store(i, std::memory_order_relaxed);
    }
    const bool worker_ec = retro_eventcount_init(&m_worker_ec);
    const bool game_ec = retro_eventcount_init(&m_game_ec);
    m_ec_ok = worker_ec && game_ec;
    if (!m_ec_ok) {
        LOG_ERROR(Lib_Vdecsw, "No eventcount: the decoder cannot run");
        if (worker_ec) {
            retro_eventcount_free(&m_worker_ec);
        }
        if (game_ec) {
            retro_eventcount_free(&m_game_ec);
        }
        return;
    }
    m_worker_thread = std::thread([this] { WorkerLoop(); });
}

VdecDecoder::~VdecDecoder() {
    if (!m_ec_ok) {
        return;
    }
    m_stop.store(true, std::memory_order_release);
    retro_eventcount_notify(&m_worker_ec);
    retro_eventcount_notify(&m_game_ec);
    m_worker_thread.join();
    retro_eventcount_free(&m_worker_ec);
    retro_eventcount_free(&m_game_ec);
}

bool VdecDecoder::PostCommand(CommandType type, const OrbisVdecswInputData* input) {
    u32 pos = m_command_tail.load(std::memory_order_relaxed);
    Command* cell;
    for (;;) {
        cell = &m_commands[pos % CommandSlots];
        const u32 seq = cell->seq.load(std::memory_order_acquire);
        const s32 diff = static_cast<s32>(seq - pos);
        if (diff == 0) {
            if (m_command_tail.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                break;
            }
        } else if (diff < 0) {
            return false; // full
        } else {
            pos = m_command_tail.load(std::memory_order_relaxed);
        }
    }
    cell->type = type;
    cell->epoch = m_epoch.load(std::memory_order_relaxed);
    if (input) {
        // The game may reuse its buffer as soon as this call returns
        cell->input = *input;
        const u8* au = static_cast<const u8*>(input->au_data);
        cell->au.assign(au, au + input->au_size);
    }
    cell->seq.store(pos + 1, std::memory_order_release);
    retro_eventcount_notify(&m_worker_ec);
    return true;
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
    if (!m_ec_ok) {
        return ORBIS_VDECSW_ERROR_API_FAIL;
    }
    // Each input gives at most one picture: with the pictures not taken yet, the inputs in
    // flight must fit the picture ring, so the worker never waits for the game here
    const s32 unsynced = m_unsynced_inputs.load(std::memory_order_relaxed);
    const u32 queued =
        m_frame_tail.load(std::memory_order_acquire) - m_frame_head.load(std::memory_order_relaxed);
    if (queued + static_cast<u32>(unsynced) + 1 > FrameSlots) {
        LOG_TRACE(Lib_Vdecsw, "ORBIS_VDECSW_ERROR_OUTPUT_PENDING");
        return ORBIS_VDECSW_ERROR_OUTPUT_PENDING;
    }
    // Room in the results ring too: the inputs not synced yet are bounded by it
    if (unsynced >= static_cast<s32>(CommandSlots) - 2 ||
        !PostCommand(CommandType::Input, &input_data)) {
        LOG_ERROR(Lib_Vdecsw, "ORBIS_VDECSW_ERROR_INPUT_QUEUE_FULL");
        return ORBIS_VDECSW_ERROR_INPUT_QUEUE_FULL;
    }
    m_unsynced_inputs.fetch_add(1, std::memory_order_relaxed);
    return ORBIS_OK;
}

bool VdecDecoder::PeekCompleted(CompletedInput& out) {
    const u32 epoch = m_epoch.load(std::memory_order_relaxed);
    u32 head = m_completed_head.load(std::memory_order_relaxed);
    while (head != m_completed_tail.load(std::memory_order_acquire)) {
        out = m_completed[head % CommandSlots];
        m_completed_head.store(++head, std::memory_order_release);
        if (out.epoch == epoch) {
            return true;
        }
    }
    return false;
}

s32 VdecDecoder::SyncDecodeInput(OrbisVdecswInputResult& input_result) {
    CompletedInput entry{};
    bool got = false;
    if (m_ec_ok) {
        WaitFor(&m_game_ec, [&] {
            got = got || PeekCompleted(entry);
            return got || m_unsynced_inputs.load(std::memory_order_relaxed) == 0 ||
                   m_stop.load(std::memory_order_acquire);
        });
    }
    if (!got) {
        LOG_ERROR(Lib_Vdecsw, "ORBIS_VDECSW_ERROR_INPUT_QUEUE_EMPTY");
        return ORBIS_VDECSW_ERROR_INPUT_QUEUE_EMPTY;
    }
    m_unsynced_inputs.fetch_sub(1, std::memory_order_relaxed);
    input_result.decoded_au = entry.au_data;
    input_result.output_frame_count = entry.output_frame_count;
    input_result.reserved0 = 0;
    return entry.result;
}

s32 VdecDecoder::TrySyncDecodeInput(OrbisVdecswInputResult& input_result) {
    CompletedInput entry{};
    if (!PeekCompleted(entry)) {
        if (m_unsynced_inputs.load(std::memory_order_relaxed) == 0) {
            LOG_ERROR(Lib_Vdecsw, "ORBIS_VDECSW_ERROR_INPUT_QUEUE_EMPTY");
            return ORBIS_VDECSW_ERROR_INPUT_QUEUE_EMPTY;
        }
        LOG_TRACE(Lib_Vdecsw, "ORBIS_VDECSW_ERROR_DECODE_PENDING");
        return ORBIS_VDECSW_ERROR_DECODE_PENDING;
    }
    m_unsynced_inputs.fetch_sub(1, std::memory_order_relaxed);
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
    if (m_output_set.load(std::memory_order_acquire)) {
        LOG_ERROR(Lib_Vdecsw, "ORBIS_VDECSW_ERROR_OUTPUT_BUFFER_FULL");
        return ORBIS_VDECSW_ERROR_OUTPUT_BUFFER_FULL;
    }
    m_pending_output = frame_buffer;
    m_output_set.store(true, std::memory_order_release);
    return ORBIS_OK;
}

VdecDecoder::DecodedFrame* VdecDecoder::PeekFrame() {
    const u32 epoch = m_epoch.load(std::memory_order_relaxed);
    u32 head = m_frame_head.load(std::memory_order_relaxed);
    while (head != m_frame_tail.load(std::memory_order_acquire)) {
        DecodedFrame& frame = m_frames[head % FrameSlots];
        if (frame.epoch == epoch) {
            return &frame;
        }
        m_frame_head.store(++head, std::memory_order_release);
        retro_eventcount_notify(&m_worker_ec);
    }
    return nullptr;
}

void VdecDecoder::PopFrame() {
    m_frame_head.fetch_add(1, std::memory_order_release);
    retro_eventcount_notify(&m_worker_ec);
}

bool VdecDecoder::Finalized() const {
    return m_finalized_epoch.load(std::memory_order_acquire) ==
           m_epoch.load(std::memory_order_relaxed);
}

void VdecDecoder::ResetOutputInfo(OrbisVdecswOutputInfo& output_info) {
    output_info.is_valid = false;
    output_info.is_last_frame = false;
    output_info.is_error_frame = false;
    output_info.is_discarded_frame = false;
    output_info.picture_count = 0;
}

s32 VdecDecoder::TakeOutput(OrbisVdecswOutputInfo& output_info,
                            const OrbisVdecswFrameBuffer& frame_buffer) {
    // Finalized is read before the ring: the worker finishes a sequence after its last picture
    const bool finalized = Finalized();
    DecodedFrame* frame = PeekFrame();
    if (!frame) {
        output_info.is_last_frame = finalized;
        return ORBIS_OK;
    }
    const u32 head = m_frame_head.load(std::memory_order_relaxed);
    const bool is_last_frame =
        finalized && head + 1 == m_frame_tail.load(std::memory_order_acquire);
    const s32 result = WriteFrame(output_info, *frame, frame_buffer, is_last_frame);
    PopFrame();
    return result;
}

s32 VdecDecoder::SyncDecodeOutput(OrbisVdecswOutputInfo& output_info) {
    ResetOutputInfo(output_info);
    if (!m_output_set.load(std::memory_order_acquire)) {
        LOG_ERROR(Lib_Vdecsw, "ORBIS_VDECSW_ERROR_OUTPUT_BUFFER_EMPTY");
        return ORBIS_VDECSW_ERROR_OUTPUT_BUFFER_EMPTY;
    }
    const OrbisVdecswFrameBuffer frame_buffer = m_pending_output;
    m_output_set.store(false, std::memory_order_release);
    if (m_ec_ok) {
        WaitFor(&m_game_ec, [&] {
            return PeekFrame() != nullptr || Finalized() || m_stop.load(std::memory_order_acquire);
        });
    }
    return TakeOutput(output_info, frame_buffer);
}

s32 VdecDecoder::TrySyncDecodeOutput(OrbisVdecswOutputInfo& output_info) {
    ResetOutputInfo(output_info);
    if (!m_output_set.load(std::memory_order_acquire)) {
        LOG_ERROR(Lib_Vdecsw, "ORBIS_VDECSW_ERROR_OUTPUT_BUFFER_EMPTY");
        return ORBIS_VDECSW_ERROR_OUTPUT_BUFFER_EMPTY;
    }
    if (!PeekFrame() && !Finalized()) {
        LOG_TRACE(Lib_Vdecsw, "ORBIS_VDECSW_ERROR_DECODE_PENDING");
        return ORBIS_VDECSW_ERROR_DECODE_PENDING;
    }
    const OrbisVdecswFrameBuffer frame_buffer = m_pending_output;
    m_output_set.store(false, std::memory_order_release);
    return TakeOutput(output_info, frame_buffer);
}

s32 VdecDecoder::FinalizeDecodeSequence() {
    if (!m_ec_ok || !PostCommand(CommandType::Flush, nullptr)) {
        return ORBIS_VDECSW_ERROR_INPUT_QUEUE_FULL;
    }
    return ORBIS_OK;
}

s32 VdecDecoder::Reset() {
    // A new epoch: whatever the worker still hands back from before is dropped as it is read
    m_epoch.fetch_add(1, std::memory_order_relaxed);
    m_output_set.store(false, std::memory_order_release);
    m_unsynced_inputs.store(0, std::memory_order_relaxed);
    CompletedInput stale;
    PeekCompleted(stale);
    PeekFrame();
    if (!m_ec_ok || !PostCommand(CommandType::Reset, nullptr)) {
        return ORBIS_VDECSW_ERROR_INPUT_QUEUE_FULL;
    }
    return ORBIS_OK;
}

void VdecDecoder::WorkerLoop() {
    Common::SetCurrentThreadName("shadPS4:VdecswWorker");
    for (;;) {
        Command* cell = nullptr;
        WaitFor(&m_worker_ec, [&] {
            Command& next = m_commands[m_command_head % CommandSlots];
            if (next.seq.load(std::memory_order_acquire) == m_command_head + 1) {
                cell = &next;
                return true;
            }
            return m_stop.load(std::memory_order_acquire);
        });
        if (!cell) {
            return;
        }
        switch (cell->type) {
        case CommandType::Input:
            ProcessInput(*cell);
            break;
        case CommandType::Flush:
            ProcessFlush(cell->epoch);
            break;
        case CommandType::Reset:
            m_decoder.Reset();
            break;
        }
        cell->seq.store(m_command_head + CommandSlots, std::memory_order_release);
        ++m_command_head;
    }
}

void VdecDecoder::ProcessInput(Command& command) {
    if (command.epoch != m_epoch.load(std::memory_order_relaxed)) {
        return; // posted before a reset
    }
    if (!m_decoder.IsOpen()) {
        CompleteInput(command.input.au_data, 0, ORBIS_VDECSW_ERROR_API_FAIL, command.epoch);
        return;
    }
    const Videodec::PictureMeta meta{
        .pts = command.input.pts_data,
        .dts = command.input.dts_data,
        .attached = command.input.attached_data,
    };
    const int ret = m_decoder.Decode(command.au.data(), command.au.size(), meta);
    if (ret < 0) {
        LOG_ERROR(Lib_Vdecsw, "Error decoding the access unit: {}", ret);
        CompleteInput(command.input.au_data, 0, ORBIS_VDECSW_ERROR_API_FAIL, command.epoch);
        return;
    }
    CompleteInput(command.input.au_data, ret ? QueueFrame(command.epoch) : 0, ORBIS_OK,
                  command.epoch);
}

void VdecDecoder::ProcessFlush(u32 epoch) {
    if (epoch != m_epoch.load(std::memory_order_relaxed)) {
        return;
    }
    // The pictures still held for display order
    while (m_decoder.IsOpen() && m_decoder.Drain()) {
        QueueFrame(epoch);
    }
    m_finalized_epoch.store(epoch, std::memory_order_release);
    retro_eventcount_notify(&m_game_ec);
}

void VdecDecoder::CompleteInput(void* au_data, u32 frame_count, s32 result, u32 epoch) {
    const u32 tail = m_completed_tail.load(std::memory_order_relaxed);
    // SetDecodeInput keeps the inputs not yet synced below the ring's size
    m_completed[tail % CommandSlots] = CompletedInput{au_data, frame_count, result, epoch};
    m_completed_tail.store(tail + 1, std::memory_order_release);
    retro_eventcount_notify(&m_game_ec);
}

u32 VdecDecoder::QueueFrame(u32 epoch) {
    const u32 tail = m_frame_tail.load(std::memory_order_relaxed);
    // Only the pictures a flush hands out can find the ring full: the game takes them next
    WaitFor(&m_worker_ec, [&] {
        return tail - m_frame_head.load(std::memory_order_acquire) < FrameSlots ||
               m_stop.load(std::memory_order_acquire) ||
               epoch != m_epoch.load(std::memory_order_relaxed);
    });
    if (tail - m_frame_head.load(std::memory_order_acquire) >= FrameSlots ||
        epoch != m_epoch.load(std::memory_order_relaxed)) {
        return 0;
    }
    DecodedFrame& frame = m_frames[tail % FrameSlots];
    frame.nv12.resize(m_decoder.Nv12Size());
    if (!m_decoder.WriteNV12(frame.nv12.data(), frame.nv12.size())) {
        return 0;
    }
    frame.width = m_decoder.Width();
    frame.height = m_decoder.Height();
    frame.epoch = epoch;
    frame.meta = m_decoder.Meta();
    m_frame_tail.store(tail + 1, std::memory_order_release);
    retro_eventcount_notify(&m_game_ec);
    return 1;
}

s32 VdecDecoder::WriteFrame(OrbisVdecswOutputInfo& output_info, const DecodedFrame& frame,
                            const OrbisVdecswFrameBuffer& frame_buffer, bool is_last_frame) {
    const u64 info_size =
        m_is_avc ? sizeof(OrbisVdecswAvcPictureInfo) : sizeof(OrbisVdecswHevcPictureInfo);
    if (frame_buffer.frame_buffer_size < info_size ||
        frame.nv12.size() > frame_buffer.frame_buffer_size - info_size) {
        LOG_ERROR(Lib_Vdecsw, "The {}x{} picture does not fit the frame buffer", frame.width,
                  frame.height);
        return ORBIS_VDECSW_ERROR_API_FAIL;
    }
    std::memcpy(frame_buffer.frame_buffer, frame.nv12.data(), frame.nv12.size());

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

} // namespace Libraries::Vdecsw
