// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/bounded_threadsafe_queue.h"
#include "common/shared_slots.h"
#include "common/types.h"
#include "core/libraries/ajm/ajm.h"
#include "core/libraries/ajm/ajm_batch.h"
#include "core/libraries/ajm/ajm_instance.h"

#include <array>
#include <memory>
#include <span>
#include <thread>
#include <utility>

namespace Libraries::Ajm {

class AjmContext {
public:
    AjmContext();

    s32 InstanceCreate(AjmCodecType codec_type, AjmInstanceFlags flags, u32* out_instance_id);
    s32 InstanceDestroy(u32 instance_id);

    s32 BatchCancel(const u32 batch_id);
    s32 ModuleRegister(AjmCodecType type);
    s32 BatchWait(const u32 batch_id, const u32 timeout, AjmBatchError* const p_batch_error);
    s32 BatchStartBuffer(u8* p_batch, u32 batch_size, const int priority,
                         AjmBatchError* p_batch_error, u32* p_batch_id);

    void WorkerThread(std::stop_token stop);
    void ProcessBatch(u32 id, std::span<AjmJob> jobs);

private:
    static constexpr u32 MaxInstances = 0x2fff;
    static constexpr u32 MaxBatches = 0x0400;
    static constexpr u32 NumAjmCodecs = std::to_underlying(AjmCodecType::Max);

    [[nodiscard]] bool IsRegistered(AjmCodecType type) const;

    std::array<bool, NumAjmCodecs> registered_codecs{};

    // Ids from 1: slot id - 1. Read, filled and emptied by any thread with no lock.
    Common::SharedSlots<AjmInstance, MaxInstances - 1> instances;
    Common::SharedSlots<AjmBatch, MaxBatches - 1> batches;

    std::jthread worker_thread{};
    Common::MPSCQueue<std::shared_ptr<AjmBatch>> batch_queue;
};

} // namespace Libraries::Ajm
