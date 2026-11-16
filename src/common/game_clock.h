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
 * Each retro_run starts a frame (game_clock_frame): the frame's end is the most the time may
 * reach before the next one. The time steps from deadline to deadline, each once the threads
 * reacting to the last have waited again, and to the frame's start when none is due before
 * it, never past the frame's end: so a game gets each frame's worth of time, and no more,
 * however fast or slow the host runs it, and its deadlines come in order, never two before
 * the reaction to the first is done. The frame's length is exact over time
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

/* Reacting game threads waking others at once (game_clock_chain_begin) */
#define GAME_CLOCK_NOTIFIERS 16

/* Words a reacting game thread last bumped, each with its value then (a power of two) */
#define GAME_CLOCK_CHAINS 1024

typedef struct game_clock_parked {
    retro_atomic_ptr_t word; /* retro_atomic_int_t*, NULL: free */
    retro_atomic_int_t key;
    /* The frame its wake was first seen in flight in, 0: not seen. A wake holds the time for
     * the rest of that frame at most: a thread woken that does not get to run (suspended)
     * cannot hold it for longer. */
    retro_atomic_int_t flight_frame;
} game_clock_parked_t;

/* A game thread's own state, kept by the thread (thread-local) */
typedef struct game_clock_thread {
    int yielded; /* counted idle by a yield (game_clock_yield) */
    retro_atomic_int_t woken; /* woken: in frame (number + 1), until it waits again */
} game_clock_thread_t;

typedef struct game_clock {
    retro_atomic_64_t now;       /* the time */
    retro_atomic_64_t frame_end; /* the most the time may reach before the next frame */
    retro_atomic_64_t frame_start;  /* the time moves here when nothing holds it before */
    retro_atomic_int_t stepping;    /* a thread is stepping the time (game_clock_skip) */
    retro_atomic_int_t step_wanted; /* a step was asked for: the stepper looks again */
    retro_atomic_64_t frames;    /* frames started */
    uint64_t period;             /* a frame's whole nanoseconds */
    uint64_t rem;                /* and the fraction's numerator, over fps_num */
    uint64_t acc;
    uint64_t fps_num;
    /* The game's threads not waiting on something tracked (game_clock_idle/busy) */
    retro_atomic_int_t running;
    retro_atomic_64_t free_timers[GAME_CLOCK_TIMERS / 64]; /* a bit a timer, set if free */
    game_clock_timer_t timers[GAME_CLOCK_TIMERS];
    retro_atomic_64_t free_parked[GAME_CLOCK_PARKED / 64]; /* a bit a slot, set if free */
    game_clock_parked_t parked[GAME_CLOCK_PARKED];
    retro_atomic_int_t parked_top; /* past the highest slot ever taken: the scans stop there */
    /* The words reacting game threads are notifying (game_clock_chain_begin), NULL: free */
    retro_atomic_ptr_t notifying[GAME_CLOCK_NOTIFIERS];
    /* Each word a reacting game thread bumped, by its place (game_clock_chain_end): its tag
     * << 32 | its value just past the bump. A thread parked on it with a key behind that was
     * woken by the reaction, and reacts in turn. */
    retro_atomic_64_t chains[GAME_CLOCK_CHAINS];
    /* Threads woken that have not waited again (game_clock_thread_t.woken), counted by the
     * frame they were woken in: [tag & 1] holds (tag << 32) | count. Those of this frame and
     * the last hold the time: at none, and no wake in flight, the time steps to the next
     * deadline. One woken before the last frame (running on for ever) no longer holds it. */
    retro_atomic_64_t woken[2];
} game_clock_t;

/* At time 0, no frame started (the time stands until the first), frames of
 * @fps_den / @fps_num seconds (60/1: 1/60 s) */
void game_clock_init(game_clock_t* clock, unsigned fps_num, unsigned fps_den);

/* A frame starts (retro_run): the time moves to its start (the last frame's end), if the
 * game had not used all of the last one, as soon as nothing holds it: the deadlines before it
 * first, in order, each after the reaction to the last (never at once, past a reaction still
 * running). The one thread that starts frames.
 * Within the frame the time steps from deadline to deadline, never past its end: to the next
 * one as soon as every thread reacting (woken by a deadline, or by a thread reacting to one)
 * has waited again and no wake of one in flight. Each reaction to a deadline runs before the next
 * deadline comes, and the frame's deadlines come as fast as its threads react. A thread
 * running for any other reason (its own work, a spin, a wait the clock does not see: a
 * socket's) does not hold the time. */
void game_clock_frame(game_clock_t* clock);

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

/* The calling game thread starts or stops waiting on something tracked. A game thread counts
 * as running from game_clock_busy at its start to game_clock_idle at its end. */
void game_clock_idle(game_clock_t* clock);
void game_clock_busy(game_clock_t* clock);

/* A counted thread, @self, goes idle: game_clock_thread_done, then game_clock_idle */
void game_clock_thread_idle(game_clock_t* clock, game_clock_thread_t* self);

/* @self, about to wait (before it arms a timer for the wait), has done its reaction: one woken
 * no longer holds the time (the last of them, the time steps on) */
void game_clock_thread_done(game_clock_t* clock, game_clock_thread_t* self);

/* A timer hands @self, parked, its count back: as game_clock_busy, and @self is woken
 * (game_clock_woke). Called by the timer's fire, on the thread firing it. */
void game_clock_hand(game_clock_t* clock, game_clock_thread_t* self);

/* @self (NULL: none) reacts: woken this frame by a deadline (or late for one), or by a
 * reacting game thread. The time waits for it until it waits again, for the rest of the next
 * frame at most. */
void game_clock_woke(game_clock_t* clock, game_clock_thread_t* self);

/* A game thread, @self (NULL: none), bumps *@word (an eventcount's epoch, a semaphore's
 * releases: wakes those parked on it) between the two. If it is reacting (woken by a deadline,
 * or in turn by a thread that was, and not waited since), the threads it wakes react too: the
 * time waits for them (an engine's output thread, woken at its buffer's time, waking its
 * mixer). A wake from anything else (the emulator's threads, the GPU, a vblank; a game thread
 * running its own work) holds nothing. @return the token for game_clock_chain_end. */
int game_clock_chain_begin(game_clock_t* clock, retro_atomic_int_t* word,
                           game_clock_thread_t* self);
void game_clock_chain_end(game_clock_t* clock, retro_atomic_int_t* word, int token);

/* A counted game thread parks, idle, on a wait whose wake bumps *@word (@key: its value
 * now): until it has run again (game_clock_unpark), a reacting thread's wake of it in flight
 * (the word moved on, game_clock_chain_begin) holds the time, so that the time does not skip
 * past a thread woken by another that then waits (an engine's output thread waking its mixer,
 * then sleeping to its next buffer). Each
 * time it parks again in the same wait, game_clock_repark with the new key. Returns the
 * slot, or -1 if none is left (the wait goes on untracked). */
int game_clock_park(game_clock_t* clock, retro_atomic_int_t* word, int key);
void game_clock_repark(game_clock_t* clock, int slot, int key);
/* Running again (counted): its slot goes. @self, if woken by a reacting game thread, reacts
 * too (game_clock_woke). */
void game_clock_unpark(game_clock_t* clock, int slot, game_clock_thread_t* self);

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
 * as the game thread @self (NULL: one not counted) not counted running meanwhile: no host
 * timeout, no wall clock. A timer of the game's clock wakes it at the deadline and hands it its
 * count back. @return 1 if @ready ended it, 0 if the deadline did. */
int game_clock_wait(game_clock_t* clock, retro_eventcount_t* ec, uint64_t deadline,
                    int (*ready)(void* ctx), void* ctx, game_clock_thread_t* self);

#ifdef __cplusplus
}
#endif

#endif
