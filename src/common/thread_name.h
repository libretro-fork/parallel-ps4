/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_COMMON_THREAD_NAME_H
#define SHADPS4_COMMON_THREAD_NAME_H

#ifdef __cplusplus
extern "C" {
#endif

/* The longest name kept, without the terminator: a longer one is cut */
#define THREAD_NAME_MAX 63

/* Names the calling thread: for the log (thread_name_get) and for the system
 * (sthread_setname, which cuts it to what the system takes). A copy is kept, in the
 * thread's own storage: nothing is allocated. NULL names it "". */
void thread_name_set(const char* name);

/* The calling thread's name, as last set, or "" if it was never set. The string lives
 * as long as the thread, until its next thread_name_set. */
const char* thread_name_get(void);

#ifdef __cplusplus
}
#endif

#endif
