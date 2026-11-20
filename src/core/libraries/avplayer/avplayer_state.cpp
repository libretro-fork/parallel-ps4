// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/elf_info.h"
#include "common/logging/log.h"
#include "common/thread_name.h"
#include "core/libraries/avplayer/avplayer_error.h"
#include "core/libraries/avplayer/avplayer_state.h"
#include "core/libraries/kernel/process.h"
#include "core/libraries/kernel/time.h"

#include <retro_atomic.h>
#include "common/enum_names.h"

namespace Libraries::AvPlayer {

void PS4_SYSV_ABI AvPlayerState::AutoPlayEventCallback(void* opaque, AvPlayerEvents event_id,
                                                       s32 source_id, void* event_data) {
    auto const self = reinterpret_cast<AvPlayerState*>(opaque);

    if (event_id == AvPlayerEvents::StateReady) {
        s32 video_stream_index = -1;
        s32 audio_stream_index = -1;
        s32 timedtext_stream_index = -1;
        const s32 stream_count = self->GetStreamCount();
        if (AVPLAYER_IS_ERROR(stream_count)) {
            self->Stop();
            return;
        }
        if (stream_count == 0) {
            self->Stop();
            return;
        }
        for (u32 stream_index = 0; stream_index < stream_count; ++stream_index) {
            AvPlayerStreamInfo info{};
            if (!self->GetStreamInfo(stream_index, info)) {
                self->Stop();
                return;
            }

            const std::string_view default_language{self->m_default_language};
            switch (info.type) {
            case AvPlayerStreamType::Video:
                if (video_stream_index == -1) {
                    video_stream_index = stream_index;
                }
                if (!default_language.empty() &&
                    default_language == info.details.video.language_code) {
                    video_stream_index = stream_index;
                }
                break;
            case AvPlayerStreamType::Audio:
                if (audio_stream_index == -1) {
                    audio_stream_index = stream_index;
                }
                if (!default_language.empty() &&
                    default_language == info.details.audio.language_code) {
                    audio_stream_index = stream_index;
                }
                break;
            case AvPlayerStreamType::TimedText:
                if (timedtext_stream_index == -1) {
                    timedtext_stream_index = stream_index;
                }
                if (!default_language.empty() &&
                    default_language == info.details.subs.language_code) {
                    timedtext_stream_index = stream_index;
                }
                break;
            default:
                break;
            }
        }

        if (video_stream_index != -1) {
            self->EnableStream(video_stream_index);
        }
        if (audio_stream_index != -1) {
            self->EnableStream(audio_stream_index);
        }
        if (timedtext_stream_index != -1) {
            self->EnableStream(timedtext_stream_index);
        }
        self->Start();
        return;
    }

    DefaultEventCallback(opaque, event_id, 0, event_data);
}

void AvPlayerState::DefaultEventCallback(void* opaque, AvPlayerEvents event_id, s32 source_id,
                                         void* event_data) {
    auto const self = reinterpret_cast<AvPlayerState*>(opaque);
    const auto callback = self->m_event_replacement.event_callback;
    const auto ptr = self->m_event_replacement.object_ptr;
    if (callback != nullptr) {
        callback(ptr, event_id, 0, event_data);
    }
}

// Called inside GAME thread
AvPlayerState::AvPlayerState(const AvPlayerInitData& init_data)
    : m_init_data(init_data), m_event_replacement(init_data.event_replacement) {
    if (m_event_replacement.event_callback == nullptr || init_data.auto_start) {
        m_auto_start = true;
        m_init_data.event_replacement.event_callback = &AvPlayerState::AutoPlayEventCallback;
    } else {
        m_init_data.event_replacement.event_callback = &AvPlayerState::DefaultEventCallback;
    }
    m_init_data.event_replacement.object_ptr = this;
    if (init_data.default_language != nullptr) {
        std::memcpy(m_default_language, init_data.default_language, sizeof(m_default_language));
    }
    avplayer_fsm_init(&m_fsm, static_cast<int>(AvState::Initial));
    avplayer_transport_init(&m_transport);
    pin_gate_init(&m_source_gate);
    StartControllerThread();
}

AvPlayerState::~AvPlayerState() {
    // No thread of the player's is left, and no reader is in: then the source goes
    m_controller_thread.Stop();
    m_event_queue.Clear();
    pin_gate_close(&m_source_gate);
    delete static_cast<AvPlayerSource*>(retro_atomic_load_acquire_ptr(&m_source));
    pin_gate_free(&m_source_gate);
}

AvPlayerSource* AvPlayerState::PinSource() {
    if (!pin_gate_enter(&m_source_gate)) {
        return nullptr;
    }
    auto* const source = static_cast<AvPlayerSource*>(retro_atomic_load_acquire_ptr(&m_source));
    if (source == nullptr) {
        pin_gate_leave(&m_source_gate);
    }
    return source;
}

void AvPlayerState::UnpinSource() {
    pin_gate_leave(&m_source_gate);
}

AvState AvPlayerState::State() {
    return static_cast<AvState>(avplayer_fsm_current(&m_fsm));
}

AvState AvPlayerState::PreviousState() {
    return static_cast<AvState>(avplayer_fsm_previous(&m_fsm));
}

void AvPlayerState::PostInit(const AvPlayerPostInitData& post_init_data) {
    m_post_init_data = post_init_data;
}

// Called inside GAME thread
bool AvPlayerState::AddSource(std::string_view path, AvPlayerSourceType source_type) {
    if (path.empty()) {
        LOG_ERROR(Lib_AvPlayer, "File path is empty.");
        return false;
    }

    if (!retro_atomic_cas_int(&m_source_taken, 0, 1)) {
        LOG_ERROR(Lib_AvPlayer, "Only one source is supported.");
        return false;
    }

    s32 sdk_ver{};
    Libraries::Kernel::sceKernelGetCompiledSdkVersion(&sdk_ver);
    auto* const source = new AvPlayerSource(*this);
    if (!source->Init(m_init_data, path)) {
        SetState(AvState::Error);
        delete source;
        retro_atomic_store_release_int(&m_source_taken, 0);
        return false;
    }
    // Readers are kept out until its streams are found (ProcessEvent)
    pin_gate_close(&m_source_gate);
    retro_atomic_store_release_ptr(&m_source, source);
    AddSourceEvent();
    return true;
}

// Called inside GAME thread
s32 AvPlayerState::GetStreamCount() {
    AvPlayerSource* const source = PinSource();
    if (source == nullptr) {
        LOG_ERROR(Lib_AvPlayer, "Could not get stream count. No source.");
        return -1;
    }
    const s32 count = source->GetStreamCount();
    UnpinSource();
    return count;
}

// Called inside GAME thread
bool AvPlayerState::GetStreamInfo(u32 stream_index, AvPlayerStreamInfo& info) {
    AvPlayerSource* const source = PinSource();
    if (source == nullptr) {
        return false;
    }
    const bool got = source->GetStreamInfo(stream_index, info);
    UnpinSource();
    return got;
}

// Called inside GAME and CONTROLLER threads (auto-play)
bool AvPlayerState::Start() {
    return Transport(AVPLAYER_TRANSPORT_START);
}

bool AvPlayerState::Stop() {
    return Transport(AVPLAYER_TRANSPORT_STOP);
}

bool AvPlayerState::Transport(int op) {
    // Another thread is starting or stopping it: that one does this after, and the last of
    // what was asked meanwhile is what stands
    if (!avplayer_transport_claim(&m_transport, op, nullptr)) {
        return true;
    }
    const bool result = op == AVPLAYER_TRANSPORT_START ? DoStart() : DoStop();
    while ((op = avplayer_transport_next(&m_transport)) != AVPLAYER_TRANSPORT_NONE) {
        (void)(op == AVPLAYER_TRANSPORT_START ? DoStart() : DoStop());
    }
    return result;
}

// Has the transport
bool AvPlayerState::DoStart() {
    auto* const source = static_cast<AvPlayerSource*>(retro_atomic_load_acquire_ptr(&m_source));
    if (source == nullptr || !(State() == AvState::Ready || State() == AvState::Stop || DoStop())) {
        LOG_ERROR(Lib_AvPlayer, "Could not start playback.");
        return false;
    }
    (void)retro_atomic_exchange_int(&m_eof_stop_event_sent, 0);
    SetState(AvState::Starting);
    // Its queues and buffers are made anew: readers are kept out meanwhile
    pin_gate_close(&m_source_gate);
    const bool started = source->Start();
    pin_gate_open(&m_source_gate);
    if (!started) {
        LOG_ERROR(Lib_AvPlayer, "Could not start playback.");
        SetState(AvState::Error);
        return false;
    }
    SetState(AvState::Play);
    OnPlaybackStateChanged(AvState::Play);
    return true;
}

// Has the transport
bool AvPlayerState::DoStop() {
    auto* const source = static_cast<AvPlayerSource*>(retro_atomic_load_acquire_ptr(&m_source));
    if (source == nullptr || State() == AvState::Stop) {
        return false;
    }
    if (!source->Stop()) {
        return false;
    }
    if (!SetState(AvState::Stop)) {
        return false;
    }
    (void)retro_atomic_exchange_int(&m_eof_stop_event_sent, 1);
    OnPlaybackStateChanged(AvState::Stop);
    return true;
}

// Called inside GAME thread
bool AvPlayerState::Pause() {
    if (State() == AvState::EndOfFile) {
        return true;
    }
    const AvState state = State();
    AvPlayerSource* const source = state == AvState::Pause || state == AvState::Ready ||
                                           state == AvState::Initial || state == AvState::Unknown ||
                                           state == AvState::AddingSource
                                       ? nullptr
                                       : PinSource();
    if (source == nullptr) {
        LOG_ERROR(Lib_AvPlayer, "Could not pause playback.");
        return false;
    }
    source->Pause();
    UnpinSource();
    if (!SetState(AvState::Pause)) {
        return false;
    }
    OnPlaybackStateChanged(AvState::Pause);
    return true;
}

// Called inside GAME thread
bool AvPlayerState::Resume() {
    AvPlayerSource* const source = State() == AvState::Pause ? PinSource() : nullptr;
    if (source == nullptr) {
        LOG_ERROR(Lib_AvPlayer, "Could not resume playback.");
        return false;
    }
    source->Resume();
    UnpinSource();
    // Back to the state before the pause
    int resumed = 0;
    if (!avplayer_fsm_revert(&m_fsm, nullptr, &resumed)) {
        return true;
    }
    OnPlaybackStateChanged(static_cast<AvState>(resumed));
    return true;
}

void AvPlayerState::SetAvSyncMode(AvPlayerAvSyncMode sync_mode) {
    m_sync_mode = sync_mode;
}

void AvPlayerState::AvControllerThread() {
    thread_name_set("shadPS4:AvController");

    // An event wakes the thread at once; the buffering state is looked at every 5 ms of the
    // game's time, as it was every 5 ms of the host's
    constexpr u64 BufferingCheckNs = 5'000'000;
    while (!m_controller_thread.Stopping()) {
        if (!m_event_queue.Empty()) {
            ProcessEvent();
            continue;
        }
        Kernel::GameWaitAs(
            m_event_ready.get(), Kernel::GameDeadlineIn(BufferingCheckNs),
            [this] { return !m_event_queue.Empty() || m_controller_thread.Stopping(); }, nullptr);
        UpdateBufferingState();
    }
}

// Called inside GAME thread
void AvPlayerState::AddSourceEvent() {
    SetState(AvState::AddingSource);
    if (!m_event_queue.Push(AvPlayerEvent{
            .event = AvEventType::AddSource,
        })) {
        LOG_ERROR(Lib_AvPlayer, "The event queue is full: an event is dropped");
    }
    m_event_ready.Notify();
}

void AvPlayerState::WarningEvent(s32 id) {
    if (!m_event_queue.Push(AvPlayerEvent{
            .event = AvEventType::WarningId,
            .payload =
                {
                    .error = id,
                },
        })) {
        LOG_ERROR(Lib_AvPlayer, "The event queue is full: an event is dropped");
    }
    m_event_ready.Notify();
}

// Called inside GAME thread
void AvPlayerState::StartControllerThread() {
    m_controller_thread.Run([this] { this->AvControllerThread(); },
                            [this] { m_event_ready.Notify(); });
}

// Called inside GAME thread
bool AvPlayerState::EnableStream(u32 stream_index) {
    AvPlayerSource* const source = PinSource();
    if (source == nullptr) {
        return false;
    }
    const bool enabled = source->EnableStream(stream_index);
    UnpinSource();
    return enabled;
}

// Called inside GAME thread
bool AvPlayerState::GetVideoData(AvPlayerFrameInfo& video_info) {
    AvPlayerSource* const source = PinSource();
    if (source == nullptr) {
        return false;
    }
    const bool got = source->GetVideoData(video_info);
    UnpinSource();
    return got;
}

bool AvPlayerState::GetVideoData(AvPlayerFrameInfoEx& video_info) {
    AvPlayerSource* const source = PinSource();
    if (source == nullptr) {
        return false;
    }
    const bool got = source->GetVideoData(video_info);
    UnpinSource();
    return got;
}

bool AvPlayerState::GetAudioData(AvPlayerFrameInfo& audio_info) {
    AvPlayerSource* const source = PinSource();
    if (source == nullptr) {
        return false;
    }
    const bool got = source->GetAudioData(audio_info);
    UnpinSource();
    return got;
}

bool AvPlayerState::IsActive() {
    // Kept out while it starts: about to be active
    if (!pin_gate_enter(&m_source_gate)) {
        return State() == AvState::Starting;
    }
    auto* const source = static_cast<AvPlayerSource*>(retro_atomic_load_acquire_ptr(&m_source));
    const AvState state = State();
    const bool active = source != nullptr && state != AvState::Stop && state != AvState::Error &&
                        source->IsActive();
    pin_gate_leave(&m_source_gate);
    return active;
}

u64 AvPlayerState::CurrentTime() {
    AvPlayerSource* const source = PinSource();
    if (source == nullptr) {
        return 0;
    }
    const u64 time = source->CurrentTime();
    UnpinSource();
    return time;
}

bool AvPlayerState::SetLooping(bool is_looping) {
    AvPlayerSource* const source = PinSource();
    if (source == nullptr) {
        return false;
    }
    source->SetLooping(is_looping);
    UnpinSource();
    return true;
}

// May be called from different threads
void AvPlayerState::OnWarning(u32 id) {
    // Forward to CONTROLLER thread
    WarningEvent(id);
}

AvPlayerAvSyncMode AvPlayerState::GetSyncMode() {
    return m_sync_mode;
}

void AvPlayerState::OnError() {
    SetState(AvState::Error);
    OnPlaybackStateChanged(AvState::Error);
}

void AvPlayerState::OnEOF() {
    SetState(AvState::EndOfFile);
}

void AvPlayerState::UpdateEndOfFileState() {
    // Kept out (being changed) counts as still active
    AvPlayerSource* const source = PinSource();
    const bool active = source == nullptr || source->IsActive();
    if (source != nullptr) {
        UnpinSource();
    }
    if (active || (retro_atomic_exchange_int(&m_eof_stop_event_sent, 1) != 0)) {
        return;
    }

    EmitEvent(AvPlayerEvents::StateStop);
}

// Called inside CONTROLLER thread
void AvPlayerState::OnPlaybackStateChanged(AvState state) {
    switch (state) {
    case AvState::Ready: {
        EmitEvent(AvPlayerEvents::StateReady);
        break;
    }
    case AvState::Play: {
        EmitEvent(AvPlayerEvents::StatePlay);
        break;
    }
    case AvState::Stop: {
        EmitEvent(AvPlayerEvents::StateStop);
        break;
    }
    case AvState::Pause: {
        EmitEvent(AvPlayerEvents::StatePause);
        break;
    }
    case AvState::Buffering: {
        EmitEvent(AvPlayerEvents::StateBuffering);
        break;
    }
    default:
        break;
    }
}

// Called inside CONTROLLER and GAME threads
bool AvPlayerState::SetState(AvState state) {
    int from = 0;
    if (!avplayer_fsm_set(&m_fsm, static_cast<int>(state), &from)) {
        LOG_ERROR(Lib_AvPlayer, "Invalid state transition: %s -> %s",
                  enum_name(&enum_names_Libraries_AvPlayer_AvState, static_cast<s64>(from)),
                  enum_name(&enum_names_Libraries_AvPlayer_AvState, static_cast<s64>(state)));
        return false;
    }
    return true;
}

// Called inside CONTROLLER thread
std::optional<bool> AvPlayerState::OnBufferingCheckEvent(u32 num_frames) {
    AvPlayerSource* const source = PinSource();
    if (source == nullptr) {
        return std::nullopt;
    }
    const std::optional<bool> has = source->HasFrames(num_frames);
    UnpinSource();
    return has;
}

// Called inside CONTROLLER thread
void AvPlayerState::EmitEvent(AvPlayerEvents event_id, void* event_data) {
    LOG_INFO(Lib_AvPlayer, "Sending event to the game: id = %s",
             enum_name(&enum_names_Libraries_AvPlayer_AvPlayerEvents, static_cast<s64>(event_id)));
    const auto callback = m_init_data.event_replacement.event_callback;
    if (callback) {
        const auto ptr = m_init_data.event_replacement.object_ptr;
        callback(ptr, event_id, 0, event_data);
    }
}

// Called inside CONTROLLER thread
void AvPlayerState::ProcessEvent() {
    if (State() == AvState::Jump) {
        return;
    }

    auto event = m_event_queue.Pop();
    if (!event.has_value()) {
        return;
    }
    switch (event->event) {
    case AvEventType::WarningId: {
        EmitEvent(AvPlayerEvents::WarningId, &event->payload.error);
        break;
    }
    case AvEventType::RevertState: {
        (void)avplayer_fsm_revert(&m_fsm, nullptr, nullptr);
        break;
    }
    case AvEventType::AddSource: {
        // AddSource closed the gate on it: its streams are found with no reader in, then
        // readers are let in
        auto* const source = static_cast<AvPlayerSource*>(retro_atomic_load_acquire_ptr(&m_source));
        const bool found = source != nullptr && source->FindStreams();
        pin_gate_open(&m_source_gate);
        if (found) {
            SetState(AvState::Ready);
            OnPlaybackStateChanged(AvState::Ready);
        } else {
            OnWarning(ORBIS_AVPLAYER_ERROR_NOT_SUPPORTED);
            SetState(AvState::Error);
        }
        break;
    }
    case AvEventType::Error: {
        OnWarning(event->payload.error);
        SetState(AvState::Error);
        break;
    }
    default:
        break;
    }
}

// Called inside CONTROLLER thread
void AvPlayerState::UpdateBufferingState() {
    if (State() == AvState::EndOfFile) {
        UpdateEndOfFileState();
    } else if (State() == AvState::Buffering) {
        const auto has_frames = OnBufferingCheckEvent(10);
        if (!has_frames.has_value()) {
            return;
        }
        if (has_frames.value()) {
            const auto state = PreviousState() >= AvState::C0x0B ? PreviousState() : AvState::Play;
            SetState(state);
            OnPlaybackStateChanged(state);
        }
    } else if (State() == AvState::Play) {
        const auto has_frames = OnBufferingCheckEvent(0);
        if (!has_frames.has_value()) {
            return;
        }
        if (!has_frames.value()) {
            SetState(AvState::Buffering);
            OnPlaybackStateChanged(AvState::Buffering);
        }
    }
}

} // namespace Libraries::AvPlayer
