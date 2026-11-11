// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <deque>
#include <mutex>
#include <ranges>
#include <semaphore>

#include "common/combiner.h"
#include "common/enum_names.h"

#include "save_backup.h"
#include "save_instance.h"

#include <retro_atomic.h>
#include "common/io_file.h"
#include "common/logging/formatter.h"
#include "common/logging/log.h"
#include "common/path_util.h"
#include "common/polyfill_thread.h"
#include "common/thread.h"

constexpr std::string_view sce_sys = "sce_sys";               // system folder inside save
constexpr std::string_view backup_dir = "sce_backup";         // backup folder
constexpr std::string_view backup_dir_tmp = "sce_backup_tmp"; // in-progress backup folder
constexpr std::string_view backup_dir_old = "sce_backup_old"; // previous backup folder

namespace fs = std::filesystem;

namespace Libraries::SaveData::Backup {

static void CopyShareReadWrite(const std::filesystem::path& from, const std::filesystem::path& to) {
    // Through the VFS's copy, which clones each file where the file system can and otherwise
    // moves it without holding all of it
    if (!Common::FS::CopyAll(from, to)) {
        LOG_ERROR(Lib_SaveData, "Backup: failed to copy {} to {}", fmt::UTF(from.u8string()),
                  fmt::UTF(to.u8string()));
    }
}

static std::jthread g_backup_thread;
static std::counting_semaphore g_backup_thread_semaphore{0};

static Common::Combiner g_backup_running_mutex;

static Common::Combiner g_backup_queue_mutex;
static std::deque<BackupRequest> g_backup_queue;
static std::deque<BackupRequest> g_result_queue;

static retro_atomic_int_t g_backup_progress{0};
static retro_atomic_int_t g_backup_status{static_cast<int>(WorkerStatus::NotStarted)};

static void backup(const std::filesystem::path& dir_name) {
    g_backup_running_mutex.Run([&] {
        if (!Common::FS::Exists(dir_name)) {
            return;
        }

        const auto backup_dir = dir_name / ::backup_dir;
        const auto backup_dir_tmp = dir_name / ::backup_dir_tmp;
        const auto backup_dir_old = dir_name / ::backup_dir_old;

        Common::FS::RemoveAll(backup_dir_tmp);
        Common::FS::RemoveAll(backup_dir_old);

        std::vector<std::filesystem::path> backup_files;
        Common::FS::ForEachEntry(dir_name, [&](std::string_view name, bool) {
            if (name != ::backup_dir) {
                backup_files.push_back(dir_name / Common::FS::PathFromUTF8String(name));
            }
        });

        (void)retro_atomic_exchange_int(&g_backup_progress, 0);

        int total_count = static_cast<int>(backup_files.size());
        int current_count = 0;

        Common::FS::CreateDirectories(backup_dir_tmp);
        for (const auto& file : backup_files) {
            CopyShareReadWrite(file, backup_dir_tmp / file.filename());
            current_count++;
            (void)retro_atomic_exchange_int(&g_backup_progress, current_count * 100 / total_count);
        }
        bool has_existing_backup = Common::FS::Exists(backup_dir);
        if (has_existing_backup) {
            Common::FS::Rename(backup_dir, backup_dir_old);
        }
        Common::FS::Rename(backup_dir_tmp, backup_dir);
        if (has_existing_backup) {
            Common::FS::RemoveAll(backup_dir_old);
        }
    });
}

static void BackupThreadBody() {
    Common::SetCurrentThreadName("shadPS4:SaveData:BackupThread");
    while (static_cast<WorkerStatus>(retro_atomic_load_seq_cst_int(&g_backup_status)) !=
           WorkerStatus::Stopping) {
        (void)retro_atomic_exchange_int(&g_backup_status, static_cast<int>(WorkerStatus::Waiting));

        bool wait;
        BackupRequest req;
        g_backup_queue_mutex.Run([&] {
            wait = g_backup_queue.empty();
            if (!wait) {
                req = g_backup_queue.front();
            }
        });
        if (wait) {
            g_backup_thread_semaphore.acquire();
            const bool empty = g_backup_queue_mutex.Run([&] {
                if (g_backup_queue.empty()) {
                    return true;
                }
                req = g_backup_queue.front();
                return false;
            });
            if (empty) {
                continue;
            }
        }
        if (req.save_path.empty()) {
            break;
        }
        (void)retro_atomic_exchange_int(&g_backup_status, static_cast<int>(WorkerStatus::Running));

        LOG_INFO(Lib_SaveData, "Backing up the following directory: {}",
                 fmt::UTF(req.save_path.u8string()));
        try {
            backup(req.save_path);
        } catch (const std::filesystem::filesystem_error& err) {
            LOG_ERROR(Lib_SaveData, "Failed to backup {}: {}", fmt::UTF(req.save_path.u8string()),
                      err.what());
        }
        LOG_DEBUG(Lib_SaveData, "Backing up the following directory: {} finished",
                  fmt::UTF(req.save_path.u8string()));
        g_backup_queue_mutex.Run([&] { g_backup_queue.front().done = true; });
        g_backup_queue_mutex.Run([&] {
            g_backup_queue.pop_front();
            if (req.origin != OrbisSaveDataEventType::__DO_NOT_SAVE) {
                g_result_queue.push_back(std::move(req));
                if (g_result_queue.size() > 20) {
                    g_result_queue.pop_front();
                }
            }
        });
        std::this_thread::sleep_for(std::chrono::seconds(5)); // Don't backup too often
    }
    (void)retro_atomic_exchange_int(&g_backup_status, static_cast<int>(WorkerStatus::NotStarted));
}

void StartThread() {
    if (static_cast<WorkerStatus>(retro_atomic_load_seq_cst_int(&g_backup_status)) !=
        WorkerStatus::NotStarted) {
        return;
    }
    LOG_DEBUG(Lib_SaveData, "Starting backup thread");
    (void)retro_atomic_exchange_int(&g_backup_status, static_cast<int>(WorkerStatus::Waiting));
    g_backup_thread = std::jthread{BackupThreadBody};
}

void StopThread() {
    if (static_cast<WorkerStatus>(retro_atomic_load_seq_cst_int(&g_backup_status)) ==
            WorkerStatus::NotStarted ||
        static_cast<WorkerStatus>(retro_atomic_load_seq_cst_int(&g_backup_status)) ==
            WorkerStatus::Stopping) {
        return;
    }
    LOG_DEBUG(Lib_SaveData, "Stopping backup thread");
    (void)retro_atomic_exchange_int(&g_backup_status, static_cast<int>(WorkerStatus::Stopping));
    g_backup_queue_mutex.Run([&] { g_backup_queue.emplace_back(BackupRequest{}); });
    g_backup_thread_semaphore.release();
    while (GetWorkerStatus() != WorkerStatus::NotStarted) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

bool NewRequest(Libraries::UserService::OrbisUserServiceUserId user_id, std::string_view title_id,
                std::string_view dir_name, OrbisSaveDataEventType origin) {
    auto save_path = SaveInstance::MakeDirSavePath(user_id, title_id, dir_name);

    if (static_cast<WorkerStatus>(retro_atomic_load_seq_cst_int(&g_backup_status)) !=
            WorkerStatus::Waiting &&
        static_cast<WorkerStatus>(retro_atomic_load_seq_cst_int(&g_backup_status)) !=
            WorkerStatus::Running) {
        LOG_ERROR(Lib_SaveData, "Called backup while status is {}. Backup request to {} ignored",
                  enum_name(&enum_names_Libraries_SaveData_Backup_WorkerStatus,
                            static_cast<s64>(static_cast<WorkerStatus>(
                                retro_atomic_load_seq_cst_int(&g_backup_status)))),
                  fmt::UTF(save_path.u8string()));
        return false;
    }
    const bool queued = g_backup_queue_mutex.Run([&] {
        for (const auto& it : g_backup_queue) {
            if (it.dir_name == dir_name) {
                LOG_TRACE(Lib_SaveData, "Backup request to {} ignored. Already queued", dir_name);
                return false;
            }
        }
        g_backup_queue.push_back(BackupRequest{
            .user_id = user_id,
            .title_id = std::string{title_id},
            .dir_name = std::string{dir_name},
            .origin = origin,
            .save_path = std::move(save_path),
        });
        return true;
    });
    if (!queued) {
        return false;
    }
    g_backup_thread_semaphore.release();
    return true;
}

bool Restore(const std::filesystem::path& save_path) {
    LOG_INFO(Lib_SaveData, "Restoring backup for {}", fmt::UTF(save_path.u8string()));
    return g_backup_running_mutex.Run([&]() -> bool {
        if (!Common::FS::Exists(save_path) || !Common::FS::Exists(save_path / backup_dir)) {
            return false;
        }
        Common::FS::ForEachEntry(save_path, [&](std::string_view name, bool) {
            if (name != backup_dir) {
                Common::FS::RemoveAll(save_path / Common::FS::PathFromUTF8String(name));
            }
        });
        Common::FS::CopyAll(save_path / backup_dir, save_path);

        return true;
    });
}

WorkerStatus GetWorkerStatus() {
    return static_cast<WorkerStatus>(retro_atomic_load_seq_cst_int(&g_backup_status));
}

bool IsBackupExecutingFor(const std::filesystem::path& save_path) {
    return g_backup_queue_mutex.Run([&]() -> bool {
        const auto& it =
            std::ranges::find(g_backup_queue, save_path, [](const auto& v) { return v.save_path; });
        return it != g_backup_queue.end() && !it->done;
    });
}

std::filesystem::path MakeBackupPath(const std::filesystem::path& save_path) {
    return save_path / backup_dir;
}

std::optional<BackupRequest> PopLastEvent() {
    return g_backup_queue_mutex.Run([&]() -> std::optional<BackupRequest> {
        if (g_result_queue.empty()) {
            return std::nullopt;
        }
        auto req = std::move(g_result_queue.front());
        g_result_queue.pop_front();
        return req;
    });
}

void PushBackupEvent(BackupRequest&& req) {
    g_backup_queue_mutex.Run([&] {
        g_result_queue.push_back(std::move(req));
        if (g_result_queue.size() > 20) {
            g_result_queue.pop_front();
        }
    });
}

float GetProgress() {
    return static_cast<float>(retro_atomic_load_seq_cst_int(&g_backup_progress)) / 100.0f;
}

void ClearProgress() {
    (void)retro_atomic_exchange_int(&g_backup_progress, 0);
}

} // namespace Libraries::SaveData::Backup