// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/polyfill_thread.h"
#include "core/libraries/videoout/video_out.h"

#include <array>

#include <retro_atomic.h>
#include <retro_spsc.h>
#include <rthreads/retro_eventcount.h>
#include <rthreads/retro_seqlock.h>

namespace Vulkan {
struct Frame;
}

namespace Libraries::VideoOut {

struct VideoOutPort {
    SceVideoOutResolutionStatus resolution;
    std::array<VideoOutBuffer, MaxDisplayBuffers> buffer_slots;
    std::array<u64, MaxDisplayBuffers> buffer_labels; // should be contiguous in memory
    static_assert(sizeof(buffer_labels[0]) == 8u);
    std::array<BufferAttributeGroup, MaxDisplayBufferGroups> groups;
    /// The present thread's part of the flip status (count, times, flip_arg, current_buffer),
    /// written by it alone under flip_seq; the counts the submitters change are flip_pending
    /// and gc_queue, and submit_tsc. GetFlipStatus puts them together.
    FlipStatus flip_status;
    retro_seqlock_t flip_seq{};
    retro_atomic_int_t flip_pending{};
    retro_atomic_int_t gc_queue{};
    retro_atomic_64_t submit_tsc{};
    /// Written by the present thread alone, under vblank_seq; vblank_ec is notified after each.
    SceVideoOutVblankStatus vblank_status;
    retro_seqlock_t vblank_seq{};
    retro_eventcount_t vblank_ec{};
    /// The event queues to tell; a slot holds 0 when free, and is taken and freed with a
    /// compare-exchange, so the present thread walks them while the game adds and deletes.
    static constexpr size_t MaxEventQueues = 16;
    using EventQueues = std::array<retro_atomic_64_t, MaxEventQueues>;
    EventQueues flip_events{};
    EventQueues vblank_events{};
    /// The GPU thread waiting on a VO label parks on vo_ec; a flip that clears one notifies it.
    retro_eventcount_t vo_ec{};
    /// Close asks the present thread to clear what only it writes.
    retro_atomic_int_t reset_status{0};
    int flip_rate = 0;
    int prev_index = -1;
    bool is_open = false;
    bool is_hdr = false;

    VideoOutPort() {
        retro_seqlock_init(&flip_seq);
        retro_seqlock_init(&vblank_seq);
        retro_eventcount_init(&vblank_ec);
        retro_eventcount_init(&vo_ec);
    }
    ~VideoOutPort() {
        retro_eventcount_free(&vo_ec);
        retro_eventcount_free(&vblank_ec);
    }

    static bool AddEventQueue(EventQueues& queues, Kernel::OrbisKernelEqueue eq) {
        for (auto& slot : queues) {
            if (retro_atomic_cas_64(&slot, 0, eq)) {
                return true;
            }
        }
        return false;
    }
    static void RemoveEventQueue(EventQueues& queues, Kernel::OrbisKernelEqueue eq) {
        for (auto& slot : queues) {
            if (retro_atomic_cas_64(&slot, eq, 0)) {
                return;
            }
        }
    }

    FlipStatus GetFlipStatus() {
        FlipStatus status;
        int seq;
        do {
            seq = retro_seqlock_read_begin(&flip_seq);
            status = flip_status;
        } while (retro_seqlock_read_retry(&flip_seq, seq));
        status.submit_tsc = static_cast<u64>(retro_atomic_load_relaxed_64(&submit_tsc));
        status.gc_queue_num = static_cast<s32>(retro_atomic_load_relaxed_int(&gc_queue));
        status.flip_pending_num = static_cast<s32>(retro_atomic_load_relaxed_int(&flip_pending));
        return status;
    }

    SceVideoOutVblankStatus GetVblankStatus() {
        SceVideoOutVblankStatus status;
        int seq;
        do {
            seq = retro_seqlock_read_begin(&vblank_seq);
            status = vblank_status;
        } while (retro_seqlock_read_retry(&vblank_seq, seq));
        return status;
    }

    /// Until the next vblank.
    void WaitVblank() {
        const u64 prev_count = GetVblankStatus().count;
        for (;;) {
            const int key = retro_eventcount_prepare_wait(&vblank_ec);
            if (GetVblankStatus().count != prev_count) {
                retro_eventcount_cancel_wait(&vblank_ec);
                return;
            }
            retro_eventcount_commit_wait(&vblank_ec, key);
        }
    }

    s32 FindFreeGroup() const {
        s32 index = 0;
        while (index < groups.size() && groups[index].is_occupied) {
            index++;
        }
        return index;
    }

    bool IsVoLabel(const u64* address) const {
        const u64* start = &buffer_labels[0];
        const u64* end = &buffer_labels[MaxDisplayBuffers - 1];
        return address >= start && address <= end;
    }

    void WaitVoLabel(auto&& pred) {
        while (!pred()) {
            const int key = retro_eventcount_prepare_wait(&vo_ec);
            if (pred()) {
                retro_eventcount_cancel_wait(&vo_ec);
                return;
            }
            retro_eventcount_commit_wait(&vo_ec, key);
        }
    }

    void SignalVoLabel() {
        retro_eventcount_notify(&vo_ec);
    }

    [[nodiscard]] int NumRegisteredBuffers() const {
        return std::count_if(buffer_slots.cbegin(), buffer_slots.cend(),
                             [](auto& buffer) { return buffer.group_index != -1; });
    }
};

struct ServiceThreadParams {
    u32 unknown;
    bool set_priority;
    u32 priority;
    bool set_affinity;
    u64 affinity;
};

class VideoOutDriver {
public:
    VideoOutDriver(u32 width, u32 height);
    ~VideoOutDriver();

    int Open(const ServiceThreadParams* params);
    void Close(s32 handle);

    VideoOutPort* GetPort(s32 handle);

    int RegisterBuffers(VideoOutPort* port, s32 startIndex, void* const* addresses, s32 bufferNum,
                        const BufferAttribute* attribute);
    int UnregisterBuffers(VideoOutPort* port, s32 attributeIndex);
    int ChangeBufferAttribute(VideoOutPort* port, s32 bufferIndex,
                              const BufferAttribute* attribute);

    bool SubmitFlip(VideoOutPort* port, s32 index, s64 flip_arg, bool is_eop = false);

private:
    struct Request {
        Vulkan::Frame* frame;
        VideoOutPort* port;
        s64 flip_arg;
        s32 index;
        bool eop;

        operator bool() const noexcept {
            return frame != nullptr;
        }
    };

    void Flip(const Request& req);
    void DrawBlankFrame(); // Video port out not open
    void DrawLastFrame();  // Used when there is no flip request
    void SubmitFlipInternal(VideoOutPort* port, s32 index, s64 flip_arg, bool is_eop = false);
    void PresentThread(std::stop_token token);
    void ResetStatus();

    VideoOutPort main_port{};
    /// Flip requests, from the GPU thread (which runs every flip, EOP or not) to the present
    /// thread; a full ring has the GPU thread wait on request_taken_ec.
    retro_spsc_t requests{};
    retro_eventcount_t request_taken_ec{};
    std::jthread present_thread;
};

} // namespace Libraries::VideoOut
