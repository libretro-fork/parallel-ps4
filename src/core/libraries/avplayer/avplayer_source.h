// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "common/assert.h"
#include "core/libraries/avplayer/avplayer.h"
#include "core/libraries/avplayer/avplayer_common.h"
#include "core/libraries/avplayer/avplayer_data_streamer.h"
#include "core/libraries/kernel/threads.h"

#include "core/libraries/videodec/video_utils.h"

#include <retro_waitable_spsc.h>

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

// A queue between two threads, on libretro-common's waitable SPSC ring: no lock, and a
// waiting end sleeps on an eventcount. Close() releases both ends for good; the records
// still in it can be taken.
template <class T>
class SpscRing {
    static_assert(std::is_trivially_copyable_v<T>);

public:
    SpscRing() = default;
    SpscRing(const SpscRing&) = delete;
    SpscRing& operator=(const SpscRing&) = delete;
    ~SpscRing() {
        Free();
    }

    // Not while another thread uses the ring.
    bool Init(size_t count) {
        Free();
        m_live = retro_waitable_spsc_init(&m_queue, std::max<size_t>(count, 1) * sizeof(T));
        return m_live;
    }

    void Free() {
        if (m_live) {
            retro_waitable_spsc_free(&m_queue);
            m_live = false;
        }
    }

    // The producer's: waits for room, false once closed.
    bool Push(const T& value) {
        if (!m_live) {
            return false;
        }
        while (retro_spsc_write_avail(&m_queue.queue) < sizeof(T)) {
            if (retro_waitable_spsc_cancelled(&m_queue)) {
                return false;
            }
            retro_waitable_spsc_wait_writable(&m_queue, sizeof(T), WaitBound);
        }
        retro_waitable_spsc_write(&m_queue, &value, sizeof(T));
        return true;
    }

    // The consumer's.
    bool TryPop(T& out) {
        if (Size() == 0) {
            return false;
        }
        retro_waitable_spsc_read(&m_queue, &out, sizeof(T));
        return true;
    }

    // The consumer's: waits for a record, false once closed and empty.
    bool Pop(T& out) {
        for (;;) {
            if (TryPop(out)) {
                return true;
            }
            if (!m_live || retro_waitable_spsc_cancelled(&m_queue)) {
                return false;
            }
            retro_waitable_spsc_wait_readable(&m_queue, sizeof(T), WaitBound);
        }
    }

    bool Peek(T& out) const {
        return Size() != 0 && retro_spsc_peek(&m_queue.queue, &out, sizeof(T)) == sizeof(T);
    }

    // Exact on the consumer's side, a hint elsewhere.
    size_t Size() const {
        return retro_spsc_read_avail(&m_queue.queue) / sizeof(T);
    }

    void Close() {
        if (m_live) {
            retro_waitable_spsc_cancel(&m_queue);
        }
    }

    // Once both ends have stopped.
    void Drain() {
        retro_spsc_skip(&m_queue.queue, retro_spsc_read_avail(&m_queue.queue));
    }

private:
    static constexpr int64_t WaitBound = 1000000; // a wake comes first; this only re-checks

    retro_waitable_spsc_t m_queue{};
    bool m_live = false;
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

    // A decoded frame in a guest buffer of the pool.
    struct Frame {
        u32 buffer = 0;
        AvPlayerFrameInfoEx info{};
    };

    void CloseQueues();

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

    // The guest buffers, made at Start; the queues carry their indices. Every queue has one
    // thread at each end: buffers go from the game to a decoder, packets from the demuxer to a
    // decoder, frames from a decoder to the game.
    std::vector<GuestBuffer> m_video_pool;
    std::vector<GuestBuffer> m_audio_pool;

    SpscRing<u32> m_audio_buffers;
    SpscRing<u32> m_video_buffers;

    SpscRing<Packet> m_audio_packets;
    SpscRing<Packet> m_video_packets;

    SpscRing<Frame> m_audio_frames;
    SpscRing<Frame> m_video_frames;

    std::optional<Frame> m_current_video_frame;
    std::optional<Frame> m_current_audio_frame;

    std::optional<s32> m_video_stream_index{};
    std::optional<s32> m_audio_stream_index{};

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
