/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_HOST_SPIRV_H
#define SHADPS4_HOST_SPIRV_H

/* The renderer's own shaders, written straight as SPIR-V words in C89 when
 * they are first needed: no GLSL and no shader compiler, at build time or
 * at run time. Each keeps the interface (bindings, push constants, inputs
 * and outputs) of the GLSL shader it replaces; what used to be a
 * specialization constant is a parameter, folded into the module.
 *
 * Each function writes the module into @out, which holds @cap words, and
 * returns the number of words written, 0 when @cap is too small.
 * HOST_SPIRV_MAX_WORDS is enough for any of them. */

#include <stddef.h>
#include <stdint.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

#define HOST_SPIRV_MAX_WORDS 8192

/* A full-screen triangle; uv at location 0, from 0 to 1 across the screen */
size_t host_spirv_fs_tri_vert(uint32_t* out, size_t cap);

/* The output picture: sampler at binding 0, push constants
 * { float gamma; uint hdr; uint srgb_input; float pq_white; }. sRGB-encodes
 * linear colour (decoding sRGB input first when asked), or, when pq_white is
 * above 0, encodes it as PQ Rec.2020 with SDR white at pq_white (of 10000
 * nits), or passes HDR (already PQ) through. */
size_t host_spirv_post_process_frag(uint32_t* out, size_t cap);

/* ImGui's fragment shader for an HDR (PQ) frontend: its sRGB-encoded colour
 * times the texture at binding 0, as PQ Rec.2020 with SDR white at @white
 * (of 10000 nits). The input is ImGui's: location 0, struct { vec4 Color;
 * vec2 UV; }. */
size_t host_spirv_imgui_pq_frag(uint32_t* out, size_t cap, float white);

/* Depth from a colour texture's component for the current sample: texture
 * at binding 0, uv at location 0 */
size_t host_spirv_color_to_ms_depth_frag(uint32_t* out, size_t cap);

/* The texel under the fragment from the texture at binding 0, from the
 * current sample when @msaa */
size_t host_spirv_ms_image_blit_frag(uint32_t* out, size_t cap, int msaa);

/* One plane of a depth buffer as colour: depth, or stencil when @stencil,
 * of the texel under the fragment from the texture at binding 0 (sample
 * @sample of it when @msaa), into every component of a float target or,
 * when @uint_out, an integer one */
size_t host_spirv_depth_to_color_frag(uint32_t* out, size_t cap, int msaa, int stencil,
                                      int uint_out, uint32_t sample);

/* The GPU's page fault bits (binding 0) as a list of page addresses
 * (binding 1) */
size_t host_spirv_fault_buffer_process_comp(uint32_t* out, size_t cap, uint32_t caching_pagebits,
                                            uint32_t max_page_faults);

/* Tiles or detiles one image: tiled data at binding 0, linear at 1, the
 * TilingInfo uniform at 2. @params are the values tile_manager.cpp passed as
 * specialization constants, in their order (@num_params is 4 for a micro
 * tiled image, 12 for a macro tiled one). @word_access moves 8- and 16-bit
 * texels inside 32-bit words, for GPUs without 8- or 16-bit storage buffer
 * access. */
size_t host_spirv_tiling_comp(uint32_t* out, size_t cap, unsigned bits_per_pixel, int macro_tiled,
                              int word_access, const uint32_t* params, size_t num_params);

RETRO_END_DECLS

#endif
