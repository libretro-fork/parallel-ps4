/* The latch that stands where the core had semaphores (src/common/event_latch.c): a wait
 * returns once it is set, at once if it already is, however many wait and however often it is
 * set; cleared, it is waited for again. Threads that wait while another sets it, many times
 * over, under the thread sanitizer: no wait is left behind by a set that came just before it
 * slept. */

#include <stdio.h>

#include <retro_atomic.h>
#include <rthreads/rthreads.h>

#include "common/event_latch.h"

static int failures = 0;

#define CHECK(cond, what)                                                                      \
    do {                                                                                       \
        if (!(cond)) {                                                                         \
            printf("event_latch: FAILED: %s\n", what);                                         \
            failures++;                                                                        \
        }                                                                                      \
    } while (0)

#define WAITERS 3
#define ROUNDS 20000

static event_latch_t go[WAITERS], done[WAITERS], all;
static retro_atomic_int_t early;
static int seen[WAITERS];

/* Answers each set of its own latch, then waits with the others on the one they share */
static void waiter(void* data) {
    const int me = (int)(size_t)data;
    int r;
    for (r = 0; r < ROUNDS; r++) {
        event_latch_wait(&go[me]);
        if (!event_latch_is_set(&go[me]))
            (void)retro_atomic_fetch_add_int(&early, 1);
        event_latch_clear(&go[me]);
        seen[me]++;
        event_latch_set(&done[me]);
    }
    event_latch_wait(&all);
}

int main(void) {
    sthread_t* threads[WAITERS];
    event_latch_t go0;
    int i, r;

    event_latch_init(&go0);
    CHECK(!event_latch_is_set(&go0), "not set at first");
    event_latch_set(&go0);
    event_latch_set(&go0);
    CHECK(event_latch_is_set(&go0), "set, and set again");
    event_latch_wait(&go0);
    event_latch_wait(&go0);
    CHECK(event_latch_is_set(&go0), "a wait on a set latch returns at once, and does not use it up");
    event_latch_clear(&go0);
    CHECK(!event_latch_is_set(&go0), "cleared");

    retro_atomic_int_init(&early, 0);
    event_latch_init(&all);
    for (i = 0; i < WAITERS; i++) {
        event_latch_init(&go[i]);
        event_latch_init(&done[i]);
        threads[i] = sthread_create(waiter, (void*)(size_t)i);
    }
    /* Set as the waiter is on its way into its wait, round after round: a set that a wait
     * slept through would leave this waiting for its answer for ever */
    for (r = 0; r < ROUNDS; r++) {
        for (i = 0; i < WAITERS; i++)
            event_latch_set(&go[i]);
        for (i = 0; i < WAITERS; i++) {
            event_latch_wait(&done[i]);
            event_latch_clear(&done[i]);
        }
    }
    /* One set wakes every thread waiting on the same latch */
    event_latch_set(&all);
    for (i = 0; i < WAITERS; i++)
        sthread_join(threads[i]);
    for (i = 0; i < WAITERS; i++) {
        CHECK(seen[i] == ROUNDS, "every waiter woke for every set");
        event_latch_free(&go[i]);
        event_latch_free(&done[i]);
    }
    CHECK(retro_atomic_load_acquire_int(&early) == 0, "no wait returned with the latch not set");
    event_latch_free(&all);
    event_latch_free(&go0);

    if (failures) {
        printf("event_latch: %d failed\n", failures);
        return 1;
    }
    printf("event_latch: all passed (%d waiters, %d rounds)\n", WAITERS, ROUNDS);
    return 0;
}
