/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stddef.h>
#include <string.h>

#include "core/libraries/libc_internal/libc_internal_threads.h"

/* "SceLibcI_" and @name, or "SceLibcI", cut to what @size holds: as snprintf wrote it */
static void getMutexName(char* buf, size_t size, const char* name) {
    static const char prefix[] = "SceLibcI";
    size_t n = 0;
    size_t i;
    for (i = 0; prefix[i] != '\0' && n + 1 < size; i++) {
        buf[n++] = prefix[i];
    }
    if (name != NULL) {
        if (n + 1 < size) {
            buf[n++] = '_';
        }
        for (i = 0; name[i] != '\0' && n + 1 < size; i++) {
            buf[n++] = name[i];
        }
    }
    buf[n] = '\0';
}

s32 PS4_SYSV_ABI internal__Mtxinit(kernel_pthread_mutex_t* mtx, const char* name) {
    char mtx_name[0x20];
    kernel_pthread_mutexattr_t attr = NULL;
    s32 result;
    getMutexName(mtx_name, sizeof(mtx_name), name);

    result = kernel_pthread_mutexattr_init(&attr);
    if (result != 0) {
        return 1;
    }

    result = kernel_pthread_mutexattr_settype(&attr, KERNEL_PTHREAD_MUTEX_RECURSIVE);
    if (result == 0) {
        const s32 mtx_init_result = kernel_pthread_mutex_init_named(mtx, &attr, mtx_name);
        result = kernel_pthread_mutexattr_destroy(&attr);
        if (mtx_init_result == 0 && result == 0) {
            return 0;
        }
    } else {
        kernel_pthread_mutexattr_destroy(&attr);
    }

    return 1;
}

s32 PS4_SYSV_ABI internal__Mtxlock(kernel_pthread_mutex_t* mtx) {
    return kernel_pthread_mutex_lock(mtx) != 0;
}

s32 PS4_SYSV_ABI internal__Mtxunlock(kernel_pthread_mutex_t* mtx) {
    return kernel_pthread_mutex_unlock(mtx) != 0;
}

s32 PS4_SYSV_ABI internal__Mtxdst(kernel_pthread_mutex_t* mtx) {
    return kernel_pthread_mutex_destroy(mtx) != 0;
}
