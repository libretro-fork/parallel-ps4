/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_AJM_AJM_CONTEXT_H
#define SHADPS4_AJM_AJM_CONTEXT_H

/* A context of the game's: its codecs, its instances and its batches. Defined in
 * ajm_context.c, the batches' contents read in ajm_batch.c. */

#include <boolean.h>
#include <retro_atomic.h>
#include <retro_common_api.h>

#include "common/obj_table.h"
#include "common/types.h"
#include "core/libraries/ajm/ajm.h"

RETRO_BEGIN_DECLS

#define AJM_MAX_INSTANCES 0x2fff
#define AJM_MAX_BATCHES 0x0400

/* A batch slot's word: free (0), or in use and what has been done with it */
#define AJM_BATCH_LIVE 1
#define AJM_BATCH_PROCESSED 2
#define AJM_BATCH_CANCELED 4
#define AJM_BATCH_WAITING 8

typedef struct ajm_context {
    bool registered_codecs[AJM_CODEC_TYPE_MAX];
    /* Ids from 1: slot + 1. Filled, read and emptied by any thread with no lock; the next
     * search for a free slot starts after the last one taken. */
    obj_table_t instances; /* ajm_instance_t */
    retro_atomic_int_t instance_next;
    /* A batch is decoded when it is started: a slot is only its state, for its wait */
    retro_atomic_int_t batches[AJM_MAX_BATCHES - 1];
    retro_atomic_int_t batch_next;
} ajm_context_t;

/* NULL if it cannot be made */
ajm_context_t* ajm_context_new(void);
s32 ajm_context_instance_create(ajm_context_t* c, u32 codec_type, u64 flags, u32* out_instance_id);
s32 ajm_context_instance_destroy(ajm_context_t* c, u32 instance_id);
s32 ajm_context_batch_cancel(ajm_context_t* c, u32 batch_id);
s32 ajm_context_module_register(ajm_context_t* c, u32 type);
s32 ajm_context_batch_wait(ajm_context_t* c, u32 batch_id, u32 timeout, AjmBatchError* batch_error);
s32 ajm_context_batch_start_buffer(ajm_context_t* c, u8* p_batch, u32 batch_size, int priority,
                                   AjmBatchError* batch_error, u32* p_batch_id);

/* The jobs of the batch @batch_id, in @data's @size bytes, run on @c's instances as they are
 * read (ajm_batch.c) */
void ajm_batch_run(ajm_context_t* c, u32 batch_id, u8* data, size_t size);
/* Runs @job on its instance, or the statistics instance (ajm_context.c) */
struct ajm_job;
void ajm_context_run_job(ajm_context_t* c, u32 batch_id, struct ajm_job* job);

void* ajm_batch_job_control_buffer_ra(void* p_buffer, u32 instance_id, u64 flags,
                                      void* p_sideband_input, size_t sideband_input_size,
                                      void* p_sideband_output, size_t sideband_output_size,
                                      void* p_return_address);
void* ajm_batch_job_inline_buffer(void* p_buffer, const void* p_data_input, size_t data_input_size,
                                  const void** pp_batch_address);
void* ajm_batch_job_run_buffer_ra(void* p_buffer, u32 instance_id, u64 flags, void* p_data_input,
                                  size_t data_input_size, void* p_data_output,
                                  size_t data_output_size, void* p_sideband_output,
                                  size_t sideband_output_size, void* p_return_address);
void* ajm_batch_job_run_split_buffer_ra(void* p_buffer, u32 instance_id, u64 flags,
                                        const AjmBuffer* p_data_input_buffers,
                                        size_t num_data_input_buffers,
                                        const AjmBuffer* p_data_output_buffers,
                                        size_t num_data_output_buffers, void* p_sideband_output,
                                        size_t sideband_output_size, void* p_return_address);

RETRO_END_DECLS

#endif
