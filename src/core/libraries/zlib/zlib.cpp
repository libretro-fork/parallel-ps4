// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stop_token>
#include <unordered_map>
#include <queue>

#include <encodings/deflate.h>

#include "common/combiner.h"
#include "common/event_count.h"
#include "common/logging/log.h"
#include "common/thread.h"
#include "core/libraries/kernel/threads.h"
#include "core/libraries/libs.h"
#include "core/libraries/zlib/zlib_error.h"
#include "core/libraries/zlib/zlib_sce.h"

namespace Libraries::Zlib {

struct InflateTask {
    u64 request_id;
    const void* src;
    u32 src_length;
    void* dst;
    u32 dst_length;
};

struct InflateResult {
    u32 length;
    s32 status;
};

static Kernel::Thread task_thread;

static Common::Combiner mutex;
static std::queue<InflateTask> task_queue;
static Common::EventCount task_queue_cv;
static std::queue<u64> done_queue;
static Common::EventCount done_queue_cv;
static std::unordered_map<u64, InflateResult> results;
static u64 next_request_id;

void ZlibTaskThread(const std::stop_token& stop) {
    Common::SetCurrentThreadName("shadPS4:ZlibTaskThread");

    // One inflater for the thread, reset per request rather than made each time.
    void* inflater = rinflate_new(15);

    while (!stop.stop_requested()) {
        InflateTask task;
        {
            // Pop from the task queue, unless stop has been requested.
            const std::stop_callback wake{stop, [] { task_queue_cv.Notify(); }};
            bool got = false;
            task_queue_cv.Wait([&] {
                got = mutex.Run([&] {
                    if (task_queue.empty()) {
                        return false;
                    }
                    task = task_queue.front();
                    task_queue.pop();
                    return true;
                });
                return got || stop.stop_requested();
            });
            if (!got) {
                break;
            }
        }

        // A zlib stream (RFC 1950), whole, into the game's buffer: what uncompress() does.
        size_t decompressed_length = 0;
        size_t consumed = 0;
        int ret = RDEFLATE_PROCESS_ERROR;
        if (inflater) {
            rinflate_reset(inflater, 15);
            rinflate_set_in(inflater, static_cast<const u8*>(task.src), task.src_length);
            rinflate_set_out(inflater, static_cast<u8*>(task.dst), task.dst_length);
            do {
                size_t read = 0;
                size_t wrote = 0;
                ret = rinflate_process(inflater, &read, &wrote);
                consumed += read;
                decompressed_length += wrote;
            } while (ret == RDEFLATE_PROCESS_NEXT && consumed < task.src_length &&
                     decompressed_length < task.dst_length);
        }

        {
            // Lock, insert the new result, and push the finished request ID to the done queue.
            mutex.Run([&] {
                results[task.request_id] = InflateResult{
                    .length = static_cast<u32>(decompressed_length),
                    // Suspended with input left: the output is full. With none left, the stream
                    // ends early, which uncompress() reports as bad data.
                    .status = ret == RDEFLATE_PROCESS_END ? ORBIS_OK
                              : ret == RDEFLATE_PROCESS_NEXT && consumed < task.src_length
                                  ? ORBIS_ZLIB_ERROR_NOSPACE
                                  : ORBIS_ZLIB_ERROR_FATAL,
                };
                done_queue.push(task.request_id);
            });
        }
        done_queue_cv.Notify();
    }
    rinflate_free(inflater);
}

s32 PS4_SYSV_ABI sceZlibInitialize(const void* buffer, u32 length) {
    LOG_INFO(Lib_Zlib, "called");
    if (task_thread.Joinable()) {
        return ORBIS_ZLIB_ERROR_ALREADY_INITIALIZED;
    }

    // Initialize with empty task data
    task_queue = std::queue<InflateTask>();
    done_queue = std::queue<u64>();
    results.clear();
    next_request_id = 1;

    task_thread.Run([](const std::stop_token& stop) { ZlibTaskThread(stop); });
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceZlibInflate(const void* src, u32 src_len, void* dst, u32 dst_len,
                                u64* request_id) {
    LOG_DEBUG(Lib_Zlib, "called");
    if (!task_thread.Joinable()) {
        return ORBIS_ZLIB_ERROR_NOT_INITIALIZED;
    }
    if (!src || !src_len || !dst || !dst_len || !request_id || dst_len > 64_KB ||
        dst_len % 2_KB != 0) {
        return ORBIS_ZLIB_ERROR_INVALID;
    }

    mutex.Run([&] {
        *request_id = next_request_id++;
        task_queue.emplace(InflateTask{
            .request_id = *request_id,
            .src = src,
            .src_length = src_len,
            .dst = dst,
            .dst_length = dst_len,
        });
        task_queue_cv.Notify();
    });
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceZlibWaitForDone(u64* request_id, const u32* timeout) {
    LOG_DEBUG(Lib_Zlib, "called");
    if (!task_thread.Joinable()) {
        return ORBIS_ZLIB_ERROR_NOT_INITIALIZED;
    }
    if (!request_id) {
        return ORBIS_ZLIB_ERROR_INVALID;
    }

    {
        // Pop from the done queue, unless the timeout is reached.
        const auto take = [&] {
            return mutex.Run([&] {
                if (done_queue.empty()) {
                    return false;
                }
                *request_id = done_queue.front();
                done_queue.pop();
                return true;
            });
        };
        const auto deadline =
            std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout ? *timeout : 0);
        while (!take()) {
            const int key = retro_eventcount_prepare_wait(done_queue_cv.get());
            if (take()) {
                retro_eventcount_cancel_wait(done_queue_cv.get());
                break;
            }
            if (!timeout) {
                retro_eventcount_commit_wait(done_queue_cv.get(), key);
                continue;
            }
            const auto left = std::chrono::ceil<std::chrono::microseconds>(
                                  deadline - std::chrono::steady_clock::now())
                                  .count();
            if (left <= 0) {
                retro_eventcount_cancel_wait(done_queue_cv.get());
                if (take()) {
                    break;
                }
                return ORBIS_ZLIB_ERROR_TIMEDOUT;
            }
            retro_eventcount_commit_wait_timeout(done_queue_cv.get(), key, left);
        }
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceZlibGetResult(const u64 request_id, u32* dst_length, s32* status) {
    LOG_DEBUG(Lib_Zlib, "called");
    if (!task_thread.Joinable()) {
        return ORBIS_ZLIB_ERROR_NOT_INITIALIZED;
    }
    if (!dst_length || !status) {
        return ORBIS_ZLIB_ERROR_INVALID;
    }

    return mutex.Run([&] {
        if (!results.contains(request_id)) {
            return ORBIS_ZLIB_ERROR_NOT_FOUND;
        }
        const auto result = results[request_id];
        *dst_length = result.length;
        *status = result.status;
        return ORBIS_OK;
    });
}

s32 PS4_SYSV_ABI sceZlibFinalize() {
    LOG_INFO(Lib_Zlib, "called");
    if (!task_thread.Joinable()) {
        return ORBIS_ZLIB_ERROR_NOT_INITIALIZED;
    }
    task_thread.Stop();
    return ORBIS_OK;
}

void RegisterLib(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("m1YErdIXCp4", "libSceZlib", 1, "libSceZlib", sceZlibInitialize);
    LIB_FUNCTION("6na+Sa-B83w", "libSceZlib", 1, "libSceZlib", sceZlibFinalize);
    LIB_FUNCTION("TLar1HULv1Q", "libSceZlib", 1, "libSceZlib", sceZlibInflate);
    LIB_FUNCTION("uB8VlDD4e0s", "libSceZlib", 1, "libSceZlib", sceZlibWaitForDone);
    LIB_FUNCTION("2eDcGHC0YaM", "libSceZlib", 1, "libSceZlib", sceZlibGetResult);
};

} // namespace Libraries::Zlib
