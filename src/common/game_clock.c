/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "common/game_clock.h"

#define GAME_CLOCK_FIRING ((int64_t) - 1)
#define GAME_CLOCK_FIRED ((int64_t) - 2)

void game_clock_init(game_clock_t* clock, unsigned fps_num, unsigned fps_den) {
    const uint64_t total = (uint64_t)1000000000u * (fps_den ? fps_den : 1);
    if (!fps_num)
        fps_num = 60;
    retro_atomic_64_init(&clock->now, 0);
    retro_atomic_64_init(&clock->frame_end, 0);
    retro_atomic_64_init(&clock->frames, 0);
    clock->fps_num = fps_num;
    clock->period = total / fps_num;
    clock->rem = total % fps_num;
    clock->acc = 0;
    retro_atomic_int_init(&clock->running, 0);
    {
        unsigned i;
        for (i = 0; i < GAME_CLOCK_TIMERS / 64; i++)
            retro_atomic_64_init(&clock->free_timers[i], -1);
        for (i = 0; i < GAME_CLOCK_TIMERS; i++) {
            retro_atomic_64_init(&clock->timers[i].deadline, 0);
            clock->timers[i].fire = 0;
            clock->timers[i].ctx = 0;
            clock->timers[i].oneshot = 0;
        }
    }
    retro_atomic_int_init(&clock->finishing, 0);
    {
        unsigned i;
        for (i = 0; i < GAME_CLOCK_PARKED / 64; i++)
            retro_atomic_64_init(&clock->free_parked[i], -1);
        for (i = 0; i < GAME_CLOCK_PARKED; i++) {
            retro_atomic_ptr_init(&clock->parked[i].word, 0);
            retro_atomic_int_init(&clock->parked[i].key, 0);
        }
    }
    clock->done_frame = 0;
    clock->done = 0;
    clock->done_ctx = 0;
}

void game_clock_set_done(game_clock_t* clock, retro_atomic_int_t* done_frame,
                         void (*done)(void* ctx), void* ctx) {
    clock->done = done;
    clock->done_ctx = ctx;
    clock->done_frame = done_frame;
}

/* The timers due at @now fire: each once, by whichever thread claims it */
static void game_clock_fire_due(game_clock_t* clock, uint64_t now) {
    unsigned i;
    for (i = 0; i < GAME_CLOCK_TIMERS; i++) {
        game_clock_timer_t* t = &clock->timers[i];
        const int64_t d = retro_atomic_load_acquire_64(&t->deadline);
        if (d > 0 && (uint64_t)d <= now &&
            retro_atomic_cas_64(&t->deadline, d, GAME_CLOCK_FIRING)) {
            const int oneshot = t->oneshot;
            t->fire(t->ctx);
            if (oneshot) {
                retro_atomic_store_release_64(&t->deadline, 0);
                retro_atomic_fetch_or_64(&clock->free_timers[i / 64],
                                         (int64_t)((uint64_t)1 << (i % 64)));
            } else {
                retro_atomic_store_release_64(&t->deadline, GAME_CLOCK_FIRED);
            }
        }
    }
}

/* The earliest deadline armed, or 0 if none */
static uint64_t game_clock_earliest(game_clock_t* clock) {
    uint64_t earliest = 0;
    unsigned i;
    for (i = 0; i < GAME_CLOCK_TIMERS; i++) {
        const int64_t d = retro_atomic_load_acquire_64(&clock->timers[i].deadline);
        if (d > 0 && (earliest == 0 || (uint64_t)d < earliest))
            earliest = (uint64_t)d;
    }
    return earliest;
}

/* A parked thread has been woken and has not run yet */
static int game_clock_wake_in_flight(game_clock_t* clock) {
    unsigned i;
    for (i = 0; i < GAME_CLOCK_PARKED; i++) {
        retro_atomic_int_t* word =
            (retro_atomic_int_t*)retro_atomic_load_acquire_ptr(&clock->parked[i].word);
        if (word && retro_atomic_load_seq_cst_int(word) !=
                        retro_atomic_load_acquire_int(&clock->parked[i].key))
            return 1;
    }
    return 0;
}

/* All idle: the time moves to the earliest deadline, within the frame, and what is due fires.
 * The frame being finished: to each deadline within it in turn, all of them. Not while a
 * parked thread's wake is in flight: it runs first, and skips again once it waits. */
static void game_clock_skip(game_clock_t* clock) {
    uint64_t earliest, next;
    if (game_clock_wake_in_flight(clock))
        return;
    earliest = game_clock_earliest(clock);
    if (earliest)
        game_clock_advance(clock, earliest);
    game_clock_fire_due(clock, game_clock_now(clock));
    while (retro_atomic_load_acquire_int(&clock->finishing) && !game_clock_wake_in_flight(clock) &&
           (earliest = game_clock_earliest(clock)) != 0 &&
           earliest <= game_clock_frame_end(clock)) {
        game_clock_advance(clock, earliest);
        game_clock_fire_due(clock, game_clock_now(clock));
    }
    /* Still nobody running, and nobody waiting for a time within the frame: it is done */
    if (!clock->done_frame || retro_atomic_load_acquire_int(&clock->running) > 0)
        return;
    next = game_clock_earliest(clock);
    if (next == 0 || next > game_clock_frame_end(clock)) {
        retro_atomic_store_release_int(clock->done_frame, (int)game_clock_frames(clock));
        /* A thread that went running meanwhile may have looked before the store: then it is
         * taken back here (the store, then the count; its count, then the store's word) */
        retro_atomic_thread_fence_seq_cst();
        if (retro_atomic_load_seq_cst_int(&clock->running) > 0) {
            retro_atomic_store_release_int(clock->done_frame, 0);
            return;
        }
        if (clock->done)
            clock->done(clock->done_ctx);
    }
}

/* The time moves to @to if that is later: never back, whoever moves it */
static uint64_t game_clock_raise(game_clock_t* clock, uint64_t to) {
    int64_t have = retro_atomic_load_acquire_64(&clock->now);
    while ((uint64_t)have < to) {
        if (retro_atomic_cas_64(&clock->now, have, (int64_t)to))
            return to;
        have = retro_atomic_load_acquire_64(&clock->now);
    }
    return (uint64_t)have;
}

void game_clock_frame(game_clock_t* clock) {
    const uint64_t start = (uint64_t)retro_atomic_load_relaxed_64(&clock->frame_end);
    uint64_t end;
    clock->acc += clock->rem;
    end = start + clock->period + clock->acc / clock->fps_num;
    clock->acc %= clock->fps_num;
    /* The end first: a thread that sees the new start may advance within the new frame */
    retro_atomic_store_release_64(&clock->frame_end, (int64_t)end);
    retro_atomic_store_release_int(&clock->finishing, 0);
    game_clock_raise(clock, start);
    retro_atomic_fetch_add_64(&clock->frames, 1);
    /* Nobody running: the new frame's time is theirs to skip through at once */
    if (retro_atomic_load_acquire_int(&clock->running) <= 0)
        game_clock_skip(clock);
    else
        game_clock_fire_due(clock, game_clock_now(clock));
}

uint64_t game_clock_now(game_clock_t* clock) {
    return (uint64_t)retro_atomic_load_acquire_64(&clock->now);
}

uint64_t game_clock_frame_end(game_clock_t* clock) {
    return (uint64_t)retro_atomic_load_acquire_64(&clock->frame_end);
}

uint64_t game_clock_frames(game_clock_t* clock) {
    return (uint64_t)retro_atomic_load_acquire_64(&clock->frames);
}

uint64_t game_clock_advance(game_clock_t* clock, uint64_t until) {
    const uint64_t end = (uint64_t)retro_atomic_load_acquire_64(&clock->frame_end);
    return game_clock_raise(clock, until < end ? until : end);
}

static int game_clock_arm_ex(game_clock_t* clock, uint64_t deadline, void (*fire)(void*), void* ctx,
                             int oneshot) {
    unsigned w;
    if (deadline == 0)
        deadline = 1; /* 0 means free */
    for (w = 0; w < GAME_CLOCK_TIMERS / 64; w++) {
        int64_t bits = retro_atomic_load_acquire_64(&clock->free_timers[w]);
        while (bits != 0) {
            const uint64_t ubits = (uint64_t)bits;
            const uint64_t lowest = ubits & (~ubits + 1);
            if (retro_atomic_cas_64(&clock->free_timers[w], bits, (int64_t)(ubits & ~lowest))) {
                unsigned bit = 0;
                game_clock_timer_t* t;
                while (!((lowest >> bit) & 1u))
                    bit++;
                t = &clock->timers[w * 64 + bit];
                t->fire = fire;
                t->ctx = ctx;
                t->oneshot = oneshot;
                retro_atomic_store_release_64(&t->deadline, (int64_t)deadline);
                /* Due already, or the time moved past it while it was being armed; or the frame
                 * is being finished: the time goes to it now */
                if (retro_atomic_load_acquire_int(&clock->finishing))
                    game_clock_skip(clock);
                else if (deadline <= game_clock_now(clock))
                    game_clock_fire_due(clock, game_clock_now(clock));
                return (int)(w * 64 + bit);
            }
            bits = retro_atomic_load_acquire_64(&clock->free_timers[w]);
        }
    }
    return -1;
}

int game_clock_arm(game_clock_t* clock, uint64_t deadline, void (*fire)(void*), void* ctx) {
    return game_clock_arm_ex(clock, deadline, fire, ctx, 0);
}

int game_clock_arm_oneshot(game_clock_t* clock, uint64_t deadline, void (*fire)(void*), void* ctx) {
    return game_clock_arm_ex(clock, deadline, fire, ctx, 1);
}

int game_clock_disarm(game_clock_t* clock, int timer) {
    game_clock_timer_t* t;
    int64_t d;
    int fired;
    if (timer < 0)
        return 0;
    t = &clock->timers[timer];
    for (;;) {
        d = retro_atomic_load_acquire_64(&t->deadline);
        if (d == GAME_CLOCK_FIRED) {
            fired = 1;
            break;
        }
        if (d == GAME_CLOCK_FIRING)
            continue; /* its fire is running: a few instructions */
        if (retro_atomic_cas_64(&t->deadline, d, 0)) {
            fired = 0;
            break;
        }
    }
    retro_atomic_store_release_64(&t->deadline, 0);
    retro_atomic_fetch_or_64(&clock->free_timers[timer / 64],
                             (int64_t)((uint64_t)1 << (timer % 64)));
    return fired;
}

uint64_t game_clock_cadence(game_clock_t* clock, uint64_t* start, uint64_t* count, uint64_t units,
                            uint64_t rate) {
    const uint64_t now = game_clock_now(clock);
    const uint64_t period = units * 1000000000u / (rate ? rate : 1);
    uint64_t due = *start + *count * units * 1000000000u / (rate ? rate : 1);
    if (*count == 0 || now > due + 8 * period) {
        *start = now;
        *count = 0;
        due = now;
    }
    ++*count;
    return due;
}

void game_clock_finish(game_clock_t* clock) {
    retro_atomic_store_release_int(&clock->finishing, 1);
    game_clock_skip(clock);
}

void game_clock_idle(game_clock_t* clock) {
    if (retro_atomic_fetch_sub_int(&clock->running, 1) == 1)
        game_clock_skip(clock);
}

void game_clock_busy(game_clock_t* clock) {
    retro_atomic_fetch_add_seq_cst_int(&clock->running, 1);
    /* Running again: the frame is not done after all */
    if (clock->done_frame && retro_atomic_load_seq_cst_int(clock->done_frame) != 0)
        retro_atomic_store_release_int(clock->done_frame, 0);
}

int game_clock_running(game_clock_t* clock) {
    return retro_atomic_load_acquire_int(&clock->running);
}

void game_clock_yield(game_clock_t* clock, int* yielded) {
    if (*yielded)
        return;
    *yielded = 1;
    game_clock_idle(clock);
}

void game_clock_resume(game_clock_t* clock, int* yielded) {
    if (!*yielded)
        return;
    *yielded = 0;
    game_clock_busy(clock);
}

int game_clock_park(game_clock_t* clock, retro_atomic_int_t* word, int key) {
    unsigned w;
    for (w = 0; w < GAME_CLOCK_PARKED / 64; w++) {
        int64_t bits = retro_atomic_load_acquire_64(&clock->free_parked[w]);
        while (bits != 0) {
            const uint64_t ubits = (uint64_t)bits;
            const uint64_t lowest = ubits & (~ubits + 1);
            if (retro_atomic_cas_64(&clock->free_parked[w], bits, (int64_t)(ubits & ~lowest))) {
                unsigned bit = 0;
                game_clock_parked_t* p;
                while (!((lowest >> bit) & 1u))
                    bit++;
                p = &clock->parked[w * 64 + bit];
                retro_atomic_store_release_int(&p->key, key);
                retro_atomic_store_release_ptr(&p->word, word);
                return (int)(w * 64 + bit);
            }
            bits = retro_atomic_load_acquire_64(&clock->free_parked[w]);
        }
    }
    return -1;
}

void game_clock_repark(game_clock_t* clock, int slot, int key) {
    if (slot < 0)
        return;
    retro_atomic_store_release_int(&clock->parked[slot].key, key);
    /* The wake that held the time was not for it after all: the time may move on now */
    if (retro_atomic_load_acquire_int(&clock->running) <= 0 ||
        retro_atomic_load_acquire_int(&clock->finishing))
        game_clock_skip(clock);
}

void game_clock_unpark(game_clock_t* clock, int slot) {
    if (slot < 0)
        return;
    retro_atomic_store_release_ptr(&clock->parked[slot].word, 0);
    retro_atomic_fetch_or_64(&clock->free_parked[slot / 64], (int64_t)((uint64_t)1 << (slot % 64)));
}

struct game_clock_wait_timer {
    game_clock_t* clock;
    retro_eventcount_t* ec;
    retro_atomic_int_t fired;
    int counted;
};

static void game_clock_wait_fire(void* ctx) {
    struct game_clock_wait_timer* t = (struct game_clock_wait_timer*)ctx;
    /* The woken thread counts as running from here: the time does not skip past it before it
     * has run */
    if (t->counted)
        game_clock_busy(t->clock);
    retro_atomic_store_release_int(&t->fired, 1);
    retro_eventcount_notify(t->ec);
}

int game_clock_wait(game_clock_t* clock, retro_eventcount_t* ec, uint64_t deadline,
                    int (*ready)(void* ctx), void* ctx, int counted) {
    struct game_clock_wait_timer t;
    int timer = -1, ok = 0, fired, slot = -2; /* -2: not parked yet */
    if (ready(ctx))
        return 1;
    if (deadline != 0 && deadline <= game_clock_now(clock))
        return ready(ctx) ? 1 : 0;
    t.clock = clock;
    t.ec = ec;
    retro_atomic_int_init(&t.fired, 0);
    t.counted = counted;
    if (deadline != 0)
        timer = game_clock_arm(clock, deadline, game_clock_wait_fire, &t);
    for (;;) {
        const int key = retro_eventcount_prepare_wait(ec);
        if (ready(ctx)) {
            retro_eventcount_cancel_wait(ec);
            ok = 1;
            break;
        }
        if (retro_atomic_load_acquire_int(&t.fired)) {
            retro_eventcount_cancel_wait(ec);
            break;
        }
        /* Parked, idle: the first time it parks, then again after a wake that was not for it */
        if (counted) {
            if (slot == -2) {
                slot = game_clock_park(clock, &ec->epoch, key);
                game_clock_idle(clock);
            } else {
                game_clock_repark(clock, slot, key);
            }
        }
        retro_eventcount_commit_wait(ec, key);
    }
    /* Running again: counted before its slot goes, so that the time never finds it neither
     * running nor woken */
    fired = timer >= 0 && game_clock_disarm(clock, timer);
    if (counted && slot != -2) {
        /* A timer that fired handed this thread its count back */
        if (!fired)
            game_clock_busy(clock);
        game_clock_unpark(clock, slot);
    } else if (fired && counted) {
        /* Fired before it ever parked: its count was handed to it, and it never idled */
        game_clock_idle(clock);
    }
    return ok;
}
