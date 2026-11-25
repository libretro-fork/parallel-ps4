/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <retro_atomic.h>
#include <retro_spsc.h>
#include <rthreads/retro_eventcount.h>
#include <rthreads/retro_seqlock.h>
#include <stdio.h>
#include <string.h>

#include "common/c_bridge.h"
#include "common/stop_thread.h"
#include "common/thread_name.h"
#include "common/vblank_clock.h"
#include "core/emulator_settings.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/kernel/kernel_c.h"
#include "core/libraries/videoout/driver.h"
#include "core/libraries/videoout/videoout_error.h"
#include "core/libraries/videoout/videoout_gpu_c.h"
#include "libretro/vblank_c.h"
#include "video_core/overlay/vk_overlay.h"

/* A flip to take, from the GPU thread (which runs every flip, EOP or not) to the present
 * thread */
typedef struct videoout_request {
    struct videoout_frame* frame; /* NULL: none */
    VideoOutPort* port;
    s64 flip_arg;
    s32 index;
    bool eop;
} videoout_request_t;

static VideoOutPort main_port;
/* Flip requests; a full ring has the GPU thread wait on request_taken_ec */
static retro_spsc_t requests;
static retro_eventcount_t request_taken_ec;
static stop_thread_t present_thread = STOP_THREAD_INIT;

const char* video_out_pixel_format_string(VideoOutPixelFormat format) {
    switch (format) {
    case VIDEO_OUT_PIXEL_FORMAT_A8R8G8B8_SRGB:
        return "A8R8G8B8Srgb";
    case VIDEO_OUT_PIXEL_FORMAT_A8B8G8R8_SRGB:
        return "A8B8G8R8Srgb";
    case VIDEO_OUT_PIXEL_FORMAT_A2R10G10B10:
        return "A2R10G10B10";
    case VIDEO_OUT_PIXEL_FORMAT_A2R10G10B10_SRGB:
        return "A2R10G10B10Srgb";
    case VIDEO_OUT_PIXEL_FORMAT_A2R10G10B10_BT2020_PQ:
        return "A2R10G10B10Bt2020Pq";
    case VIDEO_OUT_PIXEL_FORMAT_A16R16G16B16_FLOAT:
        return "A16R16G16B16Float";
    default: {
        char msg[48];
        snprintf(msg, sizeof(msg), "Unknown pixel format %u", format);
        c_unreachable(__FILE__, __LINE__, "GetPixelFormatString", msg);
        return "";
    }
    }
}

void flip_status_init(FlipStatus* status) {
    memset(status, 0, sizeof(*status));
    status->flip_arg = -1;
    status->current_buffer = -1;
}

bool videoout_port_add_event_queue(retro_atomic_64_t* queues, s64 eq) {
    u32 i;
    for (i = 0; i < VIDEO_OUT_MAX_EVENT_QUEUES; i++) {
        if (retro_atomic_cas_64(&queues[i], 0, eq)) {
            return true;
        }
    }
    return false;
}

void videoout_port_remove_event_queue(retro_atomic_64_t* queues, s64 eq) {
    u32 i;
    for (i = 0; i < VIDEO_OUT_MAX_EVENT_QUEUES; i++) {
        if (retro_atomic_cas_64(&queues[i], eq, 0)) {
            return;
        }
    }
}

void videoout_port_get_flip_status(VideoOutPort* port, FlipStatus* status) {
    int seq;
    do {
        seq = retro_seqlock_read_begin(&port->flip_seq);
        *status = port->flip_status;
    } while (retro_seqlock_read_retry(&port->flip_seq, seq));
    status->submit_tsc = (u64)retro_atomic_load_relaxed_64(&port->submit_tsc);
    status->gc_queue_num = (s32)retro_atomic_load_relaxed_int(&port->gc_queue);
    status->flip_pending_num = (s32)retro_atomic_load_relaxed_int(&port->flip_pending);
}

void videoout_port_get_vblank_status(VideoOutPort* port, SceVideoOutVblankStatus* status) {
    int seq;
    do {
        seq = retro_seqlock_read_begin(&port->vblank_seq);
        *status = port->vblank_status;
    } while (retro_seqlock_read_retry(&port->vblank_seq, seq));
}

typedef struct videoout_vblank_wait {
    VideoOutPort* port;
    u64 prev_count;
} videoout_vblank_wait_t;

static int videoout_vblank_passed(void* ctx) {
    videoout_vblank_wait_t* wait = (videoout_vblank_wait_t*)ctx;
    SceVideoOutVblankStatus status;
    videoout_port_get_vblank_status(wait->port, &status);
    return status.count != wait->prev_count;
}

void videoout_port_wait_vblank(VideoOutPort* port) {
    videoout_vblank_wait_t wait;
    SceVideoOutVblankStatus status;
    videoout_port_get_vblank_status(port, &status);
    wait.port = port;
    wait.prev_count = status.count;
    kernel_game_wait(&port->vblank_ec, 0, videoout_vblank_passed, &wait);
}

s32 videoout_port_find_free_group(const VideoOutPort* port) {
    s32 index = 0;
    while (index < VIDEO_OUT_MAX_DISPLAY_BUFFER_GROUPS && port->groups[index].is_occupied) {
        index++;
    }
    return index;
}

bool videoout_port_is_vo_label(const VideoOutPort* port, const u64* address) {
    const u64* start = &port->buffer_labels[0];
    const u64* end = &port->buffer_labels[VIDEO_OUT_MAX_DISPLAY_BUFFERS - 1];
    return address >= start && address <= end;
}

void videoout_port_wait_vo_label(VideoOutPort* port, int (*ready)(void* ctx), void* ctx) {
    kernel_game_wait(&port->vo_ec, 0, ready, ctx);
}

void videoout_port_signal_vo_label(VideoOutPort* port) {
    retro_eventcount_notify(&port->vo_ec);
}

static void videoout_present_thread(void* arg);

void videoout_driver_init(u32 width, u32 height) {
    u32 i;
    memset(&main_port, 0, sizeof(main_port));
    main_port.resolution.full_width = (s32)width;
    main_port.resolution.full_height = (s32)height;
    main_port.resolution.pane_width = (s32)width;
    main_port.resolution.pane_height = (s32)height;
    main_port.resolution.refresh_rate = SCE_VIDEO_OUT_REFRESH_RATE_59_94HZ;
    main_port.resolution.screen_size_in_inch = 50;
    for (i = 0; i < VIDEO_OUT_MAX_DISPLAY_BUFFERS; i++) {
        main_port.buffer_slots[i].group_index = -1;
    }
    flip_status_init(&main_port.flip_status);
    main_port.prev_index = -1;
    retro_seqlock_init(&main_port.flip_seq);
    retro_seqlock_init(&main_port.vblank_seq);
    retro_eventcount_init(&main_port.vblank_ec);
    retro_eventcount_init(&main_port.vo_ec);

    retro_spsc_init(&requests, 256 * sizeof(videoout_request_t));
    retro_eventcount_init(&request_taken_ec);
    stop_thread_start(&present_thread, videoout_present_thread, NULL);
}

void videoout_driver_deinit(void) {
    /* Asked to stop, then woken where it waits for the frontend's next frame */
    stop_thread_request(&present_thread);
    libretro_wake_vblank_waits();
    stop_thread_join(&present_thread);
    retro_eventcount_free(&request_taken_ec);
    retro_spsc_free(&requests);
    retro_eventcount_free(&main_port.vo_ec);
    retro_eventcount_free(&main_port.vblank_ec);
}

int videoout_driver_open(const ServiceThreadParams* params) {
    (void)params;
    if (main_port.is_open) {
        return ORBIS_VIDEO_OUT_ERROR_RESOURCE_BUSY;
    }
    main_port.is_open = true;
    videoout_gpu_set_vo_port(&main_port);
    return 1;
}

static void videoout_remove_events(retro_atomic_64_t* queues, u64 id) {
    u32 i;
    for (i = 0; i < VIDEO_OUT_MAX_EVENT_QUEUES; i++) {
        const s64 event = (s64)retro_atomic_exchange_64(&queues[i], 0);
        if (event) {
            kernel_equeue_remove_event(event, id, KERNEL_EVFILT_VIDEO_OUT);
        }
    }
}

void videoout_driver_close(s32 handle) {
    u32 i;
    (void)handle;
    /* Mark as closed */
    main_port.is_open = false;
    main_port.flip_rate = 0;
    main_port.prev_index = -1;

    /* Clear port information */
    memset(main_port.buffer_labels, 0, sizeof(main_port.buffer_labels));
    memset(main_port.groups, 0, sizeof(main_port.groups));
    /* The present thread clears the status it writes */
    (void)retro_atomic_exchange_int(&main_port.flip_pending, 0);
    (void)retro_atomic_exchange_int(&main_port.gc_queue, 0);
    (void)retro_atomic_exchange_64(&main_port.submit_tsc, 0);
    retro_atomic_store_release_int(&main_port.reset_status, 1);

    /* Re-initialize buffers */
    memset(main_port.buffer_slots, 0, sizeof(main_port.buffer_slots));
    for (i = 0; i < VIDEO_OUT_MAX_DISPLAY_BUFFERS; i++) {
        main_port.buffer_slots[i].group_index = -1;
    }

    /* Clear events */
    videoout_remove_events(main_port.flip_events, ORBIS_VIDEO_OUT_INTERNAL_EVENT_FLIP);
    videoout_remove_events(main_port.vblank_events, ORBIS_VIDEO_OUT_INTERNAL_EVENT_VBLANK);
}

static void videoout_reset_status(void) {
    retro_seqlock_write_begin(&main_port.flip_seq);
    flip_status_init(&main_port.flip_status);
    retro_seqlock_write_end(&main_port.flip_seq);
    retro_seqlock_write_begin(&main_port.vblank_seq);
    memset(&main_port.vblank_status, 0, sizeof(main_port.vblank_status));
    retro_seqlock_write_end(&main_port.vblank_seq);
}

VideoOutPort* videoout_driver_get_port(s32 handle) {
    if (handle != 1) {
        return NULL;
    }
    return &main_port;
}

/* The checks registering and changing an attribute make; ORBIS_OK when it may be used */
static int videoout_check_attribute(const char* func, const BufferAttribute* attribute) {
    if (attribute->reserved0 != 0 || attribute->reserved1 != 0) {
        c_log(C_LOG_LIB_VIDEO_OUT, C_LOG_LEVEL_ERROR, func, "Invalid reserved members");
        return ORBIS_VIDEO_OUT_ERROR_INVALID_VALUE;
    }
    if (attribute->aspect_ratio != 0) {
        c_log(C_LOG_LIB_VIDEO_OUT, C_LOG_LEVEL_ERROR, func, "Invalid aspect ratio = %d",
              attribute->aspect_ratio);
        return ORBIS_VIDEO_OUT_ERROR_INVALID_ASPECT_RATIO;
    }
    if (attribute->width > attribute->pitch_in_pixel) {
        c_log(C_LOG_LIB_VIDEO_OUT, C_LOG_LEVEL_ERROR, func,
              "Buffer width %u is larger than pitch %u", attribute->width,
              attribute->pitch_in_pixel);
        return ORBIS_VIDEO_OUT_ERROR_INVALID_PITCH;
    }
    if (attribute->tiling_mode < VIDEO_OUT_TILING_MODE_TILE ||
        attribute->tiling_mode > VIDEO_OUT_TILING_MODE_LINEAR) {
        c_log(C_LOG_LIB_VIDEO_OUT, C_LOG_LEVEL_ERROR, func, "Invalid tilingMode = %u",
              (u32)attribute->tiling_mode);
        return ORBIS_VIDEO_OUT_ERROR_INVALID_TILING_MODE;
    }
    return ORBIS_OK;
}

int videoout_driver_register_buffers(VideoOutPort* port, s32 startIndex, void* const* addresses,
                                     s32 bufferNum, const BufferAttribute* attribute) {
    const s32 group_index = videoout_port_find_free_group(port);
    BufferAttributeGroup* group;
    s32 end_index;
    s32 i;
    int res;
    if (group_index >= VIDEO_OUT_MAX_DISPLAY_BUFFER_GROUPS) {
        return ORBIS_VIDEO_OUT_ERROR_NO_EMPTY_SLOT;
    }

    if (startIndex + bufferNum > VIDEO_OUT_MAX_DISPLAY_BUFFERS ||
        startIndex > VIDEO_OUT_MAX_DISPLAY_BUFFERS || bufferNum > VIDEO_OUT_MAX_DISPLAY_BUFFERS) {
        c_log(C_LOG_LIB_VIDEO_OUT, C_LOG_LEVEL_ERROR, "RegisterBuffers",
              "Attempted to register too many buffers startIndex = %d, bufferNum = %d", startIndex,
              bufferNum);
        return ORBIS_VIDEO_OUT_ERROR_INVALID_VALUE;
    }

    end_index = startIndex + bufferNum;
    for (i = startIndex; bufferNum > 0 && i < end_index; i++) {
        if (port->buffer_slots[i].group_index != -1) {
            return ORBIS_VIDEO_OUT_ERROR_SLOT_OCCUPIED;
        }
    }

    res = videoout_check_attribute("RegisterBuffers", attribute);
    if (res != ORBIS_OK) {
        return res;
    }

    c_log(C_LOG_LIB_VIDEO_OUT, C_LOG_LEVEL_INFO, "RegisterBuffers",
          "startIndex = %d, bufferNum = %d, pixelFormat = %s, aspectRatio = %d, "
          "tilingMode = %u, width = %u, height = %u, pitchInPixel = %u, option = 0x%x",
          startIndex, bufferNum, video_out_pixel_format_string(attribute->pixel_format),
          attribute->aspect_ratio, (u32)attribute->tiling_mode, attribute->width, attribute->height,
          attribute->pitch_in_pixel, attribute->option);

    group = &port->groups[group_index];
    memcpy(&group->attrib, attribute, sizeof(BufferAttribute));
    group->is_occupied = true;

    for (i = 0; i < bufferNum; i++) {
        const uintptr_t address = (uintptr_t)addresses[i];
        VideoOutBuffer* slot = &port->buffer_slots[startIndex + i];
        slot->group_index = group_index;
        slot->address_left = address;
        slot->address_right = 0;

        /* Reset flip label also when registering buffer */
        port->buffer_labels[startIndex + i] = 0;
        videoout_port_signal_vo_label(port);

        videoout_gpu_register_surface(group, address);
        c_log(C_LOG_LIB_VIDEO_OUT, C_LOG_LEVEL_INFO, "RegisterBuffers", "buffers[%u] = 0x%llx",
              (u32)(i + startIndex), (unsigned long long)address);
    }

    return group_index;
}

int videoout_driver_unregister_buffers(VideoOutPort* port, s32 attributeIndex) {
    u32 i;
    if (attributeIndex >= VIDEO_OUT_MAX_DISPLAY_BUFFER_GROUPS ||
        !port->groups[attributeIndex].is_occupied) {
        c_log(C_LOG_LIB_VIDEO_OUT, C_LOG_LEVEL_ERROR, "UnregisterBuffers",
              "Invalid attribute index %d", attributeIndex);
        return ORBIS_VIDEO_OUT_ERROR_INVALID_VALUE;
    }

    port->groups[attributeIndex].is_occupied = false;
    for (i = 0; i < VIDEO_OUT_MAX_DISPLAY_BUFFERS; i++) {
        if (port->buffer_slots[i].group_index == attributeIndex) {
            port->buffer_slots[i].group_index = -1;
        }
    }
    return ORBIS_OK;
}

int videoout_driver_change_buffer_attribute(VideoOutPort* port, s32 attributeIndex,
                                            const BufferAttribute* attribute) {
    int res;
    if (attributeIndex >= VIDEO_OUT_MAX_DISPLAY_BUFFER_GROUPS ||
        !port->groups[attributeIndex].is_occupied) {
        c_log(C_LOG_LIB_VIDEO_OUT, C_LOG_LEVEL_ERROR, "ChangeBufferAttribute",
              "Invalid attribute index %d", attributeIndex);
        return ORBIS_VIDEO_OUT_ERROR_INVALID_VALUE;
    }

    res = videoout_check_attribute("ChangeBufferAttribute", attribute);
    if (res != ORBIS_OK) {
        return res;
    }

    c_log(C_LOG_LIB_VIDEO_OUT, C_LOG_LEVEL_INFO, "ChangeBufferAttribute",
          "attributeIndex = %d, pixelFormat = %s, aspectRatio = %d, "
          "tilingMode = %u, width = %u, height = %u, pitchInPixel = %u, option = 0x%x",
          attributeIndex, video_out_pixel_format_string(attribute->pixel_format),
          attribute->aspect_ratio, (u32)attribute->tiling_mode, attribute->width, attribute->height,
          attribute->pitch_in_pixel, attribute->option);

    memcpy(&port->groups[attributeIndex].attrib, attribute, sizeof(BufferAttribute));
    return 0;
}

/* Tells each queue of @queues of the event @ident, its data @ident | (@value << 16) */
static void videoout_trigger_events(retro_atomic_64_t* queues, u64 ident, u64 value) {
    u32 i;
    for (i = 0; i < VIDEO_OUT_MAX_EVENT_QUEUES; i++) {
        const s64 event = (s64)retro_atomic_load_acquire_64(&queues[i]);
        if (event) {
            kernel_equeue_trigger_event(event, ident, KERNEL_EVFILT_VIDEO_OUT,
                                        (void*)(uintptr_t)(ident | (value << 16)));
        }
    }
}

static void videoout_flip(const videoout_request_t* req) {
    VideoOutPort* port = req->port;
    u64 process_time, tsc;

    /* Present the frame. */
    videoout_gpu_present(req->frame, false, true);

    /* Update flip status. */
    process_time = kernel_get_process_time();
    tsc = kernel_read_tsc();
    retro_seqlock_write_begin(&port->flip_seq);
    port->flip_status.count++;
    port->flip_status.process_time = process_time;
    port->flip_status.tsc = tsc;
    port->flip_status.flip_arg = req->flip_arg;
    port->flip_status.current_buffer = req->index;
    retro_seqlock_write_end(&port->flip_seq);
    if (req->eop) {
        retro_atomic_fetch_sub_int(&port->gc_queue, 1);
    }
    retro_atomic_fetch_sub_int(&port->flip_pending, 1);

    /* Trigger flip events for the port. */
    videoout_trigger_events(port->flip_events, ORBIS_VIDEO_OUT_INTERNAL_EVENT_FLIP,
                            (u64)req->flip_arg);

    /* Reset prev flip label */
    if (port->prev_index != -1) {
        port->buffer_labels[port->prev_index] = 0;
        videoout_port_signal_vo_label(port);
    }
    /* save to prev buf index */
    port->prev_index = req->index;
}

static void videoout_draw_blank_frame(void) {
    struct videoout_frame* empty_frame = videoout_gpu_prepare_blank_frame(true);
    videoout_gpu_present(empty_frame, false, false);
}

static void videoout_draw_last_frame(void) {
    struct videoout_frame* frame = videoout_gpu_prepare_last_frame();
    if (frame != NULL) {
        videoout_gpu_present(frame, true, true);
    }
}

/* On the GPU thread */
static void videoout_submit_flip_internal(VideoOutPort* port, s32 index, s64 flip_arg,
                                          bool is_eop) {
    videoout_request_t request;
    if (index == -1) {
        request.frame = videoout_gpu_prepare_blank_frame(false);
    } else {
        const VideoOutBuffer* buffer = &port->buffer_slots[index];
        C_ASSERT_MSG(buffer->group_index >= 0, "SubmitFlipInternal",
                     "Trying to flip an unregistered buffer!");
        request.frame = videoout_gpu_prepare_frame(&port->groups[buffer->group_index],
                                                   buffer->address_left, port->is_hdr);
    }
    request.port = port;
    request.flip_arg = flip_arg;
    request.index = index;
    request.eop = is_eop;

    /* The GPU thread is the only one to write; with the ring full it waits for the present
     * thread to take one */
    while (retro_spsc_write(&requests, &request, sizeof(request)) != sizeof(request)) {
        const int key = retro_eventcount_prepare_wait(&request_taken_ec);
        if (retro_spsc_write_avail(&requests) >= sizeof(request)) {
            retro_eventcount_cancel_wait(&request_taken_ec);
            continue;
        }
        retro_eventcount_commit_wait(&request_taken_ec, key);
    }
}

bool videoout_driver_submit_flip(VideoOutPort* port, s32 index, s64 flip_arg, bool is_eop) {
    /* integral GPU and CPU pending flips counter */
    s32 pending = (s32)retro_atomic_load_relaxed_int(&port->flip_pending);
    for (;;) {
        if (index != -1 && pending > 16) {
            c_log(C_LOG_LIB_VIDEO_OUT, C_LOG_LEVEL_ERROR, "SubmitFlip", "Flip queue is full");
            return false;
        }
        if (retro_atomic_cas_int(&port->flip_pending, pending, pending + 1)) {
            break;
        }
        pending = (s32)retro_atomic_load_acquire_int(&port->flip_pending);
    }
    if (is_eop) {
        retro_atomic_fetch_add_int(&port->gc_queue, 1);
    }
    retro_atomic_store_relaxed_64(&port->submit_tsc, (s64)kernel_read_tsc());

    if (!is_eop) {
        /* Non EOP flips can arrive from any thread so ask GPU thread to perform them */
        videoout_gpu_send_flip(videoout_submit_flip_internal, port, index, flip_arg, is_eop);
    } else {
        videoout_submit_flip_internal(port, index, flip_arg, is_eop);
    }
    return true;
}

static bool videoout_receive_request(videoout_request_t* request) {
    if (retro_spsc_read(&requests, request, sizeof(*request)) != sizeof(*request)) {
        return false;
    }
    retro_eventcount_notify(&request_taken_ec);
    return request->frame != NULL;
}

/* One of the console's vblanks: a flip taken if one is due, the vblank events, the status */
static void videoout_vblank(void) {
    SceVideoOutVblankStatus* vblank_status = &main_port.vblank_status;
    u64 process_time, tsc;

    if (retro_atomic_exchange_int(&main_port.reset_status, 0) != 0) {
        videoout_reset_status();
    }

    /* Check if it's time to take a request. */
    if (vblank_status->count % (u64)(main_port.flip_rate + 1) == 0) {
        videoout_request_t request;
        if (!videoout_receive_request(&request)) {
            if (!main_port.is_open) {
                videoout_draw_blank_frame();
            } else if (ovl_vk_must_keep_drawing()) {
                videoout_draw_last_frame();
            }
        } else {
            videoout_flip(&request);
        }
    }

    /* Trigger vblank events for the port */
    videoout_trigger_events(main_port.vblank_events, ORBIS_VIDEO_OUT_INTERNAL_EVENT_VBLANK,
                            vblank_status->count);

    /* Update vblank status: read by sceVideoOutGetVblankStatus and the vblank waits */
    process_time = kernel_get_process_time();
    tsc = kernel_read_tsc();
    retro_seqlock_write_begin(&main_port.vblank_seq);
    vblank_status->count++;
    vblank_status->process_time = process_time;
    vblank_status->tsc = tsc;
    retro_seqlock_write_end(&main_port.vblank_seq);
    retro_eventcount_notify(&main_port.vblank_ec);
}

static void videoout_present_thread(void* arg) {
    /* The console's vblanks come from the frontend's frames (common/vblank_clock.h): the
     * thread parks until the core's next retro_run, and gives the vblanks that frame owes. */
    uint32_t ticks;
    vblank_clock_t clock;
    (void)arg;
    thread_name_set("shadPS4:PresentThread");
    ticks = libretro_vblank_ticks();
    vblank_clock_init(&clock, ticks, emu_settings_get_vblank_frequency());

    for (;;) {
        unsigned owed = vblank_clock_owed(&clock, ticks, emu_settings_get_vblank_frequency());
        if (owed == 0) {
            if (!libretro_wait_vblank_ticks(ticks, &present_thread.stop)) {
                break;
            }
            /* The frames of the game's time and their sound are started by the frame thread
             * (libretro/host.cpp), whatever this thread is busy with */
            ticks = libretro_vblank_ticks();
            continue;
        }
        for (; owed != 0 && !stop_thread_stopping(&present_thread); owed--) {
            vblank_clock_take(&clock);
            videoout_vblank();
        }
        /* Asked to stop with vblanks still owed: they are not given, and would be found owed
         * again for ever */
        if (stop_thread_stopping(&present_thread)) {
            break;
        }
    }
}
