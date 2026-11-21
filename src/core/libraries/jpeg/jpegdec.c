/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <boolean.h>
#include <formats/rjpeg.h>

#include "common/c_bridge.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/jpeg/jpeg_error.h"
#include "core/libraries/jpeg/jpegdec.h"

/* In the game's memory after the handle: the decoder, made at create and kept for every
 * decode until delete, so that once an image of a size is decoded, decoding another no bigger
 * takes no memory */
typedef struct jpeg_dec_state {
    rjpeg_t* rjpeg;
} jpeg_dec_state;

static jpeg_dec_state* dec_state(OrbisJpegDecHandle handle) {
    return (jpeg_dec_state*)((u8*)handle + sizeof(OrbisJpegDecHandleInternal));
}

static bool valid_handle(OrbisJpegDecHandle handle) {
    return handle && ((uintptr_t)handle & 0x1F) == 0 && handle->handle == handle;
}

static bool valid_ratio(u32 ratio) {
    return ratio == 1 || ratio == 2 || ratio == 4 || ratio == 8;
}

/* The frame header (SOF) of a JPEG stream: its size, components and their sampling */
typedef struct jpeg_header {
    u32 width;
    u32 height;
    u16 num_components;
    u8 component_id[4];
    u8 sampling_factor[4];
} jpeg_header;

static void read_header(const u8* ptr, u32 size, jpeg_header* h) {
    u32 pos = 2;
    h->width = 0;
    h->height = 0;
    h->num_components = 3;
    h->component_id[0] = 1;
    h->component_id[1] = 2;
    h->component_id[2] = 3;
    h->component_id[3] = 0;
    h->sampling_factor[0] = 0x22;
    h->sampling_factor[1] = 0x11;
    h->sampling_factor[2] = 0x11;
    h->sampling_factor[3] = 0;
    while (pos + 4 < size) {
        u8 marker;
        u16 len;
        if (ptr[pos] != 0xFF) {
            pos++;
            continue;
        }
        marker = ptr[pos + 1];
        if (marker == 0xD9 || marker == 0xDA) {
            break;
        }
        len = (u16)((ptr[pos + 2] << 8) | ptr[pos + 3]);
        if (marker == 0xC0 || marker == 0xC1 || marker == 0xC2) {
            if (pos + 9 < size) {
                u16 c;
                h->height = ((u32)ptr[pos + 5] << 8) | ptr[pos + 6];
                h->width = ((u32)ptr[pos + 7] << 8) | ptr[pos + 8];
                h->num_components = ptr[pos + 9];
                for (c = 0; c < h->num_components && c < 4; ++c) {
                    if (pos + 10 + c * 3 + 1 < size) {
                        h->component_id[c] = ptr[pos + 10 + c * 3];
                        h->sampling_factor[c] = ptr[pos + 10 + c * 3 + 1];
                    }
                }
            }
            break;
        }
        pos += 2 + len;
    }
}

s32 PS4_SYSV_ABI sceJpegDecParseHeader(const OrbisJpegDecParseParam* param,
                                       OrbisJpegDecImageInfo* image_info) {
    const u8* ptr;
    jpeg_header h;
    u32 ratio;
    u16 color_space = ORBIS_JPEG_DEC_COLOR_SPACE_UNKNOWN;
    u32 required_decode_mode = 0;

    c_log(C_LOG_LIB_JPEG, C_LOG_LEVEL_TRACE, "sceJpegDecParseHeader",
          "called param = %p, image_info = %p", (const void*)param, (void*)image_info);
    if (!param || !param->jpeg_addr) {
        c_log(C_LOG_LIB_JPEG, C_LOG_LEVEL_ERROR, "sceJpegDecParseHeader", "Invalid address");
        return ORBIS_JPEG_DEC_ERROR_INVALID_ADDR;
    }
    if (param->jpeg_size == 0) {
        c_log(C_LOG_LIB_JPEG, C_LOG_LEVEL_ERROR, "sceJpegDecParseHeader", "Invalid size");
        return ORBIS_JPEG_DEC_ERROR_INVALID_SIZE;
    }
    if (param->decode_mode > 1) {
        c_log(C_LOG_LIB_JPEG, C_LOG_LEVEL_ERROR, "sceJpegDecParseHeader", "Invalid decode mode: %u",
              (unsigned)param->decode_mode);
        return ORBIS_JPEG_DEC_ERROR_INVALID_PARAM;
    }
    ratio = param->downscale_ratio;
    if (!valid_ratio(ratio)) {
        c_log(C_LOG_LIB_JPEG, C_LOG_LEVEL_ERROR, "sceJpegDecParseHeader",
              "Invalid downscale ratio: %u", ratio);
        return ORBIS_JPEG_DEC_ERROR_INVALID_PARAM;
    }
    if (!image_info) {
        c_log(C_LOG_LIB_JPEG, C_LOG_LEVEL_ERROR, "sceJpegDecParseHeader",
              "Invalid image_info pointer");
        return ORBIS_JPEG_DEC_ERROR_INVALID_ADDR;
    }

    ptr = (const u8*)param->jpeg_addr;
    if (param->jpeg_size < 4 || ptr[0] != 0xFF || ptr[1] != 0xD8) {
        c_log(C_LOG_LIB_JPEG, C_LOG_LEVEL_ERROR, "sceJpegDecParseHeader",
              "Invalid JPEG signature or stream too short");
        return ORBIS_JPEG_DEC_ERROR_INVALID_DATA;
    }
    read_header(ptr, param->jpeg_size, &h);
    if (h.width == 0 || h.height == 0) {
        c_log(C_LOG_LIB_JPEG, C_LOG_LEVEL_ERROR, "sceJpegDecParseHeader",
              "Failed to parse SOF marker in JPEG stream");
        return ORBIS_JPEG_DEC_ERROR_DECODE_ERROR;
    }

    if (h.num_components == 1) {
        color_space = ORBIS_JPEG_DEC_COLOR_SPACE_GRAYSCALE;
        required_decode_mode = 1;
    } else if (h.num_components == 3) {
        u8 max_h = 1;
        u32 scale;
        u32 total_density = 0;
        u16 c;
        color_space = ORBIS_JPEG_DEC_COLOR_SPACE_YCC;
        for (c = 0; c < h.num_components && c < 4; ++c) {
            if ((h.sampling_factor[c] >> 4) > max_h) {
                max_h = (u8)(h.sampling_factor[c] >> 4);
            }
        }
        scale = 96 / (max_h * 8);
        for (c = 0; c < 3; ++c) {
            total_density += (u32)(h.sampling_factor[c] >> 4) * (h.sampling_factor[c] & 0xF);
        }
        required_decode_mode = (total_density * scale < 0x25) ? 1 : 2;
    }

    memset(image_info, 0, sizeof(*image_info));
    image_info->image_width = h.width;
    image_info->image_height = h.height;
    image_info->color_space = color_space;
    image_info->num_components = h.num_components;
    memcpy(image_info->component_id, h.component_id, sizeof(h.component_id));
    memcpy(image_info->sampling_factor, h.sampling_factor, sizeof(h.sampling_factor));
    image_info->decode_mode = param->decode_mode;
    image_info->required_decode_mode = required_decode_mode;
    image_info->output_width = (h.width + ratio - 1) / ratio;
    image_info->output_height = (h.height + ratio - 1) / ratio;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceJpegDecQueryMemorySize(const OrbisJpegDecCreateParam* param) {
    u32 aligned_w;
    u32 sz;
    c_log(C_LOG_LIB_JPEG, C_LOG_LEVEL_TRACE, "sceJpegDecQueryMemorySize", "called param = %p",
          (const void*)param);
    if (!param) {
        c_log(C_LOG_LIB_JPEG, C_LOG_LEVEL_ERROR, "sceJpegDecQueryMemorySize",
              "Invalid param pointer");
        return ORBIS_JPEG_DEC_ERROR_INVALID_ADDR;
    }
    if (param->size != sizeof(OrbisJpegDecCreateParam)) {
        c_log(C_LOG_LIB_JPEG, C_LOG_LEVEL_ERROR, "sceJpegDecQueryMemorySize",
              "Invalid param size: %u", param->size);
        return ORBIS_JPEG_DEC_ERROR_INVALID_SIZE;
    }
    if (param->decode_mode < 1 || param->decode_mode > 2) {
        c_log(C_LOG_LIB_JPEG, C_LOG_LEVEL_ERROR, "sceJpegDecQueryMemorySize",
              "Invalid decode mode: %u", param->decode_mode);
        return ORBIS_JPEG_DEC_ERROR_INVALID_PARAM;
    }
    if (param->max_width == 0 || param->max_width > 0xFFFF) {
        c_log(C_LOG_LIB_JPEG, C_LOG_LEVEL_ERROR, "sceJpegDecQueryMemorySize",
              "Invalid max width: %u", param->max_width);
        return ORBIS_JPEG_DEC_ERROR_INVALID_PARAM;
    }
    /* The retail library's formula */
    aligned_w = ((param->max_width + 0x1f) & ~0x1fu) | 0x10;
    sz = param->decode_mode == 2 ? (aligned_w << 5) * 5 : (aligned_w << 4) * 3;
    return (s32)(sz + aligned_w * 6 + 0x7360);
}

s32 PS4_SYSV_ABI sceJpegDecCreate(const OrbisJpegDecCreateParam* param, void* memory,
                                  u32 memory_size, OrbisJpegDecHandle* handle) {
    s32 req_size;
    OrbisJpegDecHandleInternal* dec;
    jpeg_dec_state* state;
    c_log(C_LOG_LIB_JPEG, C_LOG_LEVEL_TRACE, "sceJpegDecCreate",
          "called param = %p, memory = %p, memory_size = %u", (const void*)param, memory,
          memory_size);
    if (!param || !memory) {
        c_log(C_LOG_LIB_JPEG, C_LOG_LEVEL_ERROR, "sceJpegDecCreate",
              "Invalid address param = %p, memory = %p", (const void*)param, memory);
        return ORBIS_JPEG_DEC_ERROR_INVALID_ADDR;
    }
    req_size = sceJpegDecQueryMemorySize(param);
    if (req_size < 0) {
        return req_size;
    }
    if ((u32)req_size > memory_size) {
        c_log(C_LOG_LIB_JPEG, C_LOG_LEVEL_ERROR, "sceJpegDecCreate",
              "Memory size too small: %u < %d", memory_size, req_size);
        return ORBIS_JPEG_DEC_ERROR_INVALID_SIZE;
    }
    if (!handle) {
        c_log(C_LOG_LIB_JPEG, C_LOG_LEVEL_ERROR, "sceJpegDecCreate", "Invalid handle pointer");
        return ORBIS_JPEG_DEC_ERROR_INVALID_ADDR;
    }
    dec = (OrbisJpegDecHandleInternal*)(((uintptr_t)memory + 0x1F) & ~(uintptr_t)0x1F);
    state = dec_state(dec);
    state->rjpeg = rjpeg_alloc();
    if (!state->rjpeg) {
        c_log(C_LOG_LIB_JPEG, C_LOG_LEVEL_ERROR, "sceJpegDecCreate", "No memory for a decoder");
        return ORBIS_JPEG_DEC_ERROR_FATAL;
    }
    dec->handle = dec;
    dec->handle_size = sizeof(OrbisJpegDecCreateParam);
    dec->decode_mode = param->decode_mode;
    dec->max_width = (param->max_width + 0x1f) & ~0x1fu;
    dec->stride_or_aux = 0;
    *handle = dec;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceJpegDecDelete(OrbisJpegDecHandle handle) {
    c_log(C_LOG_LIB_JPEG, C_LOG_LEVEL_TRACE, "sceJpegDecDelete", "called handle = %p",
          (void*)handle);
    if (!valid_handle(handle)) {
        c_log(C_LOG_LIB_JPEG, C_LOG_LEVEL_ERROR, "sceJpegDecDelete", "Invalid handle: %p",
              (void*)handle);
        return ORBIS_JPEG_DEC_ERROR_INVALID_HANDLE;
    }
    rjpeg_free(dec_state(handle)->rjpeg);
    dec_state(handle)->rjpeg = NULL;
    handle->handle = NULL;
    return ORBIS_OK;
}

/* A decode: where its rows go in the game's buffer */
typedef struct jpeg_dec_out {
    const OrbisJpegDecDecodeParam* param;
    u32 max_width;
    u32 ratio;
    u32 bpp;
    u32 pitch;
    u32 out_w;
    u32 out_h;
    u8* base;
    bool direct; /* rows decoded straight into the game's buffer */
    s32 error;   /* why the size was refused */
} jpeg_dec_out;

/* The image's size, before any row: the game's buffer checked against it */
static bool out_size(void* user, unsigned w, unsigned h) {
    jpeg_dec_out* o = (jpeg_dec_out*)user;
    const OrbisJpegDecDecodeParam* p = o->param;
    u32 min_pitch;
    if (w > o->max_width) {
        c_log(C_LOG_LIB_JPEG, C_LOG_LEVEL_ERROR, "sceJpegDecDecode",
              "Image width exceeds handle max_width (%u > %u)", w, o->max_width);
        o->error = ORBIS_JPEG_DEC_ERROR_EXCEED_MAX_WIDTH;
        return false;
    }
    o->out_w = (w + o->ratio - 1) / o->ratio;
    o->out_h = (h + o->ratio - 1) / o->ratio;
    min_pitch = o->out_w * o->bpp;
    if (p->image_pitch != 0) {
        if (o->bpp == 4 && (p->image_pitch & 3) != 0) {
            c_log(C_LOG_LIB_JPEG, C_LOG_LEVEL_ERROR, "sceJpegDecDecode",
                  "Misaligned image_pitch: %u", p->image_pitch);
            o->error = ORBIS_JPEG_DEC_ERROR_INVALID_PARAM;
            return false;
        }
        if (p->image_pitch < min_pitch) {
            c_log(C_LOG_LIB_JPEG, C_LOG_LEVEL_ERROR, "sceJpegDecDecode",
                  "image_pitch too small: %u < %u", p->image_pitch, min_pitch);
            o->error = ORBIS_JPEG_DEC_ERROR_INVALID_PARAM;
            return false;
        }
        if ((u64)p->image_pitch * o->out_h > p->image_size) {
            c_log(C_LOG_LIB_JPEG, C_LOG_LEVEL_ERROR, "sceJpegDecDecode",
                  "Buffer size too small for pitch (size=%u, required=%llu)", p->image_size,
                  (unsigned long long)p->image_pitch * o->out_h);
            o->error = ORBIS_JPEG_DEC_ERROR_INVALID_PARAM;
            return false;
        }
    } else if ((u64)min_pitch * o->out_h > p->image_size) {
        c_log(C_LOG_LIB_JPEG, C_LOG_LEVEL_ERROR, "sceJpegDecDecode",
              "Buffer size too small (size=%u, required=%llu)", p->image_size,
              (unsigned long long)min_pitch * o->out_h);
        o->error = ORBIS_JPEG_DEC_ERROR_INVALID_SIZE;
        return false;
    }
    o->pitch = p->image_pitch != 0 ? p->image_pitch : min_pitch;
    /* RGBA or BGRA at the image's own size goes straight into the game's rows, where they are
     * 4-byte aligned; anything else is made from rjpeg's row as each comes */
    o->direct = o->ratio == 1 && o->bpp == 4 && ((uintptr_t)o->base & 3) == 0 &&
                (o->pitch & 3) == 0 &&
                (p->pixel_format == ORBIS_JPEG_DEC_PIXEL_FORMAT_R8G8B8A8 ||
                 p->pixel_format == ORBIS_JPEG_DEC_PIXEL_FORMAT_B8G8R8A8);
    return true;
}

static uint8_t* out_row(void* user, unsigned y) {
    jpeg_dec_out* o = (jpeg_dec_out*)user;
    return o->direct ? o->base + (size_t)y * o->pitch : NULL;
}

static u8 clamp_u8(s32 v) {
    return (u8)(v < 0 ? 0 : v > 255 ? 255 : v);
}

/* Row @y written, at @px: the alpha value set, or the game's row made from it */
static void out_done(void* user, unsigned y, const uint8_t* px) {
    jpeg_dec_out* o = (jpeg_dec_out*)user;
    const OrbisJpegDecDecodeParam* p = o->param;
    u8* dst;
    u32 x;
    if (o->direct) {
        if (p->alpha_value != 0) {
            dst = o->base + (size_t)y * o->pitch;
            for (x = 0; x < o->out_w; x++) {
                dst[x * 4 + 3] = (u8)p->alpha_value;
            }
        }
        return;
    }
    /* Downscaled, every ratio-th row and pixel is taken. px is BGRA for a BGRA image, RGBA
     * for every other */
    if (y % o->ratio != 0 || y / o->ratio >= o->out_h) {
        return;
    }
    dst = o->base + (size_t)(y / o->ratio) * o->pitch;
    switch (p->pixel_format) {
    case ORBIS_JPEG_DEC_PIXEL_FORMAT_R8G8B8A8:
    case ORBIS_JPEG_DEC_PIXEL_FORMAT_B8G8R8A8:
        /* rjpeg's rows already in the game's byte order */
        for (x = 0; x < o->out_w; x++) {
            const u8* s = px + (size_t)x * o->ratio * 4;
            dst[x * 4 + 0] = s[0];
            dst[x * 4 + 1] = s[1];
            dst[x * 4 + 2] = s[2];
            dst[x * 4 + 3] = p->alpha_value != 0 ? (u8)p->alpha_value : s[3];
        }
        break;
    case ORBIS_JPEG_DEC_PIXEL_FORMAT_Y8:
        for (x = 0; x < o->out_w; x++) {
            const u8* s = px + (size_t)x * o->ratio * 4;
            dst[x] = (u8)((s[0] * 77 + s[1] * 150 + s[2] * 29) >> 8);
        }
        break;
    case ORBIS_JPEG_DEC_PIXEL_FORMAT_Y8U8Y8V8:
        /* Two pixels to four bytes, Y0 U Y1 V, the pair's chroma averaged: JFIF's full-range
         * BT.601 in 16 fractional bits */
        for (x = 0; x < o->out_w; x += 2) {
            const u8* a = px + (size_t)x * o->ratio * 4;
            const u8* b = x + 1 < o->out_w ? a + (size_t)o->ratio * 4 : a;
            const s32 r = a[0] + b[0], g = a[1] + b[1], bl = a[2] + b[2];
            dst[x * 2 + 0] = clamp_u8((19595 * a[0] + 38470 * a[1] + 7471 * a[2] + 32768) >> 16);
            dst[x * 2 + 1] =
                clamp_u8((-11059 * r - 21709 * g + 32768 * bl + (128 << 17) + 65536) >> 17);
            if (x + 1 < o->out_w) {
                dst[x * 2 + 2] =
                    clamp_u8((19595 * b[0] + 38470 * b[1] + 7471 * b[2] + 32768) >> 16);
            }
            dst[x * 2 + 3] =
                clamp_u8((32768 * r - 27439 * g - 5329 * bl + (128 << 17) + 65536) >> 17);
        }
        break;
    default:
        break;
    }
}

s32 PS4_SYSV_ABI sceJpegDecDecode(OrbisJpegDecHandle handle, const OrbisJpegDecDecodeParam* param,
                                  OrbisJpegDecImageInfo* image_info) {
    jpeg_dec_out o;
    struct rjpeg_rows rows;
    jpeg_header h;
    unsigned w = 0, hgt = 0;
    u32 ratio;
    u16 fmt;

    c_log(C_LOG_LIB_JPEG, C_LOG_LEVEL_TRACE, "sceJpegDecDecode",
          "called handle = %p, param = %p, image_info = %p", (void*)handle, (const void*)param,
          (void*)image_info);
    if (!valid_handle(handle)) {
        c_log(C_LOG_LIB_JPEG, C_LOG_LEVEL_ERROR, "sceJpegDecDecode", "Invalid handle: %p",
              (void*)handle);
        return ORBIS_JPEG_DEC_ERROR_INVALID_HANDLE;
    }
    if (!param || !param->jpeg_addr || !param->image_addr) {
        c_log(C_LOG_LIB_JPEG, C_LOG_LEVEL_ERROR, "sceJpegDecDecode", "Invalid address");
        return ORBIS_JPEG_DEC_ERROR_INVALID_ADDR;
    }
    if (param->jpeg_size == 0 || param->image_size == 0) {
        c_log(C_LOG_LIB_JPEG, C_LOG_LEVEL_ERROR, "sceJpegDecDecode",
              "Invalid buffer size (jpeg_size=%u, image_size=%u)", param->jpeg_size,
              param->image_size);
        return ORBIS_JPEG_DEC_ERROR_INVALID_SIZE;
    }
    if (param->decode_mode > 1) {
        c_log(C_LOG_LIB_JPEG, C_LOG_LEVEL_ERROR, "sceJpegDecDecode", "Invalid decode mode: %u",
              (unsigned)param->decode_mode);
        return ORBIS_JPEG_DEC_ERROR_INVALID_PARAM;
    }
    ratio = param->downscale_ratio;
    if (!valid_ratio(ratio)) {
        c_log(C_LOG_LIB_JPEG, C_LOG_LEVEL_ERROR, "sceJpegDecDecode", "Invalid downscale ratio: %u",
              ratio);
        return ORBIS_JPEG_DEC_ERROR_INVALID_PARAM;
    }
    fmt = param->pixel_format;
    if (fmt > 11 || (0xe03 & (1u << fmt)) == 0) {
        c_log(C_LOG_LIB_JPEG, C_LOG_LEVEL_ERROR, "sceJpegDecDecode", "Invalid pixel format: %u",
              (unsigned)fmt);
        return ORBIS_JPEG_DEC_ERROR_INVALID_PARAM;
    }
    if (param->alpha_value > 0xff) {
        c_log(C_LOG_LIB_JPEG, C_LOG_LEVEL_ERROR, "sceJpegDecDecode", "Invalid alpha value: %u",
              (unsigned)param->alpha_value);
        return ORBIS_JPEG_DEC_ERROR_INVALID_PARAM;
    }

    memset(&o, 0, sizeof(o));
    o.param = param;
    o.max_width = handle->max_width;
    o.ratio = ratio;
    o.bpp = fmt == ORBIS_JPEG_DEC_PIXEL_FORMAT_Y8         ? 1
            : fmt == ORBIS_JPEG_DEC_PIXEL_FORMAT_Y8U8Y8V8 ? 2
                                                          : 4;
    o.base = (u8*)param->image_addr;
    rows.size = out_size;
    rows.row = out_row;
    rows.done = out_done;
    rows.user = &o;

    /* RGBA comes as RGBA; BGRA, decoded straight into the game's rows, as BGRA */
    if (!rjpeg_decode_rows(dec_state(handle)->rjpeg, (const uint8_t*)param->jpeg_addr,
                           param->jpeg_size, fmt != ORBIS_JPEG_DEC_PIXEL_FORMAT_B8G8R8A8, &rows, &w,
                           &hgt)) {
        if (o.error) {
            return o.error;
        }
        c_log(C_LOG_LIB_JPEG, C_LOG_LEVEL_ERROR, "sceJpegDecDecode", "JPEG decode failed");
        return ORBIS_JPEG_DEC_ERROR_DECODE_ERROR;
    }

    if (image_info) {
        read_header((const u8*)param->jpeg_addr, param->jpeg_size, &h);
        memset(image_info, 0, sizeof(*image_info));
        image_info->image_width = w;
        image_info->image_height = hgt;
        image_info->color_space = h.num_components == 1 ? ORBIS_JPEG_DEC_COLOR_SPACE_GRAYSCALE
                                                        : ORBIS_JPEG_DEC_COLOR_SPACE_YCC;
        image_info->num_components = h.num_components;
        image_info->component_id[0] = 1;
        image_info->component_id[1] = 2;
        image_info->component_id[2] = 3;
        image_info->sampling_factor[0] = 0x22;
        image_info->sampling_factor[1] = 0x11;
        image_info->sampling_factor[2] = 0x11;
        image_info->decode_mode = param->decode_mode;
        image_info->required_decode_mode = 1;
        image_info->output_width = o.out_w;
        image_info->output_height = o.out_h;
    }
    return (s32)((o.out_w << 16) | (o.out_h & 0xFFFF));
}

s32 PS4_SYSV_ABI sceJpegDecDecodeWithInputControl(OrbisJpegDecHandle handle,
                                                  const OrbisJpegDecDecodeParam* param,
                                                  OrbisJpegDecImageInfo* image_info) {
    return sceJpegDecDecode(handle, param, image_info);
}
