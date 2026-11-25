/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_VIDEOOUT_GPU_C_H
#define SHADPS4_VIDEOOUT_GPU_C_H

/* The GPU's side of libSceVideoOut, for driver.c: the presenter (Vulkan::Presenter), the GPU
 * thread (AmdGpu::Liverpool) and its flip interrupt (Platform::IrqC). Defined in
 * video_core/amdgpu/liverpool.cpp, which holds the presenter and the GPU. */

#include <boolean.h>
#include <retro_common_api.h>
#include <stdint.h>

#include "common/types.h"
#include "core/libraries/videoout/buffer.h"

RETRO_BEGIN_DECLS

struct VideoOutPort;
/* A Vulkan::Frame */
struct videoout_frame;

/* Presenter::RegisterVideoOutSurface */
void videoout_gpu_register_surface(const BufferAttributeGroup* group, uintptr_t address);
/* Presenter::PrepareFrame, PrepareBlankFrame and PrepareLastFrame (NULL: none to show) */
struct videoout_frame* videoout_gpu_prepare_frame(const BufferAttributeGroup* group,
                                                  uintptr_t address, bool game_hdr);
struct videoout_frame* videoout_gpu_prepare_blank_frame(bool present_thread);
struct videoout_frame* videoout_gpu_prepare_last_frame(void);
/* Presenter::Present */
void videoout_gpu_present(struct videoout_frame* frame, bool is_reusing_frame, bool is_game_frame);
/* Presenter::IsHDRSupported, and the post-processing's gamma */
bool videoout_gpu_is_hdr_supported(void);
void videoout_gpu_set_gamma(float gamma);

/* Liverpool::SetVoPort: the port whose labels the GPU's waits look at */
void videoout_gpu_set_vo_port(struct VideoOutPort* port);

typedef void (*videoout_flip_fn)(struct VideoOutPort* port, s32 index, s64 flip_arg, bool is_eop);
/* Liverpool::SendCommand: @fn(@port, @index, @flip_arg, @is_eop) on the GPU thread, before its
 * next submission; any thread, nothing waited for */
void videoout_gpu_send_flip(videoout_flip_fn fn, struct VideoOutPort* port, s32 index, s64 flip_arg,
                            bool is_eop);

typedef void (*videoout_flip_irq_fn)(struct VideoOutPort* port, u32 buf_id, s64 flip_arg);
/* Platform::IrqC::RegisterOnce on the flip interrupt: @fn(@port, @buf_id, @flip_arg) when it
 * next comes (anything else there stops the emulator) */
void videoout_gpu_on_flip_irq(videoout_flip_irq_fn fn, struct VideoOutPort* port, u32 buf_id,
                              s64 flip_arg);

RETRO_END_DECLS

#endif
