// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

#include "vdecsw.h"
#include "video_utils.h"

namespace Libraries::Vdecsw {

class VdecDecoder {
public:
    VdecDecoder(const OrbisVdecswDecoderConfigInfo& configInfo,
                const OrbisVdecswDecoderMemoryInfo& memoryInfo);
    ~VdecDecoder();

    s32 SetDecodeInput(const OrbisVdecswInputData& inputData);
    s32 SyncDecodeInput(OrbisVdecswInputResult& inputResult);
    s32 TrySyncDecodeInput(OrbisVdecswInputResult& inputResult);
    s32 SetDecodeOutput(const OrbisVdecswFrameBuffer& frameBuffer);
    s32 SyncDecodeOutput(OrbisVdecswOutputInfo& outputInfo);
    s32 TrySyncDecodeOutput(OrbisVdecswOutputInfo& outputInfo);
    s32 FinalizeDecodeSequence();
    s32 Reset();

private:
    struct VdecSwCommand {
        enum class Type {
            Input,
            Flush,
            Reset,
        };
        Type type = Type::Input;
        std::vector<u8> au_data;
        OrbisVdecswInputData input = {};
    };

    struct CompletedInput {
        void* au_data = nullptr;
        u32 output_frame_count = 0;
        s32 result = 0;
    };

    void WorkerLoop(std::stop_token stop_token);
    void ProcessInput(VdecSwCommand& item);
    void ProcessFlush();
    void ProcessReset();
    // A picture decoded ahead of the game's output buffer, in NV12. The buffers go back to
    // m_free_frames once written out, so a stream allocates only while it fills its queue.
    struct DecodedFrame {
        std::vector<u8> nv12;
        u32 width = 0;
        u32 height = 0;
        Videodec::PictureMeta meta;
    };

    void CompleteInput(void* au_data, u32 frame_count, s32 result);
    u32 QueueFrame();
    s32 WriteFrame(OrbisVdecswOutputInfo& outputInfo, DecodedFrame& frame,
                   const OrbisVdecswFrameBuffer& frameBuffer, bool isLastFrame);
    void ClearFrameQueue();

private:
    Videodec::SwDecoder m_decoder;
    bool m_is_avc = false;

    std::mutex m_mutex;
    std::condition_variable_any m_input_cv;
    std::condition_variable_any m_output_cv;
    std::jthread m_worker_thread;
    std::deque<VdecSwCommand> m_queue;
    std::deque<CompletedInput> m_completed_inputs;
    s32 m_unsynced_inputs = 0;
    bool m_finalized = false;
    std::optional<OrbisVdecswFrameBuffer> m_pending_output;
    std::deque<DecodedFrame> m_frame_queue;
    std::vector<std::vector<u8>> m_free_frames;
};

} // namespace Libraries::Vdecsw
