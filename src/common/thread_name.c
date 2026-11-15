/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <string.h>

#include <rthreads/rthreads.h>

#include "common/thread_name.h"

#if defined(_MSC_VER)
#define THREAD_NAME_LOCAL __declspec(thread)
#else
#define THREAD_NAME_LOCAL __thread
#endif

static THREAD_NAME_LOCAL char thread_name_buf[THREAD_NAME_MAX + 1];

void thread_name_set(const char* name) {
    size_t len = name ? strlen(name) : 0;
    if (len > THREAD_NAME_MAX)
        len = THREAD_NAME_MAX;
    if (len)
        memcpy(thread_name_buf, name, len);
    thread_name_buf[len] = '\0';
    sthread_setname(thread_name_buf);
}

const char* thread_name_get(void) {
    return thread_name_buf;
}
