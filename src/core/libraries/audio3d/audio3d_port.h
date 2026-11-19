/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* libSceAudio3d's ports, without a lock. A port's sound waits in rings of blocks the ring owns,
 * made when the port opens: one thread puts a block in (the game's thread handing over a bed
 * or an object's sound) and one takes it out (the mix, or the push to the output), each
 * claimed by a flag that turns a second thread away rather than making it wait. Objects are a
 * table made when the port opens, reserved by compare-and-swap; an object's gain is one atomic
 * word. Nothing is allocated once the port is open. */

#ifndef SHADPS4_AUDIO3D_PORT_H
#define SHADPS4_AUDIO3D_PORT_H

#include <retro_atomic.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

enum { AUDIO3D_S16 = 0, AUDIO3D_FLOAT = 1 };

/* What a block holds */
typedef struct audio3d_block {
    unsigned frames;
    unsigned channels;
    int format;
} audio3d_block_t;

/* Blocks put and taken in order: at most depth queued, each of block_bytes. The counts only
 * grow, so a queue is empty where they meet whoever reads them. */
typedef struct audio3d_ring {
    unsigned char* data;
    audio3d_block_t* info;
    unsigned depth;
    unsigned block_bytes;
    retro_atomic_int_t head;    /* blocks taken */
    retro_atomic_int_t tail;    /* blocks put */
    retro_atomic_int_t drop_to; /* blocks put before this count are dropped, not taken */
    retro_atomic_int_t putting; /* a thread is putting one in */
    retro_atomic_int_t taking;  /* a thread is taking one out */
} audio3d_ring_t;

/* Empty, @depth blocks of @block_bytes made now. 0 if they could not be. */
int audio3d_ring_init(audio3d_ring_t* ring, unsigned depth, unsigned block_bytes);
void audio3d_ring_free(audio3d_ring_t* ring);

/* The next block to fill, or NULL: the ring is full, or another thread is putting one in. A
 * block got must be ended or cancelled. */
unsigned char* audio3d_ring_put_begin(audio3d_ring_t* ring);
void audio3d_ring_put_end(audio3d_ring_t* ring, unsigned frames, unsigned channels, int format);
void audio3d_ring_put_cancel(audio3d_ring_t* ring);

/* The oldest block and what it holds, or NULL: nothing queued, or another thread is taking
 * one. It stays the ring's, and is not filled again, until audio3d_ring_take_end. */
const unsigned char* audio3d_ring_take_begin(audio3d_ring_t* ring, audio3d_block_t* info);
void audio3d_ring_take_end(audio3d_ring_t* ring);

/* From any thread: what is queued now is dropped; what is put after stays. */
void audio3d_ring_reset(audio3d_ring_t* ring);

/* Blocks queued and not dropped. */
unsigned audio3d_ring_level(audio3d_ring_t* ring);

/* Copies @frames frames (at most @frames_out) of @channels channels of @format from @src into
 * the next block, the rest of @frames_out frames silent. 0; AUDIO3D_RING_FULL (full, or
 * another thread is putting one in); AUDIO3D_RING_TOO_BIG (more than a block holds). */
enum { AUDIO3D_RING_FULL = 1, AUDIO3D_RING_TOO_BIG = 2 };
int audio3d_ring_put_pcm(audio3d_ring_t* ring, const void* src, unsigned frames,
                         unsigned frames_out, unsigned channels, int format);

/* An object: a ring of mono sound and a gain. */
enum {
    AUDIO3D_OBJECT_FREE = 0,
    AUDIO3D_OBJECT_CLAIMED, /* being reserved */
    AUDIO3D_OBJECT_LIVE,
    AUDIO3D_OBJECT_LEAVING /* unreserved, its sound still to be mixed */
};

typedef struct audio3d_object {
    retro_atomic_int_t state;
    retro_atomic_int_t generation; /* reservations of it so far: part of its id */
    retro_atomic_int_t gain;       /* the float's bits: 0 (silent) until the game sets one */
    audio3d_ring_t pcm;
} audio3d_object_t;

typedef struct audio3d_port {
    unsigned granularity; /* frames a block */
    unsigned depth;       /* blocks a ring queues */
    unsigned max_objects;
    unsigned out_channels; /* the mix's: 2 or 8 */
    audio3d_ring_t bed;    /* up to 8 channels */
    audio3d_ring_t mixed;  /* float, out_channels */
    audio3d_object_t* objects;
    retro_atomic_int_t next_object; /* where reserving looks first */
    retro_atomic_int_t mixing;      /* a thread is mixing */
} audio3d_port_t;

/* Everything the port holds made now. 0 if it could not be. */
int audio3d_port_init(audio3d_port_t* port, unsigned granularity, unsigned depth,
                      unsigned max_objects, unsigned out_channels);
void audio3d_port_free(audio3d_port_t* port);

/* Reserves a free object: 1 and *@id, 0 if none is free. */
int audio3d_object_reserve(audio3d_port_t* port, unsigned* id);
/* The state of the object @id names (AUDIO3D_OBJECT_*), or -1 if it names none. */
int audio3d_object_state(audio3d_port_t* port, unsigned id);
/* The live object @id names, or NULL. */
audio3d_object_t* audio3d_object_get(audio3d_port_t* port, unsigned id);
/* Unreserves @id: 1, or 0 if it was not reserved. Its queued sound is still mixed. */
int audio3d_object_unreserve(audio3d_port_t* port, unsigned id);
void audio3d_object_set_gain(audio3d_object_t* object, float gain);
/* Its queued sound dropped and its gain back to silent */
void audio3d_object_reset(audio3d_object_t* object);

/* Whether a bed block or an object's sound is queued. */
int audio3d_port_has_input(audio3d_port_t* port);

/* Mixes a block of each of the bed and the objects into the next mixed block: 1, or 0 if
 * the mixed queue is full or another thread is mixing. Objects unreserved whose sound is all
 * mixed are freed. */
int audio3d_port_mix(audio3d_port_t* port);

RETRO_END_DECLS

#endif
