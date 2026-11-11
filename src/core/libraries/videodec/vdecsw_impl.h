// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <thread>
#include <vector>

#include <rthreads/retro_eventcount.h>

#include <retro_atomic.h>
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
    // The game's calls post to the worker and take its results through rings; nothing
    // takes a lock. The Sync calls wait on an eventcount, as the game asked to wait.
    static constexpr u32 CommandSlots = 64; // inputs not yet synced, and other commands
    static constexpr u32 FrameSlots = 32;   // pictures decoded ahead of the game's buffers

    enum class CommandType : u32 { Input, Flush, Reset };

    // Bounded MPMC-style cell: `seq` says whose turn it is. The access unit buffer is kept
    // with the slot and reused, so it grows only for a larger access unit than before.
    struct Command {
        retro_atomic_int_t seq{0};
        CommandType type = CommandType::Input;
        u32 epoch = 0;
        OrbisVdecswInputData input{};
        std::vector<u8> au;
    };

    struct CompletedInput {
        void* au_data = nullptr;
        u32 output_frame_count = 0;
        s32 result = 0;
        u32 epoch = 0;
    };

    // A picture decoded ahead of the game's output buffer, in NV12, in its slot's buffer.
    struct DecodedFrame {
        std::vector<u8> nv12;
        u32 width = 0;
        u32 height = 0;
        u32 epoch = 0;
        Videodec::PictureMeta meta;
    };

    bool PostCommand(CommandType type, const OrbisVdecswInputData* input);
    void WorkerLoop();
    void ProcessInput(Command& command);
    void ProcessFlush(u32 epoch);
    void CompleteInput(void* au_data, u32 frame_count, s32 result, u32 epoch);
    u32 QueueFrame(u32 epoch);
    // The game's side: the oldest result of the current epoch, the stale ones dropped.
    bool PeekCompleted(CompletedInput& out);
    DecodedFrame* PeekFrame();
    void PopFrame();
    bool Finalized() const;
    s32 TakeOutput(OrbisVdecswOutputInfo& outputInfo, const OrbisVdecswFrameBuffer& frameBuffer);
    s32 WriteFrame(OrbisVdecswOutputInfo& outputInfo, const DecodedFrame& frame,
                   const OrbisVdecswFrameBuffer& frameBuffer, bool isLastFrame);
    void ResetOutputInfo(OrbisVdecswOutputInfo& outputInfo);

    Videodec::SwDecoder m_decoder;
    bool m_is_avc = false;

    std::array<Command, CommandSlots> m_commands;
    retro_atomic_int_t m_command_tail{0}; // posters
    u32 m_command_head = 0;             // the worker

    std::array<CompletedInput, CommandSlots> m_completed;
    retro_atomic_int_t m_completed_tail{0}; // the worker
    retro_atomic_int_t m_completed_head{0}; // the game

    std::array<DecodedFrame, FrameSlots> m_frames;
    retro_atomic_int_t m_frame_tail{0}; // the worker
    retro_atomic_int_t m_frame_head{0}; // the game

    retro_atomic_int_t m_epoch{0}; // bumped by Reset
    retro_atomic_int_t m_finalized_epoch{
        static_cast<int>(~0u)}; // the epoch whose sequence the worker finished
    retro_atomic_int_t m_unsynced_inputs{0};
    retro_atomic_int_t m_output_set{0};
    OrbisVdecswFrameBuffer m_pending_output{};
    retro_atomic_int_t m_stop{0};

    retro_eventcount_t m_worker_ec{}; // commands, frame slots, stop
    retro_eventcount_t m_game_ec{};   // results, pictures, the end of a sequence, stop
    bool m_ec_ok = false;
    std::thread m_worker_thread;
};

} // namespace Libraries::Vdecsw
