// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdlib>
#include <cstring>

#include <formats/raac.h>
#include <formats/rmp4.h>
#include <streams/file_stream.h>

#include "common/alignment.h"
#include "common/singleton.h"
#include "common/thread.h"
#include "core/file_sys/fs.h"
#include "core/libraries/avplayer/avplayer_error.h"
#include "core/libraries/avplayer/avplayer_file_streamer.h"
#include "core/libraries/avplayer/avplayer_handle_streamer.h"
#include "core/libraries/avplayer/avplayer_source.h"
#include "core/memory.h"

namespace Libraries::AvPlayer {

namespace {
// Reads a host file through libretro-common's VFS, when it could not be mapped.
class VfsStreamer final : public IDataStreamer {
public:
    explicit VfsStreamer(RFILE* file) : m_file(file) {}
    bool Init(std::string_view) override {
        return m_file != nullptr;
    }
    u64 Size() const override {
        const s64 size = filestream_get_size(m_file);
        return size > 0 ? static_cast<u64>(size) : 0;
    }
    u64 ReadAt(u64 offset, u8* dst, u64 size) override {
        if (filestream_seek(m_file, static_cast<s64>(offset), RETRO_VFS_SEEK_POSITION_START) != 0) {
            return 0;
        }
        const s64 got = filestream_read(m_file, dst, size);
        return got > 0 ? static_cast<u64>(got) : 0;
    }

private:
    RFILE* m_file;
};

constexpr size_t FirstRead = 64 * 1024;
constexpr size_t ReadStep = 1024 * 1024;
constexpr u32 MaxAudioChannels = 8;
constexpr u32 MaxAudioFrame = 2048; // HE-AAC
// Samples a decoder may fall behind the demuxer; well past what it reads ahead.
constexpr size_t MaxQueuedPackets = 4096;

bool IsTrackSupported(const rmp4_track& track) {
    return track.codec == RMP4_CODEC_H264 || track.codec == RMP4_CODEC_H265 ||
           track.codec == RMP4_CODEC_AAC;
}

u64 NanosToMillis(s64 ns) {
    return ns > 0 ? static_cast<u64>(ns / 1000000) : 0;
}
} // namespace

AvPlayerSource::AvPlayerSource(AvPlayerStateCallback& state) : m_state(state) {}

AvPlayerSource::~AvPlayerSource() {
    Stop();
    if (m_audio_decoder) {
        raac_close(m_audio_decoder);
    }
    if (m_demuxer) {
        rmp4_close(m_demuxer);
    }
    std::free(m_owned);
    if (m_file) {
        filestream_close(m_file);
    }
}

bool AvPlayerSource::Init(const AvPlayerInitData& init_data, std::string_view path) {
    m_memory_replacement = init_data.memory_replacement;
    m_max_num_video_framebuffers =
        std::min(std::max(2, init_data.num_output_video_framebuffers), 16);

    if (init_data.file_replacement.open != nullptr) {
        LOG_INFO(Lib_AvPlayer, "Opening {} through the game-provided file replacement", path);
        m_up_data_streamer = std::make_unique<AvPlayerFileStreamer>(init_data.file_replacement);
    } else {
        const auto mnt = Common::Singleton<Core::FileSys::MntPoints>::Instance();
        std::filesystem::path host_path;
        if (auto handle = mnt->Open(path, /*writable=*/false)) {
            if (auto host = handle->GetHostPath(); host.has_value()) {
                host_path = *host;
            } else {
                LOG_INFO(Lib_AvPlayer, "Opening {} through a non-host backend handle", path);
                m_up_data_streamer = std::make_unique<AvPlayerHandleStreamer>(std::move(handle));
            }
        } else {
            host_path = mnt->GetHostPath(path);
        }
        if (!m_up_data_streamer) {
            LOG_INFO(Lib_AvPlayer, "Opening {} from host path {}", path, host_path.string());
            // Mapped, the samples are read in place
            const auto u8path = host_path.u8string();
            m_file = filestream_open(reinterpret_cast<const char*>(u8path.c_str()),
                                     RETRO_VFS_FILE_ACCESS_READ,
                                     RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS);
            if (!m_file) {
                LOG_ERROR(Lib_AvPlayer, "Could not open {}", host_path.string());
                return false;
            }
            s64 mapped = 0;
            m_data = filestream_get_mapped_ptr(m_file, &mapped);
            if (m_data && mapped > 0) {
                m_size = static_cast<size_t>(mapped);
                m_loaded_hi = m_size;
                m_fully_loaded = true;
            } else {
                m_data = nullptr;
                m_up_data_streamer = std::make_unique<VfsStreamer>(m_file);
            }
        }
    }
    if (m_up_data_streamer) {
        if (!m_up_data_streamer->Init(path)) {
            return false;
        }
        m_size = static_cast<size_t>(m_up_data_streamer->Size());
        // One buffer of the file's size, filled as the demuxer reaches each part: samples are
        // read into place and handed to the decoders from there
        m_owned = static_cast<u8*>(std::malloc(std::max<size_t>(m_size, 1)));
        if (!m_owned) {
            LOG_ERROR(Lib_AvPlayer, "Could not allocate {} bytes for {}", m_size, path);
            return false;
        }
        m_data = m_owned;
    }
    return OpenDemuxer();
}

bool AvPlayerSource::Load(size_t hi) {
    // The demuxer goes in file order: everything up to hi is read, from where the last read
    // stopped, so no byte is read twice
    hi = std::min(hi, m_size);
    if (m_fully_loaded || hi <= m_loaded_hi) {
        return true;
    }
    const size_t lo = m_loaded_hi;
    if (m_up_data_streamer->ReadAt(lo, m_owned + lo, hi - lo) != hi - lo) {
        LOG_ERROR(Lib_AvPlayer, "Could not read bytes {}..{} of the source", lo, hi);
        return false;
    }
    m_loaded_hi = hi;
    return true;
}

bool AvPlayerSource::OpenDemuxer() {
    if (m_fully_loaded) {
        m_demuxer = rmp4_open_memory(m_data, m_size);
    } else {
        // The header first: the moov is read where it lies, the media only as it is reached
        size_t avail = std::min(m_size, FirstRead);
        if (!Load(avail)) {
            return false;
        }
        for (;;) {
            int need_more = 0;
            size_t lo = 0;
            size_t hi = 0;
            m_demuxer = rmp4_open_memory_avail(m_data, m_size, avail, &need_more, &lo, &hi);
            if (m_demuxer || !need_more || avail >= m_size) {
                break;
            }
            if (hi > lo && hi <= m_size) {
                const size_t keep = m_loaded_hi;
                if (m_up_data_streamer->ReadAt(lo, m_owned + lo, hi - lo) != hi - lo) {
                    return false;
                }
                m_loaded_hi = lo <= keep ? std::max(keep, hi) : keep;
                avail = std::max(avail, hi);
            } else {
                const size_t next = std::min(m_size, avail + ReadStep);
                if (!Load(next)) {
                    return false;
                }
                avail = next;
            }
        }
        if (m_demuxer) {
            // Every sample is loaded before it is read: see DemuxerThread
            rmp4_set_avail(m_demuxer, m_size);
        }
    }
    if (!m_demuxer) {
        LOG_ERROR(Lib_AvPlayer, "Not an MP4 file the demuxer can read");
        return false;
    }
    return true;
}

bool AvPlayerSource::FindStreams() {
    if (m_demuxer) {
        const int count = rmp4_num_tracks(m_demuxer);
        for (int track = 0; track < count; ++track) {
            const rmp4_track* t = rmp4_get_track(m_demuxer, track);
            if (t && IsTrackSupported(*t)) {
                m_streams.push_back({track, {}});
                m_streams.back().info = CreateStreamInfo(static_cast<u32>(m_streams.size() - 1));
            }
        }
    }
    m_duration = DurationMillis();
    return m_streams.size() > 0;
}

s32 AvPlayerSource::GetStreamCount() {
    LOG_INFO(Lib_AvPlayer, "Stream Count: {}", m_streams.size());
    return m_streams.size();
}

static u64 TrackDurationMillis(const rmp4_t* demuxer, const rmp4_track& track) {
    if (track.presentation_duration_ns > 0) {
        return NanosToMillis(track.presentation_duration_ns);
    }
    return NanosToMillis(rmp4_duration_ns(demuxer));
}

AvPlayerStreamInfo AvPlayerSource::CreateStreamInfo(u32 stream_index) {
    AvPlayerStreamInfo info = {};
    const int track_index = m_streams[stream_index].track;
    const rmp4_track& track = *rmp4_get_track(m_demuxer, track_index);
    info.type =
        track.type == RMP4_TRACK_VIDEO ? AvPlayerStreamType::Video : AvPlayerStreamType::Audio;
    info.start_time = 0; // start_time is currently unused and defaults to 0.
    info.duration = TrackDurationMillis(m_demuxer, track);

    size_t lang_len = 0;
    enum rmp4_language_kind kind = RMP4_LANGUAGE_NONE;
    const u8* lang = rmp4_get_track_language(m_demuxer, track_index, &lang_len, &kind);
    const size_t lang_copy = lang ? std::min(lang_len, size_t(3)) : 0;
    if (lang_copy) {
        LOG_INFO(Lib_AvPlayer, "Stream {} language = {}", stream_index,
                 std::string_view(reinterpret_cast<const char*>(lang), lang_len));
    } else {
        LOG_WARNING(Lib_AvPlayer, "Stream {} language is unknown", stream_index);
    }
    if (info.type == AvPlayerStreamType::Video) {
        LOG_INFO(Lib_AvPlayer, "Stream {} is a video stream.", stream_index);
        info.details.video.aspect_ratio =
            track.height ? f32(track.width) / f32(track.height) : 0.0f;
        info.details.video.width = Common::AlignUp<u32>(track.width, 16);
        info.details.video.height = Common::AlignUp<u32>(track.height, 16);
        std::memcpy(info.details.video.language_code, lang, lang_copy);
    } else {
        LOG_INFO(Lib_AvPlayer, "Stream {} is an audio stream.", stream_index);
        info.details.audio.channel_count = static_cast<u16>(track.channels);
        info.details.audio.sample_rate = track.sample_rate;
        info.details.audio.size = 0; // sceAvPlayerGetStreamInfo() is expected to set this to 0
        std::memcpy(info.details.audio.language_code, lang, lang_copy);
    }
    return info;
}

bool AvPlayerSource::GetStreamInfo(u32 stream_index, AvPlayerStreamInfo& info) {
    if (stream_index >= m_streams.size()) {
        LOG_ERROR(Lib_AvPlayer, "Could not get stream {} info.", stream_index);
        return false;
    }
    info = m_streams[stream_index].info;
    return true;
}

bool AvPlayerSource::EnableStream(u32 stream_index) {
    if (m_demuxer == nullptr || stream_index >= m_streams.size()) {
        return false;
    }
    if (m_streams[stream_index].info.type == AvPlayerStreamType::Video) {
        m_video_stream_index = stream_index;
        LOG_INFO(Lib_AvPlayer, "Video stream {} enabled", stream_index);
    } else {
        m_audio_stream_index = stream_index;
        LOG_INFO(Lib_AvPlayer, "Audio stream {} enabled", stream_index);
    }
    return true;
}

void AvPlayerSource::SetLooping(bool is_looping) {
    m_is_looping = is_looping;
}

std::optional<bool> AvPlayerSource::HasFrames(u32 num_frames) {
    return m_video_packets.Size() > num_frames || m_is_eof;
}

bool AvPlayerSource::Start() {
    std::unique_lock lock(m_state_mutex);

    if (!m_video_stream_index && !m_audio_stream_index) {
        LOG_ERROR(Lib_AvPlayer, "Could not start playback. No streams.");
        return false;
    }
    rmp4_rewind(m_demuxer);
    if (m_video_stream_index) {
        const rmp4_track& track =
            *rmp4_get_track(m_demuxer, m_streams[m_video_stream_index.value()].track);
        m_video_decoder = std::make_unique<Videodec::SwDecoder>(track.codec == RMP4_CODEC_H265);
        if (!m_video_decoder->IsOpen() ||
            !m_video_decoder->SetExtradata(track.codec_private, track.codec_private_size)) {
            LOG_ERROR(Lib_AvPlayer, "Could not open the decoder for video stream {}.",
                      m_video_stream_index.value());
            return false;
        }
        const auto pitch = Common::AlignUp<u32>(track.width, 64);
        const auto height = Common::AlignUp<u32>(track.height, 16);
        const auto size = (pitch * height * 3) / 2;
        m_video_pool.clear();
        m_video_pool.reserve(m_max_num_video_framebuffers);
        for (u32 index = 0; index < m_max_num_video_framebuffers; ++index) {
            m_video_pool.emplace_back(m_memory_replacement, 0x100, size, true);
        }
    }
    if (m_audio_stream_index) {
        const rmp4_track& track =
            *rmp4_get_track(m_demuxer, m_streams[m_audio_stream_index.value()].track);
        if (m_audio_decoder) {
            raac_close(m_audio_decoder);
        }
        m_audio_decoder = raac_open(track.codec_private, track.codec_private_size);
        if (!m_audio_decoder) {
            LOG_ERROR(Lib_AvPlayer, "Could not open the decoder for audio stream {}.",
                      m_audio_stream_index.value());
            return false;
        }
        raac_set_channel_order(m_audio_decoder, RAAC_ORDER_WAVE);
        m_audio_skip = track.media_skip;
        // A frame of HE-AAC at the most channels, as 16-bit samples
        const auto size = MaxAudioChannels * sizeof(s16) * MaxAudioFrame;
        m_audio_pool.clear();
        m_audio_pool.reserve(m_max_num_video_framebuffers * 2);
        for (u32 index = 0; index < m_max_num_video_framebuffers * 2; ++index) {
            m_audio_pool.emplace_back(m_memory_replacement, 0x10, size, false);
        }
    }
    // Fresh queues: the last playback's were closed. A frame queue holds every buffer, so a
    // decoder never waits on it.
    if (!m_video_buffers.Init(m_video_pool.size()) || !m_video_frames.Init(m_video_pool.size()) ||
        !m_audio_buffers.Init(m_audio_pool.size()) || !m_audio_frames.Init(m_audio_pool.size()) ||
        !m_video_packets.Init(MaxQueuedPackets) || !m_audio_packets.Init(MaxQueuedPackets)) {
        LOG_ERROR(Lib_AvPlayer, "Could not allocate the playback queues.");
        return false;
    }
    for (u32 index = 0; index < m_video_pool.size(); ++index) {
        m_video_buffers.Push(index);
    }
    for (u32 index = 0; index < m_audio_pool.size(); ++index) {
        m_audio_buffers.Push(index);
    }
    m_demuxer_thread.Run([this](std::stop_token stop) { this->DemuxerThread(stop); });
    m_video_decoder_thread.Run([this](std::stop_token stop) { this->VideoDecoderThread(stop); });
    m_audio_decoder_thread.Run([this](std::stop_token stop) { this->AudioDecoderThread(stop); });
    m_start_time = std::chrono::high_resolution_clock::now();
    return true;
}

void AvPlayerSource::CloseQueues() {
    m_video_buffers.Close();
    m_audio_buffers.Close();
    m_audio_packets.Close();
    m_video_packets.Close();
    m_audio_frames.Close();
    m_video_frames.Close();
}

bool AvPlayerSource::Stop() {
    std::unique_lock lock(m_state_mutex);

    if (!HasRunningThreads()) {
        LOG_WARNING(Lib_AvPlayer, "Could not stop playback: already stopped.");
        return false;
    }

    // Closed queues wake every thread waiting on one
    CloseQueues();
    m_video_decoder_thread.Stop();
    m_audio_decoder_thread.Stop();
    m_demuxer_thread.Stop();

    m_current_audio_frame.reset();
    m_current_video_frame.reset();

    // The memory stays until the next Start, as other threads may still ask for a size
    m_video_buffers.Drain();
    m_audio_buffers.Drain();
    m_audio_packets.Drain();
    m_video_packets.Drain();
    m_audio_frames.Drain();
    m_video_frames.Drain();
    m_video_pool.clear();
    m_audio_pool.clear();

    m_last_audio_ts.reset();
    m_last_data_time.reset();
    m_start_time.reset();
    m_pause_time = {};
    m_pause_duration = {};

    m_is_paused = false;
    m_is_eof = false;

    return true;
}

void AvPlayerSource::Pause() {
    m_pause_time = std::chrono::high_resolution_clock::now();
    m_is_paused = true;
}

void AvPlayerSource::Resume() {
    m_pause_duration += std::chrono::high_resolution_clock::now() - m_pause_time;
    m_is_paused = false;
}

bool AvPlayerSource::GetVideoData(AvPlayerFrameInfo& video_info) {
    AvPlayerFrameInfoEx info{};
    if (!GetVideoData(info)) {
        return false;
    }
    video_info = {};
    video_info.timestamp = u64(info.timestamp);
    video_info.p_data = reinterpret_cast<u8*>(info.p_data);
    video_info.details.video.aspect_ratio = info.details.video.aspect_ratio;
    video_info.details.video.width = info.details.video.width;
    video_info.details.video.height = info.details.video.height;
    return true;
}

bool AvPlayerSource::GetVideoData(AvPlayerFrameInfoEx& video_info) {
    if (!IsActive() || m_is_paused) {
        return false;
    }

    Frame frame;
    if (!m_video_frames.Peek(frame)) {
        return false;
    }
    if (m_state.GetSyncMode() == AvPlayerAvSyncMode::Default &&
        frame.info.timestamp > CurrentTime()) {
        return false;
    }

    // The frame handed out last time is done with
    if (m_current_video_frame.has_value()) {
        m_video_buffers.Push(m_current_video_frame->buffer);
        m_current_video_frame.reset();
    }

    m_video_frames.TryPop(frame);
    video_info = frame.info;
    m_current_video_frame = frame;
    return true;
}

bool AvPlayerSource::GetAudioData(AvPlayerFrameInfo& audio_info) {
    if (!IsActive() || m_is_paused) {
        return false;
    }

    Frame frame;
    if (!m_audio_frames.TryPop(frame)) {
        return false;
    }

    if (m_current_audio_frame.has_value()) {
        m_audio_buffers.Push(m_current_audio_frame->buffer);
        m_current_audio_frame.reset();
    }

    m_last_audio_ts = frame.info.timestamp;
    m_last_data_time = std::chrono::high_resolution_clock::now();
    m_pause_duration = std::chrono::high_resolution_clock::duration(0);

    audio_info = {};
    audio_info.timestamp = frame.info.timestamp;
    audio_info.p_data = reinterpret_cast<u8*>(frame.info.p_data);
    audio_info.details.audio.sample_rate = frame.info.details.audio.sample_rate;
    audio_info.details.audio.size = frame.info.details.audio.size;
    audio_info.details.audio.channel_count = frame.info.details.audio.channel_count;
    m_current_audio_frame = frame;
    return true;
}

u64 AvPlayerSource::DurationMillis() const {
    if (m_demuxer == nullptr) {
        return 0;
    }
    u64 duration = 0;
    for (const auto& stream : m_streams) {
        duration = std::max(duration, stream.info.duration);
    }
    return duration ? duration : NanosToMillis(rmp4_duration_ns(m_demuxer));
}

u64 AvPlayerSource::CurrentTime() {
    if (!m_start_time.has_value()) {
        return 0;
    }

    if (m_is_eof && !IsActive()) {
        return m_duration;
    }
    if (!IsActive()) {
        return 0;
    }

    using namespace std::chrono;
    const auto now = m_is_paused.load() ? m_pause_time : high_resolution_clock::now();
    if (m_audio_stream_index) {
        const auto last_audio = m_last_data_time.value_or(now);
        const auto elapsed =
            duration_cast<milliseconds>(now - last_audio - m_pause_duration).count();
        return m_last_audio_ts.value_or(0) + elapsed;
    }

    const auto elapsed =
        duration_cast<milliseconds>(now - m_start_time.value() - m_pause_duration).count();
    if (elapsed <= 0) {
        return 0;
    }
    const auto current_time = u64(elapsed);
    return m_duration != 0 && current_time > m_duration ? m_duration : current_time;
}

bool AvPlayerSource::IsActive() {
    return !m_is_eof || m_audio_packets.Size() != 0 || m_video_packets.Size() != 0 ||
           m_video_frames.Size() != 0 || m_audio_frames.Size() != 0;
}

void AvPlayerSource::DemuxerThread(std::stop_token stop) {
    using namespace std::chrono;
    Common::SetCurrentThreadName("shadPS4:AvDemuxer");

    if (!m_audio_stream_index.has_value() && !m_video_stream_index.has_value()) {
        LOG_WARNING(Lib_AvPlayer, "Could not start DEMUXER thread. No streams enabled.");
        return;
    }
    LOG_INFO(Lib_AvPlayer, "Demuxer Thread started");

    const int video_track =
        m_video_stream_index ? m_streams[m_video_stream_index.value()].track : -1;
    const int audio_track =
        m_audio_stream_index ? m_streams[m_audio_stream_index.value()].track : -1;

    while (!stop.stop_requested()) {
        if (m_video_packets.Size() > 30 &&
            (!m_audio_stream_index.has_value() || m_audio_packets.Size() > 8)) {
            std::this_thread::sleep_for(milliseconds(5));
            continue;
        }
        // The next sample's bytes are put in place first, for a file that is not mapped
        size_t lo = 0;
        size_t hi = 0;
        rmp4_read_span(m_demuxer, &lo, &hi);
        if (hi > lo && !Load(hi)) {
            m_state.OnError();
            return;
        }
        rmp4_packet packet{};
        const int res = rmp4_read_packet(m_demuxer, &packet);
        if (res == 0) {
            if (m_is_looping) {
                LOG_INFO(Lib_AvPlayer, "EOF reached in demuxer. Looping the source...");
                m_state.OnWarning(ORBIS_AVPLAYER_ERROR_WAR_LOOPING_BACK);
                // Every sample has been read once: the bytes stay in place
                m_fully_loaded = true;
                rmp4_rewind(m_demuxer);
                continue;
            }
            LOG_INFO(Lib_AvPlayer, "EOF reached in demuxer. Exiting.");
            break;
        }
        if (res < 0 || res == RMP4_READ_AGAIN) {
            LOG_ERROR(Lib_AvPlayer, "Could not read a sample: error = {}", res);
            m_state.OnError();
            return;
        }
        const Packet out{packet.data, packet.size, NanosToMillis(packet.timestamp)};
        // A full queue waits for its decoder; a closed one means the player stopped
        if (packet.track == video_track) {
            if (!m_video_packets.Push(out)) {
                break;
            }
        } else if (packet.track == audio_track) {
            if (!m_audio_packets.Push(out)) {
                break;
            }
        }
    }

    m_is_eof = true;
    // The decoders take what is left, then find the queues closed
    m_video_packets.Close();
    m_audio_packets.Close();

    m_video_decoder_thread.Join();
    m_audio_decoder_thread.Join();
    m_state.OnEOF();

    LOG_INFO(Lib_AvPlayer, "Demuxer Thread exited normally");
    m_demuxer_thread.Join();
}

void AvPlayerSource::VideoDecoderThread(std::stop_token stop) {
    using namespace std::chrono;
    Common::SetCurrentThreadName("shadPS4:AvVideoDecoder");

    if (!m_video_decoder) {
        return; // no video stream enabled
    }
    LOG_INFO(Lib_AvPlayer, "Video Decoder Thread started");
    auto& decoder = *m_video_decoder;
    std::optional<u32> spare; // a buffer taken and not filled
    // Writes the ready picture straight into a guest buffer
    const auto output = [&]() -> bool {
        if (!spare.has_value()) {
            u32 index = 0;
            if (!m_video_buffers.Pop(index)) {
                return false; // the player stopped
            }
            spare = index;
        }
        const GuestBuffer& buffer = m_video_pool[*spare];
        u8* p_buffer = buffer.GetBuffer();
        if (!decoder.WriteNV12(p_buffer, buffer.Size())) {
            LOG_ERROR(Lib_AvPlayer, "The {}x{} picture does not fit the frame buffer",
                      decoder.Width(), decoder.Height());
            return true; // the buffer is kept for the next picture
        }
        const auto width = Common::AlignUp<u32>(decoder.Width(), 16);
        const auto pitch = decoder.Pitch();
        const auto height = decoder.AlignedHeight();
        Core::Memory::Instance()->InvalidateMemory(reinterpret_cast<VAddr>(p_buffer),
                                                   decoder.Nv12Size());
        const u32 index = *spare;
        spare.reset();
        return m_video_frames.Push(Frame{
            .buffer = index,
            .info =
                {
                    .p_data = p_buffer,
                    .timestamp = decoder.Meta().pts,
                    .details =
                        {
                            .video =
                                {
                                    .width = width,
                                    .height = height,
                                    .aspect_ratio = 1.0f, // square samples
                                    .crop_left_offset = 0,
                                    .crop_right_offset = pitch - decoder.Width(),
                                    .crop_top_offset = 0,
                                    .crop_bottom_offset = height - decoder.Height(),
                                    .pitch = pitch,
                                    .luma_bit_depth = 8,
                                    .chroma_bit_depth = 8,
                                },
                        },
                },
        });
    };

    Packet packet;
    while (!stop.stop_requested() && m_video_packets.Pop(packet)) {
        const Videodec::PictureMeta meta{.pts = packet.timestamp};
        const int res = decoder.Decode(packet.data, packet.size, meta);
        if (res < 0) {
            LOG_ERROR(Lib_AvPlayer, "Could not decode a video sample. Error = {}", res);
            m_state.OnError();
            return;
        }
        if (res && !output()) {
            break;
        }
    }
    // The pictures still held for display order
    while (!stop.stop_requested() && decoder.Drain()) {
        if (!output()) {
            break;
        }
    }

    LOG_INFO(Lib_AvPlayer, "Video Decoder Thread exited normally");
    m_video_decoder_thread.Join();
}

void AvPlayerSource::AudioDecoderThread(std::stop_token stop) {
    using namespace std::chrono;
    Common::SetCurrentThreadName("shadPS4:AvAudioDecoder");

    if (!m_audio_decoder || !m_audio_stream_index) {
        return; // no audio stream enabled
    }
    LOG_INFO(Lib_AvPlayer, "Audio Decoder Thread started");
    const u32 channels = raac_channels(m_audio_decoder);
    const u32 sample_rate = raac_sample_rate(m_audio_decoder);
    if (channels == 0 || channels > MaxAudioChannels ||
        raac_frame_len(m_audio_decoder) > MaxAudioFrame) {
        LOG_ERROR(Lib_AvPlayer, "Unsupported audio: {} channels, {} samples a frame", channels,
                  raac_frame_len(m_audio_decoder));
        m_state.OnError();
        return;
    }
    Packet packet;
    std::optional<u32> spare; // a buffer taken and not filled
    while (!stop.stop_requested() && m_audio_packets.Pop(packet)) {
        if (!spare.has_value()) {
            u32 index = 0;
            if (!m_audio_buffers.Pop(index)) {
                break; // the player stopped
            }
            spare = index;
        }
        // Decoded straight into the guest buffer
        auto* pcm = reinterpret_cast<s16*>(m_audio_pool[*spare].GetBuffer());
        const int samples = raac_decode_s16(m_audio_decoder, packet.data, packet.size, pcm);
        if (samples < 0) {
            LOG_ERROR(Lib_AvPlayer, "Could not decode an audio sample. Error = {}", samples);
            m_state.OnError();
            return;
        }
        // The encoder delay the edit list trims comes off the start
        const u32 skip = static_cast<u32>(std::min<u64>(m_audio_skip, u64(samples)));
        m_audio_skip -= skip;
        const u32 count = static_cast<u32>(samples) - skip;
        if (count == 0) {
            continue; // the buffer is kept for the next frame
        }
        u8* p_data = reinterpret_cast<u8*>(pcm + skip * channels);
        const u32 size = count * channels * sizeof(s16);
        const u32 index = *spare;
        spare.reset();
        if (!m_audio_frames.Push(Frame{
                .buffer = index,
                .info =
                    {
                        .p_data = p_data,
                        .timestamp = packet.timestamp,
                        .details =
                            {
                                .audio =
                                    {
                                        .channel_count = u16(channels),
                                        .sample_rate = sample_rate,
                                        .size = size,
                                    },
                            },
                    },
            })) {
            break;
        }
    }

    LOG_INFO(Lib_AvPlayer, "Audio Decoder Thread exited normally");
    m_audio_decoder_thread.Join();
}

bool AvPlayerSource::HasRunningThreads() const {
    return m_demuxer_thread.Joinable() || m_video_decoder_thread.Joinable() ||
           m_audio_decoder_thread.Joinable();
}

} // namespace Libraries::AvPlayer
