/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdlib.h>
#include <string.h>

#include <retro_atomic.h>

#include "video_core/amdgpu/cmd_copy_ring.h"

struct cmd_copy_ring {
    uint32_t* dcb;
    uint32_t* ccb;
    size_t dcb_seg_words;
    size_t ccb_seg_words;
    unsigned segments;
    unsigned current;
    size_t dcb_used;
    size_t ccb_used;
    /* Submissions handed out, and in each segment the last one it holds */
    size_t gfx_seq;
    size_t ce_seq;
    size_t last_gfx[CMD_COPY_RING_MAX_SEGMENTS];
    size_t last_ce[CMD_COPY_RING_MAX_SEGMENTS];
    /* Submissions the GPU thread has finished */
    retro_atomic_size_t gfx_done;
    retro_atomic_size_t ce_done;
};

cmd_copy_ring_t* cmd_copy_ring_new(size_t dcb_words, size_t ccb_words, unsigned segments) {
    cmd_copy_ring_t* ring;
    if (segments < 2 || segments > CMD_COPY_RING_MAX_SEGMENTS)
        return NULL;
    ring = (cmd_copy_ring_t*)calloc(1, sizeof(*ring));
    if (!ring)
        return NULL;
    ring->dcb = (uint32_t*)malloc(dcb_words * segments * sizeof(uint32_t));
    ring->ccb = (uint32_t*)malloc(ccb_words * segments * sizeof(uint32_t));
    if (!ring->dcb || !ring->ccb) {
        cmd_copy_ring_free(ring);
        return NULL;
    }
    ring->dcb_seg_words = dcb_words;
    ring->ccb_seg_words = ccb_words;
    ring->segments = segments;
    retro_atomic_size_init(&ring->gfx_done, 0);
    retro_atomic_size_init(&ring->ce_done, 0);
    return ring;
}

void cmd_copy_ring_free(cmd_copy_ring_t* ring) {
    if (!ring)
        return;
    free(ring->dcb);
    free(ring->ccb);
    free(ring);
}

/* Whether the GPU thread has finished all that segment @seg holds. The counts
 * only grow; compared by their difference, so a wrap does not matter. */
static int segment_free(cmd_copy_ring_t* ring, unsigned seg) {
    size_t gfx = retro_atomic_load_acquire_size(&ring->gfx_done);
    size_t ce = retro_atomic_load_acquire_size(&ring->ce_done);
    return (size_t)(gfx - ring->last_gfx[seg]) < ((size_t)-1) / 2 &&
           (size_t)(ce - ring->last_ce[seg]) < ((size_t)-1) / 2;
}

static int advance(cmd_copy_ring_t* ring) {
    unsigned next = (ring->current + 1) % ring->segments;
    if (!segment_free(ring, next))
        return 0;
    ring->current = next;
    ring->dcb_used = 0;
    ring->ccb_used = 0;
    /* Nothing in it yet: free again as soon as what is handed out is done */
    ring->last_gfx[next] = ring->gfx_seq;
    ring->last_ce[next] = ring->ce_seq;
    return 1;
}

int cmd_copy_ring_copy(cmd_copy_ring_t* ring, const uint32_t* dcb, size_t dcb_words,
                       const uint32_t* ccb, size_t ccb_words, const uint32_t** dcb_out,
                       const uint32_t** ccb_out) {
    uint32_t* d;
    uint32_t* c;
    *dcb_out = dcb;
    *ccb_out = ccb;
    if (dcb_words > ring->dcb_seg_words || ccb_words > ring->ccb_seg_words)
        return 0;
    if (ring->dcb_used + dcb_words > ring->dcb_seg_words ||
        ring->ccb_used + ccb_words > ring->ccb_seg_words) {
        if (!advance(ring))
            return 0;
    }
    d = ring->dcb + ring->current * ring->dcb_seg_words + ring->dcb_used;
    c = ring->ccb + ring->current * ring->ccb_seg_words + ring->ccb_used;
    if (dcb_words)
        memcpy(d, dcb, dcb_words * sizeof(uint32_t));
    if (ccb_words)
        memcpy(c, ccb, ccb_words * sizeof(uint32_t));
    ring->dcb_used += dcb_words;
    ring->ccb_used += ccb_words;
    *dcb_out = d;
    *ccb_out = c;
    return 1;
}

void cmd_copy_ring_submitted(cmd_copy_ring_t* ring, int has_ce) {
    ring->last_gfx[ring->current] = ++ring->gfx_seq;
    if (has_ce)
        ring->last_ce[ring->current] = ++ring->ce_seq;
}

void cmd_copy_ring_frame_done(cmd_copy_ring_t* ring) {
    if (ring->dcb_used || ring->ccb_used)
        advance(ring);
}

void cmd_copy_ring_gfx_finished(cmd_copy_ring_t* ring) {
    retro_atomic_fetch_add_size(&ring->gfx_done, 1);
}

void cmd_copy_ring_ce_finished(cmd_copy_ring_t* ring) {
    retro_atomic_fetch_add_size(&ring->ce_done, 1);
}
