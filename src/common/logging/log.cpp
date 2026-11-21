// SPDX-FileCopyrightText: Copyright 2025-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// The log is written by one thread. Others format a line on their own stack and put it in a
// lock-free ring (Vyukov's bounded queue, a record taking one or more cells), so logging never
// takes a lock or allocates. When the ring is full a line waits for room if logging is sync or
// the line is critical; otherwise it is dropped and counted. Flush() waits for the writer.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include <compat/strl.h>
#include <features/features_cpu.h>
#include <file/file_path.h>
#include <rthreads/retro_eventcount.h>
#include <streams/file_stream.h>

#include <retro_atomic.h>
#include "common/combiner.h"
#include "common/logging/log.h"
#include "common/path_util.h"
#include "common/quick_exit.h"
#include "common/run_once.h"
#include "common/stop_thread.h"
#include "common/thread_name.h"
#include "common/types.h"
#include "core/emulator_settings.h"
#include "core/libraries/fiber/fiber.h"
#include "core/libraries/kernel/threads/pthread.h"

namespace Common::Log {

Level g_class_levels[NUM_LOG_CLASSES]{};

namespace {

constexpr size_t CELL_BYTES = 248;
constexpr size_t NUM_CELLS = 8192; // 2 MiB
constexpr size_t MAX_RECORD = 4096;

enum class Kind : u8 { Line, Session, Flush };

struct Cell {
    retro_atomic_64_t seq;
    s64 time_ms;
    u32 length; // of the whole record, in its first cell
    Kind kind;
    Level level;
    u16 cells;
    char text[CELL_BYTES];
};

constexpr const char* LEVEL_NAMES[] = {"Trace", "Debug",    "Info", "Warning",
                                       "Error", "Critical", "Off"};
constexpr size_t NUM_LEVELS = sizeof(LEVEL_NAMES) / sizeof(LEVEL_NAMES[0]);

size_t MinSize(size_t a, size_t b) {
    return a < b ? a : b;
}

Cell g_ring[NUM_CELLS];
alignas(64) retro_atomic_64_t g_tail{0};
alignas(64) u64 g_head = 0; // the writer's
retro_atomic_64_t g_dropped{0};
retro_atomic_64_t g_flushed{0};
retro_atomic_int_t g_running{0};
retro_atomic_int_t g_stop{0};
retro_eventcount_t g_writer_ec{};
// Notified by the writer as it frees cells: a line that must wait for room parks on it
retro_eventcount_t g_room_ec{};
retro_eventcount_t g_flush_ec{};
stop_thread_t g_writer = STOP_THREAD_INIT;
Common::Combiner g_control; // Setup/Shutdown only

// Settings the writer reads.
retro_atomic_int_t g_sync{1};
retro_atomic_int_t g_skip_duplicate{1};
retro_atomic_int_t g_max_skip_ms{5000};
retro_atomic_64_t g_size_limit{0};
retro_atomic_int_t g_flush_level{static_cast<int>(Level::Off)};
#ifdef _WIN32
bool g_debugger_console = false;
#endif

// Only to tell how long apart two same lines were, for skipping duplicates
s64 NowMs() {
    return static_cast<s64>(cpu_features_get_time_usec() / 1000);
}

void InitRing() {
    static run_once_t once = RUN_ONCE_INIT;
    run_once(
        &once,
        [](void*) {
            for (size_t i = 0; i < NUM_CELLS; ++i) {
                retro_atomic_store_relaxed_64(&g_ring[i].seq, i);
            }
            retro_eventcount_init(&g_writer_ec);
            retro_eventcount_init(&g_room_ec);
            retro_eventcount_init(&g_flush_ec);
        },
        nullptr);
}

// Claims room for the record, copies it in and publishes it. Returns its last ticket + 1, or 0
// when the ring is full.
u64 TryPush(Kind kind, Level level, const char* data, size_t length) {
    size_t cells = (length + CELL_BYTES - 1) / CELL_BYTES;
    cells = cells ? cells : 1;
    u64 pos = static_cast<u64>(retro_atomic_load_relaxed_64(&g_tail));
    for (;;) {
        // The writer frees cells in order, so if the last one is free, all of them are.
        const u64 last = pos + cells - 1;
        const u64 seq =
            static_cast<u64>(retro_atomic_load_acquire_64(&g_ring[last % NUM_CELLS].seq));
        const s64 diff = static_cast<s64>(seq - last);
        if (diff == 0) {
            if ((retro_atomic_cas_64(&g_tail, pos, pos + cells) ||
                 ((pos = static_cast<u64>(retro_atomic_load_acquire_64(&g_tail))), false))) {
                break;
            }
        } else if (diff < 0) {
            return 0;
        } else {
            pos = static_cast<u64>(retro_atomic_load_relaxed_64(&g_tail));
        }
    }
    Cell& first = g_ring[pos % NUM_CELLS];
    first.kind = kind;
    first.level = level;
    first.length = static_cast<u32>(length);
    first.cells = static_cast<u16>(cells);
    first.time_ms = NowMs();
    for (size_t i = 0; i < cells; ++i) {
        Cell& cell = g_ring[(pos + i) % NUM_CELLS];
        const size_t offset = i * CELL_BYTES;
        memcpy(cell.text, data + offset, MinSize(CELL_BYTES, length - offset));
        retro_atomic_store_release_64(&cell.seq, pos + i + 1);
    }
    retro_eventcount_notify(&g_writer_ec);
    return pos + cells;
}

u64 Push(Kind kind, Level level, const char* data, size_t length, bool must) {
    for (;;) {
        // Asked for before the try: room the writer frees after it wakes this one
        const int key = must ? retro_eventcount_prepare_wait(&g_room_ec) : 0;
        if (const u64 end = TryPush(kind, level, data, length)) {
            if (must) {
                retro_eventcount_cancel_wait(&g_room_ec);
            }
            return end;
        }
        if (!must || (retro_atomic_load_acquire_int(&g_stop) != 0)) {
            if (must) {
                retro_eventcount_cancel_wait(&g_room_ec);
            }
            retro_atomic_fetch_add_64(&g_dropped, 1);
            return 0;
        }
        // Parked until the writer frees cells, not spun on
        retro_eventcount_commit_wait(&g_room_ec, key);
    }
}

// The writer's state.
struct Writer {
    RFILE* main = nullptr;
    RFILE* session = nullptr;
    u64 session_size = 0;
    char record[MAX_RECORD + 1];
    char last[MAX_RECORD];
    size_t last_length = 0;
    s64 last_time = 0;
    u64 skipped = 0;
    u64 reported_drops = 0;
};
Writer g_w;

RFILE* OpenFile(const char* path, bool truncate) {
    char dir[MAX_RECORD];
    snprintf(dir, sizeof(dir), "%s", path);
    path_basedir(dir);
    if (dir[0] != '\0') {
        path_mkdir(dir);
    }
    // A log started anew keeps the one before it as "<name>.prev<ext>": a run that ended badly
    // then still has its log after the next run
    if (truncate) {
        char prev[MAX_RECORD];
        const char* const ext = path_get_extension(path);
        const size_t stem = ext[0] ? static_cast<size_t>(ext - path) - 1 : strlen(path);
        snprintf(prev, sizeof(prev), "%.*s.prev%s%s", static_cast<int>(stem), path,
                 ext[0] ? "." : "", ext);
        if (path_is_valid(path)) {
            filestream_delete(prev);
            filestream_rename(path, prev);
        }
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

void ConsoleWrite(Level level, const char* text, size_t length) {
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
        SetConsoleTextAttribute(out, colors[static_cast<size_t>(level)]);
    }
    DWORD written = 0;
    WriteFile(out, text, static_cast<DWORD>(length), &written, nullptr);
    if (console) {
        SetConsoleTextAttribute(out, info.wAttributes);
    }
    WriteFile(out, "\r\n", 2, &written, nullptr);
#else
    static constexpr const char* colors[] = {
        "\x1b[1;30m", "\x1b[0;36m", "\x1b[0;37m", "\x1b[1;33m",
        "\x1b[1;31m", "\x1b[1;35m", "\x1b[0;37m",
    };
    // Each is 7 bytes
    fwrite(colors[static_cast<size_t>(level)], 1, 7, stdout);
    fwrite(text, 1, length, stdout);
    fwrite("\x1b[0m\n", 1, 5, stdout);
#endif
}

void FlushOutputs() {
    if (g_w.main) {
        filestream_flush(g_w.main);
    }
    if (g_w.session) {
        filestream_flush(g_w.session);
    }
    fflush(stdout);
}

void Emit(Level level, const char* text, size_t length) {
    ConsoleWrite(level, text, length);
    if (!g_w.session) {
        // Before a game starts everything goes to the main file; it stays small.
        if (g_w.main) {
            filestream_write(g_w.main, text, length);
            filestream_write(g_w.main, "\n", 1);
        }
    } else if (g_w.session_size < static_cast<u64>(retro_atomic_load_relaxed_64(&g_size_limit)) ||
               level == Level::Critical) {
        filestream_write(g_w.session, text, length);
        filestream_write(g_w.session, "\n", 1);
        g_w.session_size += length + 1;
    }
    // A critical line is the last before the process ends more often than not: never left in
    // a buffer
    if (level == Level::Critical ||
        level >= static_cast<Level>(retro_atomic_load_relaxed_int(&g_flush_level))) {
        FlushOutputs();
    }
}

void ReportSkipped(Level level) {
    if (g_w.skipped == 0) {
        return;
    }
    char text[64];
    const int length = snprintf(text, sizeof(text), "Skipped %llu duplicate messages..",
                                static_cast<unsigned long long>(g_w.skipped));
    Emit(level, text, static_cast<size_t>(length));
    g_w.skipped = 0;
}

void WriteLine(Level level, s64 time_ms, const char* text, size_t length) {
    const u64 dropped = static_cast<u64>(retro_atomic_load_relaxed_64(&g_dropped));
    if (dropped != g_w.reported_drops) {
        char note[96];
        const int note_length =
            snprintf(note, sizeof(note), "[Log] <Warning> %llu messages dropped, ring full",
                     static_cast<unsigned long long>(dropped - g_w.reported_drops));
        g_w.reported_drops = dropped;
        Emit(Level::Warning, note, static_cast<size_t>(note_length));
    }
    if (retro_atomic_load_relaxed_int(&g_skip_duplicate) != 0) {
        if (length == g_w.last_length && memcmp(text, g_w.last, length) == 0 &&
            time_ms - g_w.last_time <
                static_cast<u32>(retro_atomic_load_relaxed_int(&g_max_skip_ms))) {
            ++g_w.skipped;
            return;
        }
        ReportSkipped(level);
        memcpy(g_w.last, text, length);
        g_w.last_length = length;
        g_w.last_time = time_ms;
    }
    Emit(level, text, length);
}

void OpenSession(const char* payload, size_t length) {
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
    if (static_cast<u64>(retro_atomic_load_acquire_64(&first.seq)) != g_head + 1) {
        return false;
    }
    const size_t cells = first.cells;
    const size_t length = MinSize(first.length, MAX_RECORD);
    const Kind kind = first.kind;
    const Level level = first.level;
    const s64 time_ms = first.time_ms;
    for (size_t i = 0; i < cells; ++i) {
        Cell& cell = g_ring[(g_head + i) % NUM_CELLS];
        // The producer that claimed this record is still filling it: parked until it says it
        // has (it notifies once the record is in)
        for (;;) {
            const int key = retro_eventcount_prepare_wait(&g_writer_ec);
            if (static_cast<u64>(retro_atomic_load_acquire_64(&cell.seq)) == g_head + i + 1) {
                retro_eventcount_cancel_wait(&g_writer_ec);
                break;
            }
            retro_eventcount_commit_wait(&g_writer_ec, key);
        }
        const size_t offset = i * CELL_BYTES;
        memcpy(g_w.record + offset, cell.text, MinSize(CELL_BYTES, length - offset));
        retro_atomic_store_release_64(&cell.seq, g_head + i + NUM_CELLS);
    }
    g_head += cells;
    retro_eventcount_notify(&g_room_ec);
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
        retro_atomic_store_release_64(&g_flushed, g_head);
        retro_eventcount_notify(&g_flush_ec);
        break;
    }
    return true;
}

bool RecordReady() {
    return static_cast<u64>(retro_atomic_load_acquire_64(&g_ring[g_head % NUM_CELLS].seq)) ==
           g_head + 1;
}

void WriterLoop() {
    thread_name_set("shadPS4:Log");
    for (;;) {
        if (PopRecord()) {
            continue;
        }
        FlushOutputs();
        if (retro_atomic_load_acquire_int(&g_stop) != 0) {
            // Lines claimed before the stop are being filled; take them too, parked until one
            // is in (PopRecord waits for the rest of it)
            if (g_head == static_cast<u64>(retro_atomic_load_acquire_64(&g_tail))) {
                break;
            }
            const int key = retro_eventcount_prepare_wait(&g_writer_ec);
            if (RecordReady()) {
                retro_eventcount_cancel_wait(&g_writer_ec);
            } else {
                retro_eventcount_commit_wait(&g_writer_ec, key);
            }
            continue;
        }
        const int key = retro_eventcount_prepare_wait(&g_writer_ec);
        if (RecordReady() || (retro_atomic_load_acquire_int(&g_stop) != 0)) {
            retro_eventcount_cancel_wait(&g_writer_ec);
            continue;
        }
        retro_eventcount_commit_wait(&g_writer_ec, key);
    }
    ReportSkipped(Level::Info);
    FlushOutputs();
    CloseFile(g_w.session);
    CloseFile(g_w.main);
    retro_atomic_store_release_64(&g_flushed, g_head);
    retro_eventcount_notify(&g_flush_ec);
    retro_eventcount_notify(&g_room_ec);
}

// The @length bytes at @a are @b, ASCII case aside
bool EqualsIgnoreCase(const char* a, size_t length, const char* b) {
    if (strlen(b) != length) {
        return false;
    }
    for (size_t i = 0; i < length; ++i) {
        if ((a[i] | 0x20) != (b[i] | 0x20)) {
            return false;
        }
    }
    return true;
}

Level LevelFromString(const char* name, size_t length) {
    for (size_t i = 0; i < NUM_LEVELS; ++i) {
        if (EqualsIgnoreCase(name, length, LEVEL_NAMES[i])) {
            return static_cast<Level>(i);
        }
    }
    if (EqualsIgnoreCase(name, length, "warn")) {
        return Level::Warning;
    }
    if (EqualsIgnoreCase(name, length, "err")) {
        return Level::Error;
    }
    return Level::Off;
}

const char* BaseName(const char* file) {
    const char* base = file;
    for (const char* p = file; *p; ++p) {
        if (*p == '/' || *p == '\\') {
            base = p + 1;
        }
    }
    return base;
}

void PushSession(const char* path, bool truncate) {
    char payload[MAX_RECORD];
    const size_t length = MinSize(strlen(path), sizeof(payload) - 1);
    payload[0] = truncate ? '1' : '0';
    memcpy(payload + 1, path, length);
    Push(Kind::Session, Level::Info, payload, length + 1, true);
}

/// A guest thread's name (and its fiber's), else the name the host thread gave itself
/// (thread_name.h): read from memory, no system call.
void CurrentThreadName(char* out, size_t size) {
    using namespace Libraries::Kernel;
    const char* name = thread_name_get();
    size_t length = 0;
    char guest[WORD_NAME_MAX + 1];
    if (g_curthread && word_name_get(&g_curthread->name, guest) != 0) {
        const int result = g_curthread->tcb->tcb_fiber
                               ? snprintf(out, size, "%s@@%s", guest,
                                          g_curthread->tcb->tcb_fiber->current_fiber->name)
                               : snprintf(out, size, "%s", guest);
        length = MinSize(result > 0 ? static_cast<size_t>(result) : 0, size - 1);
    } else if (*name) {
        length = MinSize(strlen(name), size - 1);
        memcpy(out, name, length);
    } else {
        static constexpr char unknown[] = "<unknown name>";
        length = MinSize(sizeof(unknown) - 1, size - 1);
        memcpy(out, unknown, length);
    }
    out[length] = '\0';
}

} // namespace

void Log(Class log_class, Level level, const char* file, int line, const char* func,
         const char* format, ...) {
    va_list args;
    va_start(args, format);
    VLog(log_class, level, file, line, func, format, args);
    va_end(args);
}

void VLog(Class log_class, Level level, const char* file, int line, const char* func,
          const char* format, va_list args) {
    if (retro_atomic_load_acquire_int(&g_running) == 0) {
        return;
    }
    char thread[96];
    CurrentThreadName(thread, sizeof(thread));
    const char* const fn = strcmp(func, "operator()") == 0 ? "lambda" : func;
    char text[MAX_RECORD];
    // The head, then the line, cut short at the record's size (its last byte is not text)
    const int head =
        snprintf(text, sizeof(text), "[%s] <%s> (%s) %s:%d %s: ", NameOf(log_class),
                 LEVEL_NAMES[static_cast<size_t>(level)], thread, BaseName(file), line, fn);
    size_t length = head > 0 ? MinSize(static_cast<size_t>(head), sizeof(text) - 1) : 0;
    if (length < sizeof(text) - 1) {
        const int body = vsnprintf(text + length, sizeof(text) - length, format, args);
        if (body > 0) {
            length = MinSize(length + static_cast<size_t>(body), sizeof(text) - 1);
        }
    }
    // Sync logging keeps every line, waiting for room; otherwise only critical lines wait.
    Push(Kind::Line, level, text, length,
         level == Level::Critical || (retro_atomic_load_relaxed_int(&g_sync) != 0));
}

void Setup(const char* shadps4_filename) {
    static run_once_t already_registered = RUN_ONCE_INIT;
    run_once(
        &already_registered,
        [](void*) {
            atexit(Shutdown);
            Common::AtQuickExit(Flush);
        },
        nullptr);

    g_control.Run([&] {
        InitRing();
        if (retro_atomic_load_acquire_int(&g_running) != 0) {
            return;
        }
#ifdef _WIN32
        g_debugger_console = strcmp(emu_settings_get_log_type(), "wincolor") != 0;
#endif
        retro_atomic_store_relaxed_64(&g_size_limit, emu_settings_get_log_size_limit());
        UpdateSinks();

        // The writer starts with nothing queued, so it can own the files from here on.
        char main_path[MAX_RECORD];
        fill_pathname_join_special(main_path, GetUserPathUTF8(Common::FS::PathType::LogDir),
                                   shadps4_filename, sizeof(main_path));
        g_w.main = OpenFile(main_path, false);
        retro_atomic_store_relaxed_int(&g_stop, 0);
        stop_thread_start(&g_writer, [](void*) { WriterLoop(); }, nullptr);
        retro_atomic_store_release_int(&g_running, 1);
    });
}

void Switch(const char* game_filename, bool append_log) {
    UpdateSinks();
    UpdateLogLevels(emu_settings_get_log_filter());
    UpdateLogFlushLevel(emu_settings_get_log_flush_level());
    retro_atomic_store_relaxed_64(&g_size_limit, emu_settings_get_log_size_limit());
    if (retro_atomic_load_acquire_int(&g_running) == 0) {
        return;
    }
    char session_path[MAX_RECORD];
    fill_pathname_join_special(session_path, GetUserPathUTF8(Common::FS::PathType::LogDir),
                               game_filename, sizeof(session_path));
    PushSession(session_path, !(append_log || emu_settings_get_log_append()));
}

void Shutdown() {
    g_control.Run([&] {
        if (retro_atomic_exchange_int(&g_running, 0) == 0) {
            return;
        }
        retro_atomic_store_release_int(&g_stop, 1);
        retro_eventcount_notify(&g_writer_ec);
        stop_thread_join(&g_writer);
    });
}

void Flush() {
    if (retro_atomic_load_acquire_int(&g_running) == 0) {
        return;
    }
    const u64 end = Push(Kind::Flush, Level::Info, "", 0, true);
    for (;;) {
        const int key = retro_eventcount_prepare_wait(&g_flush_ec);
        if (static_cast<u64>(retro_atomic_load_acquire_64(&g_flushed)) >= end ||
            retro_atomic_load_acquire_int(&g_running) == 0) {
            retro_eventcount_cancel_wait(&g_flush_ec);
            return;
        }
        retro_eventcount_commit_wait(&g_flush_ec, key);
    }
}

void UpdateSinks() {
    retro_atomic_store_relaxed_int(&g_sync, emu_settings_get_log_sync());
    retro_atomic_store_relaxed_int(&g_skip_duplicate, emu_settings_get_log_skip_duplicate());
    retro_atomic_store_relaxed_int(&g_max_skip_ms, emu_settings_get_log_max_skip_duration());
}

void UpdateLogLevels(const char* log_filter) {
    if (!emu_settings_get_log_enable()) {
        for (size_t i = 0; i < NUM_LOG_CLASSES; ++i) {
            g_class_levels[i] = Level::Off;
        }
        return;
    }
    Level default_level = Level::Info;
    Level levels[NUM_LOG_CLASSES];
    bool named[NUM_LOG_CLASSES]{};
    // Entries "class:level", apart by spaces; "*" for the class sets every other one
    for (const char* entry = log_filter; *entry;) {
        const char* const space = strchr(entry, ' ');
        const size_t length = space ? static_cast<size_t>(space - entry) : strlen(entry);
        const char* const colon = static_cast<const char*>(memchr(entry, ':', length));
        if (!colon || memchr(colon + 1, ':', length - (colon + 1 - entry))) {
            LOG_ERROR(Config, "bad log filter provided");
        } else {
            const size_t name_length = static_cast<size_t>(colon - entry);
            const Level level = LevelFromString(colon + 1, length - name_length - 1);
            if (name_length && entry[0] == '*') {
                default_level = level;
            } else {
                for (size_t i = 0; i < NUM_LOG_CLASSES; ++i) {
                    const char* const class_name = NameOf(static_cast<Class>(i));
                    if (strlen(class_name) == name_length &&
                        memcmp(class_name, entry, name_length) == 0) {
                        levels[i] = level;
                        named[i] = true;
                    }
                }
            }
        }
        entry += length;
        if (*entry == ' ') {
            ++entry;
        }
    }
    for (size_t i = 0; i < NUM_LOG_CLASSES; ++i) {
        g_class_levels[i] = named[i] ? levels[i] : default_level;
    }
}

void UpdateLogFlushLevel(const char* log_flush_level) {
    if (log_flush_level[0]) {
        retro_atomic_store_relaxed_int(
            &g_flush_level,
            static_cast<int>(LevelFromString(log_flush_level, strlen(log_flush_level))));
    }
}

} // namespace Common::Log
