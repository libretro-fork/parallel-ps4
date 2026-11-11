// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <queue>

#include "common/audio_file.h"
#include "common/combiner.h"
#include "common/event_mutex.h"
#include "common/io_file.h"
#include "common/logging/log.h"
#include "common/singleton.h"
#include "core/emulator_settings.h"
#include "core/file_sys/fs.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/libs.h"
#include "libretro/audio_mix.h"
#include "music_player_service.h"

namespace Libraries::MusicPlayerService {

class MusicPlayerManager {
public:
    static MusicPlayerManager& Instance() {
        static MusicPlayerManager s_instance;
        return s_instance;
    }

    void SetTrackList(const std::vector<std::string>& tracks) {
        m_mutex.Run([&] {
            m_tracks = tracks;
            m_tracklists[0] = tracks;
            m_tracklist_version++;
        });
    }

    void ClearTracks() {
        StopPlayback();
        m_mutex.Run([&] {
            m_tracks.clear();
            m_tracklists.clear();
            m_tracklist_version++;
        });
    }

    s32 SetUsbStorageDeviceInfo(const OrbisMusicPlayerUsbDeviceInfo* devices, u32 count) {
        if (!devices || count == 0 || count > 8) {
            LOG_ERROR(Lib_AudioOut, "invalid arguments: devices={}, count={}",
                      static_cast<const void*>(devices), count);
            return ORBIS_MUSIC_PLAYER_SERVICE_ERROR_INVALID_PARAMETER;
        }

        return m_mutex.Run([&]() -> s32 {
            m_usb_devices.clear();

            for (u32 i = 0; i < count; ++i) {
                const auto& dev = devices[i];
                if (dev.serial_number_len > 255) {
                    LOG_ERROR(Lib_AudioOut, "device[{}] serial_number_len {} > 255", i,
                              dev.serial_number_len);
                    return ORBIS_MUSIC_PLAYER_SERVICE_ERROR_INVALID_PARAMETER;
                }
                size_t str_len = strnlen(dev.serial_number, sizeof(dev.serial_number));
                if (str_len > dev.serial_number_len) {
                    LOG_ERROR(Lib_AudioOut, "device[{}] strlen {} > serial_number_len {}", i,
                              str_len, dev.serial_number_len);
                    return ORBIS_MUSIC_PLAYER_SERVICE_ERROR_INVALID_PARAMETER;
                }
                std::string serial(dev.serial_number, str_len);
                LOG_INFO(Lib_AudioOut, "registered USB device_id {} with serial '{}'",
                         dev.device_id, serial);
                m_usb_devices[serial] = dev.device_id;
            }

            return ORBIS_OK;
        });
    }

    s32 BeginTransaction() {
        return m_mutex.Run([&]() -> s32 {
            if (m_in_transaction.load()) {
                LOG_ERROR(Lib_AudioOut, "BeginTransaction: already in transaction");
                return ORBIS_MUSIC_PLAYER_SERVICE_ERROR_ALREADY_IN_TRANSACTION;
            }
            m_in_transaction = true;
            return ORBIS_OK;
        });
    }

    s32 EndTransaction() {
        return m_mutex.Run([&]() -> s32 {
            if (!m_in_transaction.load()) {
                LOG_ERROR(Lib_AudioOut, "EndTransaction: not in transaction");
                return ORBIS_MUSIC_PLAYER_SERVICE_ERROR_NOT_IN_TRANSACTION;
            }
            m_in_transaction = false;
            m_tracklist_version++;
            auto it = m_tracklists.find(0);
            if (it != m_tracklists.end()) {
                m_tracks = it->second;
            }
            return ORBIS_OK;
        });
    }

    s32 CreateTrackList(u32 tracklist_id) {
        if (tracklist_id > 99) {
            LOG_ERROR(Lib_AudioOut, "CreateTrackList: invalid tracklist_id={}", tracklist_id);
            return ORBIS_MUSIC_PLAYER_SERVICE_ERROR_INVALID_PARAMETER;
        }
        return m_mutex.Run([&]() -> s32 {
            if (!m_in_transaction.load()) {
                LOG_ERROR(Lib_AudioOut, "CreateTrackList: not in transaction");
                return ORBIS_MUSIC_PLAYER_SERVICE_ERROR_NOT_IN_TRANSACTION;
            }
            m_tracklists[tracklist_id].clear();
            return ORBIS_OK;
        });
    }

    s32 RemoveTrackList(u32 tracklist_id) {
        if (tracklist_id > 99) {
            LOG_ERROR(Lib_AudioOut, "RemoveTrackList: invalid tracklist_id={}", tracklist_id);
            return ORBIS_MUSIC_PLAYER_SERVICE_ERROR_INVALID_PARAMETER;
        }
        return m_mutex.Run([&]() -> s32 {
            if (!m_in_transaction.load()) {
                LOG_ERROR(Lib_AudioOut, "RemoveTrackList: not in transaction");
                return ORBIS_MUSIC_PLAYER_SERVICE_ERROR_NOT_IN_TRANSACTION;
            }
            m_tracklists.erase(tracklist_id);
            if (tracklist_id == 0) {
                m_tracks.clear();
            }
            return ORBIS_OK;
        });
    }

    s32 InsertTrackEntry(u32 tracklist_id, u32 track_index, const std::string& uri) {
        if (tracklist_id > 99 || track_index > 3999) {
            LOG_ERROR(Lib_AudioOut, "InsertTrackEntry: invalid tracklist_id={} or track_index={}",
                      tracklist_id, track_index);
            return ORBIS_MUSIC_PLAYER_SERVICE_ERROR_INVALID_PARAMETER;
        }
        return m_mutex.Run([&]() -> s32 {
            if (!m_in_transaction.load()) {
                LOG_ERROR(Lib_AudioOut, "InsertTrackEntry: not in transaction");
                return ORBIS_MUSIC_PLAYER_SERVICE_ERROR_NOT_IN_TRANSACTION;
            }
            auto& list = m_tracklists[tracklist_id];
            if (track_index < list.size()) {
                list.insert(list.begin() + track_index, uri);
            } else {
                list.push_back(uri);
            }
            return ORBIS_OK;
        });
    }

    s32 RemoveTrackEntry(u32 tracklist_id, u32 track_index) {
        if (tracklist_id > 99 || track_index > 3999) {
            LOG_ERROR(Lib_AudioOut, "RemoveTrackEntry: invalid tracklist_id={} or track_index={}",
                      tracklist_id, track_index);
            return ORBIS_MUSIC_PLAYER_SERVICE_ERROR_INVALID_PARAMETER;
        }
        return m_mutex.Run([&]() -> s32 {
            if (!m_in_transaction.load()) {
                LOG_ERROR(Lib_AudioOut, "RemoveTrackEntry: not in transaction");
                return ORBIS_MUSIC_PLAYER_SERVICE_ERROR_NOT_IN_TRANSACTION;
            }
            auto it = m_tracklists.find(tracklist_id);
            if (it != m_tracklists.end() && track_index < it->second.size()) {
                it->second.erase(it->second.begin() + track_index);
            }
            return ORBIS_OK;
        });
    }

    u64 GetTrackListVersion() const {
        return m_tracklist_version.load();
    }

    u32 GetTrackCount() {
        return m_mutex.Run([&]() -> u32 { return static_cast<u32>(m_tracks.size()); });
    }

    struct MusicEvent {
        u32 event_id;
        u32 track_index;
    };

    void PushEvent(u32 event_id, u32 track_index) {
        m_event_mutex.Run([&] {
            m_event_queue.push({event_id, track_index});
            m_event_cv.notify_one();
        });
    }

    bool PopEvent(MusicEvent& ev, u32 timeout_ms = 50) {
        std::unique_lock lock(m_event_mutex);
        if (m_event_cv.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                                [this] { return !m_event_queue.empty(); })) {
            ev = m_event_queue.front();
            m_event_queue.pop();
            return true;
        }
        return false;
    }

    void Play(u32 track_index, u64 start_time_ms) {
        m_control_mutex.Run([&] { PlayLocked(track_index, start_time_ms); });
    }

    void PlayLocked(u32 track_index, u64 start_time_ms) {
        StopPlaybackInternal();

        if (m_mutex.Run([&] {
                if (track_index >= m_tracks.size()) {
                    return true;
                }
                m_current_track_idx = track_index;
                return false;
            })) {
            return;
        }

        m_stop_requested = false;
        m_pause_requested = false;
        m_seek_offset_ms = -1;
        m_current_time_ms = start_time_ms;
        m_total_duration_ms = 0;
        m_play_state = 1;

        m_worker_thread =
            std::thread(&MusicPlayerManager::PlaybackWorker, this, track_index, start_time_ms);
    }

    void StopPlayback() {
        m_control_mutex.Run([&] { StopPlaybackInternal(); });
    }

    void StopPlaybackInternal() {
        m_stop_requested = true;
        m_pause_requested = false;
        m_play_state = 0;
        m_current_time_ms = 0;
        m_total_duration_ms = 0;

        if (m_worker_thread.joinable()) {
            m_worker_thread.join();
        }
    }

    void Pause() {
        m_pause_requested = true;
        m_play_state = 2;
    }

    void Unpause() {
        m_pause_requested = false;
        m_play_state = 1;
    }

    void Seek(u64 time_ms) {
        m_seek_offset_ms = static_cast<s64>(time_ms);
        m_current_time_ms = time_ms;
    }

    bool IsTrackActive(u32 track_index) {
        return (m_current_track_idx.load() == track_index) && (m_play_state.load() != 0) &&
               m_worker_thread.joinable() && !m_stop_requested.load();
    }

    void PlayOrSeek(u32 track_index, u64 start_time_ms) {
        m_control_mutex.Run([&] {
            if (IsTrackActive(track_index)) {
                if (start_time_ms > 0) {
                    Seek(start_time_ms);
                }
                Unpause();
            } else {
                PlayLocked(track_index, start_time_ms);
            }
        });
    }

    void SeekTrack(u32 track_index, u64 time_ms) {
        m_control_mutex.Run([&] {
            if (IsTrackActive(track_index)) {
                Seek(time_ms);
            } else {
                PlayLocked(track_index, time_ms);
            }
        });
    }

    void NextTrack() {
        m_control_mutex.Run([&] {
            u32 count = GetTrackCount();
            if (count > 0) {
                u32 cur = m_current_track_idx.load();
                u32 next_idx = (cur + 1) % count;
                if (m_shuffle_mode.load() == 1 && count > 1) {
                    for (int retry = 0; retry < 10 && next_idx == cur; ++retry) {
                        next_idx = static_cast<u32>(rand() % count);
                    }
                }
                PlayLocked(next_idx, 0);
            }
        });
    }

    void PreviousTrack() {
        m_control_mutex.Run([&] {
            u32 count = GetTrackCount();
            if (count > 0) {
                u32 cur = m_current_track_idx.load();
                u32 prev_idx = (cur > 0) ? (cur - 1) : 0;
                PlayLocked(prev_idx, 0);
            }
        });
    }

    void SetVolume(int vol) {
        m_volume = std::clamp(vol, 0, 25);
    }

    int GetVolume() const {
        return m_volume.load();
    }

    void SetRepeatMode(u32 mode) {
        m_repeat_mode = mode;
    }

    void SetShuffle(u32 mode) {
        m_shuffle_mode = mode;
    }

    u32 GetPlayState() const {
        return m_play_state.load();
    }

    u32 GetCurrentTrackIndex() const {
        return m_current_track_idx.load();
    }

    u64 GetCurrentTimeMs() const {
        return m_current_time_ms.load();
    }

    u64 GetTotalDurationMs() const {
        return m_total_duration_ms.load();
    }

    u32 GetRepeatMode() const {
        return m_repeat_mode.load();
    }

    u32 GetShuffleMode() const {
        return m_shuffle_mode.load();
    }

private:
    MusicPlayerManager() = default;
    ~MusicPlayerManager() {
        StopPlayback();
    }

    void PlaybackWorker(u32 start_track_index, u64 initial_start_time_ms) {
        u32 track_idx = start_track_index;
        u64 start_time_ms = initial_start_time_ms;

        while (!m_stop_requested.load()) {
            std::string guest_path;
            const bool have_track = m_mutex.Run([&] {
                if (track_idx < m_tracks.size()) {
                    guest_path = m_tracks[track_idx];
                    m_current_track_idx = track_idx;
                    return true;
                }
                return false;
            });
            if (!have_track) {
                break;
            }

            auto scheme_pos = guest_path.find("://");
            if (scheme_pos != std::string::npos) {
                guest_path = guest_path.substr(scheme_pos + 3);
            }
            if (!guest_path.starts_with("/")) {
                guest_path = "/" + guest_path;
            }

            std::string decoded_path;
            decoded_path.reserve(guest_path.size());
            for (size_t i = 0; i < guest_path.size(); ++i) {
                if (guest_path[i] == '%' && i + 2 < guest_path.size()) {
                    if (std::isxdigit(guest_path[i + 1]) && std::isxdigit(guest_path[i + 2])) {
                        char hex_str[3] = {guest_path[i + 1], guest_path[i + 2], '\0'};
                        int hex_val = static_cast<int>(std::strtol(hex_str, nullptr, 16));
                        decoded_path.push_back(static_cast<char>(hex_val));
                        i += 2;
                        continue;
                    }
                }
                decoded_path.push_back(guest_path[i]);
            }
            guest_path = std::move(decoded_path);

            std::string resolved_guest_path = guest_path;
            m_mutex.Run([&] {
                for (const auto& [serial, device_id] : m_usb_devices) {
                    std::string prefix = "/" + serial + "/";
                    if (guest_path.starts_with(prefix)) {
                        resolved_guest_path = "/mnt/usb" + std::to_string(device_id) + "/" +
                                              guest_path.substr(prefix.size());
                        break;
                    }
                }
            });

            auto* mnt = Common::Singleton<Core::FileSys::MntPoints>::Instance();
            std::filesystem::path host_path;
            if (mnt) {
                host_path = mnt->GetHostPath(resolved_guest_path);
            }
            if (host_path.empty() || !Common::FS::Exists(host_path)) {
                host_path = resolved_guest_path;
            }

            if (!Common::FS::Exists(host_path)) {
                LOG_ERROR(Lib_AudioOut, "Music file does not exist: {}", host_path.string());
                break;
            }

            Common::AudioFile::Pcm pcm;
            bool decoded = false;

            std::string ext = host_path.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (ext == ".flac" || ext == ".mp3" || ext == ".aac" || ext == ".m4a" ||
                ext == ".mp4") {
                // Mapped and decoded where it lies, into one buffer the mix resamples from.
                const auto bytes = Common::FS::FileView::Map(host_path);
                const u8* data = bytes ? bytes->data() : nullptr;
                const size_t size = bytes ? bytes->size() : 0;
                if (ext == ".flac") {
                    decoded = Common::AudioFile::DecodeFlac(data, size, pcm);
                } else if (ext == ".mp3") {
                    decoded = Common::AudioFile::DecodeMp3(data, size, pcm);
                } else if (ext == ".aac") {
                    decoded = Common::AudioFile::DecodeAdts(data, size, pcm);
                } else {
                    decoded = Common::AudioFile::DecodeMp4(data, size, pcm);
                }
            }

            if (!decoded) {
                LOG_ERROR(Lib_AudioOut, "Failed to decode audio file: {}", host_path.string());
                break;
            }

            const u64 total_frames = pcm.Frames();
            u64 total_duration = total_frames * 1000 / pcm.hz;
            m_total_duration_ms = total_duration;

            if (m_stop_requested.load()) {
                break;
            }

            // The track goes to the frontend through the core's mix, resampled to its 48 kHz
            // straight from the decoded buffer into the input's ring.
            if (pcm.channels > 2) {
                LOG_ERROR(Lib_AudioOut, "Music with {} channels is not played", pcm.channels);
                break;
            }
            audio_mix_input_t* input = audio_mix_input_open();
            if (!input) {
                LOG_ERROR(Lib_AudioOut, "No input left in the core's mix for music");
                break;
            }
            audio_mix_resampler_t resampler;
            audio_mix_resampler_init(&resampler, static_cast<unsigned>(pcm.hz),
                                     static_cast<unsigned>(pcm.channels));
            const auto frame_at = [&](u64 ms) {
                return std::min<u64>(ms * pcm.hz / 1000, total_frames) << 32;
            };
            u64 pos = start_time_ms > 0 && total_duration > 0 ? frame_at(start_time_ms) : 0;

            constexpr unsigned chunk_frames = 1024;
            while (!m_stop_requested.load()) {
                s64 seek_target = m_seek_offset_ms.exchange(-1);
                if (seek_target >= 0) {
                    pos = frame_at(static_cast<u64>(seek_target));
                    m_current_time_ms = static_cast<u64>(seek_target);
                }

                if (m_pause_requested.load()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                    continue;
                }

                if ((pos >> 32) >= total_frames) {
                    break;
                }
                // Bounded, so stop, pause and seek are seen while the frontend takes nothing.
                if (!audio_mix_input_wait(input, chunk_frames, 20000)) {
                    continue;
                }

                const float vol =
                    (m_volume.load() / 25.0f) * (emu_settings_get_volume_slider() * 0.01f);
                unsigned done = 0;
                while (done < chunk_frames) {
                    s16* dst = nullptr;
                    const unsigned room = audio_mix_input_reserve(input, &dst, chunk_frames - done);
                    if (room == 0) {
                        break;
                    }
                    const unsigned got = audio_mix_resample(&resampler, pcm.Data(), total_frames,
                                                            &pos, dst, room, vol);
                    audio_mix_input_commit(input, got);
                    done += got;
                    if (got < room) {
                        break; // the track has ended
                    }
                }
                m_current_time_ms = (pos >> 32) * 1000 / pcm.hz;
            }

            audio_mix_input_close(input);

            if (m_stop_requested.load()) {
                break;
            }

            u32 count = GetTrackCount();
            if (count == 0) {
                break;
            }

            u32 rep = m_repeat_mode.load();
            u32 shuf = m_shuffle_mode.load();

            u32 next_track = track_idx;
            if (rep == 2) {
                // Repeat one
                next_track = track_idx;
            } else if (shuf == 1) {
                // Shuffle
                if (count > 1) {
                    for (int retry = 0; retry < 10 && next_track == track_idx; ++retry) {
                        next_track = static_cast<u32>(rand() % count);
                    }
                }
            } else if (rep == 1) {
                // Repeat all
                next_track = (track_idx + 1) % count;
            } else {
                // Repeat off
                if (track_idx + 1 < count) {
                    next_track = track_idx + 1;
                } else {
                    LOG_INFO(Lib_AudioOut, "Playback reached end of playlist");
                    break;
                }
            }

            track_idx = next_track;
            start_time_ms = 0;
            m_current_track_idx = track_idx;
            m_current_time_ms = 0;
            LOG_INFO(Lib_AudioOut, "Auto-advancing to track_index={}", track_idx);
        }

        if (!m_stop_requested.load()) {
            m_play_state = 0;
            m_current_time_ms = 0;
            LOG_INFO(Lib_AudioOut, "Playback finished");
        }
    }

    Common::Combiner m_control_mutex;
    Common::Combiner m_mutex;
    std::vector<std::string> m_tracks;
    std::atomic<u32> m_play_state{0};
    std::atomic<u32> m_current_track_idx{0};
    std::atomic<u64> m_current_time_ms{0};
    std::atomic<u64> m_total_duration_ms{0};
    std::atomic<int> m_volume{25};
    std::atomic<u32> m_repeat_mode{0};
    std::atomic<u32> m_shuffle_mode{0};
    std::atomic<bool> m_stop_requested{false};
    std::atomic<bool> m_pause_requested{false};
    std::atomic<s64> m_seek_offset_ms{-1};
    std::thread m_worker_thread;
    Common::EventMutex m_event_mutex;
    Common::EventCondition m_event_cv;
    std::queue<MusicEvent> m_event_queue;
    std::unordered_map<u32, std::vector<std::string>> m_tracklists;
    std::unordered_map<std::string, u32> m_usb_devices;
    std::atomic<bool> m_in_transaction{false};
    std::atomic<u64> m_tracklist_version{1};
};

static s32 PS4_SYSV_ABI sceMusicPlayerServiceInitialize(size_t pool_size, void* pool_addr) {
    LOG_ERROR(Lib_AudioOut, "(STUBBED) called");
    return ORBIS_OK;
}

static s32 PS4_SYSV_ABI sceMusicPlayerServiceInitialize2(size_t pool_size, void* pool_addr) {
    LOG_ERROR(Lib_AudioOut, "(STUBBED) called");
    return ORBIS_OK;
}

static s32 PS4_SYSV_ABI sceMusicPlayerServiceInitialize3(size_t pool_size, void* pool_addr,
                                                         u64 unk1, u64 unk2) {
    LOG_INFO(Lib_AudioOut, "pool_size={:#x}, pool_addr={}", pool_size, pool_addr);
    if (pool_size < 0x200000 || !pool_addr) {
        LOG_ERROR(Lib_AudioOut, "invalid arguments: pool_size={:#x}, pool_addr={}", pool_size,
                  pool_addr);
        return ORBIS_MUSIC_PLAYER_SERVICE_ERROR_INVALID_PARAMETER;
    }
    return ORBIS_OK;
}

static s32 PS4_SYSV_ABI sceMusicPlayerServiceTerminate() {
    LOG_INFO(Lib_AudioOut, "called");
    MusicPlayerManager::Instance().StopPlayback();
    return ORBIS_OK;
}

static s32 PS4_SYSV_ABI sceMusicPlayerServiceSetUsbStorageDeviceInfo(
    const OrbisMusicPlayerUsbDeviceInfo* devices, u32 count) {
    LOG_INFO(Lib_AudioOut, "devices={}, count={}", static_cast<const void*>(devices), count);
    return MusicPlayerManager::Instance().SetUsbStorageDeviceInfo(devices, count);
}

static s32 PS4_SYSV_ABI sceMusicPlayerServiceRemoveAllData(s32 player_id) {
    LOG_INFO(Lib_AudioOut, "player_id={}", player_id);
    MusicPlayerManager::Instance().ClearTracks();
    return ORBIS_OK;
}

static s32 SetTrackListInternal(s32 player_id, const void* tracks, u32 count, u32 flags,
                                size_t track_entry_size) {
    if (!tracks || count == 0) {
        LOG_ERROR(Lib_AudioOut, "invalid arguments: tracks={}, count={}", tracks, count);
        return ORBIS_MUSIC_PLAYER_SERVICE_ERROR_INVALID_PARAMETER;
    }

    const auto* base = reinterpret_cast<const u8*>(tracks);
    std::vector<std::string> track_uris;
    track_uris.reserve(count);

    LOG_INFO(Lib_AudioOut, "player_id={}, count={}, flags={}, stride={:#x}", player_id, count,
             flags, track_entry_size);

    for (u32 i = 0; i < count; i++) {
        const char* uri_str = reinterpret_cast<const char*>(base + i * track_entry_size);
        track_uris.emplace_back(uri_str);
        LOG_DEBUG(Lib_AudioOut, "  track[{}]: {}", i, uri_str);
    }

    MusicPlayerManager::Instance().SetTrackList(track_uris);
    return ORBIS_OK;
}

static s32 PS4_SYSV_ABI sceMusicPlayerServiceSetTrackList(s32 player_id, const void* tracks,
                                                          u32 count) {
    return SetTrackListInternal(player_id, tracks, count, 0, 0xc2f);
}

static s32 PS4_SYSV_ABI sceMusicPlayerServiceSetTrackList2(s32 player_id, const void* tracks,
                                                           u32 count, u32 flags) {
    return SetTrackListInternal(player_id, tracks, count, flags, 0xc48);
}

static s32 PS4_SYSV_ABI sceMusicPlayerServicePlay(s32 player_id, u32 track_index, s64 start_time_s,
                                                  s32 flags) {
    LOG_INFO(Lib_AudioOut, "player_id={}, track_index={}, start_time_s={}, flags={}", player_id,
             track_index, start_time_s, flags);
    auto& mgr = MusicPlayerManager::Instance();
    u64 start_time_ms = start_time_s > 0 ? static_cast<u64>(start_time_s * 1000) : 0;
    mgr.PlayOrSeek(track_index, start_time_ms);
    return ORBIS_OK;
}

static s32 PS4_SYSV_ABI sceMusicPlayerServicePlayStartByTime(s32 player_id, u32 track_index,
                                                             s64 start_time_s) {
    LOG_INFO(Lib_AudioOut, "player_id={}, track_index={}, start_time_s={}", player_id, track_index,
             start_time_s);
    auto& mgr = MusicPlayerManager::Instance();
    u64 start_time_ms = start_time_s > 0 ? static_cast<u64>(start_time_s * 1000) : 0;
    mgr.PlayOrSeek(track_index, start_time_ms);
    return ORBIS_OK;
}

static s32 PS4_SYSV_ABI sceMusicPlayerServiceStop() {
    LOG_INFO(Lib_AudioOut, "called");
    MusicPlayerManager::Instance().StopPlayback();
    return ORBIS_OK;
}

static s32 PS4_SYSV_ABI sceMusicPlayerServicePause() {
    LOG_INFO(Lib_AudioOut, "called");
    MusicPlayerManager::Instance().Pause();
    return ORBIS_OK;
}

static s32 PS4_SYSV_ABI sceMusicPlayerServiceUnpause() {
    LOG_INFO(Lib_AudioOut, "called");
    MusicPlayerManager::Instance().Unpause();
    return ORBIS_OK;
}

static s32 PS4_SYSV_ABI sceMusicPlayerServiceNextTrack() {
    LOG_INFO(Lib_AudioOut, "called");
    MusicPlayerManager::Instance().NextTrack();
    return ORBIS_OK;
}

static s32 PS4_SYSV_ABI sceMusicPlayerServicePreviousTrack() {
    LOG_INFO(Lib_AudioOut, "called");
    MusicPlayerManager::Instance().PreviousTrack();
    return ORBIS_OK;
}

static s32 PS4_SYSV_ABI sceMusicPlayerServiceSeek(s32 player_id, u32 track_index, s64 time_s) {
    LOG_INFO(Lib_AudioOut, "player_id={}, track_index={}, time_s={}", player_id, track_index,
             time_s);
    auto& mgr = MusicPlayerManager::Instance();
    u64 time_ms = time_s > 0 ? static_cast<u64>(time_s * 1000) : 0;
    mgr.SeekTrack(track_index, time_ms);
    return ORBIS_OK;
}

static s32 PS4_SYSV_ABI sceMusicPlayerServiceSetAudioVolume(s32 volume) {
    LOG_INFO(Lib_AudioOut, "volume={}", volume);
    if (volume < 0 || volume > 25) {
        LOG_ERROR(Lib_AudioOut, "invalid volume={}", volume);
        return ORBIS_MUSIC_PLAYER_SERVICE_ERROR_INVALID_PARAMETER;
    }
    MusicPlayerManager::Instance().SetVolume(volume);
    return ORBIS_OK;
}

static s32 PS4_SYSV_ABI sceMusicPlayerServiceGetAudioVolume(s32* volume, s32* mute) {
    if (volume) {
        *volume = MusicPlayerManager::Instance().GetVolume();
    }
    if (mute) {
        *mute = 0;
    }
    return ORBIS_OK;
}

static s32 PS4_SYSV_ABI sceMusicPlayerServiceSetShuffle(u32 mode) {
    LOG_INFO(Lib_AudioOut, "mode={}", mode);
    if (mode > 1) {
        LOG_ERROR(Lib_AudioOut, "invalid mode={}", mode);
        return ORBIS_MUSIC_PLAYER_SERVICE_ERROR_INVALID_PARAMETER;
    }
    MusicPlayerManager::Instance().SetShuffle(mode);
    return ORBIS_OK;
}

static s32 PS4_SYSV_ABI sceMusicPlayerServiceSetRepeatMode(u32 mode) {
    LOG_INFO(Lib_AudioOut, "mode={}", mode);
    if (mode > 2) {
        LOG_ERROR(Lib_AudioOut, "invalid mode={}", mode);
        return ORBIS_MUSIC_PLAYER_SERVICE_ERROR_INVALID_PARAMETER;
    }
    MusicPlayerManager::Instance().SetRepeatMode(mode);
    return ORBIS_OK;
}

static s32 PS4_SYSV_ABI
sceMusicPlayerServiceGetPlayStatusExtension(OrbisMusicPlayerPlayStatusExtension* status) {
    if (!status || status->size != 0x48) {
        return ORBIS_MUSIC_PLAYER_SERVICE_ERROR_INVALID_PARAMETER;
    }

    auto& mgr = MusicPlayerManager::Instance();
    const u32 track_count = mgr.GetTrackCount();
    const u32 cur_track = mgr.GetCurrentTrackIndex();
    const u32 rep_mode = mgr.GetRepeatMode();
    const u32 shuf_mode = mgr.GetShuffleMode();

    u32 play_state = 2; // 2: stopped
    if (mgr.GetPlayState() == 1) {
        play_state = 0; // 0: playing
    } else if (mgr.GetPlayState() == 2) {
        play_state = 1; // 1: paused
    }

    status->error_code = 0;
    status->play_state = play_state;
    status->total_duration_s = mgr.GetTotalDurationMs() / 1000;
    status->current_time_s = mgr.GetCurrentTimeMs() / 1000;
    status->tracklist_id = 0;
    status->track_index = cur_track;
    status->play_index = cur_track;
    status->total_track_count = track_count;
    status->volume = static_cast<u32>(mgr.GetVolume());
    status->repeat_mode = rep_mode;
    status->shuffle_mode = shuf_mode;
    const bool can_next =
        (track_count > 0) && ((cur_track + 1 < track_count) || (rep_mode != 0) || (shuf_mode != 0));
    const bool can_prev =
        (track_count > 0) && ((cur_track > 0) || (rep_mode != 0) || (shuf_mode != 0));

    // PS4 OrbisMusicPlayerPlayStatusExtension specification:
    // 0 = skip allowed, 1 = skip disabled (playlist boundary reached with repeat/shuffle off)
    status->can_skip_next = can_next ? 0 : 1;
    status->can_skip_previous = can_prev ? 0 : 1;
    status->is_loading = 0;
    return ORBIS_OK;
}

static s32 PS4_SYSV_ABI
sceMusicPlayerServiceGetCurrentPlayStatus(SceMusicPlayerServicePlayStatus* status) {
    if (!status) {
        return ORBIS_MUSIC_PLAYER_SERVICE_ERROR_INVALID_PARAMETER;
    }
    auto& mgr = MusicPlayerManager::Instance();

    u32 play_status = 2; // 2: stopped
    if (mgr.GetPlayState() == 1) {
        play_status = 0; // 0: playing
    } else if (mgr.GetPlayState() == 2) {
        play_status = 1; // 1: paused
    }

    status->result = 0;
    status->play_status = play_status;
    status->track_index = mgr.GetCurrentTrackIndex();
    status->tracklist_id = 0;
    status->playback_time = mgr.GetCurrentTimeMs() / 1000;
    status->shuffle = mgr.GetShuffleMode();
    status->repeat = mgr.GetRepeatMode();
    return ORBIS_OK;
}

static s32 PS4_SYSV_ABI sceMusicPlayerServiceReceiveEvent(void* buffer) {
    if (!buffer) {
        LOG_ERROR(Lib_AudioOut, "invalid buffer");
        return ORBIS_MUSIC_PLAYER_SERVICE_ERROR_INVALID_PARAMETER;
    }
    u8* buf = static_cast<u8*>(buffer);
    auto& mgr = MusicPlayerManager::Instance();
    MusicPlayerManager::MusicEvent ev{};
    if (mgr.PopEvent(ev, 50)) {
        LOG_DEBUG(Lib_AudioOut, "delivering event_id={}, track_idx={}", ev.event_id,
                  ev.track_index);
        *reinterpret_cast<u32*>(buf + 0x08) = 1;
        *reinterpret_cast<u32*>(buf + 0x20) = 1;
        *reinterpret_cast<u32*>(buf + 0x24) = ev.event_id;
        *reinterpret_cast<u32*>(buf + 0x28) = ev.track_index;
        return ORBIS_OK;
    }
    return ORBIS_MUSIC_PLAYER_SERVICE_ERROR_TIMED_OUT;
}

static s32 PS4_SYSV_ABI sceMusicPlayerServiceGetPlaybackResults(void* a, void* b) {
    LOG_ERROR(Lib_AudioOut, "(STUBBED) called");
    return ORBIS_OK;
}

static s32 PS4_SYSV_ABI sceMusicPlayerServiceGetTrackInfo(void* a, void* b) {
    LOG_ERROR(Lib_AudioOut, "(STUBBED) called");
    return ORBIS_OK;
}

static s32 PS4_SYSV_ABI sceMusicPlayerServiceBeginTransaction(s32 a) {
    LOG_INFO(Lib_AudioOut, "called: a={}", a);
    return MusicPlayerManager::Instance().BeginTransaction();
}

static s32 PS4_SYSV_ABI sceMusicPlayerServiceEndTransaction(s32 a) {
    LOG_INFO(Lib_AudioOut, "called: a={}", a);
    return MusicPlayerManager::Instance().EndTransaction();
}

static s32 PS4_SYSV_ABI sceMusicPlayerServiceCreateTrackList(u32 tracklist_id) {
    LOG_INFO(Lib_AudioOut, "called: tracklist_id={}", tracklist_id);
    return MusicPlayerManager::Instance().CreateTrackList(tracklist_id);
}

static s32 PS4_SYSV_ABI sceMusicPlayerServiceRemoveTrackList(u32 tracklist_id) {
    LOG_INFO(Lib_AudioOut, "called: tracklist_id={}", tracklist_id);
    return MusicPlayerManager::Instance().RemoveTrackList(tracklist_id);
}

static s32 PS4_SYSV_ABI sceMusicPlayerServiceInsertTrackEntry(u32 tracklist_id, u32 track_index,
                                                              const void* track_entry) {
    if (!track_entry) {
        LOG_ERROR(Lib_AudioOut, "InsertTrackEntry: null track_entry");
        return ORBIS_MUSIC_PLAYER_SERVICE_ERROR_INVALID_PARAMETER;
    }
    const char* uri_str = reinterpret_cast<const char*>(track_entry);
    LOG_INFO(Lib_AudioOut, "called: tracklist_id={}, track_index={}, uri={}", tracklist_id,
             track_index, uri_str);
    return MusicPlayerManager::Instance().InsertTrackEntry(tracklist_id, track_index, uri_str);
}

static s32 PS4_SYSV_ABI sceMusicPlayerServiceRemoveTrackEntry(u32 tracklist_id, u32 track_index) {
    LOG_INFO(Lib_AudioOut, "called: tracklist_id={}, track_index={}", tracklist_id, track_index);
    return MusicPlayerManager::Instance().RemoveTrackEntry(tracklist_id, track_index);
}

static s32 PS4_SYSV_ABI sceMusicPlayerServiceGetTrackListVersion(s32 player_id, u64* version) {
    if (!version) {
        LOG_ERROR(Lib_AudioOut, "GetTrackListVersion: null version pointer");
        return ORBIS_MUSIC_PLAYER_SERVICE_ERROR_INVALID_PARAMETER;
    }
    *version = MusicPlayerManager::Instance().GetTrackListVersion();
    return ORBIS_OK;
}

void RegisterLib(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("WypjBw-l+pE", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceInitialize);
    LIB_FUNCTION("xT55MQDY9lg", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceInitialize2);
    LIB_FUNCTION("sY8beqtqGv0", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceInitialize3);
    LIB_FUNCTION("uZXjTVsS-hU", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceTerminate);
    LIB_FUNCTION("JlbWOi1Webo", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceSetUsbStorageDeviceInfo);
    LIB_FUNCTION("-JE2KLnsg9I", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceRemoveAllData);
    LIB_FUNCTION("4cuKd4S83xw", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceSetTrackList);
    LIB_FUNCTION("iawJpYIQM7s", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceSetTrackList2);
    LIB_FUNCTION("KblnYfZ5AGM", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServicePlay);
    LIB_FUNCTION("kMb+qpZTQ18", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServicePlayStartByTime);
    LIB_FUNCTION("7Ayp3diNYnk", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceStop);
    LIB_FUNCTION("fIii6-0Adxc", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServicePause);
    LIB_FUNCTION("DLXiDjC1D1A", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceUnpause);
    LIB_FUNCTION("M76dl-GnCs8", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceNextTrack);
    LIB_FUNCTION("NuJT-nmTqrI", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServicePreviousTrack);
    LIB_FUNCTION("5LhFvqzrcug", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceSeek);
    LIB_FUNCTION("tMgpmzMA4Zc", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceSetAudioVolume);
    LIB_FUNCTION("3KChwrxsVPg", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceGetAudioVolume);
    LIB_FUNCTION("DMvAm4HFKg0", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceSetShuffle);
    LIB_FUNCTION("+uCwAp6KYtQ", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceSetRepeatMode);
    LIB_FUNCTION("TVe6T-UKcoc", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceGetPlayStatusExtension);
    LIB_FUNCTION("CNsRsR5a+qc", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceGetCurrentPlayStatus);
    LIB_FUNCTION("Py0XtCoi5IU", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceReceiveEvent);
    LIB_FUNCTION("1+KpQYgv1zk", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceGetPlaybackResults);
    LIB_FUNCTION("wHKiDjNLQwI", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceGetTrackInfo);
    LIB_FUNCTION("AbiIaBA50I0", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceBeginTransaction);
    LIB_FUNCTION("7SOVShNUDXo", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceEndTransaction);
    LIB_FUNCTION("mW3EHwAVHPA", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceCreateTrackList);
    LIB_FUNCTION("EF1lpApBn4s", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceRemoveTrackList);
    LIB_FUNCTION("R1SLegxzCGU", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceInsertTrackEntry);
    LIB_FUNCTION("QEIJRZErmxQ", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceRemoveTrackEntry);
    LIB_FUNCTION("qYbmAxRGuq8", "libSceMusicPlayerService", 1, "libSceMusicPlayerService",
                 sceMusicPlayerServiceGetTrackListVersion);
}

bool IsBgmPlaying() {
    return MusicPlayerManager::Instance().GetPlayState() != 0;
}

} // namespace Libraries::MusicPlayerService
