// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// The launcher of the libretro core's emulator process, where there is no rundll32. It is
// built into the core, which runs it from memory (libretro_core.cpp):
//
//   parallel_ps4_libretro_host <path of the core> <descriptor of the shared memory>
//
// It loads the core and calls its retro_host_main (host.cpp). An executable of its own gives
// the emulator what the frontend's process cannot: an address space with nothing in the
// range the game is built for.

#define _GNU_SOURCE
#include <dlfcn.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/prctl.h>
#include <sys/resource.h>
#endif

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "parallel_ps4_libretro_host is started by the shadPS4 libretro core\n");
        return 2;
    }

#ifdef __linux__
    // Ended with the frontend, however the frontend ends.
    const pid_t parent = getppid();
    prctl(PR_SET_PDEATHSIG, SIGKILL);
    if (getppid() != parent) {
        return 1;
    }

    // No core dump of this process. Its address space is the console's, with gigabytes of
    // shared memory behind it: a dump takes minutes and can run the machine out of memory,
    // and until it is done the process has not ended, so the core cannot tell the frontend
    // of the crash and the picture just stands still. A limit of 1 is the one that also
    // stops a dump piped to a crash handler; 0 does not.
    struct rlimit core_limit;
    if (getrlimit(RLIMIT_CORE, &core_limit) == 0) {
        core_limit.rlim_cur = core_limit.rlim_max < 1 ? core_limit.rlim_max : 1;
        setrlimit(RLIMIT_CORE, &core_limit);
    }
#endif

    void* core = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!core) {
        fprintf(stderr, "parallel_ps4_libretro_host: %s\n", dlerror());
        return 2;
    }
    int (*host_main)(const char*) = (int (*)(const char*))dlsym(core, "retro_host_main");
    if (!host_main) {
        fprintf(stderr, "parallel_ps4_libretro_host: %s\n", dlerror());
        return 2;
    }
    return host_main(argv[2]);
}
