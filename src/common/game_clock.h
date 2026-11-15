/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_COMMON_GAME_CLOCK_H
#define SHADPS4_COMMON_GAME_CLOCK_H

#include <stdint.h>

#include <retro_atomic.h>
#include <rthreads/retro_eventcount.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The game's time, in nanoseconds, from the frontend's frames rather than a wall clock.
 * Each retro_run starts a frame (game_clock_frame): the time moves to the frame's start,
 * and the frame's end is the most it may reach before the next one. Within a frame it moves
 * only when the game's threads are all idle (game_clock_advance), to the earliest deadline
 * they wait for, never past the frame's end: so a game gets each frame's worth of time, and
 * no more, however fast or slow the host runs it. The frame's length is exact over time
 * (1e9 / 60 ns, its fraction carried). One thread starts frames; any thread reads and
 * advances. */
/* What the game's threads wait for on the game's time: a deadline, and what is done when
 * the time reaches it (fire, called once, on the thread that moved the time) */
#define GAME_CLOCK_TIMERS 1024

typedef struct game_clock_timer {
    retro_atomic_64_t deadline; /* 0: free; GAME_CLOCK_FIRING, GAME_CLOCK_FIRED once due */
    void (*fire)(void* ctx);
    void* ctx;
    int oneshot; /* freed once fired: nobody disarms it */
} game_clock_timer_t;

/* A game thread parked on something tracked, counted idle: the word its wake bumps (an
 * eventcount's epoch, a semaphore's releases) and its value when it parked. A word moved on
 * since is a wake in flight: the thread is about to run, and the time waits for it. */
#define GAME_CLOCK_PARKED 1024

typedef struct game_clock_parked {
    retro_atomic_ptr_t word; /* retro_atomic_int_t*, NULL: free */
    retro_atomic_int_t key;
    /* The frame its wake was first seen in flight in, 0: not seen. A wake holds the time for
     * the rest of that frame at most: a thread woken that does not get to run (suspended)
     * cannot hold it for longer. */
    retro_atomic_int_t flight_frame;
} game_clock_parked_t;

typedef struct game_clock {
    retro_atomic_64_t now;       /* the time */
    retro_atomic_64_t frame_end; /* the most the time may reach before the next frame */
    retro_atomic_64_t frames;    /* frames started */
    uint64_t period;             /* a frame's whole nanoseconds */
    uint64_t rem;                /* and the fraction's numerator, over fps_num */
    uint64_t acc;
    uint64_t fps_num;
    /* The game's threads not waiting on something tracked (game_clock_idle/busy): at none,
     * the time moves on to the earliest deadline */
    retro_atomic_int_t running;
    retro_atomic_64_t free_timers[GAME_CLOCK_TIMERS / 64]; /* a bit a timer, set if free */
    game_clock_timer_t timers[GAME_CLOCK_TIMERS];
    retro_atomic_64_t free_parked[GAME_CLOCK_PARKED / 64]; /* a bit a slot, set if free */
    game_clock_parked_t parked[GAME_CLOCK_PARKED];
    /* The frame is wanted finished (game_clock_finish): its deadlines are not held back by
     * threads running */
    retro_atomic_int_t finishing;
    /* The frame done, or 0 (game_clock_set_done), and who is told */
    retro_atomic_int_t* done_frame;
    void (*done)(void* ctx);
    void* done_ctx;
} game_clock_t;

/* At time 0, no frame started (the time stands until the first), frames of
 * @fps_den / @fps_num seconds (60/1: 1/60 s) */
void game_clock_init(game_clock_t* clock, unsigned fps_num, unsigned fps_den);

/* A frame starts (retro_run): the time moves to its start (the last frame's end), if the
 * game had not used all of the last one. The one thread that starts frames. */
void game_clock_frame(game_clock_t* clock);

/* The frame is done when the game's threads are all idle and none waits for a time within
 * it: nothing more happens in it until something outside the game wakes a thread (the next
 * frame, the GPU, input). Then *@done_frame is set to the frame's number (frames started)
 * and @done(@ctx) is called, on the thread that made it so; a game thread running again sets
 * it back to 0. @done_frame may be in memory another process reads. Set before the frames
 * start. */
void game_clock_set_done(game_clock_t* clock, retro_atomic_int_t* done_frame,
                         void (*done)(void* ctx), void* ctx);

/* The frontend wants the frame finished (it waits for its sound): from here until the next
 * frame starts, the time moves to each deadline within the frame as soon as it is armed, and
 * what is due fires, whoever is running. So a thread that never waits (a spin with no yield)
 * does not hold the frame, and the frame's sleepers all get their time. */
void game_clock_finish(game_clock_t* clock);

/* A cadence on the game's time: something done every @units of @rate a second (a device's
 * buffer: frames of a sample rate), counted from *@start in whole units so it does not drift.
 * Returns when the next one is due, and counts it. The first one, and the first after more
 * than eight periods without one (a stall), are due now, and the cadence starts again there.
 * @start and @count are the caller's, 0 to begin. */
uint64_t game_clock_cadence(game_clock_t* clock, uint64_t* start, uint64_t* count, uint64_t units,
                            uint64_t rate);

/* The time now */
uint64_t game_clock_now(game_clock_t* clock);

/* The end of the frame under way */
uint64_t game_clock_frame_end(game_clock_t* clock);

/* Frames started */
uint64_t game_clock_frames(game_clock_t* clock);

/* The game's threads are all idle until @until: the time moves there, or to the frame's end
 * if that is sooner, never back. @return the time now. */
uint64_t game_clock_advance(game_clock_t* clock, uint64_t until);

/* A deadline on the game's time: @fire(@ctx) is called once the time reaches it, on the
 * thread that moved the time (or now, by this thread, if it is not in the future). @return the
 * timer, for game_clock_disarm, or -1 if all are taken. */
int game_clock_arm(game_clock_t* clock, uint64_t deadline, void (*fire)(void*), void* ctx);

/* The same, freed once it has fired, with nobody to disarm it (a timer that cannot be
 * called off: its fire looks whether it is still wanted) */
int game_clock_arm_oneshot(game_clock_t* clock, uint64_t deadline, void (*fire)(void*), void* ctx);

/* Done with a timer. @return 1 if it fired (its fire has returned), 0 if it had not and now
 * will not */
int game_clock_disarm(game_clock_t* clock, int timer);

/* The calling game thread starts or stops waiting on something tracked. When the last
 * running one stops, the time moves to the earliest deadline (within the frame) and the
 * timers due fire. A game thread counts as running from game_clock_busy at its start to
 * game_clock_idle at its end. */
void game_clock_idle(game_clock_t* clock);
void game_clock_busy(game_clock_t* clock);

/* A counted game thread parks, idle, on a wait whose wake bumps *@word (@key: its value
 * now): until it has run again (game_clock_unpark), a wake of it in flight (the word moved
 * on) holds the time, so that the time does not skip past a thread woken by another that then
 * waits (an engine's output thread waking its mixer, then sleeping to its next buffer). Each
 * time it parks again in the same wait, game_clock_repark with the new key. Returns the
 * slot, or -1 if none is left (the wait goes on untracked). */
int game_clock_park(game_clock_t* clock, retro_atomic_int_t* word, int key);
void game_clock_repark(game_clock_t* clock, int slot, int key);
void game_clock_unpark(game_clock_t* clock, int slot);

/* The game threads counted running */
int game_clock_running(game_clock_t* clock);

/* A counted game thread yields (sched_yield, usleep(0), a spin on a lock): it waits, on
 * nothing the clock tracks, for another thread. It counts idle from here, through the yields
 * that follow, until it next waits on something tracked or ends (game_clock_resume): the
 * time is not held at the frame's start by a thread that only spins. @yielded is the
 * thread's own, 0 to start. */
void game_clock_yield(game_clock_t* clock, int* yielded);
void game_clock_resume(game_clock_t* clock, int* yielded);

/* Waits on @ec until @ready(@ctx) is nonzero or the game's time reaches @deadline (0: none),
 * as a game thread (@counted) not counted running meanwhile: no host timeout, no wall clock.
 * A timer of the game's clock wakes it at the deadline and hands it its count back.
 * @return 1 if @ready ended it, 0 if the deadline did. */
int game_clock_wait(game_clock_t* clock, retro_eventcount_t* ec, uint64_t deadline,
                    int (*ready)(void* ctx), void* ctx, int counted);

#ifdef __cplusplus
}
#endif

#endif
