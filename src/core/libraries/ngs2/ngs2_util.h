/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef NGS2_UTIL_H
#define NGS2_UTIL_H

/* What libSceNgs2 gives a game beside its sound engine: a waveform file read
 * for what a sampler needs to play it, a place among the speakers turned
 * into levels, and a sound's place in a world turned into those and a pitch.
 * Nothing here keeps state or allocates.
 *
 * The structures are the PS4's, as a game has them. */

#include <stddef.h>
#include <stdint.h>

#include "ngs2_core.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NGS2_WAVEFORM_INFO_BLOCKS 4

#define NGS2_ERROR_INVALID_OUT_ADDRESS ((int)0x804A0053)
#define NGS2_ERROR_INVALID_WAVEFORM_FORMAT ((int)0x804A0400)
#define NGS2_ERROR_UNKNOWN_WAVEFORM_FORMAT ((int)0x804A0401)
#define NGS2_ERROR_INVALID_WAVEFORM_DATA ((int)0x804A0408)
#define NGS2_ERROR_INVALID_PAN_UNIT_ANGLE ((int)0x804A0910)
#define NGS2_ERROR_INVALID_PAN_SPEAKER ((int)0x804A0911)
#define NGS2_ERROR_INVALID_PAN_MATRIX_FORMAT ((int)0x804A0912)
#define NGS2_ERROR_INVALID_PAN_WORK ((int)0x804A0913)
#define NGS2_ERROR_INVALID_PAN_PARAM ((int)0x804A0914)
#define NGS2_ERROR_INVALID_GEOM_LISTENER_ADDRESS ((int)0x804A0921)
#define NGS2_ERROR_INVALID_GEOM_SOURCE_ADDRESS ((int)0x804A0922)
#define NGS2_ERROR_INVALID_GEOM_FLAG ((int)0x804A0923)

typedef struct ngs2_waveform_info {
    ngs2_waveform_format format;
    uint32_t data_offset;
    uint32_t data_size;
    uint32_t loop_begin;
    uint32_t loop_end;
    uint32_t num_samples;
    uint32_t audio_unit_size;
    uint32_t audio_unit_samples;
    uint32_t audio_units_per_frame;
    uint32_t audio_frame_size;
    uint32_t audio_frame_samples;
    uint32_t delay_samples;
    uint32_t num_blocks;
    ngs2_waveform_block blocks[NGS2_WAVEFORM_INFO_BLOCKS];
} ngs2_waveform_info;

/* Reads @size bytes at @offset of a waveform into @dst: 0 when it did */
typedef int (*ngs2_read_fn)(void* user, uint64_t offset, void* dst, size_t size);

/* A RIFF WAVE (PCM, float or ATRAC9) or a VAGp of @size bytes (as many as
 * the reader may be asked for), read through @read. */
int ngs2_parse_waveform(ngs2_read_fn read, void* user, uint64_t size, ngs2_waveform_info* info);
int ngs2_parse_waveform_data(const void* data, size_t size, ngs2_waveform_info* info);
/* The block of a waveform that holds @count samples from @pos */
int ngs2_calc_waveform_block(const ngs2_waveform_format* format, uint32_t pos, uint32_t count,
                             ngs2_waveform_block* block);
int ngs2_waveform_frame_info(const ngs2_waveform_format* format, uint32_t* frame_size,
                             uint32_t* frame_samples, uint32_t* units_per_frame,
                             uint32_t* delay_samples);

typedef struct ngs2_pan_param {
    float angle;
    float distance;
    float fbw_level;
    float lfe_level;
} ngs2_pan_param;

typedef struct ngs2_pan_work {
    float speaker_angle[NGS2_MAX_CHANNELS];
    float unit_angle;
    uint32_t num_speakers;
} ngs2_pan_work;

int ngs2_pan_init(ngs2_pan_work* work, const float* speaker_angles, float unit_angle,
                  uint32_t num_speakers);
/* @matrix: for each of @matrix_format channels, the level of each of @count sounds */
int ngs2_pan_matrix(const ngs2_pan_work* work, const ngs2_pan_param* params, uint32_t count,
                    uint32_t matrix_format, float* matrix);

typedef struct ngs2_vector {
    float x, y, z;
} ngs2_vector;

typedef struct ngs2_geom_listener {
    ngs2_vector position;
    ngs2_vector front;
    ngs2_vector up;
    ngs2_vector velocity;
    float sound_speed;
    uint32_t reserved[2];
} ngs2_geom_listener;

typedef struct ngs2_geom_work {
    float matrix[4][4];
    ngs2_vector velocity;
    float sound_speed;
    uint32_t coordinate;
    uint32_t reserved[3];
} ngs2_geom_work;

typedef struct ngs2_geom_source {
    ngs2_vector position;
    ngs2_vector velocity;
    ngs2_vector direction;
    float cone_inner_level;
    float cone_inner_angle;
    float cone_outer_level;
    float cone_outer_angle;
    uint32_t rolloff_model;
    float max_distance;
    float rolloff_factor;
    float reference_distance;
    float doppler_factor;
    float fbw_level;
    float lfe_level;
    float max_level;
    float min_level;
    float radius;
    uint32_t num_speakers;
    uint32_t matrix_format;
    uint32_t reserved[2];
} ngs2_geom_source;

typedef struct ngs2_geom_attribute {
    float pitch_ratio;
    float levels[NGS2_MAX_LEVELS];
    ngs2_vector a3d_position;
    float a3d_volume;
    uint32_t a3d_reserved[4];
    uint32_t reserved[4];
} ngs2_geom_attribute;

int ngs2_geom_reset_listener(ngs2_geom_listener* listener);
int ngs2_geom_reset_source(ngs2_geom_source* source);
int ngs2_geom_calc_listener(const ngs2_geom_listener* listener, ngs2_geom_work* work,
                            uint32_t flags);
int ngs2_geom_apply(const ngs2_geom_work* work, const ngs2_geom_source* source,
                    ngs2_geom_attribute* attribute, uint32_t flags);

#ifdef __cplusplus
}
#endif

#endif
