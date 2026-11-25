/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_VIDEOOUT_DRIVER_H
#define SHADPS4_VIDEOOUT_DRIVER_H

/* libSceVideoOut's driver: the one port, its present thread, and the flips the game and the
 * GPU submit to it. Defined in driver.c. */

#include <boolean.h>
#include <retro_atomic.h>
#include <retro_common_api.h>
#include <rthreads/retro_eventcount.h>
#include <rthreads/retro_seqlock.h>
#include <stdint.h>

#include "common/types.h"
#include "core/libraries/videoout/buffer.h"
#include "core/libraries/videoout/video_out.h"

RETRO_BEGIN_DECLS

/* The event queues a port tells of its flips, and of its vblanks */
#define VIDEO_OUT_MAX_EVENT_QUEUES 16

typedef struct VideoOutPort {
    SceVideoOutResolutionStatus resolution;
    VideoOutBuffer buffer_slots[VIDEO_OUT_MAX_DISPLAY_BUFFERS];
    u64 buffer_labels[VIDEO_OUT_MAX_DISPLAY_BUFFERS]; /* should be contiguous in memory */
    BufferAttributeGroup groups[VIDEO_OUT_MAX_DISPLAY_BUFFER_GROUPS];
    /* The present thread's part of the flip status (count, times, flip_arg, current_buffer),
     * written by it alone under flip_seq; the counts the submitters change are flip_pending and
     * gc_queue, and submit_tsc. videoout_port_get_flip_status puts them together. */
    FlipStatus flip_status;
    retro_seqlock_t flip_seq;
    retro_atomic_int_t flip_pending;
    retro_atomic_int_t gc_queue;
    retro_atomic_64_t submit_tsc;
    /* Written by the present thread alone, under vblank_seq; vblank_ec is notified after
     * each. */
    SceVideoOutVblankStatus vblank_status;
    retro_seqlock_t vblank_seq;
    retro_eventcount_t vblank_ec;
    /* The event queues to tell; a slot holds 0 when free, and is taken and freed with a
     * compare-exchange, so the present thread walks them while the game adds and deletes. */
    retro_atomic_64_t flip_events[VIDEO_OUT_MAX_EVENT_QUEUES];
    retro_atomic_64_t vblank_events[VIDEO_OUT_MAX_EVENT_QUEUES];
    /* The GPU thread waiting on a VO label parks on vo_ec; a flip that clears one notifies
     * it. */
    retro_eventcount_t vo_ec;
    /* Close asks the present thread to clear what only it writes. */
    retro_atomic_int_t reset_status;
    int flip_rate;
    int prev_index;
    bool is_open;
    bool is_hdr;
} VideoOutPort;

/* An event queue (@eq) added to, or removed from, one of the port's lists: false when there
 * is no room */
bool videoout_port_add_event_queue(retro_atomic_64_t* queues, s64 eq);
void videoout_port_remove_event_queue(retro_atomic_64_t* queues, s64 eq);

void videoout_port_get_flip_status(VideoOutPort* port, FlipStatus* status);
void videoout_port_get_vblank_status(VideoOutPort* port, SceVideoOutVblankStatus* status);
/* Until the next vblank (not counted running meanwhile: kernel_game_wait) */
void videoout_port_wait_vblank(VideoOutPort* port);

/* The first group not occupied; VIDEO_OUT_MAX_DISPLAY_BUFFER_GROUPS when all are */
s32 videoout_port_find_free_group(const VideoOutPort* port);

/* @address is one of the port's buffer labels */
bool videoout_port_is_vo_label(const VideoOutPort* port, const u64* address);
/* From the GPU thread: until @ready(@ctx), looked at again whenever a label is cleared */
void videoout_port_wait_vo_label(VideoOutPort* port, int (*ready)(void* ctx), void* ctx);
void videoout_port_signal_vo_label(VideoOutPort* port);

/* The ServiceThreadParams sceVideoOutOpen may be given */
typedef struct ServiceThreadParams {
    u32 unknown;
    bool set_priority;
    u32 priority;
    bool set_affinity;
    u64 affinity;
} ServiceThreadParams;

/* The driver, with its port of @width by @height, and its present thread started; once */
void videoout_driver_init(u32 width, u32 height);
/* The present thread stopped (woken from its wait for the frontend's next frame) */
void videoout_driver_deinit(void);

int videoout_driver_open(const ServiceThreadParams* params);
void videoout_driver_close(s32 handle);
/* NULL for any handle but 1 */
VideoOutPort* videoout_driver_get_port(s32 handle);
int videoout_driver_register_buffers(VideoOutPort* port, s32 startIndex, void* const* addresses,
                                     s32 bufferNum, const BufferAttribute* attribute);
int videoout_driver_unregister_buffers(VideoOutPort* port, s32 attributeIndex);
int videoout_driver_change_buffer_attribute(VideoOutPort* port, s32 bufferIndex,
                                            const BufferAttribute* attribute);
/* false when the flip queue is full */
bool videoout_driver_submit_flip(VideoOutPort* port, s32 index, s64 flip_arg, bool is_eop);

RETRO_END_DECLS

#endif
