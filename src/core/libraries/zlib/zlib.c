/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stddef.h>

#include <boolean.h>
#include <encodings/deflate.h>
#include <retro_atomic.h>
#include <rthreads/retro_eventcount.h>

#include "common/c_bridge.h"
#include "common/combine.h"
#include "common/thread_name.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/kernel/kernel_c.h"
#include "core/libraries/zlib/zlib.h"
#include "core/libraries/zlib/zlib_error.h"

/* The requests asked for and not yet taken by the thread, and the results kept, at most: a
 * request's task and result are in slot (id % ZLIB_SLOTS). A result is kept until the request
 * ZLIB_SLOTS after it is done. */
#define ZLIB_SLOTS 256

typedef struct zlib_task {
    const void* src;
    u32 src_length;
    void* dst;
    u32 dst_length;
} zlib_task;

typedef struct zlib_result {
    u64 request_id; /* 0: none in this slot */
    u32 length;
    s32 status;
} zlib_result;

/* The requests are numbered from 1 and done in that order, by the one thread: the ones asked
 * for are 1 to next_request_id - 1, the thread has taken 1 to taken, finished 1 to done, and
 * sceZlibWaitForDone has handed out 1 to waited. What were a queue of tasks, a queue of done
 * ids and a map of results are the counts and two tables, made once: nothing is allocated. */
static struct {
    bool made;
    kernel_service_thread_t thread;
    retro_atomic_int_t stop;
    combiner_t mutex;
    retro_eventcount_t task_ec;
    retro_eventcount_t done_ec;
    u64 next_request_id;
    u64 taken;
    u64 done;
    u64 waited;
    zlib_task tasks[ZLIB_SLOTS];
    zlib_result results[ZLIB_SLOTS];
} g_zlib;

void zlib_library_init(void) {
    if (!g_zlib.made) {
        g_zlib.made = true;
        combiner_init(&g_zlib.mutex);
        retro_eventcount_init(&g_zlib.task_ec);
        retro_eventcount_init(&g_zlib.done_ec);
    }
}

static bool zlib_stopping(void) {
    return retro_atomic_load_acquire_int(&g_zlib.stop) != 0;
}

static bool zlib_started(void) {
    return g_zlib.thread.thread != NULL;
}

/* Takes the next task, if there is one */
typedef struct zlib_pop_call {
    bool got;
    u64 request_id;
    zlib_task task;
} zlib_pop_call;

static void zlib_pop_locked(void* ctx) {
    zlib_pop_call* c = (zlib_pop_call*)ctx;
    if (g_zlib.taken + 1 == g_zlib.next_request_id) {
        c->got = false;
        return;
    }
    g_zlib.taken++;
    c->request_id = g_zlib.taken;
    c->task = g_zlib.tasks[g_zlib.taken % ZLIB_SLOTS];
    c->got = true;
}

/* Whether a task was taken, or a stop asked for */
static bool zlib_pop_ready(zlib_pop_call* c) {
    combiner_run(&g_zlib.mutex, zlib_pop_locked, c);
    return c->got || zlib_stopping();
}

/* The finished request's result, and it done */
typedef struct zlib_finish_call {
    u64 request_id;
    u32 length;
    s32 status;
} zlib_finish_call;

static void zlib_finish_locked(void* ctx) {
    zlib_finish_call* c = (zlib_finish_call*)ctx;
    zlib_result* r = &g_zlib.results[c->request_id % ZLIB_SLOTS];
    r->request_id = c->request_id;
    r->length = c->length;
    r->status = c->status;
    g_zlib.done = c->request_id;
}

static void zlib_task_thread(void* arg) {
    /* One inflater for the thread, reset per request rather than made each time. */
    void* inflater;
    (void)arg;
    thread_name_set("shadPS4:ZlibTaskThread");
    inflater = rinflate_new(15);

    while (!zlib_stopping()) {
        zlib_pop_call pop;
        zlib_finish_call finish;
        size_t decompressed_length = 0;
        size_t consumed = 0;
        int ret = RDEFLATE_PROCESS_ERROR;

        /* Pop from the task queue, unless stop has been requested (which wakes the wait) */
        while (!zlib_pop_ready(&pop)) {
            const int key = retro_eventcount_prepare_wait(&g_zlib.task_ec);
            if (zlib_pop_ready(&pop)) {
                retro_eventcount_cancel_wait(&g_zlib.task_ec);
                break;
            }
            retro_eventcount_commit_wait(&g_zlib.task_ec, key);
        }
        if (!pop.got) {
            break;
        }

        /* A zlib stream (RFC 1950), whole, into the game's buffer: what uncompress() does. */
        if (inflater) {
            rinflate_reset(inflater, 15);
            rinflate_set_in(inflater, (const uint8_t*)pop.task.src, pop.task.src_length);
            rinflate_set_out(inflater, (uint8_t*)pop.task.dst, pop.task.dst_length);
            do {
                size_t read = 0;
                size_t wrote = 0;
                ret = rinflate_process(inflater, &read, &wrote);
                consumed += read;
                decompressed_length += wrote;
            } while (ret == RDEFLATE_PROCESS_NEXT && consumed < pop.task.src_length &&
                     decompressed_length < pop.task.dst_length);
        }

        /* Insert the new result, and the request is done. Suspended with the output full,
         * or with input left: no space, as uncompress() has it, whether or not the inflater
         * has read ahead to the end of the input. Suspended with input gone and room left,
         * the stream ends early, which uncompress() reports as bad data. */
        finish.request_id = pop.request_id;
        finish.length = (u32)decompressed_length;
        if (ret == RDEFLATE_PROCESS_END) {
            finish.status = ORBIS_OK;
        } else if (ret == RDEFLATE_PROCESS_NEXT &&
                   (consumed < pop.task.src_length || decompressed_length == pop.task.dst_length)) {
            finish.status = ORBIS_ZLIB_ERROR_NOSPACE;
        } else {
            finish.status = ORBIS_ZLIB_ERROR_FATAL;
        }
        combiner_run(&g_zlib.mutex, zlib_finish_locked, &finish);
        retro_eventcount_notify(&g_zlib.done_ec);
    }
    rinflate_free(inflater);
}

s32 PS4_SYSV_ABI sceZlibInitialize(const void* buffer, u32 length) {
    u32 i;
    (void)buffer;
    (void)length;
    c_log(C_LOG_LIB_ZLIB, C_LOG_LEVEL_INFO, "sceZlibInitialize", "called");
    if (zlib_started()) {
        return ORBIS_ZLIB_ERROR_ALREADY_INITIALIZED;
    }

    /* Initialize with empty task data: no thread runs to see it yet */
    g_zlib.next_request_id = 1;
    g_zlib.taken = 0;
    g_zlib.done = 0;
    g_zlib.waited = 0;
    for (i = 0; i < ZLIB_SLOTS; i++) {
        g_zlib.results[i].request_id = 0;
    }
    retro_atomic_store_release_int(&g_zlib.stop, 0);

    if (!kernel_service_thread_start(&g_zlib.thread, zlib_task_thread, NULL)) {
        c_log(C_LOG_LIB_ZLIB, C_LOG_LEVEL_ERROR, "sceZlibInitialize",
              "The task thread could not be made");
        return ORBIS_ZLIB_ERROR_FATAL;
    }
    return ORBIS_OK;
}

/* A new request, given the next id, if a slot is free for it */
typedef struct zlib_inflate_call {
    zlib_task task;
    u64* request_id;
    s32 result;
} zlib_inflate_call;

static void zlib_inflate_locked(void* ctx) {
    zlib_inflate_call* c = (zlib_inflate_call*)ctx;
    if (g_zlib.next_request_id - 1 - g_zlib.taken >= ZLIB_SLOTS) {
        c->result = ORBIS_ZLIB_ERROR_BUSY;
        return;
    }
    *c->request_id = g_zlib.next_request_id;
    g_zlib.tasks[g_zlib.next_request_id % ZLIB_SLOTS] = c->task;
    g_zlib.next_request_id++;
    c->result = ORBIS_OK;
}

s32 PS4_SYSV_ABI sceZlibInflate(const void* src, u32 src_len, void* dst, u32 dst_len,
                                u64* request_id) {
    zlib_inflate_call c;
    c_log(C_LOG_LIB_ZLIB, C_LOG_LEVEL_DEBUG, "sceZlibInflate", "called");
    if (!zlib_started()) {
        return ORBIS_ZLIB_ERROR_NOT_INITIALIZED;
    }
    if (!src || !src_len || !dst || !dst_len || !request_id || dst_len > 64 * 1024 ||
        dst_len % (2 * 1024) != 0) {
        return ORBIS_ZLIB_ERROR_INVALID;
    }

    c.task.src = src;
    c.task.src_length = src_len;
    c.task.dst = dst;
    c.task.dst_length = dst_len;
    c.request_id = request_id;
    combiner_run(&g_zlib.mutex, zlib_inflate_locked, &c);
    if (c.result == ORBIS_OK) {
        retro_eventcount_notify(&g_zlib.task_ec);
    }
    return c.result;
}

/* Pops from the done queue, if anything is in it */
typedef struct zlib_take_call {
    u64* request_id;
    bool got;
} zlib_take_call;

static void zlib_take_locked(void* ctx) {
    zlib_take_call* c = (zlib_take_call*)ctx;
    if (g_zlib.waited == g_zlib.done) {
        c->got = false;
        return;
    }
    g_zlib.waited++;
    *c->request_id = g_zlib.waited;
    c->got = true;
}

static int zlib_take(void* ctx) {
    zlib_take_call* c = (zlib_take_call*)ctx;
    combiner_run(&g_zlib.mutex, zlib_take_locked, c);
    return c->got ? 1 : 0;
}

s32 PS4_SYSV_ABI sceZlibWaitForDone(u64* request_id, const u32* timeout) {
    zlib_take_call c;
    u64 deadline;
    c_log(C_LOG_LIB_ZLIB, C_LOG_LEVEL_DEBUG, "sceZlibWaitForDone", "called");
    if (!zlib_started()) {
        return ORBIS_ZLIB_ERROR_NOT_INITIALIZED;
    }
    if (!request_id) {
        return ORBIS_ZLIB_ERROR_INVALID;
    }

    /* Pop from the done queue, unless the timeout is reached. The timeout is of the game's
     * time (milliseconds); none: until one is done */
    c.request_id = request_id;
    if (zlib_take(&c)) {
        return ORBIS_OK;
    }
    if (timeout && *timeout == 0) {
        return ORBIS_ZLIB_ERROR_TIMEDOUT;
    }
    deadline = timeout ? kernel_game_deadline_in((u64)*timeout * 1000000) : 0;
    if (!kernel_game_wait(&g_zlib.done_ec, deadline, zlib_take, &c)) {
        return ORBIS_ZLIB_ERROR_TIMEDOUT;
    }
    return ORBIS_OK;
}

/* The result of a request, if it is kept */
typedef struct zlib_get_result_call {
    u64 request_id;
    u32* dst_length;
    s32* status;
    s32 result;
} zlib_get_result_call;

static void zlib_get_result_locked(void* ctx) {
    zlib_get_result_call* c = (zlib_get_result_call*)ctx;
    const zlib_result* r = &g_zlib.results[c->request_id % ZLIB_SLOTS];
    if (c->request_id == 0 || r->request_id != c->request_id) {
        c->result = ORBIS_ZLIB_ERROR_NOT_FOUND;
        return;
    }
    *c->dst_length = r->length;
    *c->status = r->status;
    c->result = ORBIS_OK;
}

s32 PS4_SYSV_ABI sceZlibGetResult(u64 request_id, u32* dst_length, s32* status) {
    zlib_get_result_call c;
    c_log(C_LOG_LIB_ZLIB, C_LOG_LEVEL_DEBUG, "sceZlibGetResult", "called");
    if (!zlib_started()) {
        return ORBIS_ZLIB_ERROR_NOT_INITIALIZED;
    }
    if (!dst_length || !status) {
        return ORBIS_ZLIB_ERROR_INVALID;
    }

    c.request_id = request_id;
    c.dst_length = dst_length;
    c.status = status;
    combiner_run(&g_zlib.mutex, zlib_get_result_locked, &c);
    return c.result;
}

s32 PS4_SYSV_ABI sceZlibFinalize(void) {
    c_log(C_LOG_LIB_ZLIB, C_LOG_LEVEL_INFO, "sceZlibFinalize", "called");
    if (!zlib_started()) {
        return ORBIS_ZLIB_ERROR_NOT_INITIALIZED;
    }
    /* With a full fence, as an eventcount's notifier's state change */
    (void)retro_atomic_exchange_int(&g_zlib.stop, 1);
    retro_eventcount_notify(&g_zlib.task_ec);
    kernel_service_thread_join(&g_zlib.thread);
    retro_atomic_store_release_int(&g_zlib.stop, 0);
    return ORBIS_OK;
}
