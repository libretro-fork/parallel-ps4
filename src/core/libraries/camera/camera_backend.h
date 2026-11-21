/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBRARIES_CAMERA_BACKEND_H
#define SHADPS4_LIBRARIES_CAMERA_BACKEND_H

/* Where the camera's frames come from. The core has none until the frontend's camera is passed
 * through: camera_get_backend() gives one that is never attached. */

#include <boolean.h>
#include <retro_common_api.h>

#include "common/types.h"

RETRO_BEGIN_DECLS

#define CAMERA_WIDTH 1280
#define CAMERA_HEIGHT 800

typedef struct camera_frame {
    const u8* yuy2;   /* borrowed: valid until the next acquire_frame or stop */
    u64 timestamp_us; /* when it was captured */
} camera_frame_t;

typedef struct camera_backend {
    /* A camera is there (sceCameraIsAttached). */
    bool (*is_attached)(void);
    /* The game starts the camera: frames at the size above, up to @fps a second. */
    bool (*start)(u32 fps);
    void (*stop)(void);
    /* The newest frame not handed over yet, or false when there is none since the last. */
    bool (*acquire_frame)(camera_frame_t* frame);
} camera_backend_t;

const camera_backend_t* camera_get_backend(void);

RETRO_END_DECLS

#endif
