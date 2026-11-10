// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once
#include <memory>

namespace Libraries::AudioIn {

struct PortIn;

// Where a game's audio input comes from. The core has no microphone yet (SilentAudioIn): the
// frontend's microphone (libretro's RETRO_ENVIRONMENT_GET_MICROPHONE_INTERFACE) is the one meant
// to plug in here, its samples passed over from the core's process. A port reads at its own
// rate, format and length (PortIn).
class PortInBackend {
public:
    virtual ~PortInBackend() = default;
    // Fills one buffer of the port, waiting for it the way a microphone paces it. Returns the
    // frames read.
    virtual int Read(void* out_buffer) = 0;
    // Drops what is captured and not read yet.
    virtual void Clear() = 0;
    // A microphone is there (sceAudioInGetSilentState).
    virtual bool IsAvailable() = 0;
};

class AudioInBackend {
public:
    AudioInBackend() = default;
    virtual ~AudioInBackend() = default;
    virtual std::unique_ptr<PortInBackend> Open(PortIn& port) = 0;
};

// No microphone in the core: silence, at a microphone's pace (silent_audio_in.cpp).
class SilentAudioIn final : public AudioInBackend {
public:
    std::unique_ptr<PortInBackend> Open(PortIn& port) override;
};

// The backend in use, made when the game first opens a port (audioin.cpp).
std::unique_ptr<AudioInBackend> MakeAudioInBackend();

} // namespace Libraries::AudioIn