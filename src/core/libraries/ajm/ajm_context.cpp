// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/assert.h"
#include "common/logging/log.h"
#include "common/thread_name.h"
#include "core/libraries/ajm/ajm.h"
#include "core/libraries/ajm/ajm_at9.h"
#include "core/libraries/ajm/ajm_context.h"
#include "core/libraries/ajm/ajm_error.h"
#include "core/libraries/ajm/ajm_instance.h"
#include "core/libraries/ajm/ajm_instance_statistics.h"
#include "core/libraries/ajm/ajm_mp3.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/kernel/time.h"

#include <span>
#include <utility>
#include <retro_atomic.h>

namespace Libraries::Ajm {

constexpr u32 ORBIS_AJM_WAIT_INFINITE = -1;
constexpr int INSTANCE_ID_MASK = 0x3FFF;

AjmContext::AjmContext() = default;

bool AjmContext::IsRegistered(AjmCodecType type) const {
    return registered_codecs[std::to_underlying(type)];
}

s32 AjmContext::BatchCancel(const u32 batch_id) {
    const auto batch = batch_id - 1 < batches.size() ? batches.Get(batch_id - 1) : nullptr;
    if (batch == nullptr) {
        return ORBIS_AJM_ERROR_INVALID_BATCH;
    }

    if (retro_atomic_load_seq_cst_int(&batch->processed) != 0) {
        return ORBIS_OK;
    }

    bool expected = false;
    (retro_atomic_cas_int(&batch->canceled, expected, true) ||
     ((expected = (retro_atomic_load_acquire_int(&batch->canceled) != 0)), false));
    return ORBIS_OK;
}

s32 AjmContext::ModuleRegister(AjmCodecType type) {
    if (std::to_underlying(type) >= NumAjmCodecs) {
        return ORBIS_AJM_ERROR_INVALID_PARAMETER;
    }
    if (IsRegistered(type)) {
        return ORBIS_AJM_ERROR_CODEC_ALREADY_REGISTERED;
    }
    registered_codecs[std::to_underlying(type)] = true;
    return ORBIS_OK;
}

void AjmContext::ProcessBatch(u32 id, std::span<AjmJob> jobs) {
    // Perform operation requested by control flags.
    for (auto& job : jobs) {
        LOG_TRACE(Lib_Ajm, "Processing job %u for instance %u. flags = 0x%" PRIx64, id,
                  job.instance_id, job.flags.raw);

        if (job.instance_id == AJM_INSTANCE_STATISTICS) {
            AjmInstanceStatistics::Getinstance().ExecuteJob(job);
        } else {
            const u32 index = (job.instance_id & INSTANCE_ID_MASK) - 1;
            const auto instance = index < instances.size() ? instances.Get(index) : nullptr;
            ASSERT_MSG(instance != nullptr, "Attempting to execute job on null instance");

            instance->ExecuteJob(job);
        }
    }
}

s32 AjmContext::BatchWait(const u32 batch_id, const u32 timeout, AjmBatchError* const batch_error) {
    const auto batch = batch_id - 1 < batches.size() ? batches.Get(batch_id - 1) : nullptr;
    if (batch == nullptr) {
        return ORBIS_AJM_ERROR_INVALID_BATCH;
    }

    bool expected = false;
    if (!(retro_atomic_cas_int(&batch->waiting, expected, true) ||
          ((expected = (retro_atomic_load_acquire_int(&batch->waiting) != 0)), false))) {
        return ORBIS_AJM_ERROR_BUSY;
    }

    // Decoded when it was started (BatchStartBuffer): never in progress, nothing to wait for
    (void)timeout;

    batches.Take(batch_id - 1);

    if (retro_atomic_load_seq_cst_int(&batch->canceled) != 0) {
        return ORBIS_AJM_ERROR_CANCELLED;
    }

    return ORBIS_OK;
}

int AjmContext::BatchStartBuffer(u8* p_batch, u32 batch_size, const int priority,
                                 AjmBatchError* batch_error, u32* out_batch_id) {
    if ((batch_size & 7) != 0) {
        LOG_ERROR(Lib_Ajm, "ORBIS_AJM_ERROR_MALFORMED_BATCH");
        return ORBIS_AJM_ERROR_MALFORMED_BATCH;
    }

    const auto batch_info = AjmBatch::FromBatchBuffer({p_batch, batch_size});
    const auto slot = batches.Insert(batch_info);
    if (!slot.has_value()) {
        return ORBIS_AJM_ERROR_OUT_OF_MEMORY;
    }
    *out_batch_id = static_cast<u32>(*slot) + 1;
    batch_info->id = *out_batch_id;

    // Decoded here, on the game's thread, before the call returns: as the console's decoder,
    // it is done well within the time the game gives it, so the game finds it done whenever it
    // looks (a poll with no wait included). A decoder thread of its own was late now and then:
    // a game that polled found the batch in progress and went on without its sound (FMOD's
    // streams: a gap, a click). The decode is the game's work, counted running as such.
    retro_atomic_store_release_int(&batch_info->processed, 1);
    if (!batch_info->jobs.empty()) {
        ProcessBatch(batch_info->id, batch_info->jobs);
    }

    return ORBIS_OK;
}

s32 AjmContext::InstanceCreate(AjmCodecType codec_type, AjmInstanceFlags flags, u32* out_instance) {
    if (codec_type >= AjmCodecType::Max) {
        return ORBIS_AJM_ERROR_INVALID_PARAMETER;
    }
    if (flags.version == 0) {
        return ORBIS_AJM_ERROR_WRONG_REVISION_FLAG;
    }
    if (!IsRegistered(codec_type)) {
        return ORBIS_AJM_ERROR_CODEC_NOT_REGISTERED;
    }
    const auto slot = instances.Insert(std::make_shared<AjmInstance>(codec_type, flags));
    if (!slot.has_value()) {
        return ORBIS_AJM_ERROR_OUT_OF_RESOURCES;
    }
    *out_instance = (static_cast<u32>(*slot) + 1) | (static_cast<u32>(codec_type) << 14);

    LOG_INFO(Lib_Ajm, "instance = %u", *out_instance);
    return ORBIS_OK;
}

s32 AjmContext::InstanceDestroy(u32 instance_id) {
    const u32 index = (instance_id & INSTANCE_ID_MASK) - 1;
    if (index >= instances.size() || !instances.Take(index)) {
        return ORBIS_AJM_ERROR_INVALID_INSTANCE;
    }
    return ORBIS_OK;
}

} // namespace Libraries::Ajm
