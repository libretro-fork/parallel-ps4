/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <boolean.h>
#include <retro_inline.h>

#include "core/libraries/camera/camera_helpers.h"

static INLINE u8 clamp_u8(int v) {
    return (u8)(v < 0 ? 0 : (v > 255 ? 255 : v));
}

void camera_yuy2_to_y8(const u8* src, u8* dst, int width, int height) {
    int x, y;
    for (y = 0; y < height; ++y) {
        const u8* row = src + (u64)y * width * 2;
        u8* out_row = dst + (u64)y * width;
        for (x = 0; x < width; x += 2) {
            out_row[x] = row[x * 2 + 0];     /* Y0 */
            out_row[x + 1] = row[x * 2 + 2]; /* Y1 */
        }
    }
}

void camera_yuy2_to_y16(const u8* src, u16* dst, int width, int height) {
    int x, y;
    for (y = 0; y < height; ++y) {
        const u8* row = src + (u64)y * width * 2;
        u16* out_row = dst + (u64)y * width;
        for (x = 0; x < width; x += 2) {
            out_row[x] = (u16)(row[x * 2 + 0] << 2);
            out_row[x + 1] = (u16)(row[x * 2 + 2] << 2);
        }
    }
}

static INLINE void yuv_to_rgb(u8 y, u8 u, u8 v, u8* r, u8* g, u8* b) {
    const int c = (int)y - 16;
    const int d = (int)u - 128;
    const int e = (int)v - 128;

    *r = clamp_u8((298 * c + 409 * e + 128) >> 8);
    *g = clamp_u8((298 * c - 100 * d - 208 * e + 128) >> 8);
    *b = clamp_u8((298 * c + 516 * d + 128) >> 8);
}

void camera_yuy2_to_raw8(const u8* src, u8* dst, int width, int height) {
    int x, y;
    for (y = 0; y < height; ++y) {
        const u8* row = src + (u64)y * width * 2;
        u8* out_row = dst + (u64)y * width;
        const bool even_row = (y & 1) == 0;

        for (x = 0; x < width; x += 2) {
            u8 r0, g0, b0;
            u8 r1, g1, b1;
            yuv_to_rgb(row[x * 2 + 0], row[x * 2 + 1], row[x * 2 + 3], &r0, &g0, &b0);
            yuv_to_rgb(row[x * 2 + 2], row[x * 2 + 1], row[x * 2 + 3], &r1, &g1, &b1);

            if (even_row) {
                out_row[x] = b0;
                out_row[x + 1] = g1;
            } else {
                out_row[x] = g0;
                out_row[x + 1] = r1;
            }
        }
    }
}

void camera_yuy2_to_raw16(const u8* src, u16* dst, int width, int height) {
    int x, y;
    for (y = 0; y < height; ++y) {
        const u8* row = src + (u64)y * width * 2;
        u16* out_row = dst + (u64)y * width;
        const bool even_row = (y & 1) == 0;

        for (x = 0; x < width; x += 2) {
            u8 r0, g0, b0;
            u8 r1, g1, b1;
            yuv_to_rgb(row[x * 2 + 0], row[x * 2 + 1], row[x * 2 + 3], &r0, &g0, &b0);
            yuv_to_rgb(row[x * 2 + 2], row[x * 2 + 1], row[x * 2 + 3], &r1, &g1, &b1);

            if (even_row) {
                out_row[x] = (u16)(b0 << 4);
                out_row[x + 1] = (u16)(g1 << 4);
            } else {
                out_row[x] = (u16)(g0 << 4);
                out_row[x + 1] = (u16)(r1 << 4);
            }
        }
    }
}

s32 camera_size_of_base_format(OrbisCameraBaseFormat f) {
    switch (f) {
    case ORBIS_CAMERA_FORMAT_YUV422:
        return 2;
    case ORBIS_CAMERA_FORMAT_RAW16:
        return 2;
    case ORBIS_CAMERA_FORMAT_RAW8:
        return 1;
    case ORBIS_CAMERA_FORMAT_NO_USE:
    default:
        return 0;
    }
}

s32 camera_size_of_scale_format(OrbisCameraScaleFormat f) {
    switch (f) {
    case ORBIS_CAMERA_SCALE_FORMAT_YUV422:
        return 2;
    case ORBIS_CAMERA_SCALE_FORMAT_Y16:
        return 2;
    case ORBIS_CAMERA_SCALE_FORMAT_Y8:
        return 1;
    case ORBIS_CAMERA_SCALE_FORMAT_NO_USE:
    default:
        return 0;
    }
}
