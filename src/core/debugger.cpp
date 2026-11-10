//  SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
//  SPDX-License-Identifier: GPL-2.0-or-later

#include "debugger.h"

#include <iostream>
#include <thread>

#if defined(_WIN32)
#include <Windows.h>
#include <debugapi.h>
#elif defined(__linux__)
#include <cstdio>
#include <unistd.h>

#include <file/file_path.h>
#include <streams/file_stream.h>
#elif defined(__APPLE__) || defined(__FreeBSD__)
#include <errno.h>
#include <signal.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <unistd.h>
#endif

bool Core::Debugger::IsDebuggerAttached() {
#if defined(_WIN32)
    return IsDebuggerPresent();
#elif defined(__linux__)
    RFILE* status_file = filestream_open("/proc/self/status", RETRO_VFS_FILE_ACCESS_READ,
                                         RETRO_VFS_FILE_ACCESS_HINT_NONE);
    if (!status_file) {
        return false;
    }
    char line[256];
    int tracer_pid = 0;
    while (filestream_gets(status_file, line, sizeof(line))) {
        if (std::sscanf(line, "TracerPid: %d", &tracer_pid) == 1) {
            break;
        }
    }
    filestream_close(status_file);
    return tracer_pid != 0;
#elif defined(__APPLE__)
    int mib[4];
    struct kinfo_proc info;
    size_t size = sizeof(info);

    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC;
    mib[2] = KERN_PROC_PID;
    mib[3] = getpid();

    if (sysctl(mib, 4, &info, &size, nullptr, 0) == 0) {
        return (info.kp_proc.p_flag & P_TRACED) != 0;
    }
    return false;
#elif defined(__FreeBSD__)
    return false;
#else
#error "Unsupported platform"
#endif
}

void Core::Debugger::WaitForDebuggerAttach() {
    int count = 0;
    while (!IsDebuggerAttached()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        if (--count <= 0) {
            count = 10;
            std::cerr << "Waiting for debugger to attach..." << std::endl;
        }
    }
}
int Core::Debugger::GetCurrentPid() {
#if defined(_WIN32)
    return GetCurrentProcessId();
#elif defined(__APPLE__) || defined(__linux__) || defined(__FreeBSD__)
    return getpid();
#else
#error "Unsupported platform"
#endif
}

void Core::Debugger::WaitForPid(int pid) {
#if defined(_WIN32)
    HANDLE process_handle = OpenProcess(SYNCHRONIZE, FALSE, pid);
    if (process_handle != nullptr) {
        std::cerr << "Waiting for process " << pid << " to exit..." << std::endl;
        WaitForSingleObject(process_handle, INFINITE);
        CloseHandle(process_handle);
    }
#elif defined(__linux__)
    std::string proc_path = "/proc/" + std::to_string(pid);

    while (path_is_directory(proc_path.c_str())) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        std::cerr << "Waiting for process " << pid << " to exit..." << std::endl;
    }
#elif defined(__APPLE__) || defined(__FreeBSD__)
    while (kill(pid, 0) == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        std::cerr << "Waiting for process " << pid << " to exit..." << std::endl;
    }
#else
#error "Unsupported platform"
#endif
}
