/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <string.h>

#include <rthreads/rthreads.h>

#include "core/libraries/ult/ult_sync.h"

static void (*before_park)(void);
static void (*after_park)(void);

void ult_sync_set_park_hooks(void (*before)(void), void (*after)(void)) {
    before_park = before;
    after_park = after;
}

/* Parks on @ec until @attempt succeeds, rechecking after every notify */
static void wait_until(retro_eventcount_t* ec, int (*attempt)(void*), void* ctx) {
    while (!attempt(ctx)) {
        const int key = retro_eventcount_prepare_wait(ec);
        if (attempt(ctx)) {
            retro_eventcount_cancel_wait(ec);
            return;
        }
        retro_eventcount_commit_wait(ec, key);
    }
}

/* The same for a wait in one of the library's objects: with the park hooks */
static void park_until(retro_eventcount_t* ec, int (*attempt)(void*), void* ctx) {
    if (attempt(ctx))
        return;
    if (before_park)
        before_park();
    wait_until(ec, attempt, ctx);
    if (after_park)
        after_park();
}

/* A number per thread that no other thread has, and never none */
static void* this_thread(void) {
    return (void*)sthread_get_current_thread_id();
}

/* ---- tokens ---- */

void ult_tokens_init(ult_tokens* t, int count) {
    retro_atomic_int_init(&t->tokens, count);
    retro_eventcount_init(&t->ec);
}

void ult_tokens_free(ult_tokens* t) {
    retro_eventcount_free(&t->ec);
}

static int take_from(retro_atomic_int_t* word, int count) {
    int now = retro_atomic_load_acquire_int(word);
    while (now >= count) {
        if (retro_atomic_cas_int(word, now, now - count))
            return 1;
        now = retro_atomic_load_acquire_int(word);
    }
    return 0;
}

int ult_tokens_try_take(ult_tokens* t) {
    return take_from(&t->tokens, 1);
}

static int tokens_attempt(void* ctx) {
    return ult_tokens_try_take((ult_tokens*)ctx);
}

void ult_tokens_take(ult_tokens* t) {
    wait_until(&t->ec, tokens_attempt, t);
}

void ult_tokens_give(ult_tokens* t) {
    retro_atomic_fetch_add_int(&t->tokens, 1);
    retro_eventcount_notify(&t->ec);
}

/* ---- semaphore ---- */

void ult_semaphore_init(ult_semaphore* s, int resources) {
    retro_atomic_int_init(&s->resources, resources);
    retro_eventcount_init(&s->ec);
}

void ult_semaphore_free(ult_semaphore* s) {
    retro_eventcount_free(&s->ec);
}

int ult_semaphore_try_acquire(ult_semaphore* s, int count) {
    return take_from(&s->resources, count);
}

struct acquire_ctx {
    ult_semaphore* s;
    int count;
};

static int acquire_attempt(void* ctx) {
    struct acquire_ctx* a = (struct acquire_ctx*)ctx;
    return ult_semaphore_try_acquire(a->s, a->count);
}

void ult_semaphore_acquire(ult_semaphore* s, int count) {
    struct acquire_ctx a;
    a.s = s;
    a.count = count;
    park_until(&s->ec, acquire_attempt, &a);
}

void ult_semaphore_release(ult_semaphore* s, int count) {
    retro_atomic_fetch_add_int(&s->resources, count);
    retro_eventcount_notify(&s->ec);
}

/* ---- mutex ---- */

void ult_mutex_init(ult_mutex* m, int recursive) {
    retro_atomic_ptr_init(&m->owner, NULL);
    m->count = 0;
    m->recursive = recursive;
    retro_eventcount_init(&m->ec);
}

void ult_mutex_free(ult_mutex* m) {
    retro_eventcount_free(&m->ec);
}

int ult_mutex_try_lock(ult_mutex* m) {
    void* self = this_thread();
    if (retro_atomic_load_relaxed_ptr(&m->owner) == self) {
        if (!m->recursive)
            return ULT_MUTEX_RELOCKED;
        m->count++;
        return ULT_MUTEX_LOCKED;
    }
    if (!retro_atomic_cas_ptr(&m->owner, NULL, self))
        return ULT_MUTEX_BUSY;
    m->count = 1;
    return ULT_MUTEX_LOCKED;
}

struct lock_ctx {
    ult_mutex* m;
    int result;
};

static int lock_attempt(void* ctx) {
    struct lock_ctx* l = (struct lock_ctx*)ctx;
    l->result = ult_mutex_try_lock(l->m);
    return l->result != ULT_MUTEX_BUSY;
}

int ult_mutex_lock(ult_mutex* m) {
    struct lock_ctx l;
    l.m = m;
    l.result = ULT_MUTEX_BUSY;
    park_until(&m->ec, lock_attempt, &l);
    return l.result;
}

int ult_mutex_held_by_caller(ult_mutex* m) {
    return retro_atomic_load_relaxed_ptr(&m->owner) == this_thread();
}

int ult_mutex_unlock(ult_mutex* m) {
    if (!ult_mutex_held_by_caller(m))
        return 0;
    if (--m->count == 0) {
        retro_atomic_store_release_ptr(&m->owner, NULL);
        retro_eventcount_notify(&m->ec);
    }
    return 1;
}

/* Lets go of every lock the caller has on it; how many there were, to relock */
static uint32_t mutex_unlock_all(ult_mutex* m) {
    const uint32_t held = m->count;
    m->count = 0;
    retro_atomic_store_release_ptr(&m->owner, NULL);
    retro_eventcount_notify(&m->ec);
    return held;
}

static int relock_attempt(void* ctx) {
    ult_mutex* m = (ult_mutex*)ctx;
    return retro_atomic_cas_ptr(&m->owner, NULL, this_thread()) != 0;
}

static void mutex_relock(ult_mutex* m, uint32_t held) {
    park_until(&m->ec, relock_attempt, m);
    m->count = held;
}

/* ---- condition variable ---- */

void ult_condition_init(ult_condition* c, ult_mutex* mutex) {
    c->mutex = mutex;
    retro_atomic_int_init(&c->arrived, 0);
    retro_atomic_int_init(&c->released, 0);
    retro_eventcount_init(&c->ec);
}

void ult_condition_free(ult_condition* c) {
    retro_eventcount_free(&c->ec);
}

struct wait_ctx {
    ult_condition* c;
    uint32_t number;
};

static int wait_attempt(void* ctx) {
    struct wait_ctx* w = (struct wait_ctx*)ctx;
    /* Through once the signals have reached its number (the counters may wrap) */
    const uint32_t upto = (uint32_t)retro_atomic_load_acquire_int(&w->c->released);
    return (int32_t)(upto - w->number) > 0;
}

int ult_condition_wait(ult_condition* c) {
    struct wait_ctx w;
    uint32_t held;
    if (!ult_mutex_held_by_caller(c->mutex))
        return 0;
    /* Numbered before the mutex is let go: a signal sent under the mutex counts this one */
    w.c = c;
    w.number = (uint32_t)retro_atomic_fetch_add_int(&c->arrived, 1);
    held = mutex_unlock_all(c->mutex);
    park_until(&c->ec, wait_attempt, &w);
    mutex_relock(c->mutex, held);
    return 1;
}

static void condition_release(ult_condition* c, int all) {
    for (;;) {
        const uint32_t upto = (uint32_t)retro_atomic_load_acquire_int(&c->released);
        const uint32_t last = (uint32_t)retro_atomic_load_acquire_int(&c->arrived);
        if (upto == last)
            return; /* nobody waiting */
        if (retro_atomic_cas_int(&c->released, (int)upto, (int)(all ? last : upto + 1))) {
            retro_eventcount_notify(&c->ec);
            return;
        }
    }
}

void ult_condition_signal(ult_condition* c) {
    condition_release(c, 0);
}

void ult_condition_signal_all(ult_condition* c) {
    condition_release(c, 1);
}

/* ---- combiner ---- */

typedef struct combiner_request {
    mpsc_stack_node_t node; /* first: the stack links requests through it */
    void (*call)(void*);
    void* ctx;
    retro_atomic_int_t done;
} combiner_request;

static void combiner_init(ult_combiner* c) {
    mpsc_stack_init(&c->pending);
    retro_atomic_int_init(&c->busy, 0);
    retro_eventcount_init(&c->done_ec);
}

static void combiner_free(ult_combiner* c) {
    retro_eventcount_free(&c->done_ec);
}

/* With busy held: runs what is queued, then lets go of busy, unless more came
 * meanwhile and this thread can take busy again. */
static void combine(ult_combiner* c) {
    for (;;) {
        mpsc_stack_node_t* node = mpsc_stack_reverse(mpsc_stack_drain(&c->pending));
        const int ran = node != NULL;
        while (node) {
            combiner_request* request = (combiner_request*)node;
            node = node->next;
            request->call(request->ctx);
            /* The caller's: it may be gone as soon as done is set */
            retro_atomic_store_release_int(&request->done, 1);
        }
        if (ran)
            retro_eventcount_notify(&c->done_ec);
        (void)retro_atomic_exchange_int(&c->busy, 0);
        /* A push whose taking of busy failed is seen here (combiner_run's fence) */
        retro_atomic_thread_fence_seq_cst();
        if (mpsc_stack_empty(&c->pending) || retro_atomic_exchange_int(&c->busy, 1) != 0)
            return;
    }
}

/* Runs @call with no other function of this combiner running. @call hands
 * nothing to the same combiner. */
static void combiner_run(ult_combiner* c, void (*call)(void*), void* ctx) {
    combiner_request request;
    request.call = call;
    request.ctx = ctx;
    retro_atomic_int_init(&request.done, 0);
    mpsc_stack_push(&c->pending, &request.node);
    /* Pairs with combine's fence: a push that loses the race for busy is seen there */
    retro_atomic_thread_fence_seq_cst();
    for (;;) {
        int key;
        if (retro_atomic_exchange_int(&c->busy, 1) == 0)
            combine(c);
        if (retro_atomic_load_acquire_int(&request.done) != 0)
            return;
        key = retro_eventcount_prepare_wait(&c->done_ec);
        /* Not run yet, and nobody combining: whoever released found it pushed and is
         * about to take over, or this thread can */
        if (retro_atomic_load_acquire_int(&request.done) != 0 ||
            retro_atomic_load_seq_cst_int(&c->busy) == 0) {
            retro_eventcount_cancel_wait(&c->done_ec);
            continue;
        }
        retro_eventcount_commit_wait(&c->done_ec, key);
    }
}

/* ---- data pool ---- */

/* The links are unaligned in the work area: read and written bytewise */
static uint32_t pool_next(const ult_data_pool* p, uint32_t slot) {
    uint32_t next;
    memcpy(&next, p->data + (uint64_t)p->capacity * p->item_size + (uint64_t)slot * sizeof(next),
           sizeof(next));
    return next;
}

static void pool_set_next(ult_data_pool* p, uint32_t slot, uint32_t next) {
    memcpy(p->data + (uint64_t)p->capacity * p->item_size + (uint64_t)slot * sizeof(next), &next,
           sizeof(next));
}

void ult_data_pool_init(ult_data_pool* p, void* work_area, uint32_t capacity, uint64_t item_size) {
    uint32_t i;
    combiner_init(&p->combiner);
    retro_eventcount_init(&p->space);
    p->data = (uint8_t*)work_area;
    p->capacity = capacity;
    p->item_size = item_size;
    for (i = 0; i < capacity; i++)
        pool_set_next(p, i, i + 1 < capacity ? i + 1 : ULT_NONE);
    p->free_head = capacity ? 0 : ULT_NONE;
}

void ult_data_pool_free(ult_data_pool* p) {
    retro_eventcount_free(&p->space);
    combiner_free(&p->combiner);
}

struct pool_op {
    ult_data_pool* p;
    uint32_t slot;
};

static void pool_take_op(void* ctx) {
    struct pool_op* op = (struct pool_op*)ctx;
    op->slot = op->p->free_head;
    if (op->slot != ULT_NONE)
        op->p->free_head = pool_next(op->p, op->slot);
}

/* A free item, or ULT_NONE */
static uint32_t pool_take(ult_data_pool* p) {
    struct pool_op op;
    op.p = p;
    op.slot = ULT_NONE;
    combiner_run(&p->combiner, pool_take_op, &op);
    return op.slot;
}

static void pool_give_op(void* ctx) {
    struct pool_op* op = (struct pool_op*)ctx;
    pool_set_next(op->p, op->slot, op->p->free_head);
    op->p->free_head = op->slot;
}

static void pool_give(ult_data_pool* p, uint32_t slot) {
    struct pool_op op;
    op.p = p;
    op.slot = slot;
    combiner_run(&p->combiner, pool_give_op, &op);
    retro_eventcount_notify(&p->space);
}

/* ---- queue ---- */

void ult_queue_init(ult_queue* q, ult_data_pool* pool) {
    q->pool = pool;
    q->head = q->tail = ULT_NONE;
    retro_eventcount_init(&q->filled);
}

void ult_queue_free(ult_queue* q) {
    retro_eventcount_free(&q->filled);
}

struct queue_op {
    ult_queue* q;
    uint32_t slot;
};

static void append_op(void* ctx) {
    struct queue_op* op = (struct queue_op*)ctx;
    ult_queue* q = op->q;
    pool_set_next(q->pool, op->slot, ULT_NONE);
    if (q->tail == ULT_NONE)
        q->head = op->slot;
    else
        pool_set_next(q->pool, q->tail, op->slot);
    q->tail = op->slot;
}

static void queue_append(ult_queue* q, uint32_t slot, const void* item) {
    struct queue_op op;
    /* The item is this thread's alone until it is linked in */
    memcpy(q->pool->data + (uint64_t)slot * q->pool->item_size, item, (size_t)q->pool->item_size);
    op.q = q;
    op.slot = slot;
    combiner_run(&q->pool->combiner, append_op, &op);
    retro_eventcount_notify(&q->filled);
}

static void take_head_op(void* ctx) {
    struct queue_op* op = (struct queue_op*)ctx;
    ult_queue* q = op->q;
    op->slot = q->head;
    if (op->slot != ULT_NONE) {
        q->head = pool_next(q->pool, op->slot);
        if (q->head == ULT_NONE)
            q->tail = ULT_NONE;
    }
}

static uint32_t queue_take_head(ult_queue* q) {
    struct queue_op op;
    op.q = q;
    op.slot = ULT_NONE;
    combiner_run(&q->pool->combiner, take_head_op, &op);
    return op.slot;
}

static void queue_remove(ult_queue* q, uint32_t slot, void* item) {
    if (item) /* none when a queue being destroyed drops what it holds */
        memcpy(item, q->pool->data + (uint64_t)slot * q->pool->item_size,
               (size_t)q->pool->item_size);
    pool_give(q->pool, slot);
}

int ult_queue_try_push(ult_queue* q, const void* item) {
    const uint32_t slot = pool_take(q->pool);
    if (slot == ULT_NONE)
        return 0;
    queue_append(q, slot, item);
    return 1;
}

static int push_attempt(void* ctx) {
    struct queue_op* op = (struct queue_op*)ctx;
    op->slot = pool_take(op->q->pool);
    return op->slot != ULT_NONE;
}

void ult_queue_push(ult_queue* q, const void* item) {
    struct queue_op op;
    op.q = q;
    op.slot = ULT_NONE;
    park_until(&q->pool->space, push_attempt, &op);
    queue_append(q, op.slot, item);
}

int ult_queue_try_pop(ult_queue* q, void* item) {
    const uint32_t slot = queue_take_head(q);
    if (slot == ULT_NONE)
        return 0;
    queue_remove(q, slot, item);
    return 1;
}

static int pop_attempt(void* ctx) {
    struct queue_op* op = (struct queue_op*)ctx;
    op->slot = queue_take_head(op->q);
    return op->slot != ULT_NONE;
}

void ult_queue_pop(ult_queue* q, void* item) {
    struct queue_op op;
    op.q = q;
    op.slot = ULT_NONE;
    park_until(&q->filled, pop_attempt, &op);
    queue_remove(q, op.slot, item);
}
