/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdlib.h>
#include <string.h>

#include "core/libraries/audio3d/audio3d_port.h"

/* How a bed of more than two channels folds into stereo */
#define DOWNMIX_FRONT 1.0f
#define DOWNMIX_CENTER 0.7071f
#define DOWNMIX_SURROUND 0.7071f

#define OBJECT_SLOTS 1024u /* the part of an id that is the slot */

/* Counts are compared by their difference, so that they may wrap */
static int count_diff(int a, int b) {
    return (int)((unsigned)a - (unsigned)b);
}

int audio3d_ring_init(audio3d_ring_t* ring, unsigned depth, unsigned block_bytes) {
    memset(ring, 0, sizeof(*ring));
    ring->data = (unsigned char*)calloc(depth, block_bytes);
    ring->info = (audio3d_block_t*)calloc(depth, sizeof(*ring->info));
    ring->depth = depth;
    ring->block_bytes = block_bytes;
    retro_atomic_int_init(&ring->head, 0);
    retro_atomic_int_init(&ring->tail, 0);
    retro_atomic_int_init(&ring->drop_to, 0);
    retro_atomic_int_init(&ring->putting, 0);
    retro_atomic_int_init(&ring->taking, 0);
    if (!ring->data || !ring->info) {
        audio3d_ring_free(ring);
        return 0;
    }
    return 1;
}

void audio3d_ring_free(audio3d_ring_t* ring) {
    free(ring->data);
    free(ring->info);
    ring->data = NULL;
    ring->info = NULL;
}

/* Taking: the head past what was dropped. Only with the taking flag held. */
static int ring_skip_dropped(audio3d_ring_t* ring) {
    int head = retro_atomic_load_acquire_int(&ring->head);
    int drop_to = retro_atomic_load_acquire_int(&ring->drop_to);
    if (count_diff(drop_to, head) > 0) {
        head = drop_to;
        retro_atomic_store_release_int(&ring->head, head);
    }
    return head;
}

/* The blocks dropped are given back now if no thread is taking one */
static void ring_reclaim(audio3d_ring_t* ring) {
    if (retro_atomic_cas_int(&ring->taking, 0, 1)) {
        (void)ring_skip_dropped(ring);
        retro_atomic_store_release_int(&ring->taking, 0);
    }
}

unsigned char* audio3d_ring_put_begin(audio3d_ring_t* ring) {
    int tail;
    if (!retro_atomic_cas_int(&ring->putting, 0, 1)) {
        return NULL;
    }
    tail = retro_atomic_load_acquire_int(&ring->tail);
    if (count_diff(tail, retro_atomic_load_acquire_int(&ring->head)) >= (int)ring->depth) {
        ring_reclaim(ring);
        if (count_diff(tail, retro_atomic_load_acquire_int(&ring->head)) >= (int)ring->depth) {
            retro_atomic_store_release_int(&ring->putting, 0);
            return NULL;
        }
    }
    return ring->data + (size_t)((unsigned)tail % ring->depth) * ring->block_bytes;
}

void audio3d_ring_put_end(audio3d_ring_t* ring, unsigned frames, unsigned channels, int format) {
    const int tail = retro_atomic_load_acquire_int(&ring->tail);
    audio3d_block_t* info = &ring->info[(unsigned)tail % ring->depth];
    info->frames = frames;
    info->channels = channels;
    info->format = format;
    retro_atomic_store_release_int(&ring->tail, (int)((unsigned)tail + 1u));
    retro_atomic_store_release_int(&ring->putting, 0);
}

void audio3d_ring_put_cancel(audio3d_ring_t* ring) {
    retro_atomic_store_release_int(&ring->putting, 0);
}

const unsigned char* audio3d_ring_take_begin(audio3d_ring_t* ring, audio3d_block_t* info) {
    int head;
    unsigned slot;
    if (!retro_atomic_cas_int(&ring->taking, 0, 1)) {
        return NULL;
    }
    head = ring_skip_dropped(ring);
    if (head == retro_atomic_load_acquire_int(&ring->tail)) {
        retro_atomic_store_release_int(&ring->taking, 0);
        return NULL;
    }
    slot = (unsigned)head % ring->depth;
    if (info) {
        *info = ring->info[slot];
    }
    return ring->data + (size_t)slot * ring->block_bytes;
}

void audio3d_ring_take_end(audio3d_ring_t* ring) {
    const int head = retro_atomic_load_acquire_int(&ring->head);
    retro_atomic_store_release_int(&ring->head, (int)((unsigned)head + 1u));
    retro_atomic_store_release_int(&ring->taking, 0);
}

void audio3d_ring_reset(audio3d_ring_t* ring) {
    const int tail = retro_atomic_load_acquire_int(&ring->tail);
    /* Only ever forward */
    for (;;) {
        const int drop_to = retro_atomic_load_acquire_int(&ring->drop_to);
        if (count_diff(tail, drop_to) <= 0 || retro_atomic_cas_int(&ring->drop_to, drop_to, tail)) {
            break;
        }
    }
    ring_reclaim(ring);
}

unsigned audio3d_ring_level(audio3d_ring_t* ring) {
    const int tail = retro_atomic_load_acquire_int(&ring->tail);
    int head = retro_atomic_load_acquire_int(&ring->head);
    const int drop_to = retro_atomic_load_acquire_int(&ring->drop_to);
    if (count_diff(drop_to, head) > 0) {
        head = drop_to;
    }
    return count_diff(tail, head) > 0 ? (unsigned)count_diff(tail, head) : 0u;
}

int audio3d_ring_put_pcm(audio3d_ring_t* ring, const void* src, unsigned frames,
                         unsigned frames_out, unsigned channels, int format) {
    const unsigned sample_bytes = format == AUDIO3D_S16 ? 2u : 4u;
    const size_t block = (size_t)frames_out * channels * sample_bytes;
    size_t copy;
    unsigned char* dst;
    if (block > ring->block_bytes) {
        return AUDIO3D_RING_TOO_BIG;
    }
    dst = audio3d_ring_put_begin(ring);
    if (!dst) {
        return AUDIO3D_RING_FULL;
    }
    copy = (size_t)(frames < frames_out ? frames : frames_out) * channels * sample_bytes;
    memcpy(dst, src, copy);
    memset(dst + copy, 0, block - copy);
    audio3d_ring_put_end(ring, frames_out, channels, format);
    return 0;
}

int audio3d_port_init(audio3d_port_t* port, unsigned granularity, unsigned depth,
                      unsigned max_objects, unsigned out_channels) {
    unsigned i;
    memset(port, 0, sizeof(*port));
    port->granularity = granularity;
    port->depth = depth;
    port->max_objects = max_objects;
    port->out_channels = out_channels;
    retro_atomic_int_init(&port->next_object, 0);
    retro_atomic_int_init(&port->mixing, 0);
    port->objects = (audio3d_object_t*)calloc(max_objects, sizeof(*port->objects));
    if (!port->objects || !audio3d_ring_init(&port->bed, depth, granularity * 8u * 4u) ||
        !audio3d_ring_init(&port->mixed, depth, granularity * out_channels * 4u)) {
        audio3d_port_free(port);
        return 0;
    }
    for (i = 0; i < max_objects; i++) {
        audio3d_object_t* object = &port->objects[i];
        retro_atomic_int_init(&object->state, AUDIO3D_OBJECT_FREE);
        retro_atomic_int_init(&object->generation, 0);
        retro_atomic_int_init(&object->gain, 0);
        /* Mono, float at most */
        if (!audio3d_ring_init(&object->pcm, depth, granularity * 4u)) {
            port->max_objects = i;
            audio3d_port_free(port);
            return 0;
        }
    }
    return 1;
}

void audio3d_port_free(audio3d_port_t* port) {
    unsigned i;
    if (port->objects) {
        for (i = 0; i < port->max_objects; i++) {
            audio3d_ring_free(&port->objects[i].pcm);
        }
    }
    free(port->objects);
    port->objects = NULL;
    audio3d_ring_free(&port->bed);
    audio3d_ring_free(&port->mixed);
}

static float gain_of(audio3d_object_t* object) {
    const int bits = retro_atomic_load_acquire_int(&object->gain);
    float gain;
    memcpy(&gain, &bits, sizeof(gain));
    return gain;
}

void audio3d_object_set_gain(audio3d_object_t* object, float gain) {
    int bits;
    memcpy(&bits, &gain, sizeof(bits));
    retro_atomic_store_release_int(&object->gain, bits);
}

void audio3d_object_reset(audio3d_object_t* object) {
    audio3d_ring_reset(&object->pcm);
    retro_atomic_store_release_int(&object->gain, 0);
}

int audio3d_object_reserve(audio3d_port_t* port, unsigned* id) {
    unsigned n;
    const unsigned start =
        (unsigned)retro_atomic_load_acquire_int(&port->next_object) % port->max_objects;
    for (n = 0; n < port->max_objects; n++) {
        const unsigned slot = (start + n) % port->max_objects;
        audio3d_object_t* object = &port->objects[slot];
        unsigned generation;
        if (retro_atomic_load_acquire_int(&object->state) != AUDIO3D_OBJECT_FREE ||
            !retro_atomic_cas_int(&object->state, AUDIO3D_OBJECT_FREE, AUDIO3D_OBJECT_CLAIMED)) {
            continue;
        }
        /* A new id: the slot, and how often it has been reserved (never 0, and an id never
         * all ones) */
        generation =
            ((unsigned)retro_atomic_load_acquire_int(&object->generation) + 1u) & 0x1fffffu;
        if (generation == 0) {
            generation = 1;
        }
        retro_atomic_store_release_int(&object->generation, (int)generation);
        audio3d_object_reset(object);
        retro_atomic_store_release_int(&port->next_object, (int)(slot + 1u));
        retro_atomic_store_release_int(&object->state, AUDIO3D_OBJECT_LIVE);
        *id = generation * OBJECT_SLOTS + slot;
        return 1;
    }
    return 0;
}

static audio3d_object_t* object_of(audio3d_port_t* port, unsigned id) {
    const unsigned slot = id % OBJECT_SLOTS;
    audio3d_object_t* object;
    if (slot >= port->max_objects) {
        return NULL;
    }
    object = &port->objects[slot];
    if ((unsigned)retro_atomic_load_acquire_int(&object->generation) != id / OBJECT_SLOTS) {
        return NULL;
    }
    return object;
}

int audio3d_object_state(audio3d_port_t* port, unsigned id) {
    audio3d_object_t* object = object_of(port, id);
    return object ? retro_atomic_load_acquire_int(&object->state) : -1;
}

audio3d_object_t* audio3d_object_get(audio3d_port_t* port, unsigned id) {
    audio3d_object_t* object = object_of(port, id);
    if (!object || retro_atomic_load_acquire_int(&object->state) != AUDIO3D_OBJECT_LIVE) {
        return NULL;
    }
    return object;
}

/* An object unreserved is freed once its sound is all mixed: by whichever thread sees that */
static void object_free_if_drained(audio3d_object_t* object) {
    if (retro_atomic_load_acquire_int(&object->state) == AUDIO3D_OBJECT_LEAVING &&
        audio3d_ring_level(&object->pcm) == 0) {
        (void)retro_atomic_cas_int(&object->state, AUDIO3D_OBJECT_LEAVING, AUDIO3D_OBJECT_FREE);
    }
}

int audio3d_object_unreserve(audio3d_port_t* port, unsigned id) {
    audio3d_object_t* object = object_of(port, id);
    if (!object ||
        !retro_atomic_cas_int(&object->state, AUDIO3D_OBJECT_LIVE, AUDIO3D_OBJECT_LEAVING)) {
        return 0;
    }
    object_free_if_drained(object);
    return 1;
}

int audio3d_port_has_input(audio3d_port_t* port) {
    unsigned i;
    if (audio3d_ring_level(&port->bed) != 0) {
        return 1;
    }
    for (i = 0; i < port->max_objects; i++) {
        const int state = retro_atomic_load_acquire_int(&port->objects[i].state);
        if ((state == AUDIO3D_OBJECT_LIVE || state == AUDIO3D_OBJECT_LEAVING) &&
            audio3d_ring_level(&port->objects[i].pcm) != 0) {
            return 1;
        }
    }
    return 0;
}

/* A ring's blocks start on 4-byte bounds: read in place */
static float sample_at(const audio3d_block_t* info, const unsigned char* data, unsigned frame,
                       unsigned channel) {
    const unsigned i = frame * info->channels + channel;
    return info->format == AUDIO3D_S16 ? ((const short*)data)[i] / 32768.0f
                                       : ((const float*)data)[i];
}

/* The oldest block of @ring, times @gain, added into @mix; taken either way */
static void mix_in(audio3d_port_t* port, audio3d_ring_t* ring, float gain, float* mix) {
    audio3d_block_t info;
    const unsigned char* data = audio3d_ring_take_begin(ring, &info);
    unsigned frames, i, c;
    if (!data) {
        return;
    }
    /* No gain: silent, and dropped */
    if (gain == 0.0f) {
        audio3d_ring_take_end(ring);
        return;
    }
    frames = info.frames < port->granularity ? info.frames : port->granularity;
    if (port->out_channels == 8) {
        /* Each channel to its own place, as the console orders them (front pair, centre, LFE,
         * then the surround pairs); mono to the front pair */
        for (i = 0; i < frames; i++) {
            float* out = mix + i * 8u;
            if (info.channels == 1) {
                const float v = sample_at(&info, data, i, 0) * gain;
                out[0] += v;
                out[1] += v;
            } else {
                for (c = 0; c < info.channels && c < 8u; c++) {
                    out[c] += sample_at(&info, data, i, c) * gain;
                }
            }
        }
    } else {
        for (i = 0; i < frames; i++) {
            float left, right;
            if (info.channels == 1) {
                left = right = sample_at(&info, data, i, 0);
            } else {
                left = DOWNMIX_FRONT * sample_at(&info, data, i, 0);
                right = DOWNMIX_FRONT * sample_at(&info, data, i, 1);
                if (info.channels >= 3) {
                    const float center = DOWNMIX_CENTER * sample_at(&info, data, i, 2);
                    left += center;
                    right += center;
                }
                /* The LFE is left out of the fold */
                if (info.channels >= 6) {
                    left += DOWNMIX_SURROUND * sample_at(&info, data, i, 4);
                    right += DOWNMIX_SURROUND * sample_at(&info, data, i, 5);
                }
                if (info.channels >= 8) {
                    left += DOWNMIX_SURROUND * sample_at(&info, data, i, 6);
                    right += DOWNMIX_SURROUND * sample_at(&info, data, i, 7);
                }
            }
            mix[i * 2u + 0] += left * gain;
            mix[i * 2u + 1] += right * gain;
        }
    }
    audio3d_ring_take_end(ring);
}

int audio3d_port_mix(audio3d_port_t* port) {
    unsigned char* block;
    float* mix;
    unsigned i;
    if (!retro_atomic_cas_int(&port->mixing, 0, 1)) {
        return 0;
    }
    block = audio3d_ring_put_begin(&port->mixed);
    if (!block) {
        retro_atomic_store_release_int(&port->mixing, 0);
        return 0;
    }
    /* The mix is made in the block it is played from */
    mix = (float*)block;
    memset(mix, 0, (size_t)port->granularity * port->out_channels * sizeof(float));
    mix_in(port, &port->bed, 1.0f, mix);
    for (i = 0; i < port->max_objects; i++) {
        audio3d_object_t* object = &port->objects[i];
        const int state = retro_atomic_load_acquire_int(&object->state);
        if (state != AUDIO3D_OBJECT_LIVE && state != AUDIO3D_OBJECT_LEAVING) {
            continue;
        }
        mix_in(port, &object->pcm, gain_of(object), mix);
        object_free_if_drained(object);
    }
    audio3d_ring_put_end(&port->mixed, port->granularity, port->out_channels, AUDIO3D_FLOAT);
    retro_atomic_store_release_int(&port->mixing, 0);
    return 1;
}
