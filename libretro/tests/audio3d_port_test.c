/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* libSceAudio3d's ports without a lock (audio3d_port.c): a ring's blocks in order and whole
 * while one thread puts, one takes and a third drops what is queued; objects reserved and
 * unreserved by four threads while a fifth mixes, no slot had by two at once; and the mix's
 * sums, stereo and 7.1, as the library made them under its lock. */

#include <stdio.h>
#include <string.h>

#include <retro_atomic.h>
#include <rthreads/rthreads.h>

#include "core/libraries/audio3d/audio3d_port.h"

static int failures;

#define CHECK(cond, ...)                                                                         \
    do {                                                                                         \
        if (!(cond)) {                                                                           \
            printf("audio3d_port: " __VA_ARGS__);                                                \
            printf("\n");                                                                        \
            failures++;                                                                          \
        }                                                                                        \
    } while (0)

static int near(float a, float b) {
    return a - b < 1e-5f && b - a < 1e-5f;
}

static void test_ring(void) {
    audio3d_ring_t ring;
    audio3d_block_t info;
    const unsigned char* got;
    short pcm[4] = {1000, -1000, 2000, -2000};
    unsigned char* block;
    int i;
    CHECK(audio3d_ring_init(&ring, 3, 16), "ring not made");
    for (i = 0; i < 3; i++) {
        block = audio3d_ring_put_begin(&ring);
        CHECK(block != NULL, "block %d not given", i);
        block[0] = (unsigned char)i;
        audio3d_ring_put_end(&ring, (unsigned)i + 1, 1, AUDIO3D_S16);
    }
    CHECK(audio3d_ring_put_begin(&ring) == NULL, "a full ring gave a block");
    CHECK(audio3d_ring_level(&ring) == 3, "level %u, not 3", audio3d_ring_level(&ring));
    got = audio3d_ring_take_begin(&ring, &info);
    CHECK(got && got[0] == 0 && info.frames == 1, "first block not first");
    /* Taken and not ended: not filled again */
    CHECK(audio3d_ring_put_begin(&ring) == NULL, "a block being taken was given to fill");
    audio3d_ring_take_end(&ring);
    /* Dropped: the two queued; one put after stays */
    audio3d_ring_reset(&ring);
    CHECK(audio3d_ring_level(&ring) == 0, "level %u after a reset", audio3d_ring_level(&ring));
    block = audio3d_ring_put_begin(&ring);
    CHECK(block != NULL, "no block after a reset");
    block[0] = 9;
    audio3d_ring_put_end(&ring, 9, 1, AUDIO3D_S16);
    got = audio3d_ring_take_begin(&ring, &info);
    CHECK(got && got[0] == 9 && info.frames == 9, "a dropped block was taken, or the new lost");
    audio3d_ring_take_end(&ring);
    CHECK(audio3d_ring_take_begin(&ring, NULL) == NULL, "an empty ring gave a block");
    /* Sound copied and the rest silent; too big refused */
    CHECK(audio3d_ring_put_pcm(&ring, pcm, 2, 4, 2, AUDIO3D_S16) == 0, "pcm not put");
    got = audio3d_ring_take_begin(&ring, &info);
    CHECK(got && memcmp(got, pcm, 8) == 0 && got[8] == 0 && got[15] == 0 && info.frames == 4 &&
              info.channels == 2,
          "pcm not copied and padded");
    audio3d_ring_take_end(&ring);
    CHECK(audio3d_ring_put_pcm(&ring, pcm, 2, 5, 2, AUDIO3D_S16) == AUDIO3D_RING_TOO_BIG,
          "too big taken");
    audio3d_ring_free(&ring);
}

/* One thread puts blocks filled with their number, one takes them, one drops what is queued
 * now and then: every block taken whole, and in order */
#define RING_BLOCKS 200000
static audio3d_ring_t shared_ring;
static retro_atomic_int_t ring_done;
static long ring_taken, ring_torn, ring_backwards;

static void ring_putter(void* arg) {
    int n = 1;
    (void)arg;
    while (n <= RING_BLOCKS) {
        unsigned char* block = audio3d_ring_put_begin(&shared_ring);
        if (!block) {
            sthread_yield();
            continue;
        }
        memset(block, n & 0xff, 64);
        memcpy(block, &n, sizeof(n));
        audio3d_ring_put_end(&shared_ring, (unsigned)n, 1, AUDIO3D_FLOAT);
        n++;
    }
    retro_atomic_store_release_int(&ring_done, 1);
}

static void ring_dropper(void* arg) {
    (void)arg;
    while (!retro_atomic_load_acquire_int(&ring_done)) {
        audio3d_ring_reset(&shared_ring);
        sthread_yield();
    }
}

static void ring_taker(void* arg) {
    int last = 0;
    (void)arg;
    for (;;) {
        audio3d_block_t info;
        const unsigned char* block = audio3d_ring_take_begin(&shared_ring, &info);
        int n, i;
        if (!block) {
            if (retro_atomic_load_acquire_int(&ring_done) &&
                audio3d_ring_level(&shared_ring) == 0) {
                break;
            }
            sthread_yield();
            continue;
        }
        memcpy(&n, block, sizeof(n));
        for (i = (int)sizeof(n); i < 64; i++) {
            if (block[i] != (unsigned char)(n & 0xff)) {
                ring_torn++;
                break;
            }
        }
        if (n <= last || (unsigned)n != info.frames) {
            ring_backwards++;
        }
        last = n;
        ring_taken++;
        audio3d_ring_take_end(&shared_ring);
    }
}

static void test_ring_threads(void) {
    sthread_t *put, *take, *drop;
    CHECK(audio3d_ring_init(&shared_ring, 4, 64), "ring not made");
    retro_atomic_int_init(&ring_done, 0);
    put = sthread_create(ring_putter, NULL);
    take = sthread_create(ring_taker, NULL);
    drop = sthread_create(ring_dropper, NULL);
    sthread_join(put);
    sthread_join(drop);
    sthread_join(take);
    CHECK(ring_torn == 0, "%ld of %ld blocks taken half overwritten", ring_torn, ring_taken);
    CHECK(ring_backwards == 0, "%ld blocks taken out of order", ring_backwards);
    CHECK(ring_taken > 0, "nothing taken");
    printf("audio3d_port: %d blocks put, %ld taken whole and in order, the rest dropped\n",
           RING_BLOCKS, ring_taken);
    audio3d_ring_free(&shared_ring);
}

static void test_objects(void) {
    audio3d_port_t port;
    unsigned ids[4], id;
    float one = 1.0f;
    int i;
    CHECK(audio3d_port_init(&port, 256, 2, 4, 2), "port not made");
    for (i = 0; i < 4; i++) {
        CHECK(audio3d_object_reserve(&port, &ids[i]), "object %d not reserved", i);
        CHECK(audio3d_object_get(&port, ids[i]) != NULL, "object %d not found", i);
    }
    CHECK(!audio3d_object_reserve(&port, &id), "a fifth object of four");
    /* Unreserved with sound queued: not free until it is mixed */
    CHECK(audio3d_ring_put_pcm(&audio3d_object_get(&port, ids[0])->pcm, &one, 1, 256, 1,
                               AUDIO3D_FLOAT) == 0,
          "sound not queued");
    CHECK(audio3d_object_unreserve(&port, ids[0]), "not unreserved");
    CHECK(!audio3d_object_unreserve(&port, ids[0]), "unreserved twice");
    CHECK(audio3d_object_get(&port, ids[0]) == NULL, "an unreserved object found");
    CHECK(audio3d_object_state(&port, ids[0]) == AUDIO3D_OBJECT_LEAVING, "not leaving");
    CHECK(audio3d_object_state(&port, 12345u) == -1, "an id of nothing has a state");
    CHECK(!audio3d_object_reserve(&port, &id), "an object reserved before its sound was mixed");
    CHECK(audio3d_port_has_input(&port), "its sound not seen");
    CHECK(audio3d_port_mix(&port), "not mixed");
    CHECK(audio3d_object_reserve(&port, &id), "not free once mixed");
    CHECK(id != ids[0], "the same id again");
    CHECK(audio3d_object_get(&port, ids[0]) == NULL, "an old id finds the new object");
    /* Empty: freed at once */
    CHECK(audio3d_object_unreserve(&port, ids[1]) && audio3d_object_reserve(&port, &id),
          "an empty object not freed at once");
    audio3d_port_free(&port);
}

static void test_mix(void) {
    audio3d_port_t port;
    audio3d_block_t info;
    const float* out;
    short bed[256 * 6];
    float mono[256];
    unsigned id;
    int i;
    for (i = 0; i < 256; i++) {
        bed[i * 6 + 0] = 16384;  /* 0.5 */
        bed[i * 6 + 1] = -16384; /* -0.5 */
        bed[i * 6 + 2] = 8192;   /* 0.25: centre */
        bed[i * 6 + 3] = 32767;  /* LFE: not in stereo */
        bed[i * 6 + 4] = 8192;
        bed[i * 6 + 5] = 0;
        mono[i] = 0.5f;
    }
    /* Stereo: the bed folded, the object at a gain of 0.5 on both sides */
    CHECK(audio3d_port_init(&port, 256, 2, 2, 2), "port not made");
    CHECK(audio3d_ring_put_pcm(&port.bed, bed, 256, 256, 6, AUDIO3D_S16) == 0, "bed not put");
    CHECK(audio3d_object_reserve(&port, &id), "no object");
    audio3d_object_set_gain(audio3d_object_get(&port, id), 0.5f);
    CHECK(audio3d_ring_put_pcm(&audio3d_object_get(&port, id)->pcm, mono, 256, 256, 1,
                               AUDIO3D_FLOAT) == 0,
          "object sound not put");
    CHECK(audio3d_port_mix(&port), "not mixed");
    CHECK(!audio3d_port_has_input(&port), "input left after the mix");
    out = (const float*)audio3d_ring_take_begin(&port.mixed, &info);
    CHECK(out && info.channels == 2 && info.frames == 256 && info.format == AUDIO3D_FLOAT,
          "no stereo mix");
    if (out) {
        CHECK(near(out[0], 0.5f + 0.7071f * 0.25f + 0.7071f * 0.25f + 0.25f) &&
                  near(out[1], -0.5f + 0.7071f * 0.25f + 0.25f) && near(out[510], out[0]),
              "stereo mix %f %f", out[0], out[1]);
        audio3d_ring_take_end(&port.mixed);
    }
    /* Nothing queued: silence; gain 0: dropped unheard */
    CHECK(audio3d_ring_put_pcm(&audio3d_object_get(&port, id)->pcm, mono, 256, 256, 1,
                               AUDIO3D_FLOAT) == 0,
          "object sound not put");
    audio3d_object_reset(audio3d_object_get(&port, id));
    CHECK(audio3d_ring_level(&audio3d_object_get(&port, id)->pcm) == 0, "reset kept sound");
    CHECK(audio3d_ring_put_pcm(&audio3d_object_get(&port, id)->pcm, mono, 256, 256, 1,
                               AUDIO3D_FLOAT) == 0,
          "object sound not put");
    CHECK(audio3d_port_mix(&port), "not mixed");
    out = (const float*)audio3d_ring_take_begin(&port.mixed, &info);
    CHECK(out && out[0] == 0.0f && out[1] == 0.0f, "a reset object was heard");
    if (out) {
        audio3d_ring_take_end(&port.mixed);
    }
    CHECK(audio3d_ring_level(&audio3d_object_get(&port, id)->pcm) == 0, "unheard sound kept");
    /* The mixed queue full: no mix */
    CHECK(audio3d_port_mix(&port) && audio3d_port_mix(&port) && !audio3d_port_mix(&port),
          "mixed past a full queue");
    audio3d_port_free(&port);

    /* 7.1: each channel in its place, mono to the front pair */
    CHECK(audio3d_port_init(&port, 256, 2, 2, 8), "port not made");
    CHECK(audio3d_ring_put_pcm(&port.bed, bed, 256, 256, 6, AUDIO3D_S16) == 0, "bed not put");
    CHECK(audio3d_object_reserve(&port, &id), "no object");
    audio3d_object_set_gain(audio3d_object_get(&port, id), 2.0f);
    CHECK(audio3d_ring_put_pcm(&audio3d_object_get(&port, id)->pcm, mono, 256, 256, 1,
                               AUDIO3D_FLOAT) == 0,
          "object sound not put");
    CHECK(audio3d_port_mix(&port), "not mixed");
    out = (const float*)audio3d_ring_take_begin(&port.mixed, &info);
    CHECK(out && info.channels == 8, "no 7.1 mix");
    if (out) {
        CHECK(near(out[0], 1.5f) && near(out[1], 0.5f) && near(out[2], 0.25f) &&
                  near(out[3], 32767 / 32768.0f) && near(out[4], 0.25f) && out[5] == 0.0f &&
                  out[6] == 0.0f && out[7] == 0.0f,
              "7.1 mix %f %f %f %f %f", out[0], out[1], out[2], out[3], out[4]);
        audio3d_ring_take_end(&port.mixed);
    }
    audio3d_port_free(&port);
}

/* Four threads reserve, hand over sound and unreserve while a fifth mixes and drains: an
 * object is never had by two threads at once */
#define OBJECTS 2
#define TURNS 20000
static audio3d_port_t shared_port;
static int owner[1024];
static retro_atomic_int_t objects_done;

static void object_user(void* arg) {
    const int me = (int)(size_t)arg;
    float sound[256];
    int turn;
    memset(sound, 0, sizeof(sound));
    for (turn = 0; turn < TURNS; turn++) {
        unsigned id;
        audio3d_object_t* object;
        if (!audio3d_object_reserve(&shared_port, &id)) {
            sthread_yield();
            continue;
        }
        if (owner[id % 1024] != 0) {
            failures++;
        }
        owner[id % 1024] = me;
        object = audio3d_object_get(&shared_port, id);
        if (!object) {
            failures++;
            continue;
        }
        audio3d_object_set_gain(object, 1.0f);
        (void)audio3d_ring_put_pcm(&object->pcm, sound, 256, 256, 1, AUDIO3D_FLOAT);
        if (owner[id % 1024] != me) {
            failures++;
        }
        owner[id % 1024] = 0;
        if (!audio3d_object_unreserve(&shared_port, id)) {
            failures++;
        }
    }
}

static void object_mixer(void* arg) {
    (void)arg;
    while (!retro_atomic_load_acquire_int(&objects_done)) {
        if (audio3d_port_mix(&shared_port)) {
            audio3d_block_t info;
            if (audio3d_ring_take_begin(&shared_port.mixed, &info)) {
                audio3d_ring_take_end(&shared_port.mixed);
            }
        } else {
            sthread_yield();
        }
    }
}

static void test_object_threads(void) {
    sthread_t* users[4];
    sthread_t* mixer;
    int i;
    CHECK(audio3d_port_init(&shared_port, 256, 2, OBJECTS, 2), "port not made");
    retro_atomic_int_init(&objects_done, 0);
    mixer = sthread_create(object_mixer, NULL);
    for (i = 0; i < 4; i++) {
        users[i] = sthread_create(object_user, (void*)(size_t)(i + 1));
    }
    for (i = 0; i < 4; i++) {
        sthread_join(users[i]);
    }
    retro_atomic_store_release_int(&objects_done, 1);
    sthread_join(mixer);
    /* All unreserved: free once their sound is mixed */
    while (audio3d_port_mix(&shared_port)) {
        if (audio3d_ring_take_begin(&shared_port.mixed, NULL)) {
            audio3d_ring_take_end(&shared_port.mixed);
        }
        if (!audio3d_port_has_input(&shared_port)) {
            break;
        }
    }
    for (i = 0; i < OBJECTS; i++) {
        CHECK(retro_atomic_load_acquire_int(&shared_port.objects[i].state) ==
                  AUDIO3D_OBJECT_FREE,
              "object %d not freed", i);
    }
    printf("audio3d_port: %d objects, 4 threads reserving %d times each, one mixing\n",
           OBJECTS, TURNS);
    audio3d_port_free(&shared_port);
}

int main(void) {
    test_ring();
    test_ring_threads();
    test_objects();
    test_mix();
    test_object_threads();
    if (failures) {
        printf("audio3d_port: %d FAILED\n", failures);
        return 1;
    }
    printf("audio3d_port: all passed\n");
    return 0;
}
