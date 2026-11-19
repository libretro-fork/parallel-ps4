/* The service thread that can be asked to stop (src/common/stop_thread.c), which stands where
 * the core had std::jthread and stop tokens: started, it runs; asked to stop and woken, it
 * ends and is joined; it can be started again; one never started is joined at no cost. A
 * thread asked to stop as it goes into its wait is not left asleep, 5000 times over under the
 * thread sanitizer. */

#include <stdio.h>

#include <retro_atomic.h>
#include <rthreads/retro_eventcount.h>

#include "common/stop_thread.h"

static int failures = 0;

#define CHECK(cond, what)                                                                      \
    do {                                                                                       \
        if (!(cond)) {                                                                         \
            printf("stop_thread: FAILED: %s\n", what);                                         \
            failures++;                                                                        \
        }                                                                                      \
    } while (0)

static stop_thread_t worker = STOP_THREAD_INIT;
static retro_eventcount_t work;
static retro_atomic_int_t jobs, done, running;

/* Does each job it is given, waits for the next, ends when asked to stop */
static void work_loop(void* arg) {
    (void)arg;
    retro_atomic_store_release_int(&running, 1);
    for (;;) {
        int key;
        while (retro_atomic_load_acquire_int(&done) < retro_atomic_load_acquire_int(&jobs))
            (void)retro_atomic_fetch_add_int(&done, 1);
        if (stop_thread_stopping(&worker))
            break;
        key = retro_eventcount_prepare_wait(&work);
        if (retro_atomic_load_acquire_int(&done) < retro_atomic_load_acquire_int(&jobs) ||
            stop_thread_stopping(&worker)) {
            retro_eventcount_cancel_wait(&work);
            continue;
        }
        retro_eventcount_commit_wait(&work, key);
    }
    retro_atomic_store_release_int(&running, 0);
}

int main(void) {
    int i;
    retro_eventcount_init(&work);
    retro_atomic_int_init(&jobs, 0);
    retro_atomic_int_init(&done, 0);
    retro_atomic_int_init(&running, 0);

    CHECK(!stop_thread_started(&worker) && !stop_thread_stopping(&worker), "not started at first");
    stop_thread_join(&worker);
    CHECK(!stop_thread_started(&worker), "joining one never started does nothing");

    for (i = 0; i < 5000; i++) {
        if (!stop_thread_start(&worker, work_loop, NULL)) {
            CHECK(0, "started");
            break;
        }
        if (i == 0)
            CHECK(stop_thread_started(&worker) && !stop_thread_start(&worker, work_loop, NULL),
                  "started once: not a second time");
        /* A job every other round, so the stop finds the thread both working and on its way
         * into its wait */
        if (i & 1) {
            (void)retro_atomic_fetch_add_int(&jobs, 1);
            retro_eventcount_notify(&work);
        }
        stop_thread_request(&worker);
        retro_eventcount_notify(&work);
        stop_thread_join(&worker);
        if (stop_thread_started(&worker) || retro_atomic_load_acquire_int(&running) ||
            retro_atomic_load_acquire_int(&done) != retro_atomic_load_acquire_int(&jobs)) {
            CHECK(0, "asked to stop: its jobs done, ended, joined");
            break;
        }
    }
    CHECK(stop_thread_start(&worker, work_loop, NULL) && !stop_thread_stopping(&worker),
          "started again: not asked to stop any more");
    stop_thread_request(&worker);
    retro_eventcount_notify(&work);
    stop_thread_join(&worker);
    retro_eventcount_free(&work);

    if (failures) {
        printf("stop_thread: %d failed\n", failures);
        return 1;
    }
    printf("stop_thread: all passed (5000 threads started, stopped and joined)\n");
    return 0;
}
