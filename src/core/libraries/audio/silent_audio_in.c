/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The core has no microphone: a port reports none attached and reads silence, a buffer at a
 * time, at the pace a microphone would fill it, so a game reading in a loop is not spun. The
 * pace is the game's time, which the frontend's frames move: not the host's clock. */

#include <string.h>

#include "common/c_bridge.h"
#include "core/libraries/audio/audioin.h"
#include "core/libraries/audio/audioin_backend.h"
#include "core/libraries/kernel/kernel_c.h"

static int silent_read(audioin_port_backend_t* backend, void* out_buffer) {
    u64 now;
    backend->next += backend->period;
    now = kernel_game_now();
    if (backend->next > now) {
        kernel_game_sleep_until(backend->next);
    } else {
        backend->next = now; /* fell behind: no burst to catch up */
    }
    memset(out_buffer, 0, backend->bytes);
    return (int)backend->samples;
}

static void silent_clear(audioin_port_backend_t* backend) {
    (void)backend;
}

static bool silent_is_available(audioin_port_backend_t* backend) {
    (void)backend;
    return false;
}

static const audioin_port_backend_ops_t silent_ops = {silent_read, silent_clear,
                                                      silent_is_available};

bool silent_audio_in_open(audioin_port_backend_t* backend, const struct PortIn* port) {
    backend->ops = &silent_ops;
    backend->bytes = (size_t)port->samples_num * port->sample_size * port->channels_num;
    backend->samples = port->samples_num;
    backend->period = port->freq ? (u64)port->samples_num * 1000000000ull / port->freq : 0;
    backend->next = kernel_game_now();
    c_log(C_LOG_LIB_AUDIO_IN, C_LOG_LEVEL_INFO, "SilentPortBackend",
          "No microphone: audio input port reads silence");
    return true;
}
