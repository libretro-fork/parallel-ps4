/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "audio_pace.h"

uint32_t audio_pace_take(audio_pace_t* pace, uint32_t queued, uint32_t frame, uint32_t* skip) {
    *skip = 0;
    if (!pace->primed) {
        if (queued < 2 * frame)
            return 0;
        pace->primed = 1;
    }
    if (queued > 3 * frame) {
        *skip = queued - 2 * frame;
        queued = 2 * frame;
    }
    if (queued < frame) {
        pace->primed = 0; /* ran dry: two in hand again before more */
        return queued;
    }
    return frame;
}
