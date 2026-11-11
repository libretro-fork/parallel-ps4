// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <coroutine>
#include <cstddef>
#include <exception>
#include <span>
#include <thread>
#include <vector>

#include <queues/mpsc_stack.h>
#include <rthreads/retro_eventcount.h>

#include "common/assert.h"
#include "common/slot_vector.h"
#include "common/types.h"
#include "common/unique_function.h"
#include "video_core/amdgpu/cb_db_extent.h"
#include "video_core/amdgpu/regs.h"

namespace Vulkan {
class Rasterizer;
}

namespace Libraries::VideoOut {
struct VideoOutPort;
}

namespace AmdGpu {

struct Liverpool {
    static constexpr u32 GfxQueueId = 0u;
    static constexpr u32 NumGfxRings = 1u;     // actually 2, but HP is reserved by system software
    static constexpr u32 NumComputePipes = 7u; // actually 8, but #7 is reserved by system software
    static constexpr u32 NumQueuesPerPipe = 8u;
    static constexpr u32 NumComputeRings = NumComputePipes * NumQueuesPerPipe;
    static constexpr u32 NumTotalQueues = NumGfxRings + NumComputeRings;
    static_assert(NumTotalQueues < 64u); // need to fit into u64 bitmap for ffs

    enum ContextRegs : u32 {
        DbZInfo = 0xA010,
        CbColor0Base = 0xA318,
        CbColor1Base = 0xA327,
        CbColor2Base = 0xA336,
        CbColor3Base = 0xA345,
        CbColor4Base = 0xA354,
        CbColor5Base = 0xA363,
        CbColor6Base = 0xA372,
        CbColor7Base = 0xA381,
        CbColor0Cmask = 0xA31F,
        CbColor1Cmask = 0xA32E,
        CbColor2Cmask = 0xA33D,
        CbColor3Cmask = 0xA34C,
        CbColor4Cmask = 0xA35B,
        CbColor5Cmask = 0xA36A,
        CbColor6Cmask = 0xA379,
        CbColor7Cmask = 0xA388,
    };

    Regs regs{};
    u32 index_buffer_num_indices{};
    std::array<CbDbExtent, NUM_COLOR_BUFFERS> last_cb_extent{};
    CbDbExtent last_db_extent{};

public:
    explicit Liverpool();
    ~Liverpool();

    void SubmitGfx(std::span<const u32> dcb, std::span<const u32> ccb);
    void SubmitAsc(u32 gnm_vqid, std::span<const u32> acb);

    void SubmitDone() noexcept {
        mapped_queues[GfxQueueId].ccb_buffer_offset = 0;
        mapped_queues[GfxQueueId].dcb_buffer_offset = 0;
        submit_done.store(true, std::memory_order_release);
        retro_eventcount_notify(&work_ec);
    }

    void WaitGpuIdle() noexcept {
        for (;;) {
            const int key = retro_eventcount_prepare_wait(&idle_ec);
            if (num_submits.load() == 0) {
                retro_eventcount_cancel_wait(&idle_ec);
                return;
            }
            retro_eventcount_commit_wait(&idle_ec, key);
        }
    }

    bool IsGpuIdle() const {
        return num_submits == 0;
    }

    void SetVoPort(Libraries::VideoOut::VideoOutPort* port) {
        vo_port = port;
    }

    void BindRasterizer(Vulkan::Rasterizer* rasterizer_) {
        rasterizer = rasterizer_;
    }

    /// Runs @func on the GPU thread, before its next submission; with @wait_done, waits until
    /// it has run. Any thread; pushed onto a lock-free stack.
    template <bool wait_done = false>
    void SendCommand(auto&& func) {
        if constexpr (wait_done) {
            // On this thread's stack until it has run
            std::atomic<bool> done{false};
            Command command{{}, Common::UniqueFunction<void>{[&func] { func(); }}, &done};
            PushCommand(&command);
            for (;;) {
                const int key = retro_eventcount_prepare_wait(&command_done_ec);
                if (done.load(std::memory_order_acquire)) {
                    retro_eventcount_cancel_wait(&command_done_ec);
                    break;
                }
                retro_eventcount_commit_wait(&command_done_ec, key);
            }
        } else {
            PushCommand(new Command{{}, Common::UniqueFunction<void>{std::move(func)}, nullptr});
        }
    }

    void ReserveCopyBufferSpace() {
        GpuQueue& gfx_queue = mapped_queues[GfxQueueId];
        constexpr size_t GfxReservedSize = 2_MB >> 2;
        gfx_queue.ccb_buffer.reserve(GfxReservedSize);
        gfx_queue.dcb_buffer.reserve(GfxReservedSize);
    }

    inline ComputeProgram& GetCsRegs() {
        return mapped_queues[curr_qid].cs_state;
    }

    struct AscQueueInfo {
        static constexpr size_t Pm4BufferSize = 1024;
        VAddr map_addr;
        u32* read_addr;
        u32 ring_size_dw;
        u32 pipe_id;
        std::array<u32, Pm4BufferSize> tmp_packet;
        u32 tmp_dwords;
    };
    Common::SlotVector<AscQueueInfo> asc_queues{64};

    std::thread::id GetGpuCommandProcessorThread() {
        return gpu_id;
    }

#ifdef __linux__
    u32 GetGpuCommandProcessorThreadId() {
        return gpu_tid;
    }
#endif

private:
    struct Task {
        struct promise_type {
            // Links the task into a queue's lock-free stack (TaskQueue)
            mpsc_stack_node_t node{};

            auto get_return_object() {
                Task task{};
                task.handle = std::coroutine_handle<promise_type>::from_promise(*this);
                return task;
            }
            static constexpr std::suspend_always initial_suspend() noexcept {
                // We want the task to be suspended at start
                return {};
            }
            static constexpr std::suspend_always final_suspend() noexcept {
                return {};
            }
            void unhandled_exception() {
                try {
                    std::rethrow_exception(std::current_exception());
                } catch (const std::exception& e) {
                    UNREACHABLE_MSG("Unhandled exception: {}", e.what());
                }
            }
            void return_void() {}
            struct empty {};
            std::suspend_always yield_value(empty&&) {
                return {};
            }
        };

        using Handle = std::coroutine_handle<promise_type>;
        Handle handle;
    };

    using CmdBuffer = std::pair<std::span<const u32>, std::span<const u32>>;
    CmdBuffer CopyCmdBuffers(std::span<const u32> dcb, std::span<const u32> ccb);
    Task ProcessGraphics(std::span<const u32> dcb);
    Task ProcessCeUpdate(std::span<const u32> ccb);
    template <bool is_indirect = false>
    Task ProcessCompute(std::span<const u32> acb, u32 vqid);

    void ProcessCommands();
    void ProcessCeQueue();
    /// One submission fewer; the last wakes WaitGpuIdle.
    void SubmitFinished();
    void Process(std::stop_token stoken);

    /// Submissions in order: pushed by any thread onto a lock-free stack, and taken by the GPU
    /// thread into a list of its own, oldest first, which only it touches.
    struct TaskQueue {
        mpsc_stack_t incoming{};
        mpsc_stack_node_t* head{};
        mpsc_stack_node_t* tail{};
        size_t size{};

        TaskQueue() {
            mpsc_stack_init(&incoming);
        }
        void Push(Task::Handle handle) {
            mpsc_stack_push(&incoming, &handle.promise().node);
        }
        /// GPU thread: what was pushed since, onto the end of its list.
        void Take() {
            mpsc_stack_node_t* node = mpsc_stack_drain(&incoming);
            if (!node) {
                return;
            }
            node = mpsc_stack_reverse(node);
            if (tail) {
                tail->next = node;
            } else {
                head = node;
            }
            for (; node; node = node->next) {
                tail = node;
                ++size;
            }
        }
        bool Empty() const {
            return head == nullptr;
        }
        Task::Handle Front() const {
            auto* promise = reinterpret_cast<Task::promise_type*>(
                reinterpret_cast<char*>(head) - offsetof(Task::promise_type, node));
            return Task::Handle::from_promise(*promise);
        }
        void Pop() {
            head = head->next;
            if (!head) {
                tail = nullptr;
            }
            --size;
        }
    };

    struct GpuQueue {
        std::atomic<u32> dcb_buffer_offset;
        std::atomic<u32> ccb_buffer_offset;
        std::vector<u32> dcb_buffer;
        std::vector<u32> ccb_buffer;
        TaskQueue submits{};
        ComputeProgram cs_state{};
    };
    std::array<GpuQueue, NumTotalQueues> mapped_queues{};
    TaskQueue ce_submits;
    std::atomic<u32> num_mapped_queues{1u}; // GFX is always available

    /// A function for the GPU thread to run (SendCommand)
    struct Command {
        mpsc_stack_node_t node; // first: the stack links commands through it
        Common::UniqueFunction<void> func;
        /// Set once it has run, where the sender waits; else the command was allocated and
        /// is deleted once it has run.
        std::atomic<bool>* done;
    };
    void PushCommand(Command* command) {
        num_commands.fetch_add(1, std::memory_order_relaxed);
        mpsc_stack_push(&commands, &command->node);
        retro_eventcount_notify(&work_ec);
    }

    VAddr indirect_args_addr{};
    u32 num_counter_pairs{};
    u64 pixel_counter{};

    struct ConstantEngine {
        [[nodiscard]] u32 Diff() const {
            ASSERT_MSG(ce_count >= de_count, "DE counter is ahead of CE");
            return ce_count - de_count;
        }

        u32 ce_count{};
        u32 de_count{};
        static std::array<u8, 48_KB> constants_heap;
    } cblock{};

    Vulkan::Rasterizer* rasterizer{};
    Libraries::VideoOut::VideoOutPort* vo_port{};
    const bool guest_markers_enabled;
    std::jthread process_thread{};
    std::atomic<u32> num_submits{};
    std::atomic<u32> num_commands{};
    std::atomic<bool> submit_done{};
    mpsc_stack_t commands{};
    /// The GPU thread parks on work_ec until there is something to do; WaitGpuIdle on
    /// idle_ec until num_submits is 0; SendCommand<true> on command_done_ec until it has run.
    retro_eventcount_t work_ec{};
    retro_eventcount_t idle_ec{};
    retro_eventcount_t command_done_ec{};
    std::thread::id gpu_id;
#ifdef __linux__
    u32 gpu_tid;
#endif
    s32 curr_qid{-1};
};

} // namespace AmdGpu
