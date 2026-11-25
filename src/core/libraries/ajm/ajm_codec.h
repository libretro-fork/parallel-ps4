/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_AJM_AJM_CODEC_H
#define SHADPS4_AJM_AJM_CODEC_H

/* What the codecs share: the input and output of a decode, and the gapless counts. Defined in
 * ajm_instance.c. */

#include <stddef.h>

#include <boolean.h>
#include <retro_common_api.h>

#include "common/types.h"
#include "core/libraries/ajm/ajm.h"

RETRO_BEGIN_DECLS

/* A piece of the game's memory */
typedef struct ajm_span {
    u8* data;
    size_t size;
} ajm_span_t;

/* The game's output buffers, written in order: each chunk is used from its start, and the
 * write goes on in the next as one fills */
typedef struct ajm_output {
    ajm_span_t* chunks;
    size_t count;
    size_t current;
} ajm_output_t;

typedef struct ajm_gapless {
    AjmSidebandGaplessDecode init;
    AjmSidebandGaplessDecode current;
} ajm_gapless_t;

bool ajm_gapless_is_end(const ajm_gapless_t* g);
void ajm_gapless_reset(ajm_gapless_t* g);

typedef struct ajm_decoder_result {
    s32 result;
    s32 internal_result;
    u32 frames_decoded;
    u32 samples_written;
    bool is_reset;
} ajm_decoder_result_t;

/* The input left to decode: advanced as the codec takes it */
typedef struct ajm_input {
    const u8* data;
    size_t size;
} ajm_input_t;

void ajm_input_advance(ajm_input_t* in, size_t n);

/* The bytes of a sample of @format */
u8 ajm_pcm_size(u32 format);

/* The bytes left in @out */
size_t ajm_output_size(const ajm_output_t* out);
bool ajm_output_is_empty(const ajm_output_t* out);
/* Writes @count samples of @sample_bytes each: the samples written */
size_t ajm_output_write(ajm_output_t* out, const void* pcm, size_t count, size_t sample_bytes);
/* Writes @count frames of @channels samples of @sample_bytes each straight into the output
 * memory: @fill(@ctx, first, n, dst) produces frames [first, first + n) at dst. Only a frame
 * that straddles two chunks, or a chunk the samples would sit unaligned in, goes through a
 * frame-sized buffer. Returns the samples written. */
size_t ajm_output_write_frames(ajm_output_t* out, size_t count, u32 channels, size_t sample_bytes,
                               void (*fill)(void* ctx, size_t first, size_t n, void* dst),
                               void* ctx);

RETRO_END_DECLS

#endif
