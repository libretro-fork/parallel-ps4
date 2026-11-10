// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <chrono>
#include <cstring>
#include <thread>

#include "common/logging/log.h"
#include "core/libraries/audio/audioin.h"
#include "core/libraries/audio/audioin_backend.h"

namespace Libraries::AudioIn {

namespace {

// The core has no microphone: a port reports none attached and reads silence, a buffer at a
// time, at the pace a microphone would fill it, so a game reading in a loop is not spun.
class SilentPortBackend final : public PortInBackend {
public:
    explicit SilentPortBackend(const PortIn& port)
        : bytes(static_cast<size_t>(port.samples_num) * port.sample_size * port.channels_num),
          samples(port.samples_num),
          period(std::chrono::nanoseconds(
              port.freq ? static_cast<s64>(port.samples_num) * 1'000'000'000 / port.freq : 0)),
          next(std::chrono::steady_clock::now()) {
        LOG_INFO(Lib_AudioIn, "No microphone: audio input port reads silence");
    }

    int Read(void* out_buffer) override {
        next += period;
        const auto now = std::chrono::steady_clock::now();
        if (next > now) {
            std::this_thread::sleep_until(next);
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
    const std::chrono::steady_clock::duration period;
    std::chrono::steady_clock::time_point next;
};

} // Anonymous namespace

std::unique_ptr<PortInBackend> SilentAudioIn::Open(PortIn& port) {
    return std::make_unique<SilentPortBackend>(port);
}

} // namespace Libraries::AudioIn
