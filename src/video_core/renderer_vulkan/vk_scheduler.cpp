// SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstddef>
#include <functional>

#include "common/assert.h"
#include "common/thread_name.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"

namespace Vulkan {

Common::Combiner Scheduler::submit_combiner;

namespace {

// The deferred operations of every scheduler, made when the first scheduler is: a game keeps
// at most a few hundred in flight. The free ones are a lock-free pool (common/node_pool.h).
// Should they all be in flight, one more is made, and kept in the pool once given back.
constexpr size_t OpPoolSize = 4096;

struct OpPool {
    node_pool_t free_ops;

    OpPool() {
        node_pool_init(&free_ops);
        Scheduler::PendingOp* const block = new Scheduler::PendingOp[OpPoolSize];
        for (size_t i = 0; i < OpPoolSize; ++i) {
            node_pool_push(&free_ops, &block[i].free_link);
        }
    }
};

OpPool& Pool() {
    static OpPool pool;
    return pool;
}

void BeginOneTime(VkCommandBuffer cmdbuf) {
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    const VkResult result = vkBeginCommandBuffer(cmdbuf, &begin_info);
    ASSERT_MSG(result == VK_SUCCESS, "Failed to begin a command buffer: %s", ResultName(result));
}

} // Anonymous namespace

Scheduler::PendingOp* Scheduler::AcquireOp() {
    if (node_pool_node_t* const link = node_pool_pop(&Pool().free_ops)) {
        return reinterpret_cast<PendingOp*>(reinterpret_cast<char*>(link) -
                                            offsetof(PendingOp, free_link));
    }
    return new PendingOp;
}

void Scheduler::ReleaseOp(PendingOp* op) {
    op->destroy(op->storage);
    node_pool_push(&Pool().free_ops, &op->free_link);
}

Scheduler::Scheduler(const Instance& instance)
    : instance{instance}, work_semaphore{instance}, command_pool{instance, &work_semaphore} {
    retro_eventcount_init(&priority_pending_ops_ec);
    // Made here, not in play: the pool, and room for the sessions of a submission
    (void)Pool();
    session_capacity = 64;
    sessions = static_cast<Session*>(malloc(session_capacity * sizeof(Session)));
    submit_buffers =
        static_cast<VkCommandBuffer*>(malloc(session_capacity * 2 * sizeof(VkCommandBuffer)));
    ASSERT_MSG(sessions && submit_buffers, "Out of memory making the scheduler");
    BeginSession();
    stop_thread_start(
        &priority_pending_ops_thread,
        [](void* self) { static_cast<Scheduler*>(self)->PriorityPendingOpsThread(); }, this);
}

Scheduler::~Scheduler() {
    // Asked to stop, then woken where it waits for an operation
    stop_thread_request(&priority_pending_ops_thread);
    retro_eventcount_notify(&priority_pending_ops_ec);
    stop_thread_join(&priority_pending_ops_thread);
    retro_eventcount_free(&priority_pending_ops_ec);
    free(sessions);
    free(submit_buffers);
}

void Scheduler::BeginRendering(const RenderState& new_state) {
    if (is_rendering && render_state == new_state) {
        return;
    }
    EndRendering();
    is_rendering = true;
    render_state = new_state;

    VkRenderingAttachmentInfo color_attachments[RenderState::MaxColorAttachments];
    for (u32 i = 0; i < render_state.num_color_attachments; ++i) {
        const RenderAttachment& cb = render_state.color_attachments[i];
        VkRenderingAttachmentInfo& attachment = color_attachments[i];
        attachment = {};
        attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        attachment.imageView = cb.image_view;
        attachment.imageLayout = cb.image_layout;
        attachment.loadOp = cb.is_clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
        attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        memcpy(attachment.clearValue.color.uint32, cb.clear_value, sizeof(cb.clear_value));
    }

    const RenderAttachment& db = render_state.depth_stencil_attachment;
    VkRenderingAttachmentInfo depth_attachment{};
    depth_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depth_attachment.imageView = db.image_view;
    depth_attachment.imageLayout = db.image_layout;
    depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    VkRenderingAttachmentInfo stencil_attachment = depth_attachment;
    depth_attachment.loadOp =
        db.depth_clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    memcpy(&depth_attachment.clearValue.depthStencil.depth, &db.clear_value[0], sizeof(float));
    stencil_attachment.loadOp =
        db.stencil_clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    stencil_attachment.clearValue.depthStencil.stencil = db.clear_value[1];

    VkRenderingInfo rendering_info{};
    rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering_info.renderArea.extent = {render_state.width, render_state.height};
    rendering_info.layerCount = render_state.num_layers;
    rendering_info.colorAttachmentCount = render_state.num_color_attachments;
    rendering_info.pColorAttachments = color_attachments;
    rendering_info.pDepthAttachment = db.has_depth ? &depth_attachment : nullptr;
    rendering_info.pStencilAttachment = db.has_stencil ? &stencil_attachment : nullptr;

    instance.Mark(CommandBuffer(), "render pass: w, h, colours|depth<<8|stencil<<9, layers",
                  render_state.width, render_state.height,
                  u64(render_state.num_color_attachments) | u64(db.has_depth) << 8 |
                      u64(db.has_stencil) << 9,
                  render_state.num_layers);
    vkCmdBeginRendering(sessions[num_sessions - 1].primary, &rendering_info);
}

void Scheduler::EndRendering() {
    if (!is_rendering) {
        return;
    }
    is_rendering = false;
    vkCmdEndRendering(sessions[num_sessions - 1].primary);
}

VkCommandBuffer Scheduler::UploadCommandBuffer() {
    VkCommandBuffer& upload_cmdbuf = sessions[num_sessions - 1].upload;
    if (upload_cmdbuf) {
        return upload_cmdbuf;
    }
    upload_cmdbuf = static_cast<VkCommandBuffer>(command_pool.Commit());
    BeginOneTime(upload_cmdbuf);
    return upload_cmdbuf;
}

void Scheduler::Flush(SubmitInfo& info) {
    // When flushing, we only send data to the driver; no waiting is necessary.
    SubmitExecution(info);
}

void Scheduler::Flush() {
    SubmitInfo info{};
    Flush(info);
}

void Scheduler::Finish() {
    // When finishing, we need to wait for the submission to have executed on the device.
    const u64 presubmit_tick = CurrentTick();
    SubmitInfo info{};
    SubmitExecution(info);
    Wait(presubmit_tick);
}

void Scheduler::Wait(u64 tick) {
    if (tick >= work_semaphore.CurrentTick()) {
        // Make sure we are not waiting for the current tick without signalling
        SubmitInfo info{};
        Flush(info);
    }
    work_semaphore.Wait(tick);
}

void Scheduler::PopPendingOperations() {
    if (retro_atomic_load_relaxed_ptr(&pending_runner) == ThisThread()) {
        // From an operation being run
        RunPendingOperations();
        return;
    }
    pending_combiner.Run([this] {
        retro_atomic_store_relaxed_ptr(&pending_runner, ThisThread());
        RunPendingOperations();
        retro_atomic_store_relaxed_ptr(&pending_runner, nullptr);
    });
}

void Scheduler::RunPendingOperations() {
    work_semaphore.Refresh();
    pending_ops.Take();
    while (PendingOp* op = pending_ops.Front()) {
        if (!work_semaphore.IsFree(op->gpu_tick)) {
            break;
        }
        // Off the list before it runs, in case it runs the list again
        pending_ops.Pop();
        op->Run();
        ReleaseOp(op);
        pending_ops.Take();
    }
}

void Scheduler::BeginSession() {
    EndSession();

    if (num_sessions == session_capacity) {
        session_capacity *= 2;
        Session* const grown =
            static_cast<Session*>(realloc(sessions, session_capacity * sizeof(Session)));
        VkCommandBuffer* const grown_buffers = static_cast<VkCommandBuffer*>(
            realloc(submit_buffers, session_capacity * 2 * sizeof(VkCommandBuffer)));
        ASSERT_MSG(grown && grown_buffers, "Out of memory growing the scheduler's sessions");
        sessions = grown;
        submit_buffers = grown_buffers;
    }
    Session& session = sessions[num_sessions++];
    session = {};

    session.primary = static_cast<VkCommandBuffer>(command_pool.Commit());
    BeginOneTime(session.primary);

    // Invalidate dynamic state so it gets applied to the new command buffer.
    dynamic_state.Invalidate();
}

void Scheduler::EndSession() {
    if (num_sessions == 0) {
        return;
    }

    if (on_session) {
        on_session();
    }

    const Session& session = sessions[num_sessions - 1];
    if (session.upload) {
        const VkResult upload_result = vkEndCommandBuffer(session.upload);
        ASSERT_MSG(upload_result == VK_SUCCESS, "Failed to end a command buffer: %s",
                   ResultName(upload_result));
    }

    EndRendering();
    const VkResult result = vkEndCommandBuffer(session.primary);
    ASSERT_MSG(result == VK_SUCCESS, "Failed to end a command buffer: %s", ResultName(result));
}

void Scheduler::SubmitExecution(SubmitInfo& info) {
    submit_combiner.Run([&] {
        const u64 signal_value = work_semaphore.NextTick();

        if (on_submit) {
            on_submit(info);
        }

        EndSession();

        // Kept, so a submission does not allocate
        u32 num_cmd_buffers = 0;
        for (u32 i = 0; i < num_sessions; ++i) {
            if (sessions[i].upload) {
                submit_buffers[num_cmd_buffers++] = sessions[i].upload;
            }
            submit_buffers[num_cmd_buffers++] = sessions[i].primary;
        }
        num_sessions = 0;

        info.AddSignal(static_cast<VkSemaphore>(work_semaphore.Handle()), signal_value);

        VkPipelineStageFlags wait_stage_masks[SubmitInfo::MaxSemaphores];
        for (u32 i = 0; i < SubmitInfo::MaxSemaphores; ++i) {
            wait_stage_masks[i] = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        }

        VkTimelineSemaphoreSubmitInfo timeline_si{};
        timeline_si.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
        timeline_si.waitSemaphoreValueCount = info.num_wait_semas;
        timeline_si.pWaitSemaphoreValues = info.wait_ticks;
        timeline_si.signalSemaphoreValueCount = info.num_signal_semas;
        timeline_si.pSignalSemaphoreValues = info.signal_ticks;

        VkSubmitInfo submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.pNext = &timeline_si;
        submit_info.waitSemaphoreCount = info.num_wait_semas;
        submit_info.pWaitSemaphores = info.wait_semas;
        submit_info.pWaitDstStageMask = wait_stage_masks;
        submit_info.commandBufferCount = num_cmd_buffers;
        submit_info.pCommandBuffers = submit_buffers;
        submit_info.signalSemaphoreCount = info.num_signal_semas;
        submit_info.pSignalSemaphores = info.signal_semas;

        const VkResult submit_result =
            vkQueueSubmit(instance.GetGraphicsQueue(), 1, &submit_info, info.fence);
        if (submit_result == VK_ERROR_DEVICE_LOST) {
            instance.ReportDeviceLost();
        }
        ASSERT_MSG(submit_result != VK_ERROR_DEVICE_LOST, "Device lost during submit");

        work_semaphore.Refresh();
        BeginSession();

        // Apply pending operations
        PopPendingOperations();
    });
}

void Scheduler::PriorityPendingOpsThread() {
    thread_name_set("shadPS4:GpuSchedPriorityPendingOpsRunner");

    while (!stop_thread_stopping(&priority_pending_ops_thread)) {
        priority_pending_ops.Take();
        PendingOp* op = priority_pending_ops.Pop();
        if (!op) {
            const int key = retro_eventcount_prepare_wait(&priority_pending_ops_ec);
            if (!mpsc_stack_empty(&priority_pending_ops.incoming) ||
                stop_thread_stopping(&priority_pending_ops_thread)) {
                retro_eventcount_cancel_wait(&priority_pending_ops_ec);
                continue;
            }
            retro_eventcount_commit_wait(&priority_pending_ops_ec, key);
            continue;
        }

        work_semaphore.Wait(op->gpu_tick);
        if (stop_thread_stopping(&priority_pending_ops_thread)) {
            ReleaseOp(op);
            break;
        }

        op->Run();
        ReleaseOp(op);
    }
}

namespace {

void CmdSetStencilOps(VkCommandBuffer cmdbuf, VkStencilFaceFlags faces, const StencilOps& ops) {
    vkCmdSetStencilOp(cmdbuf, faces, ops.fail_op, ops.pass_op, ops.depth_fail_op, ops.compare_op);
}

} // Anonymous namespace

void DynamicState::Commit(const Instance& instance, VkCommandBuffer cmdbuf) {
    if (dirty_state.viewports) {
        dirty_state.viewports = false;
        vkCmdSetViewportWithCount(cmdbuf, num_viewports, viewports);
    }
    if (dirty_state.scissors) {
        dirty_state.scissors = false;
        vkCmdSetScissorWithCount(cmdbuf, num_scissors, scissors);
    }
    if (dirty_state.depth_test_enabled) {
        dirty_state.depth_test_enabled = false;
        vkCmdSetDepthTestEnable(cmdbuf, depth_test_enabled ? VK_TRUE : VK_FALSE);
    }
    if (dirty_state.depth_write_enabled) {
        dirty_state.depth_write_enabled = false;
        // Note that this must be set in a command buffer even if depth test is disabled.
        vkCmdSetDepthWriteEnable(cmdbuf, depth_write_enabled ? VK_TRUE : VK_FALSE);
    }
    if (depth_test_enabled && dirty_state.depth_compare_op) {
        dirty_state.depth_compare_op = false;
        vkCmdSetDepthCompareOp(cmdbuf, depth_compare_op);
    }
    if (dirty_state.depth_bounds_test_enabled) {
        dirty_state.depth_bounds_test_enabled = false;
        if (instance.IsDepthBoundsSupported()) {
            vkCmdSetDepthBoundsTestEnable(cmdbuf, depth_bounds_test_enabled ? VK_TRUE : VK_FALSE);
        }
    }
    if (depth_bounds_test_enabled && dirty_state.depth_bounds) {
        dirty_state.depth_bounds = false;
        if (instance.IsDepthBoundsSupported()) {
            vkCmdSetDepthBounds(cmdbuf, depth_bounds_min, depth_bounds_max);
        }
    }
    if (dirty_state.depth_bias_enabled) {
        dirty_state.depth_bias_enabled = false;
        vkCmdSetDepthBiasEnable(cmdbuf, depth_bias_enabled ? VK_TRUE : VK_FALSE);
    }
    if (depth_bias_enabled && dirty_state.depth_bias) {
        dirty_state.depth_bias = false;
        vkCmdSetDepthBias(cmdbuf, depth_bias_constant, depth_bias_clamp, depth_bias_slope);
    }
    if (dirty_state.stencil_test_enabled) {
        dirty_state.stencil_test_enabled = false;
        vkCmdSetStencilTestEnable(cmdbuf, stencil_test_enabled ? VK_TRUE : VK_FALSE);
    }
    if (stencil_test_enabled) {
        if (dirty_state.stencil_front_ops && dirty_state.stencil_back_ops &&
            stencil_front_ops == stencil_back_ops) {
            dirty_state.stencil_front_ops = false;
            dirty_state.stencil_back_ops = false;
            CmdSetStencilOps(cmdbuf, VK_STENCIL_FACE_FRONT_AND_BACK, stencil_front_ops);
        } else {
            if (dirty_state.stencil_front_ops) {
                dirty_state.stencil_front_ops = false;
                CmdSetStencilOps(cmdbuf, VK_STENCIL_FACE_FRONT_BIT, stencil_front_ops);
            }
            if (dirty_state.stencil_back_ops) {
                dirty_state.stencil_back_ops = false;
                CmdSetStencilOps(cmdbuf, VK_STENCIL_FACE_BACK_BIT, stencil_back_ops);
            }
        }
        if (dirty_state.stencil_front_reference && dirty_state.stencil_back_reference &&
            stencil_front_reference == stencil_back_reference) {
            dirty_state.stencil_front_reference = false;
            dirty_state.stencil_back_reference = false;
            vkCmdSetStencilReference(cmdbuf, VK_STENCIL_FACE_FRONT_AND_BACK,
                                     stencil_front_reference);
        } else {
            if (dirty_state.stencil_front_reference) {
                dirty_state.stencil_front_reference = false;
                vkCmdSetStencilReference(cmdbuf, VK_STENCIL_FACE_FRONT_BIT,
                                         stencil_front_reference);
            }
            if (dirty_state.stencil_back_reference) {
                dirty_state.stencil_back_reference = false;
                vkCmdSetStencilReference(cmdbuf, VK_STENCIL_FACE_BACK_BIT, stencil_back_reference);
            }
        }
        if (dirty_state.stencil_front_write_mask && dirty_state.stencil_back_write_mask &&
            stencil_front_write_mask == stencil_back_write_mask) {
            dirty_state.stencil_front_write_mask = false;
            dirty_state.stencil_back_write_mask = false;
            vkCmdSetStencilWriteMask(cmdbuf, VK_STENCIL_FACE_FRONT_AND_BACK,
                                     stencil_front_write_mask);
        } else {
            if (dirty_state.stencil_front_write_mask) {
                dirty_state.stencil_front_write_mask = false;
                vkCmdSetStencilWriteMask(cmdbuf, VK_STENCIL_FACE_FRONT_BIT,
                                         stencil_front_write_mask);
            }
            if (dirty_state.stencil_back_write_mask) {
                dirty_state.stencil_back_write_mask = false;
                vkCmdSetStencilWriteMask(cmdbuf, VK_STENCIL_FACE_BACK_BIT, stencil_back_write_mask);
            }
        }
        if (dirty_state.stencil_front_compare_mask && dirty_state.stencil_back_compare_mask &&
            stencil_front_compare_mask == stencil_back_compare_mask) {
            dirty_state.stencil_front_compare_mask = false;
            dirty_state.stencil_back_compare_mask = false;
            vkCmdSetStencilCompareMask(cmdbuf, VK_STENCIL_FACE_FRONT_AND_BACK,
                                       stencil_front_compare_mask);
        } else {
            if (dirty_state.stencil_front_compare_mask) {
                dirty_state.stencil_front_compare_mask = false;
                vkCmdSetStencilCompareMask(cmdbuf, VK_STENCIL_FACE_FRONT_BIT,
                                           stencil_front_compare_mask);
            }
            if (dirty_state.stencil_back_compare_mask) {
                dirty_state.stencil_back_compare_mask = false;
                vkCmdSetStencilCompareMask(cmdbuf, VK_STENCIL_FACE_BACK_BIT,
                                           stencil_back_compare_mask);
            }
        }
    }
    if (dirty_state.primitive_restart_enable) {
        dirty_state.primitive_restart_enable = false;
        vkCmdSetPrimitiveRestartEnable(cmdbuf, primitive_restart_enable ? VK_TRUE : VK_FALSE);
    }
    if (dirty_state.rasterizer_discard_enable) {
        dirty_state.rasterizer_discard_enable = false;
        vkCmdSetRasterizerDiscardEnable(cmdbuf, rasterizer_discard_enable ? VK_TRUE : VK_FALSE);
    }
    if (dirty_state.cull_mode) {
        dirty_state.cull_mode = false;
        vkCmdSetCullMode(cmdbuf, cull_mode);
    }
    if (dirty_state.front_face) {
        dirty_state.front_face = false;
        vkCmdSetFrontFace(cmdbuf, front_face);
    }
    if (dirty_state.blend_constants) {
        dirty_state.blend_constants = false;
        vkCmdSetBlendConstants(cmdbuf, blend_constants);
    }
    if (dirty_state.color_write_masks) {
        dirty_state.color_write_masks = false;
        if (instance.IsDynamicColorWriteMaskSupported()) {
            vkCmdSetColorWriteMaskEXT(cmdbuf, 0, AmdGpu::NUM_COLOR_BUFFERS, color_write_masks);
        }
    }
    if (dirty_state.line_width) {
        dirty_state.line_width = false;
        vkCmdSetLineWidth(cmdbuf, line_width);
    }
    if (dirty_state.sample_locations && sample_locs_grid) {
        dirty_state.sample_locations = false;
        VkSampleLocationEXT locations[32];
        const u32 count = u32(sample_locs_grid) * sample_locs_grid * sample_locs_samples;
        for (u32 i = 0; i < count; ++i) {
            locations[i] = {sample_locs[i * 2] / 16.0f, sample_locs[i * 2 + 1] / 16.0f};
        }
        VkSampleLocationsInfoEXT sample_locations{};
        sample_locations.sType = VK_STRUCTURE_TYPE_SAMPLE_LOCATIONS_INFO_EXT;
        sample_locations.sampleLocationsPerPixel =
            static_cast<VkSampleCountFlagBits>(sample_locs_samples);
        sample_locations.sampleLocationGridSize = {sample_locs_grid, sample_locs_grid};
        sample_locations.sampleLocationsCount = count;
        sample_locations.pSampleLocations = locations;
        vkCmdSetSampleLocationsEXT(cmdbuf, &sample_locations);
    }
    if (dirty_state.feedback_loop_enabled && instance.IsAttachmentFeedbackLoopLayoutSupported()) {
        dirty_state.feedback_loop_enabled = false;
        vkCmdSetAttachmentFeedbackLoopEnableEXT(
            cmdbuf, feedback_loop_enabled ? VK_IMAGE_ASPECT_COLOR_BIT : VK_IMAGE_ASPECT_NONE);
    }
}

} // namespace Vulkan
