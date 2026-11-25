/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdlib.h>
#include <string.h>

#include <retro_atomic.h>

#include "common/c_bridge.h"
#include "core/libraries/ajm/ajm_context.h"
#include "core/libraries/ajm/ajm_error.h"
#include "core/libraries/ajm/ajm_instance.h"
#include "core/libraries/error_codes.h"

#define INSTANCE_ID_MASK 0x3FFFu
#define NUM_INSTANCE_SLOTS (AJM_MAX_INSTANCES - 1)
#define NUM_BATCH_SLOTS (AJM_MAX_BATCHES - 1)

ajm_context_t* ajm_context_new(void) {
    ajm_context_t* c = (ajm_context_t*)calloc(1, sizeof(*c));
    if (!c) {
        return NULL;
    }
    if (!obj_table_init(&c->instances, NUM_INSTANCE_SLOTS)) {
        free(c);
        return NULL;
    }
    return c;
}

static bool ajm_context_is_registered(const ajm_context_t* c, u32 type) {
    return c->registered_codecs[type];
}

/* An instance found held, given back: freed by the last one to let go of a destroyed one */
static void ajm_context_release_instance(ajm_context_t* c, int index, ajm_instance_t* instance) {
    if (obj_table_release(&c->instances, index)) {
        ajm_instance_delete(instance);
        obj_table_reuse(&c->instances, index);
    }
}

/* The batch slot of @batch_id, NULL for an id out of range */
static retro_atomic_int_t* ajm_context_batch(ajm_context_t* c, u32 batch_id) {
    return batch_id - 1 < (u32)NUM_BATCH_SLOTS ? &c->batches[batch_id - 1] : NULL;
}

s32 ajm_context_batch_cancel(ajm_context_t* c, u32 batch_id) {
    retro_atomic_int_t* slot = ajm_context_batch(c, batch_id);
    int word;
    if (slot == NULL) {
        return ORBIS_AJM_ERROR_INVALID_BATCH;
    }
    for (;;) {
        word = retro_atomic_load_acquire_int(slot);
        if (!(word & AJM_BATCH_LIVE)) {
            return ORBIS_AJM_ERROR_INVALID_BATCH;
        }
        if ((word & (AJM_BATCH_PROCESSED | AJM_BATCH_CANCELED)) != 0) {
            return ORBIS_OK;
        }
        if (retro_atomic_cas_int(slot, word, word | AJM_BATCH_CANCELED)) {
            return ORBIS_OK;
        }
    }
}

s32 ajm_context_module_register(ajm_context_t* c, u32 type) {
    if (type >= AJM_CODEC_TYPE_MAX) {
        return ORBIS_AJM_ERROR_INVALID_PARAMETER;
    }
    if (ajm_context_is_registered(c, type)) {
        return ORBIS_AJM_ERROR_CODEC_ALREADY_REGISTERED;
    }
    c->registered_codecs[type] = true;
    return ORBIS_OK;
}

void ajm_context_run_job(ajm_context_t* c, u32 batch_id, ajm_job_t* job) {
    const u32 index = (job->instance_id & INSTANCE_ID_MASK) - 1;
    ajm_instance_t* instance;
#ifndef NDEBUG
    c_log(C_LOG_LIB_AJM, C_LOG_LEVEL_TRACE, "ProcessBatch",
          "Processing job %u for instance %u. flags = 0x%llx", batch_id, job->instance_id,
          (unsigned long long)job->flags);
#else
    (void)batch_id;
#endif
    if (job->instance_id == AJM_INSTANCE_STATISTICS) {
        ajm_statistics_execute_job(job);
        return;
    }
    instance = index < (u32)NUM_INSTANCE_SLOTS
                   ? (ajm_instance_t*)obj_table_hold(&c->instances, (int)index)
                   : NULL;
    C_ASSERT_MSG(instance != NULL, "ProcessBatch", "Attempting to execute job on null instance");
    ajm_instance_execute_job(instance, job);
    ajm_context_release_instance(c, (int)index, instance);
}

s32 ajm_context_batch_wait(ajm_context_t* c, u32 batch_id, u32 timeout,
                           AjmBatchError* batch_error) {
    retro_atomic_int_t* slot = ajm_context_batch(c, batch_id);
    int word;
    (void)batch_error;
    if (slot == NULL) {
        return ORBIS_AJM_ERROR_INVALID_BATCH;
    }
    for (;;) {
        word = retro_atomic_load_acquire_int(slot);
        if (!(word & AJM_BATCH_LIVE)) {
            return ORBIS_AJM_ERROR_INVALID_BATCH;
        }
        if (word & AJM_BATCH_WAITING) {
            return ORBIS_AJM_ERROR_BUSY;
        }
        if (retro_atomic_cas_int(slot, word, word | AJM_BATCH_WAITING)) {
            break;
        }
    }
    /* Decoded when it was started (ajm_context_batch_start_buffer): never in progress,
     * nothing to wait for */
    (void)timeout;
    word = retro_atomic_exchange_int(slot, 0);
    if (word & AJM_BATCH_CANCELED) {
        return ORBIS_AJM_ERROR_CANCELLED;
    }
    return ORBIS_OK;
}

s32 ajm_context_batch_start_buffer(ajm_context_t* c, u8* p_batch, u32 batch_size, int priority,
                                   AjmBatchError* batch_error, u32* out_batch_id) {
    int start;
    int n;
    int index = -1;
    (void)priority;
    (void)batch_error;
    if ((batch_size & 7) != 0) {
        c_log(C_LOG_LIB_AJM, C_LOG_LEVEL_ERROR, "BatchStartBuffer",
              "ORBIS_AJM_ERROR_MALFORMED_BATCH");
        return ORBIS_AJM_ERROR_MALFORMED_BATCH;
    }

    /* A free slot, looking from where the last one taken left off */
    start = retro_atomic_load_relaxed_int(&c->batch_next);
    for (n = 0; n < NUM_BATCH_SLOTS; ++n) {
        const int i = (start + n) % NUM_BATCH_SLOTS;
        if (retro_atomic_load_relaxed_int(&c->batches[i]) == 0 &&
            retro_atomic_cas_int(&c->batches[i], 0, AJM_BATCH_LIVE)) {
            index = i;
            break;
        }
    }
    if (index < 0) {
        return ORBIS_AJM_ERROR_OUT_OF_MEMORY;
    }
    retro_atomic_store_relaxed_int(&c->batch_next, (index + 1) % NUM_BATCH_SLOTS);
    *out_batch_id = (u32)index + 1;

    /* Decoded here, on the game's thread, before the call returns: as the console's decoder,
     * it is done well within the time the game gives it, so the game finds it done whenever it
     * looks (a poll with no wait included). A decoder thread of its own was late now and then:
     * a game that polled found the batch in progress and went on without its sound (FMOD's
     * streams: a gap, a click). The decode is the game's work, counted running as such. */
    (void)retro_atomic_fetch_or_int(&c->batches[index], AJM_BATCH_PROCESSED);
    ajm_batch_run(c, (u32)index + 1, p_batch, batch_size);
    return ORBIS_OK;
}

s32 ajm_context_instance_create(ajm_context_t* c, u32 codec_type, u64 flags, u32* out_instance) {
    ajm_instance_t* instance;
    int start;
    int index;
    if (codec_type >= AJM_CODEC_TYPE_MAX) {
        return ORBIS_AJM_ERROR_INVALID_PARAMETER;
    }
    if (AJM_INSTANCE_FLAGS_VERSION(flags) == 0) {
        return ORBIS_AJM_ERROR_WRONG_REVISION_FLAG;
    }
    if (!ajm_context_is_registered(c, codec_type)) {
        return ORBIS_AJM_ERROR_CODEC_NOT_REGISTERED;
    }
    instance = ajm_instance_new(codec_type, flags);
    if (!instance) {
        return ORBIS_AJM_ERROR_OUT_OF_MEMORY;
    }
    /* A free slot, looking from where the last one taken left off */
    start = retro_atomic_load_relaxed_int(&c->instance_next);
    index = obj_table_add(&c->instances, instance, start, NUM_INSTANCE_SLOTS);
    if (index < 0) {
        index = obj_table_add(&c->instances, instance, 0, start);
    }
    if (index < 0) {
        ajm_instance_delete(instance);
        return ORBIS_AJM_ERROR_OUT_OF_RESOURCES;
    }
    retro_atomic_store_relaxed_int(&c->instance_next, (index + 1) % NUM_INSTANCE_SLOTS);
    *out_instance = ((u32)index + 1) | (codec_type << 14);
    c_log(C_LOG_LIB_AJM, C_LOG_LEVEL_INFO, "InstanceCreate", "instance = %u", *out_instance);
    return ORBIS_OK;
}

s32 ajm_context_instance_destroy(ajm_context_t* c, u32 instance_id) {
    const u32 index = (instance_id & INSTANCE_ID_MASK) - 1;
    ajm_instance_t* instance;
    if (index >= (u32)NUM_INSTANCE_SLOTS) {
        return ORBIS_AJM_ERROR_INVALID_INSTANCE;
    }
    instance = (ajm_instance_t*)obj_table_peek(&c->instances, (int)index);
    switch (obj_table_doom(&c->instances, (int)index)) {
    case 1:
        ajm_instance_delete(instance);
        obj_table_reuse(&c->instances, (int)index);
        return ORBIS_OK;
    case 0:
        return ORBIS_OK;
    default:
        return ORBIS_AJM_ERROR_INVALID_INSTANCE;
    }
}
