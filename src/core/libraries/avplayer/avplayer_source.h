// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string_view>
#include <utility>

#include "common/assert.h"
#include "core/libraries/avplayer/avplayer.h"
#include "core/libraries/avplayer/avplayer_common.h"
#include "core/libraries/avplayer/avplayer_data_streamer.h"
#include "core/libraries/kernel/threads.h"

#include "core/libraries/videodec/video_utils.h"

struct RFILE;
typedef struct rmp4 rmp4_t;
typedef struct raac raac_t;

namespace Libraries::AvPlayer {

class AvPlayerStateCallback {
public:
    virtual ~AvPlayerStateCallback() = default;

    virtual AvPlayerAvSyncMode GetSyncMode() = 0;

    virtual void OnWarning(u32 id) = 0;
    virtual void OnError() = 0;
    virtual void OnEOF() = 0;
};

class GuestBuffer {
public:
    GuestBuffer(const AvPlayerMemAllocator& memory_replacement, u32 align, u32 size,
                bool is_texture) noexcept
        : m_memory_replacement(memory_replacement),
          m_data(is_texture ? AllocateTexture(memory_replacement, align, size)
                            : Allocate(memory_replacement, align, size)),
          m_size(size), m_is_texture(is_texture) {
        ASSERT_MSG(m_data, "Could not allocate frame buffer.");
    }

    ~GuestBuffer() {
        if (m_data != nullptr) {
            if (m_is_texture) {
                DeallocateTexture(m_memory_replacement, m_data);
            } else {
                Deallocate(m_memory_replacement, m_data);
            }
            m_data = {};
        }
    }

    GuestBuffer(const GuestBuffer&) noexcept = delete;
    GuestBuffer& operator=(const GuestBuffer&) noexcept = delete;

    GuestBuffer(GuestBuffer&& r) noexcept
        : m_memory_replacement(r.m_memory_replacement), m_data(r.m_data), m_size(r.m_size),
          m_is_texture(r.m_is_texture) {
        r.m_data = nullptr;
    };

    GuestBuffer& operator=(GuestBuffer&& r) noexcept {
        std::swap(m_data, r.m_data);
        return *this;
    }

    u8* GetBuffer() const noexcept {
        return m_data;
    }

    u64 Size() const noexcept {
        return m_size;
    }

private:
    static u8* Allocate(const AvPlayerMemAllocator& memory_replacement, u32 align, u32 size) {
        return reinterpret_cast<u8*>(
            memory_replacement.allocate(memory_replacement.object_ptr, align, size));
    }

    static void Deallocate(const AvPlayerMemAllocator& memory_replacement, void* ptr) {
        memory_replacement.deallocate(memory_replacement.object_ptr, ptr);
    }

    static u8* AllocateTexture(const AvPlayerMemAllocator& memory_replacement, u32 align,
                               u32 size) {
        return reinterpret_cast<u8*>(
            memory_replacement.allocate_texture(memory_replacement.object_ptr, align, size));
    }

    static void DeallocateTexture(const AvPlayerMemAllocator& memory_replacement, void* ptr) {
        memory_replacement.deallocate_texture(memory_replacement.object_ptr, ptr);
    }

    const AvPlayerMemAllocator& m_memory_replacement;
    u8* m_data = nullptr;
    u64 m_size = 0;
    bool m_is_texture = false;
};

struct Frame {
    GuestBuffer buffer;
    AvPlayerFrameInfoEx info;
};

class EventCV {
public:
    template <class Pred>
    void Wait(Pred pred) {
        std::unique_lock lock(m_mutex);
        m_cv.wait(lock, std::move(pred));
    }

    template <class Pred>
    bool Wait(std::stop_token stop, Pred pred) {
        std::unique_lock lock(m_mutex);
        return m_cv.wait(lock, std::move(stop), std::move(pred));
    }

    template <class Pred, class Rep, class Period>
    bool WaitFor(std::chrono::duration<Rep, Period> timeout, Pred pred) {
        std::unique_lock lock(m_mutex);
        return m_cv.wait_for(lock, timeout, std::move(pred));
    }

    void Notify() {
        std::unique_lock lock(m_mutex);
        m_cv.notify_all();
    }

private:
    std::mutex m_mutex{};
    std::condition_variable_any m_cv{};
};

class AvPlayerSource {
public:
    AvPlayerSource(AvPlayerStateCallback& state);
    ~AvPlayerSource();

    bool Init(const AvPlayerInitData& init_data, std::string_view path);
    bool FindStreams();
    s32 GetStreamCount();
    bool GetStreamInfo(u32 stream_index, AvPlayerStreamInfo& info);
    bool EnableStream(u32 stream_index);
    void SetLooping(bool is_looping);
    std::optional<bool> HasFrames(u32 num_frames);
    bool Start();
    bool Stop();
    void Pause();
    void Resume();
    bool GetAudioData(AvPlayerFrameInfo& audio_info);
    bool GetVideoData(AvPlayerFrameInfo& video_info);
    bool GetVideoData(AvPlayerFrameInfoEx& video_info);
    u64 CurrentTime();
    bool IsActive();

private:
    // An MP4 sample, read in place from the file's bytes.
    struct Packet {
        const u8* data = nullptr;
        size_t size = 0;
        u64 timestamp = 0; // milliseconds
    };

    u64 DurationMillis() const;
    AvPlayerStreamInfo CreateStreamInfo(u32 stream_index);
    bool OpenDemuxer();
    // Makes the bytes below hi of a file that is not mapped readable.
    bool Load(size_t hi);

    void DemuxerThread(std::stop_token stop);
    void VideoDecoderThread(std::stop_token stop);
    void AudioDecoderThread(std::stop_token stop);

    bool HasRunningThreads() const;

    AvPlayerStateCallback& m_state;

    struct Stream {
        int track = 0; // rmp4's track index
        AvPlayerStreamInfo info;
    };

    std::vector<Stream> m_streams;
    u64 m_duration = 0;

    AvPlayerMemAllocator m_memory_replacement{};
    u32 m_max_num_video_framebuffers{};

    std::atomic_bool m_is_looping = false;
    std::atomic_bool m_is_paused = false;
    std::atomic_bool m_is_eof = false;

    std::unique_ptr<IDataStreamer> m_up_data_streamer;

    AvPlayerQueue<GuestBuffer> m_audio_buffers;
    AvPlayerQueue<GuestBuffer> m_video_buffers;

    AvPlayerQueue<Packet> m_audio_packets;
    AvPlayerQueue<Packet> m_video_packets;

    AvPlayerQueue<Frame> m_audio_frames;
    AvPlayerQueue<Frame> m_video_frames;

    std::optional<Frame> m_current_video_frame;
    std::optional<Frame> m_current_audio_frame;

    std::optional<s32> m_video_stream_index{};
    std::optional<s32> m_audio_stream_index{};

    EventCV m_audio_packets_cv{};
    EventCV m_audio_frames_cv{};
    EventCV m_audio_buffers_cv{};

    EventCV m_video_packets_cv{};
    EventCV m_video_frames_cv{};
    EventCV m_video_buffers_cv{};

    std::mutex m_state_mutex{};
    Kernel::Thread m_demuxer_thread{};
    Kernel::Thread m_video_decoder_thread{};
    Kernel::Thread m_audio_decoder_thread{};

    // The file's bytes: mapped, or for a file read through callbacks, a buffer of its size
    // filled as the demuxer reaches each sample.
    RFILE* m_file = nullptr;
    const u8* m_data = nullptr;
    u8* m_owned = nullptr;
    size_t m_size = 0;
    size_t m_loaded_hi = 0; // the file is read in order: what lies below is in place
    bool m_fully_loaded = false;
    rmp4_t* m_demuxer = nullptr;

    std::unique_ptr<Videodec::SwDecoder> m_video_decoder;
    raac_t* m_audio_decoder = nullptr;
    u64 m_audio_skip = 0; // samples a channel the edit list trims, still to drop

    std::optional<u64> m_last_audio_ts{};
    std::optional<std::chrono::high_resolution_clock::time_point> m_last_data_time{};
    std::optional<std::chrono::high_resolution_clock::time_point> m_start_time{};
    std::chrono::high_resolution_clock::time_point m_pause_time{};
    std::chrono::high_resolution_clock::duration m_pause_duration{};
};

} // namespace Libraries::AvPlayer
