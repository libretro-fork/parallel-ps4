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
    retro_atomic_64_init(&clock->frame_start, 0);
    retro_atomic_int_init(&clock->stepping, 0);
    retro_atomic_int_init(&clock->step_wanted, 0);
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
    retro_atomic_64_init(&clock->woken[0], 0);
    retro_atomic_64_init(&clock->woken[1], 0);
    {
        unsigned i;
        for (i = 0; i < GAME_CLOCK_PARKED / 64; i++)
            retro_atomic_64_init(&clock->free_parked[i], -1);
        for (i = 0; i < GAME_CLOCK_PARKED; i++) {
            retro_atomic_ptr_init(&clock->parked[i].word, 0);
            retro_atomic_int_init(&clock->parked[i].key, 0);
            retro_atomic_int_init(&clock->parked[i].flight_frame, 0);
        }
        retro_atomic_int_init(&clock->parked_top, 0);
        for (i = 0; i < GAME_CLOCK_NOTIFIERS; i++)
            retro_atomic_ptr_init(&clock->notifying[i], 0);
        for (i = 0; i < GAME_CLOCK_CHAINS; i++)
            retro_atomic_64_init(&clock->chains[i], 0);
    }
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

static int game_clock_chained(game_clock_t* clock, retro_atomic_int_t* word, int key);
static int game_clock_tag(game_clock_t* clock);

/* A parked thread woken by a reacting game thread (game_clock_chain_begin) has not run yet,
 * this frame or the last. Any other wake (a thread of the emulator's, the GPU's, a vblank's;
 * a game thread not reacting) holds nothing. */
static int game_clock_wake_in_flight(game_clock_t* clock) {
    const int frame = (int)game_clock_frames(clock) + 1; /* never 0 */
    const unsigned top = (unsigned)retro_atomic_load_acquire_int(&clock->parked_top);
    unsigned i;
    for (i = 0; i < top; i++) {
        game_clock_parked_t* p = &clock->parked[i];
        retro_atomic_int_t* word = (retro_atomic_int_t*)retro_atomic_load_acquire_ptr(&p->word);
        int seen;
        if (!word ||
            retro_atomic_load_seq_cst_int(word) == retro_atomic_load_acquire_int(&p->key) ||
            !game_clock_chained(clock, word, retro_atomic_load_acquire_int(&p->key)))
            continue;
        seen = retro_atomic_load_acquire_int(&p->flight_frame);
        if (seen == 0 && retro_atomic_cas_int(&p->flight_frame, 0, frame))
            seen = frame;
        if (seen == frame || seen == frame - 1 || seen == 0)
            return 1;
        /* In flight since before the last frame: it is not running, and holds nothing */
    }
    return 0;
}

/* A word's place in the chains, and the tag that tells it from another's there */
static unsigned game_clock_chain_slot(retro_atomic_int_t* word) {
    const uint64_t a = (uint64_t)(uintptr_t)word;
    return (unsigned)(((a >> 2) * 0x9E3779B97F4A7C15ull) >> 54) & (GAME_CLOCK_CHAINS - 1);
}

static uint32_t game_clock_chain_tag(retro_atomic_int_t* word) {
    const uint64_t a = (uint64_t)(uintptr_t)word;
    return (uint32_t)(a ^ (a >> 32)) | 1u; /* never 0: an empty place */
}

/* @self is reacting: woken this frame or the last, and not waited since */
static int game_clock_self_reacting(game_clock_t* clock, game_clock_thread_t* self) {
    const int tag = game_clock_tag(clock);
    const int was = self ? retro_atomic_load_acquire_int(&self->woken) : 0;
    return was != 0 && (was == tag || was == tag - 1 || (tag == 1 && was == 0x40000000)) &&
           retro_atomic_load_acquire_int(&self->leads);
}

int game_clock_chain_begin(game_clock_t* clock, retro_atomic_int_t* word,
                           game_clock_thread_t* self) {
    int i;
    if (!game_clock_self_reacting(clock, self))
        return -1;
    for (i = 0; i < GAME_CLOCK_NOTIFIERS; i++)
        if (retro_atomic_cas_ptr(&clock->notifying[i], 0, word))
            return i;
    return -1;
}

void game_clock_chain_end(game_clock_t* clock, retro_atomic_int_t* word, int token) {
    if (token < 0)
        return;
    /* The word's value past its bump: a thread parked on it before (its key behind it) was
     * woken by a reacting thread. Then the word is no longer being chained: one that looks
     * after this sees the value (game_clock_chained). */
    retro_atomic_store_release_64(&clock->chains[game_clock_chain_slot(word)],
                                  (int64_t)(((uint64_t)game_clock_chain_tag(word) << 32) |
                                            (uint32_t)retro_atomic_load_seq_cst_int(word)));
    retro_atomic_store_release_ptr(&clock->notifying[token], 0);
}

/* Whether a thread parked on @word at @key was woken by a reacting game thread: one is
 * bumping it now, or bumped it past @key (the word's last chained value is ahead of the key) */
static int game_clock_chained(game_clock_t* clock, retro_atomic_int_t* word, int key) {
    uint64_t v;
    int i;
    for (i = 0; i < GAME_CLOCK_NOTIFIERS; i++)
        if ((retro_atomic_int_t*)retro_atomic_load_acquire_ptr(&clock->notifying[i]) == word)
            return 1;
    v = (uint64_t)retro_atomic_load_acquire_64(&clock->chains[game_clock_chain_slot(word)]);
    return (uint32_t)(v >> 32) == game_clock_chain_tag(word) &&
           (int32_t)((uint32_t)v - (uint32_t)key) > 0;
}

static uint64_t game_clock_raise(game_clock_t* clock, uint64_t to) {
    int64_t have = retro_atomic_load_acquire_64(&clock->now);
    while ((uint64_t)have < to) {
        if (retro_atomic_cas_64(&clock->now, have, (int64_t)to))
            return to;
        have = retro_atomic_load_acquire_64(&clock->now);
    }
    return (uint64_t)have;
}

/* The frame tag a thread woken now gets: never 0 (none) */
static int game_clock_tag(game_clock_t* clock) {
    return (int)((game_clock_frames(clock) & 0x3fffffff) + 1);
}

/* A thread woken this frame or the last is still reacting */
static int game_clock_reacting(game_clock_t* clock) {
    const int tag = game_clock_tag(clock);
    int k;
    for (k = 0; k < 2; k++) {
        const int want = k == 0 ? tag : (tag == 1 ? 0x40000000 : tag - 1);
        const uint64_t v = (uint64_t)retro_atomic_load_acquire_64(&clock->woken[want & 1]);
        if ((int)(v >> 32) == want && (uint32_t)v != 0)
            return 1;
    }
    return 0;
}

/* The frame's time steps to its next deadline, and what is due there fires, as soon as every
 * thread a step or a wake of this frame woke has waited again (woken) and no wake is in flight:
 * in order, each reaction to a deadline run before the next deadline comes, and as fast as the
 * threads react. A thread running for any other reason (its own work, a spin, a wait the clock
 * does not see: a socket's) does not hold the time: the frame's sleepers all get their time
 * within the frame, whatever else runs. Never past the frame's end. */
static void game_clock_step(game_clock_t* clock) {
    for (;;) {
        uint64_t earliest;
        if (game_clock_reacting(clock) || game_clock_wake_in_flight(clock))
            return;
        earliest = game_clock_earliest(clock);
        /* Up to the frame's start, after the deadlines before it */
        if (earliest == 0 || earliest > (uint64_t)retro_atomic_load_acquire_64(&clock->frame_start))
            game_clock_raise(clock, (uint64_t)retro_atomic_load_acquire_64(&clock->frame_start));
        if (earliest == 0 || earliest > game_clock_frame_end(clock))
            return;
        game_clock_advance(clock, earliest);
        game_clock_fire_due(clock, game_clock_now(clock));
        /* Nothing fired (disarmed meanwhile): on to the next */
    }
}

/* One thread steps the time at a time: nothing held when it looked stays so until it has
 * stepped (a thread that looked, then was preempted, cannot step past a reaction that began
 * meanwhile: only the stepper's own step begins one). A thread that finds another stepping
 * leaves it a request, which the stepper takes up before it lets go. */
static void game_clock_skip(game_clock_t* clock) {
    (void)retro_atomic_exchange_int(&clock->step_wanted, 1);
    retro_atomic_thread_fence_seq_cst();
    while (retro_atomic_load_seq_cst_int(&clock->step_wanted)) {
        if (!retro_atomic_cas_int(&clock->stepping, 0, 1))
            return; /* the stepper looks again before it lets go */
        (void)retro_atomic_exchange_int(&clock->step_wanted, 0);
        game_clock_step(clock);
        (void)retro_atomic_exchange_int(&clock->stepping, 0);
        retro_atomic_thread_fence_seq_cst();
    }
}

void game_clock_frame(game_clock_t* clock) {
    const uint64_t start = (uint64_t)retro_atomic_load_relaxed_64(&clock->frame_end);
    uint64_t end;
    clock->acc += clock->rem;
    end = start + clock->period + clock->acc / clock->fps_num;
    clock->acc %= clock->fps_num;
    /* The end first: a thread that sees the new start may advance within the new frame */
    retro_atomic_store_release_64(&clock->frame_end, (int64_t)end);
    retro_atomic_store_release_64(&clock->frame_start, (int64_t)start);
    retro_atomic_fetch_add_64(&clock->frames, 1);
    /* What came due by the frame's start fires, and the frame's time steps on, as its threads
     * react: not while one is still reacting (a mixer still filling the buffer its output's
     * deadline at the frame's start would take) */
    game_clock_skip(clock);
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
                /* Due already, or the time moved past it while it was being armed: it fires now.
                 * Else the time steps to it once nobody woken is still running. */
                if (deadline <= game_clock_now(clock))
                    game_clock_fire_due(clock, game_clock_now(clock));
                game_clock_skip(clock);
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

void game_clock_idle(game_clock_t* clock) {
    retro_atomic_fetch_sub_int(&clock->running, 1);
}

void game_clock_thread_done(game_clock_t* clock, game_clock_thread_t* self) {
    const int tag = retro_atomic_exchange_int(&self->woken, 0);
    retro_atomic_64_t* count;
    if (!tag)
        return;
    /* Counted in its frame's count, unless that frame's count has lapsed (two frames on): the
     * last of them waits again, and the time steps on */
    count = &clock->woken[tag & 1];
    for (;;) {
        const uint64_t v = (uint64_t)retro_atomic_load_acquire_64(count);
        if ((int)(v >> 32) != tag || (uint32_t)v == 0)
            break;
        if (retro_atomic_cas_64(count, (int64_t)v, (int64_t)(v - 1))) {
            if ((uint32_t)v == 1)
                game_clock_skip(clock);
            break;
        }
    }
}

void game_clock_thread_idle(game_clock_t* clock, game_clock_thread_t* self) {
    game_clock_thread_done(clock, self);
    game_clock_idle(clock);
}

static void game_clock_woke_as(game_clock_t* clock, game_clock_thread_t* self, int leads) {
    int tag, was;
    retro_atomic_64_t* count;
    if (!self)
        return;
    if (leads)
        retro_atomic_store_release_int(&self->leads, 1);
    tag = game_clock_tag(clock);
    was = retro_atomic_load_acquire_int(&self->woken);
    /* Still reacting, counted this frame or the last */
    if (was == tag || (was != 0 && (was == tag - 1 || (tag == 1 && was == 0x40000000))))
        return;
    /* Not woken, or its count lapsed: counted afresh in this frame's */
    if (!leads)
        retro_atomic_store_release_int(&self->leads, 0);
    if (!retro_atomic_cas_int(&self->woken, was, tag))
        return; /* only the thread itself and a timer handing it its count race here */
    count = &clock->woken[tag & 1];
    for (;;) {
        const uint64_t v = (uint64_t)retro_atomic_load_acquire_64(count);
        const uint64_t next =
            (int)(v >> 32) == tag ? v + 1 : (((uint64_t)(uint32_t)tag << 32) | 1u);
        if (retro_atomic_cas_64(count, (int64_t)v, (int64_t)next))
            break;
    }
}

void game_clock_woke(game_clock_t* clock, game_clock_thread_t* self) {
    game_clock_woke_as(clock, self, 1);
}

void game_clock_pacing(game_clock_thread_t* self, int on) {
    if (self)
        retro_atomic_store_release_int(&self->pacing, on);
}

void game_clock_hand(game_clock_t* clock, game_clock_thread_t* self) {
    game_clock_busy(clock);
    /* Only an audio device's time starts a reaction: any other sleep's end holds nothing */
    if (self && retro_atomic_load_acquire_int(&self->pacing))
        game_clock_woke_as(clock, self, 1);
}

void game_clock_busy(game_clock_t* clock) {
    retro_atomic_fetch_add_int(&clock->running, 1);
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
                retro_atomic_store_release_int(&p->flight_frame, 0);
                retro_atomic_store_release_ptr(&p->word, word);
                for (;;) {
                    const int top = retro_atomic_load_acquire_int(&clock->parked_top);
                    if (top > (int)(w * 64 + bit) ||
                        retro_atomic_cas_int(&clock->parked_top, top, (int)(w * 64 + bit) + 1))
                        break;
                }
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
    retro_atomic_store_release_int(&clock->parked[slot].flight_frame, 0);
    /* The wake that held the time was not for it after all: the time may move on now */
    game_clock_skip(clock);
}

void game_clock_unpark(game_clock_t* clock, int slot, game_clock_thread_t* self) {
    if (slot < 0)
        return;
    /* Woken by a reacting game thread (bumping the word now, or bumped it past its key: the
     * one bumping is looked at first, so a wake it sent is never missed), it reacts in turn */
    if (self) {
        retro_atomic_int_t* word =
            (retro_atomic_int_t*)retro_atomic_load_acquire_ptr(&clock->parked[slot].word);
        if (game_clock_chained(clock, word,
                               retro_atomic_load_acquire_int(&clock->parked[slot].key)))
            game_clock_woke_as(clock, self, 0);
    }
    retro_atomic_store_release_ptr(&clock->parked[slot].word, 0);
    retro_atomic_fetch_or_64(&clock->free_parked[slot / 64], (int64_t)((uint64_t)1 << (slot % 64)));
    /* Its wake no longer in flight: the time may step on (unless it reacts, woken) */
    game_clock_skip(clock);
}

struct game_clock_wait_timer {
    game_clock_t* clock;
    retro_eventcount_t* ec;
    retro_atomic_int_t fired;
    game_clock_thread_t* self;
};

static void game_clock_wait_fire(void* ctx) {
    struct game_clock_wait_timer* t = (struct game_clock_wait_timer*)ctx;
    /* The woken thread counts as running from here: the time does not skip past it before it
     * has run */
    if (t->self)
        game_clock_hand(t->clock, t->self);
    retro_atomic_store_release_int(&t->fired, 1);
    retro_eventcount_notify(t->ec);
}

int game_clock_wait(game_clock_t* clock, retro_eventcount_t* ec, uint64_t deadline,
                    int (*ready)(void* ctx), void* ctx, game_clock_thread_t* self) {
    struct game_clock_wait_timer t;
    int timer = -1, ok = 0, fired, slot = -2; /* -2: not parked yet */
    if (ready(ctx))
        return 1;
    if (deadline != 0 && deadline <= game_clock_now(clock))
        return ready(ctx) ? 1 : 0;
    t.clock = clock;
    t.ec = ec;
    retro_atomic_int_init(&t.fired, 0);
    t.self = self;
    for (;;) {
        const int key = retro_eventcount_prepare_wait(ec);
        /* Ready before it ever waits: what it does next is still the reaction it was in (or
         * none, as it was), whoever made it ready */
        if (ready(ctx)) {
            retro_eventcount_cancel_wait(ec);
            ok = 1;
            break;
        }
        if (retro_atomic_load_acquire_int(&t.fired)) {
            retro_eventcount_cancel_wait(ec);
            break;
        }
        if (slot == -2) {
            /* Parked first, its key behind any wake from here on: a reacting thread's wake is
             * in flight from the moment its reaction is done, never neither. Then its reaction
             * done, before its timer can hand it a new one, and idle. */
            if (self) {
                slot = game_clock_park(clock, &ec->epoch, key);
                game_clock_thread_done(clock, self);
            } else {
                slot = -3; /* not parked */
            }
            if (deadline != 0)
                timer = game_clock_arm(clock, deadline, game_clock_wait_fire, &t);
            if (self)
                game_clock_idle(clock);
        } else if (self) {
            /* Parked again after a wake that was not for it */
            game_clock_repark(clock, slot, key);
        }
        retro_eventcount_commit_wait(ec, key);
    }
    /* Running again: counted before its slot goes, so that the time never finds it neither
     * running nor woken */
    fired = timer >= 0 && game_clock_disarm(clock, timer);
    if (self && slot >= -1) {
        /* A timer that fired handed this thread its count back; woken by a game thread, it
         * reacts too, from before its slot goes (never neither) */
        if (!fired)
            game_clock_busy(clock);
        game_clock_unpark(clock, slot, self);
    }
    return ok;
}
