/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* libSceUlt's objects without the game (src/core/libraries/ult/ult_sync.c): the
 * queue keeps every item, in order, across threads that fill and empty it and
 * through a pool with too few items for them; two queues share a pool; the
 * mutex excludes, counts its recursive locks and refuses a second lock and a
 * stranger's unlock; the semaphore lets no more through than it has; no signal
 * of a condition variable is lost and signal_all wakes every waiter; and with
 * a runtime of one worker no two user-level threads ever run at once, changing
 * only where one waits. */

#include <stdio.h>
#include <string.h>

#include <retro_atomic.h>
#include <rthreads/rthreads.h>

#include "core/libraries/ult/ult_sync.h"

static int failures;

#define CHECK(cond, what) \
   do { \
      if (!(cond)) { \
         printf("FAIL %s (line %d)\n", what, __LINE__); \
         failures++; \
      } \
   } while (0)

#define MAX_THREADS 8

static void run_threads(void (*func)(void *), void **args, int count) {
   sthread_t *threads[MAX_THREADS];
   int i;
   for (i = 0; i < count; i++)
      threads[i] = sthread_create(func, args ? args[i] : NULL);
   for (i = 0; i < count; i++)
      sthread_join(threads[i]);
}

static void run_one(void (*func)(void *), void *arg) {
   sthread_join(sthread_create(func, arg));
}

/* ---- queue ---- */

#define CAPACITY 5u

typedef struct item {
   uint32_t producer;
   uint32_t sequence;
   uint32_t check;
} item_t;

static ult_queue queue;
static retro_atomic_int_t popped, out_of_order, corrupt;

static void producer(void *arg) {
   const uint32_t p = (uint32_t)(size_t)arg;
   uint32_t n;
   for (n = 0; n < 20000; n++) {
      item_t it;
      it.producer = p;
      it.sequence = n;
      it.check = p * 31 + n;
      ult_queue_push(&queue, &it);
   }
}

static void consumer(void *arg) {
   uint32_t last[3] = {0, 0, 0};
   int seen[3] = {0, 0, 0};
   (void)arg;
   while (retro_atomic_fetch_add_int(&popped, 1) < 60000) {
      item_t it;
      ult_queue_pop(&queue, &it);
      if (it.producer > 2 || it.check != it.producer * 31 + it.sequence) {
         retro_atomic_fetch_add_int(&corrupt, 1);
         continue;
      }
      /* Each consumer sees a producer's items in the order they were pushed */
      if (seen[it.producer] && it.sequence <= last[it.producer])
         retro_atomic_fetch_add_int(&out_of_order, 1);
      seen[it.producer] = 1;
      last[it.producer] = it.sequence;
   }
}

static void queue_thread(void *arg) {
   const size_t t = (size_t)arg;
   if (t < 3)
      producer(arg);
   else
      consumer(arg);
}

static void test_queue(void) {
   /* 5 items of 12 bytes for 3 producers of 20000 each: pushes wait for the poppers */
   static uint8_t work[ULT_DATA_POOL_WORK_AREA(CAPACITY, sizeof(item_t)) + 1];
   ult_data_pool pool;
   ult_queue other;
   void *args[5];
   item_t it;
   uint32_t n;
   size_t i;
   ult_data_pool_init(&pool, work + 1, CAPACITY, sizeof(item_t)); /* unaligned, as a game's may be */
   ult_queue_init(&queue, &pool);
   retro_atomic_int_init(&popped, 0);
   retro_atomic_int_init(&out_of_order, 0);
   retro_atomic_int_init(&corrupt, 0);
   for (i = 0; i < 5; i++)
      args[i] = (void *)i;
   run_threads(queue_thread, args, 5);
   CHECK(retro_atomic_load_acquire_int(&corrupt) == 0, "every item whole");
   CHECK(retro_atomic_load_acquire_int(&out_of_order) == 0, "first in, first out");
   CHECK(!ult_queue_try_pop(&queue, &it), "all 60000 popped, none left");

   /* Full and empty, without waiting */
   for (n = 0; n < CAPACITY; n++) {
      item_t full;
      full.producer = 9;
      full.sequence = n;
      full.check = 0;
      CHECK(ult_queue_try_push(&queue, &full), "room for the pool's items");
   }
   memset(&it, 0, sizeof(it));
   CHECK(!ult_queue_try_push(&queue, &it), "no push into a full pool");
   /* A second queue of the same pool gets nothing until the first gives an item back */
   ult_queue_init(&other, &pool);
   CHECK(!ult_queue_try_push(&other, &it), "the pool's items are shared by its queues");
   CHECK(ult_queue_try_pop(&queue, &it) && it.sequence == 0, "oldest first");
   CHECK(ult_queue_try_push(&other, &it) && ult_queue_try_pop(&other, &it) && it.producer == 9,
         "the second queue takes the freed item");
   for (n = 1; n < CAPACITY; n++)
      CHECK(ult_queue_try_pop(&queue, &it) && it.sequence == n, "in order");
   CHECK(!ult_queue_try_pop(&queue, &it) && !ult_queue_try_pop(&other, &it), "both empty");
   /* An item dropped, as a queue being destroyed drops what it holds */
   CHECK(ult_queue_try_push(&queue, &it) && ult_queue_try_pop(&queue, NULL), "dropped");
   CHECK(ult_queue_try_push(&queue, &it) && ult_queue_try_pop(&queue, &it), "and its place back");
   ult_queue_free(&other);
   ult_queue_free(&queue);
   ult_data_pool_free(&pool);
}

/* ---- mutex ---- */

static ult_mutex plain, recursive;
static long counter;

static void count_up(void *arg) {
   int n;
   (void)arg;
   for (n = 0; n < 50000; n++) {
      ult_mutex_lock(&plain);
      counter++;
      ult_mutex_unlock(&plain);
   }
}

static void stranger(void *arg) {
   (void)arg;
   CHECK(ult_mutex_try_lock(&plain) == ULT_MUTEX_BUSY, "busy for another thread");
   CHECK(!ult_mutex_unlock(&plain), "not unlocked by a stranger");
}

static void still_held(void *arg) {
   (void)arg;
   CHECK(ult_mutex_try_lock(&recursive) == ULT_MUTEX_BUSY, "still held");
}

static void free_after(void *arg) {
   (void)arg;
   CHECK(ult_mutex_try_lock(&recursive) == ULT_MUTEX_LOCKED, "free after both");
   ult_mutex_unlock(&recursive);
}

static void test_mutex(void) {
   ult_mutex_init(&plain, 0);
   ult_mutex_init(&recursive, 1);
   counter = 0;
   run_threads(count_up, NULL, 4);
   CHECK(counter == 200000, "mutex excludes");
   CHECK(ult_mutex_lock(&plain) == ULT_MUTEX_LOCKED, "locked");
   CHECK(ult_mutex_try_lock(&plain) == ULT_MUTEX_RELOCKED, "a plain mutex is not locked twice");
   run_one(stranger, NULL);
   CHECK(ult_mutex_unlock(&plain) && !ult_mutex_unlock(&plain), "unlocked once");
   CHECK(ult_mutex_lock(&recursive) == ULT_MUTEX_LOCKED &&
               ult_mutex_try_lock(&recursive) == ULT_MUTEX_LOCKED,
         "a recursive mutex locked twice");
   CHECK(ult_mutex_unlock(&recursive), "first unlock");
   run_one(still_held, NULL);
   CHECK(ult_mutex_unlock(&recursive), "second unlock");
   run_one(free_after, NULL);
   ult_mutex_free(&plain);
   ult_mutex_free(&recursive);
}

/* ---- semaphore ---- */

static ult_semaphore semaphore;
static retro_atomic_int_t inside, most;

static void through(void *arg) {
   int n;
   (void)arg;
   for (n = 0; n < 5000; n++) {
      int now, seen;
      ult_semaphore_acquire(&semaphore, 1);
      now = retro_atomic_fetch_add_int(&inside, 1) + 1;
      seen = retro_atomic_load_acquire_int(&most);
      while (now > seen && !retro_atomic_cas_int(&most, seen, now))
         seen = retro_atomic_load_acquire_int(&most);
      retro_atomic_fetch_sub_int(&inside, 1);
      ult_semaphore_release(&semaphore, 1);
   }
}

static void test_semaphore(void) {
   ult_semaphore_init(&semaphore, 3);
   retro_atomic_int_init(&inside, 0);
   retro_atomic_int_init(&most, 0);
   run_threads(through, NULL, 8);
   CHECK(retro_atomic_load_acquire_int(&most) <= 3, "no more through than it has");
   CHECK(ult_semaphore_try_acquire(&semaphore, 3) && !ult_semaphore_try_acquire(&semaphore, 1),
         "all three, then none");
   ult_semaphore_release(&semaphore, 2);
   CHECK(!ult_semaphore_try_acquire(&semaphore, 3) && ult_semaphore_try_acquire(&semaphore, 2),
         "two given back");
   ult_semaphore_free(&semaphore);
}

/* ---- condition variable ---- */

static ult_mutex cmutex;
static ult_condition condition;
static int turn, go, woken;
static retro_atomic_int_t waiting;

static void other_side(void *arg) {
   int n;
   (void)arg;
   for (n = 0; n < 20000; n++) {
      ult_mutex_lock(&cmutex);
      while (turn != 1)
         ult_condition_wait(&condition);
      turn = 0;
      ult_condition_signal_all(&condition);
      ult_mutex_unlock(&cmutex);
   }
}

static void waiter(void *arg) {
   (void)arg;
   ult_mutex_lock(&cmutex);
   retro_atomic_fetch_add_int(&waiting, 1);
   while (go == 0)
      ult_condition_wait(&condition);
   go--;
   woken++;
   ult_mutex_unlock(&cmutex);
}

static void test_condition(void) {
   sthread_t *other, *threads[6];
   int n, t;
   ult_mutex_init(&cmutex, 0);
   ult_condition_init(&condition, &cmutex);
   /* A handshake of 20000 turns: a lost signal would hang it */
   turn = 0;
   other = sthread_create(other_side, NULL);
   for (n = 0; n < 20000; n++) {
      ult_mutex_lock(&cmutex);
      while (turn != 0)
         ult_condition_wait(&condition);
      turn = 1;
      ult_condition_signal_all(&condition);
      ult_mutex_unlock(&cmutex);
   }
   sthread_join(other);
   CHECK(turn == 0, "20000 turns handed over");
   CHECK(!ult_condition_wait(&condition), "no wait without the mutex");

   /* Six waiters: one signal wakes one, signal_all the rest */
   go = woken = 0;
   retro_atomic_int_init(&waiting, 0);
   for (t = 0; t < 6; t++)
      threads[t] = sthread_create(waiter, NULL);
   while (retro_atomic_load_acquire_int(&waiting) < 6)
      sthread_yield();
   ult_mutex_lock(&cmutex); /* all six are in the wait once this is had */
   go = 1;
   ult_condition_signal(&condition);
   ult_mutex_unlock(&cmutex);
   for (;;) {
      int one;
      ult_mutex_lock(&cmutex);
      one = woken == 1;
      ult_mutex_unlock(&cmutex);
      if (one)
         break;
      sthread_yield();
   }
   ult_mutex_lock(&cmutex);
   CHECK(woken == 1, "one signal, one waiter");
   go = 5;
   ult_condition_signal_all(&condition);
   ult_mutex_unlock(&cmutex);
   for (t = 0; t < 6; t++)
      sthread_join(threads[t]);
   CHECK(woken == 6, "signal_all wakes the rest");
   ult_condition_free(&condition);
   ult_mutex_free(&cmutex);
}

/* ---- a runtime of one worker ---- */

static ult_tokens tokens;
static ult_mutex tmutex;
static ult_queue tqueue;
static retro_atomic_int_t running, overlaps;
static long shared; /* changed with no lock: only one of them runs at a time */

/* Every thread that parks while this test runs is one of its user-level threads */
static void stop(void) {
   retro_atomic_fetch_sub_int(&running, 1);
   ult_tokens_give(&tokens);
}

static void start(void) {
   ult_tokens_take(&tokens);
   if (retro_atomic_fetch_add_int(&running, 1) + 1 != 1)
      retro_atomic_fetch_add_int(&overlaps, 1);
}

static void ulthread(void *arg) {
   const size_t t = (size_t)arg;
   int n;
   start();
   for (n = 0; n < 3000; n++) {
      shared++;
      if (t % 2 == 0) {
         ult_queue_push(&tqueue, &n); /* waits when the two items are taken */
      } else {
         int value;
         ult_queue_pop(&tqueue, &value); /* waits when there is none */
      }
      shared++;
      ult_mutex_lock(&tmutex);
      shared++;
      ult_mutex_unlock(&tmutex);
   }
   stop();
}

static void test_tokens(void) {
   static uint8_t work[ULT_DATA_POOL_WORK_AREA(2, sizeof(int))];
   ult_data_pool pool;
   void *args[6];
   size_t i;
   ult_tokens_init(&tokens, 1);
   ult_mutex_init(&tmutex, 0);
   ult_data_pool_init(&pool, work, 2, sizeof(int));
   ult_queue_init(&tqueue, &pool);
   retro_atomic_int_init(&running, 0);
   retro_atomic_int_init(&overlaps, 0);
   shared = 0;
   ult_sync_set_park_hooks(stop, start);
   for (i = 0; i < 6; i++)
      args[i] = (void *)i;
   run_threads(ulthread, args, 6);
   ult_sync_set_park_hooks(NULL, NULL);
   CHECK(retro_atomic_load_acquire_int(&overlaps) == 0,
         "one worker: never two running at once");
   CHECK(shared == 6 * 3000 * 3, "what they share kept whole with no lock of its own");
   ult_queue_free(&tqueue);
   ult_data_pool_free(&pool);
   ult_mutex_free(&tmutex);
   ult_tokens_free(&tokens);
}

int main(void) {
   test_queue();
   test_mutex();
   test_semaphore();
   test_condition();
   test_tokens();
   if (failures) {
      printf("%d failures\n", failures);
      return 1;
   }
   printf("ult_sync_test: passed\n");
   return 0;
}
