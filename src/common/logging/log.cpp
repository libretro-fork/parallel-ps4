// SPDX-FileCopyrightText: Copyright 2025-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// The log is written by one thread. Others format a line on their own stack and put it in a
// lock-free ring (Vyukov's bounded queue, a record taking one or more cells), so logging never
// takes a lock or allocates. When the ring is full a line waits for room if logging is sync or
// the line is critical; otherwise it is dropped and counted. Flush() waits for the writer.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string_view>
#include <thread>
#include <fmt/format.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include <file/file_path.h>
#include <rthreads/retro_eventcount.h>
#include <streams/file_stream.h>

#include "common/logging/log.h"
#include "common/path_util.h"
#include "common/thread.h"
#include "common/types.h"
#include "core/emulator_settings.h"

namespace Common::Log {

std::array<Level, NUM_LOG_CLASSES> g_class_levels{};

namespace {

constexpr std::size_t CELL_BYTES = 248;
constexpr std::size_t NUM_CELLS = 8192; // 2 MiB
constexpr std::size_t MAX_RECORD = 4096;

enum class Kind : u8 { Line, Session, Flush };

struct Cell {
    std::atomic<u64> seq;
    s64 time_ms;
    u32 length; // of the whole record, in its first cell
    Kind kind;
    Level level;
    u16 cells;
    char text[CELL_BYTES];
};

constexpr std::array<std::string_view, 7> LEVEL_NAMES{"Trace", "Debug",    "Info", "Warning",
                                                      "Error", "Critical", "Off"};

Cell g_ring[NUM_CELLS];
alignas(64) std::atomic<u64> g_tail{0};
alignas(64) u64 g_head = 0; // the writer's
std::atomic<u64> g_dropped{0};
std::atomic<u64> g_flushed{0};
std::atomic<bool> g_running{false};
std::atomic<bool> g_stop{false};
retro_eventcount_t g_writer_ec{};
retro_eventcount_t g_flush_ec{};
std::thread g_writer;
std::mutex g_control; // Setup/Shutdown only

// Settings the writer reads.
std::atomic<bool> g_sync{true};
std::atomic<bool> g_skip_duplicate{true};
std::atomic<u32> g_max_skip_ms{5000};
std::atomic<u64> g_size_limit{0};
std::atomic<Level> g_flush_level{Level::Off};
#ifdef _WIN32
bool g_debugger_console = false;
#endif

s64 NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

void InitRing() {
    static std::once_flag once;
    std::call_once(once, [] {
        for (std::size_t i = 0; i < NUM_CELLS; ++i) {
            g_ring[i].seq.store(i, std::memory_order_relaxed);
        }
        retro_eventcount_init(&g_writer_ec);
        retro_eventcount_init(&g_flush_ec);
    });
}

// Claims room for the record, copies it in and publishes it. Returns its last ticket + 1, or 0
// when the ring is full.
u64 TryPush(Kind kind, Level level, const char* data, std::size_t length) {
    const std::size_t cells = std::max<std::size_t>(1, (length + CELL_BYTES - 1) / CELL_BYTES);
    u64 pos = g_tail.load(std::memory_order_relaxed);
    for (;;) {
        // The writer frees cells in order, so if the last one is free, all of them are.
        const u64 last = pos + cells - 1;
        const u64 seq = g_ring[last % NUM_CELLS].seq.load(std::memory_order_acquire);
        const s64 diff = static_cast<s64>(seq - last);
        if (diff == 0) {
            if (g_tail.compare_exchange_weak(pos, pos + cells, std::memory_order_relaxed)) {
                break;
            }
        } else if (diff < 0) {
            return 0;
        } else {
            pos = g_tail.load(std::memory_order_relaxed);
        }
    }
    Cell& first = g_ring[pos % NUM_CELLS];
    first.kind = kind;
    first.level = level;
    first.length = static_cast<u32>(length);
    first.cells = static_cast<u16>(cells);
    first.time_ms = NowMs();
    for (std::size_t i = 0; i < cells; ++i) {
        Cell& cell = g_ring[(pos + i) % NUM_CELLS];
        const std::size_t offset = i * CELL_BYTES;
        std::memcpy(cell.text, data + offset, std::min(CELL_BYTES, length - offset));
        cell.seq.store(pos + i + 1, std::memory_order_release);
    }
    retro_eventcount_notify(&g_writer_ec);
    return pos + cells;
}

u64 Push(Kind kind, Level level, const char* data, std::size_t length, bool must) {
    for (;;) {
        if (const u64 end = TryPush(kind, level, data, length)) {
            return end;
        }
        if (!must || g_stop.load(std::memory_order_acquire)) {
            g_dropped.fetch_add(1, std::memory_order_relaxed);
            return 0;
        }
        std::this_thread::yield();
    }
}

// The writer's state.
struct Writer {
    RFILE* main = nullptr;
    RFILE* session = nullptr;
    u64 session_size = 0;
    char record[MAX_RECORD + 1];
    char last[MAX_RECORD];
    std::size_t last_length = 0;
    s64 last_time = 0;
    u64 skipped = 0;
    u64 reported_drops = 0;
};
Writer g_w;

RFILE* OpenFile(const char* path, bool truncate) {
    char dir[MAX_RECORD];
    std::snprintf(dir, sizeof(dir), "%s", path);
    path_basedir(dir);
    if (dir[0] != '\0') {
        path_mkdir(dir);
    }
    // Appending opens the file as it is; a missing one is created.
    RFILE* file =
        truncate
            ? nullptr
            : filestream_open(path,
                              RETRO_VFS_FILE_ACCESS_WRITE | RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING,
                              RETRO_VFS_FILE_ACCESS_HINT_NONE);
    if (file) {
        filestream_seek(file, 0, RETRO_VFS_SEEK_POSITION_END);
    } else {
        file = filestream_open(path, RETRO_VFS_FILE_ACCESS_WRITE, RETRO_VFS_FILE_ACCESS_HINT_NONE);
    }
    return file;
}

void CloseFile(RFILE*& file) {
    if (file) {
        filestream_close(file);
        file = nullptr;
    }
}

void ConsoleWrite(Level level, const char* text, std::size_t length) {
#ifdef _WIN32
    if (g_debugger_console) {
        OutputDebugStringA(text);
        OutputDebugStringA("\n");
        return;
    }
    static constexpr WORD colors[] = {
        FOREGROUND_INTENSITY,
        FOREGROUND_GREEN | FOREGROUND_BLUE,
        FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
        FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY,
        FOREGROUND_RED | FOREGROUND_INTENSITY,
        FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
        FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
    };
    const HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO info{};
    const bool console = GetConsoleScreenBufferInfo(out, &info) != 0;
    if (console) {
        SetConsoleTextAttribute(out, colors[static_cast<std::size_t>(level)]);
    }
    DWORD written = 0;
    WriteFile(out, text, static_cast<DWORD>(length), &written, nullptr);
    if (console) {
        SetConsoleTextAttribute(out, info.wAttributes);
    }
    WriteFile(out, "\r\n", 2, &written, nullptr);
#else
    static constexpr std::string_view colors[] = {
        "\x1b[1;30m", "\x1b[0;36m", "\x1b[0;37m", "\x1b[1;33m",
        "\x1b[1;31m", "\x1b[1;35m", "\x1b[0;37m",
    };
    const std::string_view color = colors[static_cast<std::size_t>(level)];
    std::fwrite(color.data(), 1, color.size(), stdout);
    std::fwrite(text, 1, length, stdout);
    std::fwrite("\x1b[0m\n", 1, 5, stdout);
#endif
}

void FlushOutputs() {
    if (g_w.main) {
        filestream_flush(g_w.main);
    }
    if (g_w.session) {
        filestream_flush(g_w.session);
    }
    std::fflush(stdout);
}

void Emit(Level level, const char* text, std::size_t length) {
    ConsoleWrite(level, text, length);
    if (!g_w.session) {
        // Before a game starts everything goes to the main file; it stays small.
        if (g_w.main) {
            filestream_write(g_w.main, text, length);
            filestream_write(g_w.main, "\n", 1);
        }
    } else if (g_w.session_size < g_size_limit.load(std::memory_order_relaxed) ||
               level == Level::Critical) {
        filestream_write(g_w.session, text, length);
        filestream_write(g_w.session, "\n", 1);
        g_w.session_size += length + 1;
    }
    if (level >= g_flush_level.load(std::memory_order_relaxed)) {
        FlushOutputs();
    }
}

void ReportSkipped(Level level) {
    if (g_w.skipped == 0) {
        return;
    }
    char text[64];
    const int length = std::snprintf(text, sizeof(text), "Skipped %llu duplicate messages..",
                                     static_cast<unsigned long long>(g_w.skipped));
    Emit(level, text, static_cast<std::size_t>(length));
    g_w.skipped = 0;
}

void WriteLine(Level level, s64 time_ms, const char* text, std::size_t length) {
    const u64 dropped = g_dropped.load(std::memory_order_relaxed);
    if (dropped != g_w.reported_drops) {
        char note[96];
        const int note_length =
            std::snprintf(note, sizeof(note), "[Log] <Warning> %llu messages dropped, ring full",
                          static_cast<unsigned long long>(dropped - g_w.reported_drops));
        g_w.reported_drops = dropped;
        Emit(Level::Warning, note, static_cast<std::size_t>(note_length));
    }
    if (g_skip_duplicate.load(std::memory_order_relaxed)) {
        if (length == g_w.last_length && std::memcmp(text, g_w.last, length) == 0 &&
            time_ms - g_w.last_time < g_max_skip_ms.load(std::memory_order_relaxed)) {
            ++g_w.skipped;
            return;
        }
        ReportSkipped(level);
        std::memcpy(g_w.last, text, length);
        g_w.last_length = length;
        g_w.last_time = time_ms;
    }
    Emit(level, text, length);
}

void OpenSession(const char* payload, std::size_t length) {
    // "<truncate flag><path>"
    if (length < 2) {
        return;
    }
    ReportSkipped(Level::Info);
    CloseFile(g_w.session);
    g_w.session = OpenFile(payload + 1, payload[0] == '1');
    g_w.session_size = 0;
}

// Takes one ready record, if there is one.
bool PopRecord() {
    Cell& first = g_ring[g_head % NUM_CELLS];
    if (first.seq.load(std::memory_order_acquire) != g_head + 1) {
        return false;
    }
    const std::size_t cells = first.cells;
    const std::size_t length = std::min<std::size_t>(first.length, MAX_RECORD);
    const Kind kind = first.kind;
    const Level level = first.level;
    const s64 time_ms = first.time_ms;
    for (std::size_t i = 0; i < cells; ++i) {
        Cell& cell = g_ring[(g_head + i) % NUM_CELLS];
        // The producer that claimed this record is still filling it.
        while (cell.seq.load(std::memory_order_acquire) != g_head + i + 1) {
            std::this_thread::yield();
        }
        const std::size_t offset = i * CELL_BYTES;
        std::memcpy(g_w.record + offset, cell.text, std::min(CELL_BYTES, length - offset));
        cell.seq.store(g_head + i + NUM_CELLS, std::memory_order_release);
    }
    g_head += cells;
    g_w.record[length] = '\0';
    switch (kind) {
    case Kind::Line:
        WriteLine(level, time_ms, g_w.record, length);
        break;
    case Kind::Session:
        OpenSession(g_w.record, length);
        break;
    case Kind::Flush:
        ReportSkipped(Level::Info);
        FlushOutputs();
        g_flushed.store(g_head, std::memory_order_release);
        retro_eventcount_notify(&g_flush_ec);
        break;
    }
    return true;
}

bool RecordReady() {
    return g_ring[g_head % NUM_CELLS].seq.load(std::memory_order_acquire) == g_head + 1;
}

void WriterLoop() {
    Common::SetCurrentThreadName("shadPS4:Log");
    for (;;) {
        if (PopRecord()) {
            continue;
        }
        FlushOutputs();
        if (g_stop.load(std::memory_order_acquire)) {
            // Lines claimed before the stop are being filled; take them too.
            if (g_head == g_tail.load(std::memory_order_acquire)) {
                break;
            }
            std::this_thread::yield();
            continue;
        }
        const int key = retro_eventcount_prepare_wait(&g_writer_ec);
        if (RecordReady() || g_stop.load(std::memory_order_acquire)) {
            retro_eventcount_cancel_wait(&g_writer_ec);
            continue;
        }
        retro_eventcount_commit_wait(&g_writer_ec, key);
    }
    ReportSkipped(Level::Info);
    FlushOutputs();
    CloseFile(g_w.session);
    CloseFile(g_w.main);
    g_flushed.store(g_head, std::memory_order_release);
    retro_eventcount_notify(&g_flush_ec);
}

Level LevelFromString(std::string_view name) {
    const auto equals = [](std::string_view a, std::string_view b) {
        return a.size() == b.size() &&
               std::equal(a.begin(), a.end(), b.begin(),
                          [](char x, char y) { return (x | 0x20) == (y | 0x20); });
    };
    for (std::size_t i = 0; i < LEVEL_NAMES.size(); ++i) {
        if (equals(name, LEVEL_NAMES[i])) {
            return static_cast<Level>(i);
        }
    }
    if (equals(name, "warn")) {
        return Level::Warning;
    }
    if (equals(name, "err")) {
        return Level::Error;
    }
    return Level::Off;
}

std::string_view BaseName(const char* file) {
    const char* base = file;
    for (const char* p = file; *p; ++p) {
        if (*p == '/' || *p == '\\') {
            base = p + 1;
        }
    }
    return base;
}

void PushSession(const std::string& path, bool truncate) {
    char payload[MAX_RECORD];
    const std::size_t length = std::min(path.size(), sizeof(payload) - 1);
    payload[0] = truncate ? '1' : '0';
    std::memcpy(payload + 1, path.data(), length);
    Push(Kind::Session, Level::Info, payload, length + 1, true);
}

} // namespace

void VLog(Class log_class, Level level, const char* file, int line, const char* func,
          fmt::string_view format, fmt::format_args args) {
    if (!g_running.load(std::memory_order_acquire)) {
        return;
    }
    char thread[64];
    Common::GetCurrentThreadName(thread, sizeof(thread));
    const std::string_view fn = std::string_view(func) == "operator()" ? "lambda" : func;
    char text[MAX_RECORD];
    const auto head = fmt::format_to_n(
        text, sizeof(text), "[{}] <{}> ({}) {}:{} {}: ", NameOf(log_class),
        LEVEL_NAMES[static_cast<std::size_t>(level)], thread, BaseName(file), line, fn);
    std::size_t length = std::min(head.size, sizeof(text));
    if (length < sizeof(text)) {
        const auto body = fmt::vformat_to_n(text + length, sizeof(text) - length, format, args);
        length = std::min(length + body.size, sizeof(text));
    }
    // Sync logging keeps every line, waiting for room; otherwise only critical lines wait.
    Push(Kind::Line, level, text, length,
         level == Level::Critical || g_sync.load(std::memory_order_relaxed));
}

void Setup(std::string_view shadps4_filename) {
    static std::once_flag already_registered;
    std::call_once(already_registered, []() {
        std::atexit(Shutdown);
        std::at_quick_exit(Flush);
    });

    std::scoped_lock lock{g_control};
    InitRing();
    if (g_running.load(std::memory_order_acquire)) {
        return;
    }
#ifdef _WIN32
    g_debugger_console = std::string_view(emu_settings_get_log_type()) != "wincolor";
#endif
    g_size_limit.store(emu_settings_get_log_size_limit(), std::memory_order_relaxed);
    UpdateSinks();

    // The writer starts with nothing queued, so it can own the files from here on.
    g_w.main = OpenFile(
        (GetUserPath(Common::FS::PathType::LogDir) / shadps4_filename).string().c_str(), false);
    g_stop.store(false, std::memory_order_relaxed);
    g_writer = std::thread(WriterLoop);
    g_running.store(true, std::memory_order_release);
}

void Switch(std::string_view game_filename, bool append_log) {
    UpdateSinks();
    UpdateLogLevels(emu_settings_get_log_filter());
    UpdateLogFlushLevel(emu_settings_get_log_flush_level());
    g_size_limit.store(emu_settings_get_log_size_limit(), std::memory_order_relaxed);
    if (!g_running.load(std::memory_order_acquire)) {
        return;
    }
    PushSession((GetUserPath(Common::FS::PathType::LogDir) / game_filename).string(),
                !(append_log || emu_settings_get_log_append()));
}

void Shutdown() {
    std::scoped_lock lock{g_control};
    if (!g_running.exchange(false, std::memory_order_acq_rel)) {
        return;
    }
    g_stop.store(true, std::memory_order_release);
    retro_eventcount_notify(&g_writer_ec);
    g_writer.join();
}

void Flush() {
    if (!g_running.load(std::memory_order_acquire)) {
        return;
    }
    const u64 end = Push(Kind::Flush, Level::Info, "", 0, true);
    for (;;) {
        const int key = retro_eventcount_prepare_wait(&g_flush_ec);
        if (g_flushed.load(std::memory_order_acquire) >= end ||
            !g_running.load(std::memory_order_acquire)) {
            retro_eventcount_cancel_wait(&g_flush_ec);
            return;
        }
        retro_eventcount_commit_wait(&g_flush_ec, key);
    }
}

void UpdateSinks() {
    g_sync.store(emu_settings_get_log_sync(), std::memory_order_relaxed);
    g_skip_duplicate.store(emu_settings_get_log_skip_duplicate(), std::memory_order_relaxed);
    g_max_skip_ms.store(emu_settings_get_log_max_skip_duration(), std::memory_order_relaxed);
}

void UpdateLogLevels(std::string_view log_filter) {
    if (!emu_settings_get_log_enable()) {
        g_class_levels.fill(Level::Off);
        return;
    }
    Level default_level = Level::Info;
    std::array<Level, NUM_LOG_CLASSES> levels;
    std::array<bool, NUM_LOG_CLASSES> named{};
    while (!log_filter.empty()) {
        const std::size_t space = log_filter.find(' ');
        const std::string_view entry = log_filter.substr(0, space);
        log_filter =
            space == std::string_view::npos ? std::string_view{} : log_filter.substr(space + 1);
        const std::size_t colon = entry.find(':');
        if (colon == std::string_view::npos ||
            entry.find(':', colon + 1) != std::string_view::npos) {
            LOG_ERROR(Config, "bad log filter provided");
            continue;
        }
        const std::string_view name = entry.substr(0, colon);
        const Level level = LevelFromString(entry.substr(colon + 1));
        if (!name.empty() && name[0] == '*') {
            default_level = level;
            continue;
        }
        for (std::size_t i = 0; i < NUM_LOG_CLASSES; ++i) {
            if (NameOf(static_cast<Class>(i)) == name) {
                levels[i] = level;
                named[i] = true;
            }
        }
    }
    for (std::size_t i = 0; i < NUM_LOG_CLASSES; ++i) {
        g_class_levels[i] = named[i] ? levels[i] : default_level;
    }
}

void UpdateLogFlushLevel(std::string_view log_flush_level) {
    if (!log_flush_level.empty()) {
        g_flush_level.store(LevelFromString(log_flush_level), std::memory_order_relaxed);
    }
}

} // namespace Common::Log
