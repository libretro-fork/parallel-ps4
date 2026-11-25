/* SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <retro_atomic.h>

#include "common/c_bridge.h"
#include "common/enum_names.h"
#include "core/libraries/ajm/ajm.h"
#include "core/libraries/ajm/ajm_context.h"
#include "core/libraries/ajm/ajm_error.h"
#include "core/libraries/ajm/ajm_mp3.h"
#include "core/libraries/error_codes.h"

#define ORBIS_AJM_CHANNELMASK_MONO 0x0004
#define ORBIS_AJM_CHANNELMASK_STEREO 0x0003
#define ORBIS_AJM_CHANNELMASK_QUAD 0x0033
#define ORBIS_AJM_CHANNELMASK_5POINT1 0x060F
#define ORBIS_AJM_CHANNELMASK_7POINT1 0x063F

/* The game's contexts, ids from 1: made, never taken away */
#define AJM_MAX_CONTEXTS 256
static retro_atomic_ptr_t contexts[AJM_MAX_CONTEXTS];
static retro_atomic_int_t num_contexts;

static ajm_context_t* ajm_find_context(u32 context_id) {
    if (context_id == 0 || context_id > AJM_MAX_CONTEXTS) {
        return NULL;
    }
    return (ajm_context_t*)retro_atomic_load_acquire_ptr(&contexts[context_id - 1]);
}

u32 ajm_channel_mask(u32 num_channels) {
    switch (num_channels) {
    case 1:
        return ORBIS_AJM_CHANNELMASK_MONO;
    case 2:
        return ORBIS_AJM_CHANNELMASK_STEREO;
    case 4:
        return ORBIS_AJM_CHANNELMASK_QUAD;
    case 6:
        return ORBIS_AJM_CHANNELMASK_5POINT1;
    case 8:
        return ORBIS_AJM_CHANNELMASK_7POINT1;
    default:
        c_log(C_LOG_LIB_AJM, C_LOG_LEVEL_CRITICAL, "GetChannelMask",
              "Unexpected number of channels: %u", num_channels);
        C_ASSERT_MSG(0, "GetChannelMask", "Unreachable code!");
        return 0;
    }
}

int PS4_SYSV_ABI sceAjmBatchCancel(u32 context_id, u32 batch_id) {
    ajm_context_t* c;
    c_log(C_LOG_LIB_AJM, C_LOG_LEVEL_INFO, "sceAjmBatchCancel",
          "called context_id = %u batch_id = %u", context_id, batch_id);
    c = ajm_find_context(context_id);
    if (c == NULL) {
        return ORBIS_AJM_ERROR_INVALID_CONTEXT;
    }
    return ajm_context_batch_cancel(c, batch_id);
}

int PS4_SYSV_ABI sceAjmBatchErrorDump(void) {
    c_log(C_LOG_LIB_AJM, C_LOG_LEVEL_ERROR, "sceAjmBatchErrorDump", "(STUBBED) called");
    return ORBIS_OK;
}

void* PS4_SYSV_ABI sceAjmBatchJobControlBufferRa(void* p_buffer, u32 instance_id, u64 flags,
                                                 void* p_sideband_input, size_t sideband_input_size,
                                                 void* p_sideband_output,
                                                 size_t sideband_output_size,
                                                 void* p_return_address) {
    return ajm_batch_job_control_buffer_ra(p_buffer, instance_id, flags, p_sideband_input,
                                           sideband_input_size, p_sideband_output,
                                           sideband_output_size, p_return_address);
}

void* PS4_SYSV_ABI sceAjmBatchJobInlineBuffer(void* p_buffer, const void* p_data_input,
                                              size_t data_input_size,
                                              const void** pp_batch_address) {
    return ajm_batch_job_inline_buffer(p_buffer, p_data_input, data_input_size, pp_batch_address);
}

void* PS4_SYSV_ABI sceAjmBatchJobRunBufferRa(void* p_buffer, u32 instance_id, u64 flags,
                                             void* p_data_input, size_t data_input_size,
                                             void* p_data_output, size_t data_output_size,
                                             void* p_sideband_output, size_t sideband_output_size,
                                             void* p_return_address) {
    return ajm_batch_job_run_buffer_ra(p_buffer, instance_id, flags, p_data_input, data_input_size,
                                       p_data_output, data_output_size, p_sideband_output,
                                       sideband_output_size, p_return_address);
}

void* PS4_SYSV_ABI sceAjmBatchJobRunSplitBufferRa(
    void* p_buffer, u32 instance_id, u64 flags, const AjmBuffer* p_data_input_buffers,
    size_t num_data_input_buffers, const AjmBuffer* p_data_output_buffers,
    size_t num_data_output_buffers, void* p_sideband_output, size_t sideband_output_size,
    void* p_return_address) {
    return ajm_batch_job_run_split_buffer_ra(p_buffer, instance_id, flags, p_data_input_buffers,
                                             num_data_input_buffers, p_data_output_buffers,
                                             num_data_output_buffers, p_sideband_output,
                                             sideband_output_size, p_return_address);
}

int PS4_SYSV_ABI sceAjmBatchStartBuffer(u32 context_id, u8* p_batch, u32 batch_size, int priority,
                                        AjmBatchError* batch_error, u32* out_batch_id) {
    ajm_context_t* c;
#ifndef NDEBUG
    c_log(C_LOG_LIB_AJM, C_LOG_LEVEL_TRACE, "sceAjmBatchStartBuffer",
          "called context = %u, batch_size = 0x%x, priority = %d", context_id, batch_size,
          priority);
#endif
    c = ajm_find_context(context_id);
    if (c == NULL) {
        return ORBIS_AJM_ERROR_INVALID_CONTEXT;
    }
    return ajm_context_batch_start_buffer(c, p_batch, batch_size, priority, batch_error,
                                          out_batch_id);
}

int PS4_SYSV_ABI sceAjmBatchWait(u32 context_id, u32 batch_id, u32 timeout,
                                 AjmBatchError* batch_error) {
    ajm_context_t* c;
#ifndef NDEBUG
    c_log(C_LOG_LIB_AJM, C_LOG_LEVEL_TRACE, "sceAjmBatchWait",
          "called context = %u, batch_id = %u, timeout = %u", context_id, batch_id, timeout);
#endif
    c = ajm_find_context(context_id);
    if (c == NULL) {
        return ORBIS_AJM_ERROR_INVALID_CONTEXT;
    }
    return ajm_context_batch_wait(c, batch_id, timeout, batch_error);
}

int PS4_SYSV_ABI sceAjmDecAt9ParseConfigData(void) {
    c_log(C_LOG_LIB_AJM, C_LOG_LEVEL_ERROR, "sceAjmDecAt9ParseConfigData", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAjmDecMp3ParseFrame(const u8* buf, u32 stream_size, int parse_ofl,
                                        struct AjmDecMp3ParseFrame* frame) {
    return ajm_mp3_parse_frame(buf, stream_size, parse_ofl, frame);
}

int PS4_SYSV_ABI sceAjmFinalize(void) {
    c_log(C_LOG_LIB_AJM, C_LOG_LEVEL_ERROR, "sceAjmFinalize", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAjmInitialize(s64 reserved, u32* p_context_id) {
    ajm_context_t* c;
    int index;
    c_log(C_LOG_LIB_AJM, C_LOG_LEVEL_INFO, "sceAjmInitialize", "called reserved = %lld",
          (long long)reserved);
    if (p_context_id == NULL || reserved != 0) {
        return ORBIS_AJM_ERROR_INVALID_PARAMETER;
    }
    index = retro_atomic_fetch_add_int(&num_contexts, 1);
    if (index >= AJM_MAX_CONTEXTS) {
        (void)retro_atomic_fetch_add_int(&num_contexts, -1);
        return ORBIS_AJM_ERROR_OUT_OF_RESOURCES;
    }
    c = ajm_context_new();
    if (c == NULL) {
        return ORBIS_AJM_ERROR_OUT_OF_MEMORY;
    }
    retro_atomic_store_release_ptr(&contexts[index], c);
    *p_context_id = (u32)index + 1;
    return ORBIS_OK;
}

u32 PS4_SYSV_ABI sceAjmInstanceCodecType(u32 instance_id) {
    return (instance_id >> 14) & 0x1F;
}

int PS4_SYSV_ABI sceAjmInstanceCreate(u32 context_id, u32 codec_type, u64 flags,
                                      u32* out_instance) {
    ajm_context_t* c;
    c_log(C_LOG_LIB_AJM, C_LOG_LEVEL_INFO, "sceAjmInstanceCreate",
          "called context = %u, codec_type = %s, flags = 0x%llx", context_id,
          enum_name(&enum_names_AjmCodecType, (int64_t)codec_type), (unsigned long long)flags);
    c = ajm_find_context(context_id);
    if (c == NULL) {
        return ORBIS_AJM_ERROR_INVALID_CONTEXT;
    }
    return ajm_context_instance_create(c, codec_type, flags, out_instance);
}

int PS4_SYSV_ABI sceAjmInstanceDestroy(u32 context_id, u32 instance_id) {
    ajm_context_t* c;
    c_log(C_LOG_LIB_AJM, C_LOG_LEVEL_INFO, "sceAjmInstanceDestroy",
          "called context = %u, instance = %u", context_id, instance_id);
    c = ajm_find_context(context_id);
    if (c == NULL) {
        return ORBIS_AJM_ERROR_INVALID_CONTEXT;
    }
    return ajm_context_instance_destroy(c, instance_id);
}

int PS4_SYSV_ABI sceAjmInstanceExtend(void) {
    c_log(C_LOG_LIB_AJM, C_LOG_LEVEL_ERROR, "sceAjmInstanceExtend", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAjmInstanceSwitch(void) {
    c_log(C_LOG_LIB_AJM, C_LOG_LEVEL_ERROR, "sceAjmInstanceSwitch", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAjmMemoryRegister(u32 context_id, void* ptr, size_t num_pages) {
    (void)context_id;
    (void)ptr;
    (void)num_pages;
    /* All memory is already shared with our implementation since we do not use any hardware. */
#ifndef NDEBUG
    c_log(C_LOG_LIB_AJM, C_LOG_LEVEL_TRACE, "sceAjmMemoryRegister", "(STUBBED) called");
#endif
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAjmMemoryUnregister(u32 context_id, void* ptr) {
    (void)context_id;
    (void)ptr;
    /* All memory is already shared with our implementation since we do not use any hardware. */
#ifndef NDEBUG
    c_log(C_LOG_LIB_AJM, C_LOG_LEVEL_TRACE, "sceAjmMemoryUnregister", "(STUBBED) called");
#endif
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAjmModuleRegister(u32 context_id, u32 codec_type, s64 reserved) {
    ajm_context_t* c;
    c_log(C_LOG_LIB_AJM, C_LOG_LEVEL_INFO, "sceAjmModuleRegister",
          "called context = %u, codec_type = %u", context_id, codec_type);
    if (reserved != 0) {
        return ORBIS_AJM_ERROR_INVALID_PARAMETER;
    }
    c = ajm_find_context(context_id);
    if (c == NULL) {
        return ORBIS_AJM_ERROR_INVALID_CONTEXT;
    }
    return ajm_context_module_register(c, codec_type);
}

int PS4_SYSV_ABI sceAjmModuleUnregister(void) {
    c_log(C_LOG_LIB_AJM, C_LOG_LEVEL_ERROR, "sceAjmModuleUnregister", "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceAjmStrError(void) {
    c_log(C_LOG_LIB_AJM, C_LOG_LEVEL_ERROR, "sceAjmStrError", "(STUBBED) called");
    return ORBIS_OK;
}
