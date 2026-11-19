// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstring>

#include "common/logging/log.h"
#include "core/libraries/audio/audioin.h"
#include "core/libraries/audio/audioin_backend.h"
#include "core/libraries/kernel/time.h"

namespace Libraries::AudioIn {

namespace {

// The core has no microphone: a port reports none attached and reads silence, a buffer at a
// time, at the pace a microphone would fill it, so a game reading in a loop is not spun. The
// pace is the game's time, which the frontend's frames move: not the host's clock.
class SilentPortBackend final : public PortInBackend {
public:
    explicit SilentPortBackend(const PortIn& port)
        : bytes(static_cast<size_t>(port.samples_num) * port.sample_size * port.channels_num),
          samples(port.samples_num),
          period(port.freq ? static_cast<u64>(port.samples_num) * 1'000'000'000 / port.freq : 0),
          next(Libraries::Kernel::GameNow()) {
        LOG_INFO(Lib_AudioIn, "No microphone: audio input port reads silence");
    }

    int Read(void* out_buffer) override {
        next += period;
        const u64 now = Libraries::Kernel::GameNow();
        if (next > now) {
            Libraries::Kernel::GameSleepUntil(next);
        } else {
            next = now; // fell behind: no burst to catch up
        }
        std::memset(out_buffer, 0, bytes);
        return static_cast<int>(samples);
    }

    void Clear() override {}

    bool IsAvailable() override {
        return false;
    }

private:
    const size_t bytes;
    const u32 samples;
    // Nanoseconds of the game's time: a buffer's length, and when the next is full
    const u64 period;
    u64 next;
};

} // Anonymous namespace

std::unique_ptr<PortInBackend> SilentAudioIn::Open(PortIn& port) {
    return std::make_unique<SilentPortBackend>(port);
}

} // namespace Libraries::AudioIn
