/* What stands where the core had std::call_once (src/common/run_once.c): the function is
 * called once however many threads come to it together, every one of them returns only after
 * it has ended, and a later call costs nothing. Four threads at 2000 onces, under both
 * sanitizers. */

#include <stdio.h>

#include <retro_atomic.h>
#include <rthreads/rthreads.h>

#include "common/run_once.h"

static int failures = 0;

#define CHECK(cond, what)                                                                      \
    do {                                                                                       \
        if (!(cond)) {                                                                         \
            printf("run_once: FAILED: %s\n", what);                                            \
            failures++;                                                                        \
        }                                                                                      \
    } while (0)

#define THREADS 4
#define ONCES 2000

static run_once_t onces[ONCES];
static retro_atomic_int_t calls[ONCES], ended[ONCES];
static retro_atomic_int_t early, start;

static void slow_init(void* arg) {
    const int i = (int)(size_t)arg;
    int spin;
    (void)retro_atomic_fetch_add_int(&calls[i], 1);
    /* Long enough for the others to arrive while it runs */
    for (spin = 0; spin < 200; spin++)
        sthread_yield();
    retro_atomic_store_release_int(&ended[i], 1);
}

static void racer(void* arg) {
    int i;
    (void)arg;
    while (!retro_atomic_load_acquire_int(&start))
        sthread_yield();
    for (i = 0; i < ONCES; i++) {
        run_once(&onces[i], slow_init, (void*)(size_t)i);
        if (!retro_atomic_load_acquire_int(&ended[i]))
            (void)retro_atomic_fetch_add_int(&early, 1);
    }
}

static int plain_calls;
static void plain(void* arg) {
    (void)arg;
    plain_calls++;
}

int main(void) {
    sthread_t* threads[THREADS];
    static run_once_t one = RUN_ONCE_INIT, flag = RUN_ONCE_INIT;
    int i, wrong = 0;

    run_once_setup();
    run_once_setup();
    run_once(&one, plain, NULL);
    run_once(&one, plain, NULL);
    run_once(&one, plain, NULL);
    CHECK(plain_calls == 1, "called once, however often asked");
    CHECK(run_once_first(&flag) && !run_once_first(&flag) && !run_once_first(&flag),
          "the first asker alone is first");

    retro_atomic_int_init(&early, 0);
    retro_atomic_int_init(&start, 0);
    for (i = 0; i < THREADS; i++)
        threads[i] = sthread_create(racer, NULL);
    retro_atomic_store_release_int(&start, 1);
    for (i = 0; i < THREADS; i++)
        sthread_join(threads[i]);
    for (i = 0; i < ONCES; i++)
        if (retro_atomic_load_acquire_int(&calls[i]) != 1)
            wrong++;
    CHECK(wrong == 0, "each called exactly once by four threads together");
    CHECK(retro_atomic_load_acquire_int(&early) == 0,
          "no thread returned before the call had ended");

    if (failures) {
        printf("run_once: %d failed\n", failures);
        return 1;
    }
    printf("run_once: all passed (%d onces, %d threads)\n", ONCES, THREADS);
    return 0;
}
