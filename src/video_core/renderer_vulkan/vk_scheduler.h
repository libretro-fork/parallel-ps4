// SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <new>
#include <string.h>

#include <queues/mpsc_stack.h>
#include <retro_atomic.h>
#include <rthreads/retro_eventcount.h>

#include "common/combiner.h"
#include "common/interval_set.h"
#include "common/node_pool.h"
#include "common/stop_thread.h"
#include "common/unique_function.h"
#include "video_core/amdgpu/regs_color.h"
#include "video_core/amdgpu/regs_primitive.h"
#include "video_core/renderer_vulkan/vk_resource_pool.h"
#include "video_core/renderer_vulkan/vk_semaphore.h"
#include "vulkan/vulkan.hpp"

namespace Vulkan {

class Instance;

struct RenderAttachment {
    VkImageView image_view;
    VkImageLayout image_layout;
    u32 clear_value[4];
    union {
        u32 is_clear;
        struct {
            bool has_depth;
            bool depth_clear;
            bool has_stencil;
            bool stencil_clear;
        };
    };
};
static_assert(__has_unique_object_representations(RenderAttachment));

struct RenderState {
    static constexpr u32 MaxColorAttachments = 8;
    RenderAttachment color_attachments[MaxColorAttachments];
    RenderAttachment depth_stencil_attachment;
    u16 width;
    u16 height;
    u16 num_layers;
    u16 num_color_attachments;

    bool operator==(const RenderState& other) const noexcept {
        return memcmp(this, &other, sizeof(RenderState)) == 0;
    }
};
static_assert(__has_unique_object_representations(RenderState));

struct SubmitInfo {
    static constexpr u32 MaxSemaphores = 4;
    VkSemaphore wait_semas[MaxSemaphores];
    u64 wait_ticks[MaxSemaphores];
    VkSemaphore signal_semas[MaxSemaphores];
    u64 signal_ticks[MaxSemaphores];
    VkFence fence;
    u32 num_wait_semas;
    u32 num_signal_semas;

    void AddWait(vk::Semaphore semaphore, u64 tick = 1) {
        wait_semas[num_wait_semas] = static_cast<VkSemaphore>(semaphore);
        wait_ticks[num_wait_semas++] = tick;
    }

    void AddSignal(vk::Semaphore semaphore, u64 tick = 1) {
        signal_semas[num_signal_semas] = static_cast<VkSemaphore>(semaphore);
        signal_ticks[num_signal_semas++] = tick;
    }

    void AddSignal(vk::Fence fence_) {
        fence = static_cast<VkFence>(fence_);
    }
};

using Viewports = Common::StaticVector<vk::Viewport, AmdGpu::NUM_VIEWPORTS>;
using Scissors = Common::StaticVector<vk::Rect2D, AmdGpu::NUM_VIEWPORTS>;

struct StencilOps {
    vk::StencilOp fail_op{};
    vk::StencilOp pass_op{};
    vk::StencilOp depth_fail_op{};
    vk::CompareOp compare_op{};

    bool operator==(const StencilOps& other) const {
        return fail_op == other.fail_op && pass_op == other.pass_op &&
               depth_fail_op == other.depth_fail_op && compare_op == other.compare_op;
    }
};
struct DynamicState {
    struct {
        bool viewports : 1;
        bool scissors : 1;

        bool depth_test_enabled : 1;
        bool depth_write_enabled : 1;
        bool depth_compare_op : 1;

        bool depth_bounds_test_enabled : 1;
        bool depth_bounds : 1;

        bool depth_bias_enabled : 1;
        bool depth_bias : 1;

        bool stencil_test_enabled : 1;
        bool stencil_front_ops : 1;
        bool stencil_front_reference : 1;
        bool stencil_front_write_mask : 1;
        bool stencil_front_compare_mask : 1;
        bool stencil_back_ops : 1;
        bool stencil_back_reference : 1;
        bool stencil_back_write_mask : 1;
        bool stencil_back_compare_mask : 1;

        bool primitive_restart_enable : 1;
        bool rasterizer_discard_enable : 1;
        bool cull_mode : 1;
        bool front_face : 1;

        bool blend_constants : 1;
        bool color_write_masks : 1;
        bool line_width : 1;
        bool feedback_loop_enabled : 1;
        bool sample_locations : 1;
    } dirty_state{};

    Viewports viewports{};
    Scissors scissors{};

    bool depth_test_enabled{};
    bool depth_write_enabled{};
    vk::CompareOp depth_compare_op{};

    bool depth_bounds_test_enabled{};
    float depth_bounds_min{};
    float depth_bounds_max{};

    bool depth_bias_enabled{};
    float depth_bias_constant{};
    float depth_bias_clamp{};
    float depth_bias_slope{};

    bool stencil_test_enabled{};
    StencilOps stencil_front_ops{};
    u32 stencil_front_reference{};
    u32 stencil_front_write_mask{};
    u32 stencil_front_compare_mask{};
    StencilOps stencil_back_ops{};
    u32 stencil_back_reference{};
    u32 stencil_back_write_mask{};
    u32 stencil_back_compare_mask{};

    bool primitive_restart_enable{};
    bool rasterizer_discard_enable{};
    vk::CullModeFlags cull_mode{};
    vk::FrontFace front_face{};

    float blend_constants[4]{};
    vk::ColorComponentFlags color_write_masks[AmdGpu::NUM_COLOR_BUFFERS]{};
    float line_width{};
    bool feedback_loop_enabled{};
    /// The game's sample positions: side of the pixel block they repeat over (0: none set),
    /// samples in a pixel, and an x and a y for each in sixteenths of a pixel
    u8 sample_locs_grid{};
    u8 sample_locs_samples{};
    u8 sample_locs[64]{};

    /// Commits the dynamic state to the provided command buffer.
    void Commit(const Instance& instance, const vk::CommandBuffer& cmdbuf);

    /// Invalidates all dynamic state to be flushed into the next command buffer.
    void Invalidate() {
        memset(&dirty_state, 0xFF, sizeof(dirty_state));
    }

    void SetViewports(const Viewports& viewports_) {
        if (!(viewports == viewports_)) {
            viewports = viewports_;
            dirty_state.viewports = true;
        }
    }

    void SetScissors(const Scissors& scissors_) {
        if (!(scissors == scissors_)) {
            scissors = scissors_;
            dirty_state.scissors = true;
        }
    }

    void SetDepthTestEnabled(const bool enabled) {
        if (depth_test_enabled != enabled) {
            depth_test_enabled = enabled;
            dirty_state.depth_test_enabled = true;
        }
    }

    void SetDepthWriteEnabled(const bool enabled) {
        if (depth_write_enabled != enabled) {
            depth_write_enabled = enabled;
            dirty_state.depth_write_enabled = true;
        }
    }

    void SetDepthCompareOp(const vk::CompareOp compare_op) {
        if (depth_compare_op != compare_op) {
            depth_compare_op = compare_op;
            dirty_state.depth_compare_op = true;
        }
    }

    void SetDepthBoundsTestEnabled(const bool enabled) {
        if (depth_bounds_test_enabled != enabled) {
            depth_bounds_test_enabled = enabled;
            dirty_state.depth_bounds_test_enabled = true;
        }
    }

    void SetDepthBounds(const float min, const float max) {
        if (depth_bounds_min != min || depth_bounds_max != max) {
            depth_bounds_min = min;
            depth_bounds_max = max;
            dirty_state.depth_bounds = true;
        }
    }

    void SetDepthBiasEnabled(const bool enabled) {
        if (depth_bias_enabled != enabled) {
            depth_bias_enabled = enabled;
            dirty_state.depth_bias_enabled = true;
        }
    }

    void SetDepthBias(const float constant, const float clamp, const float slope) {
        if (depth_bias_constant != constant || depth_bias_clamp != clamp ||
            depth_bias_slope != slope) {
            depth_bias_constant = constant;
            depth_bias_clamp = clamp;
            depth_bias_slope = slope;
            dirty_state.depth_bias = true;
        }
    }

    void SetStencilTestEnabled(const bool enabled) {
        if (stencil_test_enabled != enabled) {
            stencil_test_enabled = enabled;
            dirty_state.stencil_test_enabled = true;
        }
    }

    void SetStencilOps(const StencilOps& front_ops, const StencilOps& back_ops) {
        if (stencil_front_ops != front_ops) {
            stencil_front_ops = front_ops;
            dirty_state.stencil_front_ops = true;
        }
        if (stencil_back_ops != back_ops) {
            stencil_back_ops = back_ops;
            dirty_state.stencil_back_ops = true;
        }
    }

    void SetStencilReferences(const u32 front_reference, const u32 back_reference) {
        if (stencil_front_reference != front_reference) {
            stencil_front_reference = front_reference;
            dirty_state.stencil_front_reference = true;
        }
        if (stencil_back_reference != back_reference) {
            stencil_back_reference = back_reference;
            dirty_state.stencil_back_reference = true;
        }
    }

    void SetStencilWriteMasks(const u32 front_write_mask, const u32 back_write_mask) {
        if (stencil_front_write_mask != front_write_mask) {
            stencil_front_write_mask = front_write_mask;
            dirty_state.stencil_front_write_mask = true;
        }
        if (stencil_back_write_mask != back_write_mask) {
            stencil_back_write_mask = back_write_mask;
            dirty_state.stencil_back_write_mask = true;
        }
    }

    void SetStencilCompareMasks(const u32 front_compare_mask, const u32 back_compare_mask) {
        if (stencil_front_compare_mask != front_compare_mask) {
            stencil_front_compare_mask = front_compare_mask;
            dirty_state.stencil_front_compare_mask = true;
        }
        if (stencil_back_compare_mask != back_compare_mask) {
            stencil_back_compare_mask = back_compare_mask;
            dirty_state.stencil_back_compare_mask = true;
        }
    }

    void SetPrimitiveRestartEnabled(const bool enabled) {
        if (primitive_restart_enable != enabled) {
            primitive_restart_enable = enabled;
            dirty_state.primitive_restart_enable = true;
        }
    }

    void SetCullMode(const vk::CullModeFlags cull_mode_) {
        if (cull_mode != cull_mode_) {
            cull_mode = cull_mode_;
            dirty_state.cull_mode = true;
        }
    }

    void SetFrontFace(const vk::FrontFace front_face_) {
        if (front_face != front_face_) {
            front_face = front_face_;
            dirty_state.front_face = true;
        }
    }

    void SetBlendConstants(const float* blend_constants_) {
        if (memcmp(blend_constants, blend_constants_, sizeof(blend_constants)) != 0) {
            memcpy(blend_constants, blend_constants_, sizeof(blend_constants));
            dirty_state.blend_constants = true;
        }
    }

    void SetRasterizerDiscardEnabled(const bool enabled) {
        if (rasterizer_discard_enable != enabled) {
            rasterizer_discard_enable = enabled;
            dirty_state.rasterizer_discard_enable = true;
        }
    }

    /// @color_write_masks_ holds one mask per colour buffer
    void SetColorWriteMasks(const vk::ColorComponentFlags* color_write_masks_) {
        if (memcmp(color_write_masks, color_write_masks_, sizeof(color_write_masks)) != 0) {
            memcpy(color_write_masks, color_write_masks_, sizeof(color_write_masks));
            dirty_state.color_write_masks = true;
        }
    }

    void SetLineWidth(const float width) {
        if (line_width != width) {
            line_width = width;
            dirty_state.line_width = true;
        }
    }

    void SetSampleLocations(u8 grid, u8 samples, const u8* locs) {
        const size_t bytes = size_t(grid) * grid * samples * 2;
        if (sample_locs_grid != grid || sample_locs_samples != samples ||
            memcmp(sample_locs, locs, bytes) != 0) {
            sample_locs_grid = grid;
            sample_locs_samples = samples;
            memcpy(sample_locs, locs, bytes);
            dirty_state.sample_locations = true;
        }
    }

    void SetAttachmentFeedbackLoopEnabled(const bool enabled) {
        if (feedback_loop_enabled != enabled) {
            feedback_loop_enabled = enabled;
            dirty_state.feedback_loop_enabled = true;
        }
    }
};

using SessionFunc = Common::UniqueFunction<void>;
using SubmitFunc = Common::UniqueFunction<void, SubmitInfo&>;

class Scheduler {
public:
    explicit Scheduler(const Instance& instance);
    ~Scheduler();

    /// Sends the current execution context to the GPU
    /// and increments the scheduler timeline semaphore.
    void Flush(SubmitInfo& info);

    /// Sends the current execution context to the GPU
    /// and increments the scheduler timeline semaphore.
    void Flush();

    /// Sends the current execution context to the GPU and waits for it to complete.
    void Finish();

    /// Waits for the given tick to trigger on the GPU.
    void Wait(u64 tick);

    /// Attempts to execute operations whose tick the GPU has caught up with.
    void PopPendingOperations();

    /// Starts a new rendering scope with provided state.
    void BeginRendering(const RenderState& new_state);

    /// Ends current rendering scope.
    void EndRendering();

    /// Starts a new session.
    void BeginSession();

    /// Returns the current command buffer used for uploads.
    vk::CommandBuffer UploadCommandBuffer();

    /// Sets a function to be called on every session finalization.
    void SetSessionCallback(SessionFunc&& on_session) {
        this->on_session = static_cast<SessionFunc&&>(on_session);
    }

    /// Sets a function to be called on every scheduler submission.
    void SetSubmitCallback(SubmitFunc&& on_submit) {
        this->on_submit = static_cast<SubmitFunc&&>(on_submit);
    }

    /// Returns the current render state.
    const RenderState& GetRenderState() const {
        return render_state;
    }

    /// Returns the current pipeline dynamic state tracking.
    DynamicState& GetDynamicState() {
        return dynamic_state;
    }

    /// Returns the current command buffer.
    vk::CommandBuffer CommandBuffer() const {
        return vk::CommandBuffer{sessions[num_sessions - 1].primary};
    }

    /// Returns the current command buffer tick.
    [[nodiscard]] u64 CurrentTick() const noexcept {
        return work_semaphore.CurrentTick();
    }

    /// Returns true when a tick has been triggered by the GPU.
    [[nodiscard]] bool IsFree(u64 tick) noexcept {
        if (work_semaphore.IsFree(tick)) {
            return true;
        }
        work_semaphore.Refresh();
        return work_semaphore.IsFree(tick);
    }

    /// Returns the scheduler timeline semaphore.
    [[nodiscard]] Semaphore* GetWorkSemaphore() noexcept {
        return &work_semaphore;
    }

    /// Defers an operation until the gpu has reached the current cpu tick.
    /// Will be run when submitting or calling PopPendingOperations.
    template <typename Func>
    void DeferOperation(Func func) {
        pending_ops.Push(MakeOp(func));
    }

    /// Defers an operation until the gpu has reached the current cpu tick.
    /// Runs as soon as possible in another thread.
    template <typename Func>
    void DeferPriorityOperation(Func func) {
        priority_pending_ops.Push(MakeOp(func));
        retro_eventcount_notify(&priority_pending_ops_ec);
    }

    /// The submissions to the device's queue, which Vulkan has used by one thread at a time,
    /// run one at a time here, from every scheduler.
    static Common::Combiner submit_combiner;

private:
    void EndSession();

    void SubmitExecution(SubmitInfo& info);

    void PriorityPendingOpsThread();
    void RunPendingOperations();

private:
    const Instance& instance;
    Semaphore work_semaphore;
    CommandPool command_pool;
    DynamicState dynamic_state;
    SessionFunc on_session{};
    SubmitFunc on_submit{};
    struct Session {
        VkCommandBuffer upload{VK_NULL_HANDLE};
        VkCommandBuffer primary{VK_NULL_HANDLE};
    };
    /// The sessions of the submission being recorded, the last the current one: room made up
    /// front, doubled past it
    Session* sessions{};
    u32 num_sessions{};
    u32 session_capacity{};
    /// The command buffers of a submission, two a session, with the sessions' room
    VkCommandBuffer* submit_buffers{};

public:
    /// Room for an operation's captures, kept in the operation itself
    static constexpr size_t PendingOpBytes = 224;
    /// A deferred operation: the function made in place in it, run and destroyed through
    /// two pointers. Taken from a pool made up front (AcquireOp, ReleaseOp in
    /// vk_scheduler.cpp), so deferring an operation does not allocate.
    struct PendingOp {
        mpsc_stack_node_t node;     // first: the stack links operations through it
        node_pool_node_t free_link; // in the pool while free
        u64 gpu_tick;
        void (*invoke)(void* storage);
        void (*destroy)(void* storage);
        alignas(16) unsigned char storage[PendingOpBytes];

        void Run() {
            invoke(storage);
        }
    };

private:
    static PendingOp* AcquireOp();
    /// Destroys the function and gives the operation back to the pool
    static void ReleaseOp(PendingOp* op);

    /// @func is moved into the operation's own storage
    template <typename F>
    PendingOp* MakeOp(F& func) {
        static_assert(sizeof(F) <= PendingOpBytes, "the operation's captures do not fit");
        static_assert(alignof(F) <= 16, "the operation's captures are aligned past 16");
        PendingOp* op = AcquireOp();
        new (op->storage) F(static_cast<F&&>(func));
        op->invoke = [](void* storage) { (*static_cast<F*>(storage))(); };
        op->destroy = [](void* storage) { static_cast<F*>(storage)->~F(); };
        op->gpu_tick = CurrentTick();
        return op;
    }
    /// Operations in order: pushed by any thread onto a lock-free stack, taken oldest first
    /// into a list that only the one running them touches.
    struct PendingOps {
        mpsc_stack_t incoming{};
        PendingOp* head{};
        PendingOp* tail{};

        PendingOps() {
            mpsc_stack_init(&incoming);
        }
        ~PendingOps() {
            Take();
            while (PendingOp* op = Pop()) {
                ReleaseOp(op);
            }
        }
        void Push(PendingOp* op) {
            mpsc_stack_push(&incoming, &op->node);
        }
        void Take() {
            mpsc_stack_node_t* node = mpsc_stack_reverse(mpsc_stack_drain(&incoming));
            if (!node) {
                return;
            }
            if (tail) {
                tail->node.next = node;
            } else {
                head = reinterpret_cast<PendingOp*>(node);
            }
            for (; node; node = node->next) {
                tail = reinterpret_cast<PendingOp*>(node);
            }
        }
        PendingOp* Front() const {
            return head;
        }
        PendingOp* Pop() {
            PendingOp* op = head;
            if (op) {
                head = reinterpret_cast<PendingOp*>(op->node.next);
                if (!head) {
                    tail = nullptr;
                }
            }
            return op;
        }
    };
    /// PopPendingOperations runs one at a time, in pending_combiner; an operation that calls it
    /// again runs it there and then, as the recursive mutex allowed.
    PendingOps pending_ops;
    Common::Combiner pending_combiner;
    /// The thread running the pending operations, while it does: its ThisThread()
    retro_atomic_ptr_t pending_runner{};
    static void* ThisThread() {
        static thread_local char tag;
        return &tag;
    }
    PendingOps priority_pending_ops;
    retro_eventcount_t priority_pending_ops_ec{};
    stop_thread_t priority_pending_ops_thread = STOP_THREAD_INIT;
    RenderState render_state;
    bool is_rendering = false;
};

} // namespace Vulkan
