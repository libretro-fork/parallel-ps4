/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_AUDIO_AUDIOIN_BACKEND_H
#define SHADPS4_AUDIO_AUDIOIN_BACKEND_H

/* Where a game's audio input comes from. The core has no microphone yet (silent_audio_in.c):
 * the frontend's microphone (libretro's RETRO_ENVIRONMENT_GET_MICROPHONE_INTERFACE) is the one
 * meant to plug in here, its samples passed over from the core's process. A port reads at its
 * own rate, format and length (PortIn). */

#include <boolean.h>
#include <retro_common_api.h>
#include <stddef.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

struct PortIn;
struct audioin_port_backend;

typedef struct audioin_port_backend_ops {
    /* Fills one buffer of the port, waiting for it the way a microphone paces it. Returns the
     * frames read. */
    int (*read)(struct audioin_port_backend* backend, void* out_buffer);
    /* Drops what is captured and not read yet. */
    void (*clear)(struct audioin_port_backend* backend);
    /* A microphone is there (sceAudioInGetSilentState). */
    bool (*is_available)(struct audioin_port_backend* backend);
} audioin_port_backend_ops_t;

/* A port's backend, kept in the port: nothing is allocated */
typedef struct audioin_port_backend {
    const audioin_port_backend_ops_t* ops;
    /* The silent backend's: a buffer's size and frames, and on the game's time (ns) a buffer's
     * length and when the next is full */
    size_t bytes;
    u32 samples;
    u64 period;
    u64 next;
} audioin_port_backend_t;

/* No microphone in the core: silence, at a microphone's pace (silent_audio_in.c). Opens
 * @backend for @port: false when it cannot. */
bool silent_audio_in_open(audioin_port_backend_t* backend, const struct PortIn* port);

RETRO_END_DECLS

#endif
