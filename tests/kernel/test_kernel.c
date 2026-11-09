/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/kernel.h>
#include <pbl/util/size.h>

#include <clar.h>
#include <kernel_test.h>

// The kernel on the POSIX arch: threads are pthreads that run one at
// a time under the kernel's scheduling decisions, and time only moves when a
// test delivers ticks or every thread is blocked.

[[noreturn]] void pbl_panic_bare_at(const char *filename, int line) {
  fprintf(stderr, "kernel assert at %s:%d\n", filename, line);
  abort();
}

#define STACK 4096
static uint8_t s_stacks[8][STACK] PBL_ALIGNED(8);
static struct pbl_thread s_threads[8];

static char s_trace[128];

static void prv_trace(char c) {
  size_t n = strlen(s_trace);
  if (n + 1 < sizeof(s_trace)) {
    s_trace[n] = c;
    s_trace[n + 1] = '\0';
  }
}

static struct pbl_thread *prv_spawn(int i, const char *name, pbl_prio_t prio, void (*entry)(void *),
                                    void *arg) {
  struct pbl_thread_attr attr = {
    .name = name,
    .entry = entry,
    .arg = arg,
    .prio = prio,
    .privileged = true,
    .stack = s_stacks[i],
    .stack_size = STACK,
  };
  cl_assert_equal_i(pbl_thread_create(&s_threads[i], &attr), 0);
  return &s_threads[i];
}

void test_kernel__initialize(void) {
  s_trace[0] = '\0';
  memset(s_threads, 0, sizeof(s_threads));
}

void test_kernel__cleanup(void) {
}

// ---- scheduling -------------------------------------------------------------

static void prv_lo_entry(void *arg) {
  prv_trace('L');
  pbl_test_kernel_stop();
}

static void prv_hi_entry(void *arg) {
  prv_trace('H');
  pbl_thread_sleep(PBL_TICKS(5));
  prv_trace('h');
}

void test_kernel__priority_preempts(void) {
  prv_spawn(0, "lo", 1, prv_lo_entry, nullptr);
  prv_spawn(1, "hi", 3, prv_hi_entry, nullptr);
  pbl_test_kernel_run();
  // hi runs first, sleeps, lo runs to the stop while hi is asleep
  cl_assert_equal_s(s_trace, "HL");
}

static void prv_sleep_then_stop(void *arg) {
  pbl_tick_t start = pbl_uptime_ticks();
  pbl_thread_sleep(PBL_TICKS(10));
  cl_assert_equal_i(pbl_uptime_ticks() - start, 10);
  pbl_thread_sleep(PBL_MSEC(0)); // yield only
  prv_trace('S');
  pbl_test_kernel_stop();
}

void test_kernel__sleep_advances_virtual_time(void) {
  prv_spawn(0, "sleeper", 2, prv_sleep_then_stop, nullptr);
  pbl_test_kernel_run();
  cl_assert_equal_s(s_trace, "S");
}

static void prv_rr_entry(void *arg) {
  char c = (char)(uintptr_t)arg;
  for (int i = 0; i < 3; i++) {
    prv_trace(c);
    pbl_thread_yield();
  }
  if (c == 'B') {
    pbl_test_kernel_stop();
  }
}

void test_kernel__yield_round_robins_equal_priority(void) {
  prv_spawn(0, "a", 2, prv_rr_entry, (void *)'A');
  prv_spawn(1, "b", 2, prv_rr_entry, (void *)'B');
  pbl_test_kernel_run();
  cl_assert_equal_s(s_trace, "ABABAB");
}

static void prv_spin_entry(void *arg) {
  char c = (char)(uintptr_t)arg;
  for (int i = 0; i < 4; i++) {
    prv_trace(c);
    // Deliver a tick "from an ISR" while running: time slicing rotates us out.
    pbl_test_tick(1);
  }
  if (c == 'B') {
    pbl_test_kernel_stop();
  }
}

void test_kernel__tick_time_slices(void) {
  prv_spawn(0, "a", 2, prv_spin_entry, (void *)'A');
  prv_spawn(1, "b", 2, prv_spin_entry, (void *)'B');
  pbl_test_kernel_run();
  cl_assert_equal_s(s_trace, "ABABABAB");
}

// ---- delays and uptime ------------------------------------------------------

static void prv_sleep_rounds_up(void *arg) {
  // 1024 Hz: 10 ms is 10.24 ticks, rounded up plus the partial tick.
  uint64_t start = pbl_uptime_ticks();
  pbl_sleep_ms(10);
  cl_assert_equal_i(pbl_uptime_ticks() - start, 12);

  start = pbl_uptime_ticks();
  pbl_sleep_us(1);
  cl_assert_equal_i(pbl_uptime_ticks() - start, 2);

  start = pbl_uptime_ticks();
  pbl_sleep_ms(0);
  cl_assert_equal_i(pbl_uptime_ticks() - start, 0);

  pbl_test_kernel_stop();
}

void test_kernel__sleep_waits_at_least_the_request(void) {
  prv_spawn(0, "sleeper", 2, prv_sleep_rounds_up, nullptr);
  pbl_test_kernel_run();
}

static void prv_sleep_past_wrap(void *arg) {
  for (int i = 0; i < 3; i++) {
    pbl_thread_sleep(PBL_TICKS(INT32_MAX));
  }
  cl_assert(pbl_uptime_ticks() >= 3ULL * INT32_MAX);
  cl_assert_equal_i(pbl_uptime_ms(), pbl_uptime_ticks() * 1000 / PBL_TICK_HZ);
  pbl_test_kernel_stop();
}

void test_kernel__uptime_does_not_wrap(void) {
  prv_spawn(0, "sleeper", 2, prv_sleep_past_wrap, nullptr);
  pbl_test_kernel_run();
}

static void prv_realtime_follows_uptime(void *arg) {
  pbl_realtime_set(1800000000);
  cl_assert_equal_i(pbl_realtime(), 1800000000);
  pbl_thread_sleep(PBL_TICKS(3 * PBL_TICK_HZ / 2));
  cl_assert_equal_i(pbl_realtime(), 1800000001);
  cl_assert(pbl_realtime_ms() >= 1800000001500ULL);
  pbl_realtime_set(1700000000);
  cl_assert_equal_i(pbl_realtime(), 1700000000);
  pbl_test_kernel_stop();
}

void test_kernel__realtime_follows_uptime(void) {
  prv_spawn(0, "clock", 2, prv_realtime_follows_uptime, nullptr);
  pbl_test_kernel_run();
}

void test_kernel__busy_wait_waits_at_least_the_request(void) {
  struct timespec t0, t1;
  clock_gettime(CLOCK_MONOTONIC, &t0);
  pbl_busy_wait_us(2000);
  clock_gettime(CLOCK_MONOTONIC, &t1);
  int64_t ns = (t1.tv_sec - t0.tv_sec) * 1000000000LL + (t1.tv_nsec - t0.tv_nsec);
  cl_assert(ns >= 2000000);
}

static int s_poll_count;

static bool prv_poll_ready(int after) {
  return ++s_poll_count > after;
}

static void prv_poll_sleep(void *arg) {
  s_poll_count = 0;
  cl_assert_equal_i(PBL_POLL_SLEEP_MS(prv_poll_ready(3), 1, 100), 0);
  cl_assert_equal_i(s_poll_count, 4);

  // Never true: times out after at least 20 ms, with one last evaluation.
  s_poll_count = 0;
  uint64_t start = pbl_uptime_ms();
  cl_assert_equal_i(PBL_POLL_SLEEP_MS(prv_poll_ready(1000), 5, 20), -ETIMEDOUT);
  cl_assert(pbl_uptime_ms() - start >= 20);
  cl_assert(s_poll_count >= 2 && s_poll_count <= 6);

  pbl_test_kernel_stop();
}

void test_kernel__poll_sleep(void) {
  prv_spawn(0, "poller", 2, prv_poll_sleep, nullptr);
  pbl_test_kernel_run();
}

void test_kernel__poll_busy(void) {
  s_poll_count = 0;
  cl_assert_equal_i(PBL_POLL_BUSY_US(prv_poll_ready(2), 10, 100000), 0);
  cl_assert_equal_i(s_poll_count, 3);

  struct timespec t0, t1;
  clock_gettime(CLOCK_MONOTONIC, &t0);
  cl_assert_equal_i(PBL_POLL_BUSY_US(false, 100, 2000), -ETIMEDOUT);
  clock_gettime(CLOCK_MONOTONIC, &t1);
  int64_t ns = (t1.tv_sec - t0.tv_sec) * 1000000000LL + (t1.tv_nsec - t0.tv_nsec);
  cl_assert(ns >= 2000000);
}

// ---- semaphores -------------------------------------------------------------

static PBL_SEM_DEFINE(s_sem, 0, 1);

static void prv_taker(void *arg) {
  prv_trace('t');
  cl_assert_equal_i(pbl_sem_take(&s_sem, PBL_FOREVER), 0);
  prv_trace('T');
  cl_assert_equal_i(pbl_sem_take(&s_sem, PBL_TICKS(3)), -EAGAIN);
  prv_trace('x');
  cl_assert_equal_i(pbl_sem_take(&s_sem, PBL_NO_WAIT), -EBUSY);
  pbl_test_kernel_stop();
}

static void prv_giver(void *arg) {
  prv_trace('g');
  pbl_sem_give(&s_sem);
  prv_trace('G');
}

void test_kernel__semaphore_handoff_and_timeout(void) {
  pbl_sem_init(&s_sem, s_sem.initial, s_sem.limit);
  prv_spawn(0, "taker", 3, prv_taker, nullptr);
  prv_spawn(1, "giver", 2, prv_giver, nullptr);
  pbl_test_kernel_run();
  // taker blocks, giver gives and is preempted by the higher-priority taker,
  // which then times out on its second take after the giver finished
  cl_assert_equal_s(s_trace, "tgTGx");
}

static void prv_isr_taker(void *arg) {
  cl_assert_equal_i(pbl_sem_take(&s_sem, PBL_FOREVER), 0);
  prv_trace('T');
  pbl_test_kernel_stop();
}

static void prv_isr_giver(void *arg) {
  prv_trace('i');
  pbl_test_isr_enter();
  pbl_sem_give(&s_sem);
  prv_trace('g');
  pbl_test_isr_exit(); // the higher-priority taker runs on ISR exit
  prv_trace('r');
}

void test_kernel__semaphore_give_from_isr(void) {
  pbl_sem_init(&s_sem, s_sem.initial, s_sem.limit);
  prv_spawn(0, "taker", 3, prv_isr_taker, nullptr);
  prv_spawn(1, "giver", 2, prv_isr_giver, nullptr);
  pbl_test_kernel_run();
  cl_assert_equal_s(s_trace, "igT");
}

void test_kernel__semaphore_counts_to_limit(void) {
  struct pbl_sem s;
  pbl_sem_init(&s, 0, 2);
  pbl_sem_give(&s);
  pbl_sem_give(&s);
  pbl_sem_give(&s);
  cl_assert_equal_i(pbl_sem_count(&s), 2);
  pbl_sem_reset(&s);
  cl_assert_equal_i(pbl_sem_count(&s), 0);
}

// ---- timers -----------------------------------------------------------------

static struct pbl_timer s_timer;
static uint64_t s_timer_fired_at;
static bool s_timer_in_isr;

static void prv_timer_fn(struct pbl_timer *timer) {
  cl_assert(timer == &s_timer);
  s_timer_fired_at = pbl_uptime_ticks();
  s_timer_in_isr = pbl_in_isr();
  pbl_sem_give(&s_sem);
}

static void prv_timer_waiter(void *arg) {
  pbl_timer_start(&s_timer, PBL_TICKS(7));
  cl_assert(pbl_timer_is_active(&s_timer));
  cl_assert_equal_i(pbl_sem_take(&s_sem, PBL_FOREVER), 0);
  cl_assert_equal_i(s_timer_fired_at, 7);
  cl_assert(s_timer_in_isr);
  cl_assert(!pbl_timer_is_active(&s_timer));
  pbl_test_kernel_stop();
}

void test_kernel__timer_expires_in_isr_while_idle(void) {
  pbl_sem_init(&s_sem, 0, 1);
  pbl_timer_init(&s_timer, prv_timer_fn);
  prv_spawn(0, "waiter", 2, prv_timer_waiter, nullptr);
  pbl_test_kernel_run();
}

static void prv_timer_restart_stop(void *arg) {
  pbl_timer_start(&s_timer, PBL_TICKS(5));
  pbl_thread_sleep(PBL_TICKS(3));
  pbl_timer_start(&s_timer, PBL_TICKS(5));
  cl_assert_equal_i(pbl_sem_take(&s_sem, PBL_FOREVER), 0);
  cl_assert_equal_i(s_timer_fired_at, 8);

  pbl_timer_start(&s_timer, PBL_TICKS(2));
  pbl_timer_stop(&s_timer);
  cl_assert(!pbl_timer_is_active(&s_timer));
  cl_assert_equal_i(pbl_sem_take(&s_sem, PBL_TICKS(10)), -EAGAIN);
  pbl_test_kernel_stop();
}

void test_kernel__timer_restart_and_stop(void) {
  pbl_sem_init(&s_sem, 0, 1);
  pbl_timer_init(&s_timer, prv_timer_fn);
  prv_spawn(0, "waiter", 2, prv_timer_restart_stop, nullptr);
  pbl_test_kernel_run();
}

#define NUM_ORDER_TIMERS 3

static struct pbl_timer s_order_timers[NUM_ORDER_TIMERS];
static int s_timer_order[NUM_ORDER_TIMERS];
static int s_timer_count;

static void prv_timer_order_fn(struct pbl_timer *timer) {
  s_timer_order[s_timer_count++] = (int)(timer - s_order_timers);
  if (s_timer_count == NUM_ORDER_TIMERS) {
    pbl_sem_give(&s_sem);
  }
}

static void prv_timer_order(void *arg) {
  pbl_timer_start(&s_order_timers[0], PBL_TICKS(9));
  pbl_timer_start(&s_order_timers[1], PBL_TICKS(3));
  pbl_timer_start(&s_order_timers[2], PBL_TICKS(6));
  cl_assert_equal_i(pbl_sem_take(&s_sem, PBL_FOREVER), 0);
  cl_assert_equal_i(pbl_uptime_ticks(), 9);
  pbl_test_kernel_stop();
}

void test_kernel__timers_expire_in_order(void) {
  pbl_sem_init(&s_sem, 0, 1);
  s_timer_count = 0;
  for (int i = 0; i < NUM_ORDER_TIMERS; i++) {
    pbl_timer_init(&s_order_timers[i], prv_timer_order_fn);
  }
  prv_spawn(0, "waiter", 2, prv_timer_order, nullptr);
  pbl_test_kernel_run();
  cl_assert_equal_i(s_timer_order[0], 1);
  cl_assert_equal_i(s_timer_order[1], 2);
  cl_assert_equal_i(s_timer_order[2], 0);
}

// ---- mutexes ----------------------------------------------------------------

static PBL_MUTEX_DEFINE(s_mutex);

static void prv_pi_low(void *arg) {
  cl_assert_equal_i(pbl_mutex_lock(&s_mutex, PBL_FOREVER), 0);
  prv_trace('l');
  // Stay busy through a tick: high and medium wake, high blocks on the
  // mutex and lends us its priority, so medium must not run yet.
  pbl_test_tick(1);
  cl_assert_equal_i(pbl_thread_current()->prio, 4);
  prv_trace('L');
  pbl_mutex_unlock(&s_mutex);
  cl_assert_equal_i(pbl_thread_current()->prio, 1);
  prv_trace('d');
}

static void prv_pi_medium(void *arg) {
  pbl_thread_sleep(PBL_TICKS(1));
  prv_trace('M');
}

static void prv_pi_high(void *arg) {
  pbl_thread_sleep(PBL_TICKS(1));
  prv_trace('h');
  cl_assert_equal_i(pbl_mutex_lock(&s_mutex, PBL_FOREVER), 0);
  cl_assert(pbl_mutex_is_owner(&s_mutex));
  prv_trace('H');
  pbl_mutex_unlock(&s_mutex);
  pbl_thread_sleep(PBL_TICKS(5));
  pbl_test_kernel_stop();
}

void test_kernel__mutex_priority_inheritance(void) {
  pbl_mutex_init(&s_mutex);
  prv_spawn(0, "low", 1, prv_pi_low, nullptr);
  prv_spawn(1, "med", 2, prv_pi_medium, nullptr);
  prv_spawn(2, "high", 4, prv_pi_high, nullptr);
  pbl_test_kernel_run();
  // low takes the lock; high blocks and boosts low over medium; low hands
  // the mutex to high, then medium outranks the deboosted low
  cl_assert_equal_s(s_trace, "lhLHMd");
}

static void prv_recursive(void *arg) {
  cl_assert_equal_i(pbl_mutex_lock(&s_mutex, PBL_FOREVER), 0);
  cl_assert_equal_i(pbl_mutex_lock(&s_mutex, PBL_FOREVER), 0);
  cl_assert_equal_i(s_mutex.count, 2);
  pbl_mutex_unlock(&s_mutex);
  cl_assert(pbl_mutex_is_owner(&s_mutex));
  pbl_mutex_unlock(&s_mutex);
  cl_assert(!pbl_mutex_is_owner(&s_mutex));
  pbl_test_kernel_stop();
}

void test_kernel__mutex_recursive(void) {
  pbl_mutex_init(&s_mutex);
  prv_spawn(0, "r", 2, prv_recursive, nullptr);
  pbl_test_kernel_run();
}

static void prv_holder(void *arg) {
  pbl_mutex_lock(&s_mutex, PBL_FOREVER);
  pbl_thread_sleep(PBL_TICKS(10));
  pbl_mutex_unlock(&s_mutex);
}

static void prv_timed_locker(void *arg) {
  pbl_thread_sleep(PBL_TICKS(1));
  cl_assert_equal_i(pbl_mutex_lock(&s_mutex, PBL_NO_WAIT), -EBUSY);
  cl_assert_equal_i(pbl_mutex_lock(&s_mutex, PBL_TICKS(3)), -EAGAIN);
  cl_assert_equal_i(pbl_mutex_lock(&s_mutex, PBL_FOREVER), 0);
  cl_assert(pbl_uptime_ticks() >= 10);
  pbl_mutex_unlock(&s_mutex);
  pbl_test_kernel_stop();
}

void test_kernel__mutex_timeouts(void) {
  pbl_mutex_init(&s_mutex);
  prv_spawn(0, "holder", 2, prv_holder, nullptr);
  prv_spawn(1, "locker", 3, prv_timed_locker, nullptr);
  pbl_test_kernel_run();
}

// ---- message queues ---------------------------------------------------------

static PBL_MSGQ_DEFINE(s_q, sizeof(int), 2);

static void prv_producer(void *arg) {
  for (int i = 1; i <= 4; i++) {
    cl_assert_equal_i(pbl_msgq_put(&s_q, &i, PBL_FOREVER), 0);
    prv_trace('0' + i);
  }
}

static void prv_consumer(void *arg) {
  pbl_thread_sleep(PBL_TICKS(1)); // let the producer fill the queue and block
  cl_assert_equal_i(pbl_msgq_num_used(&s_q), 2);
  for (int i = 1; i <= 4; i++) {
    int v;
    cl_assert_equal_i(pbl_msgq_get(&s_q, &v, PBL_FOREVER), 0);
    cl_assert_equal_i(v, i);
    prv_trace('a' + i - 1);
  }
  int v;
  cl_assert_equal_i(pbl_msgq_get(&s_q, &v, PBL_TICKS(2)), -EAGAIN);
  cl_assert_equal_i(pbl_msgq_get(&s_q, &v, PBL_NO_WAIT), -EBUSY);
  pbl_test_kernel_stop();
}

void test_kernel__msgq_blocks_full_and_empty(void) {
  pbl_msgq_init(&s_q, s_q.buf, s_q.msg_size, s_q.max_msgs);
  prv_spawn(0, "producer", 2, prv_producer, nullptr);
  prv_spawn(1, "consumer", 3, prv_consumer, nullptr);
  pbl_test_kernel_run();
  // producer puts 1,2 then blocks; every put wakes the higher-priority
  // consumer, which drains the message before the producer can trace it
  cl_assert_equal_s(s_trace, "12abc3d4");
}

void test_kernel__msgq_front_peek_purge(void) {
  int buf[3];
  struct pbl_msgq q;
  pbl_msgq_init(&q, buf, sizeof(int), 3);
  int v = 1;
  cl_assert_equal_i(pbl_msgq_put(&q, &v, PBL_NO_WAIT), 0);
  v = 2;
  cl_assert_equal_i(pbl_msgq_put_front(&q, &v, PBL_NO_WAIT), 0);
  cl_assert_equal_i(pbl_msgq_peek(&q, &v), 0);
  cl_assert_equal_i(v, 2);
  cl_assert_equal_i(pbl_msgq_num_free(&q), 1);
  pbl_msgq_purge(&q);
  cl_assert_equal_i(pbl_msgq_num_used(&q), 0);
  cl_assert_equal_i(pbl_msgq_peek(&q, &v), -EBUSY);
}

// ---- poll groups ------------------------------------------------------------

static PBL_MSGQ_DEFINE(s_qa, sizeof(int), 4);
static PBL_MSGQ_DEFINE(s_qb, sizeof(int), 4);
static PBL_POLL_GROUP_DEFINE(s_group);

static void prv_poll_waiter(void *arg) {
  cl_assert(pbl_poll_group_is_empty(&s_group));
  cl_assert(pbl_poll_group_wait(&s_group, PBL_TICKS(2)) == nullptr);
  struct pbl_msgq *ready = pbl_poll_group_wait(&s_group, PBL_FOREVER);
  cl_assert(ready == &s_qb);
  int v;
  cl_assert_equal_i(pbl_msgq_get(ready, &v, PBL_NO_WAIT), 0);
  cl_assert_equal_i(v, 7);
  // both queues loaded: the scan rotates so neither starves
  v = 1;
  pbl_msgq_put(&s_qa, &v, PBL_NO_WAIT);
  pbl_msgq_put(&s_qb, &v, PBL_NO_WAIT);
  cl_assert(pbl_poll_group_wait(&s_group, PBL_NO_WAIT) == &s_qa);
  pbl_msgq_get(&s_qa, &v, PBL_NO_WAIT);
  cl_assert(pbl_poll_group_wait(&s_group, PBL_NO_WAIT) == &s_qb);
  pbl_msgq_get(&s_qb, &v, PBL_NO_WAIT);
  cl_assert(pbl_poll_group_wait(&s_group, PBL_NO_WAIT) == nullptr);
  pbl_test_kernel_stop();
}

static void prv_poll_poster(void *arg) {
  pbl_thread_sleep(PBL_TICKS(5));
  int v = 7;
  pbl_msgq_put(&s_qb, &v, PBL_NO_WAIT);
}

void test_kernel__poll_group(void) {
  pbl_msgq_init(&s_qa, s_qa.buf, s_qa.msg_size, s_qa.max_msgs);
  pbl_msgq_init(&s_qb, s_qb.buf, s_qb.msg_size, s_qb.max_msgs);
  pbl_poll_group_init(&s_group);
  pbl_poll_group_add(&s_group, &s_qa);
  pbl_poll_group_add(&s_group, &s_qb);
  prv_spawn(0, "waiter", 3, prv_poll_waiter, nullptr);
  prv_spawn(1, "poster", 2, prv_poll_poster, nullptr);
  pbl_test_kernel_run();
}

// ---- thread lifecycle -------------------------------------------------------

static void prv_victim(void *arg) {
  for (;;) {
    prv_trace('v');
    pbl_thread_sleep(PBL_TICKS(1));
  }
}

static void prv_controller(void *arg) {
  struct pbl_thread *victim = arg;
  pbl_thread_sleep(PBL_TICKS(2));
  pbl_thread_suspend(victim);
  cl_assert_equal_i(pbl_thread_state(victim), PBL_THREAD_SUSPENDED);
  size_t before = strlen(s_trace);
  pbl_thread_sleep(PBL_TICKS(5));
  cl_assert_equal_i(strlen(s_trace), before);
  pbl_thread_resume(victim);
  pbl_thread_sleep(PBL_TICKS(3));
  cl_assert(strlen(s_trace) > before);
  pbl_thread_abort(victim);
  cl_assert_equal_i(pbl_thread_state(victim), PBL_THREAD_DEAD);
  cl_assert_equal_i(pbl_thread_count(), 2); // idle + this one
  pbl_test_kernel_stop();
}

void test_kernel__suspend_resume_abort(void) {
  struct pbl_thread *victim = prv_spawn(0, "victim", 2, prv_victim, nullptr);
  prv_spawn(1, "ctl", 3, prv_controller, victim);
  pbl_test_kernel_run();
}

static void prv_short_lived(void *arg) {
  prv_trace('s');
  // returning from the entry ends the thread
}

static void prv_watcher(void *arg) {
  pbl_thread_sleep(PBL_TICKS(1));
  cl_assert_equal_i(pbl_thread_state(&s_threads[0]), PBL_THREAD_DEAD);
  prv_trace('w');
  pbl_test_kernel_stop();
}

void test_kernel__thread_exit_on_return(void) {
  prv_spawn(0, "short", 3, prv_short_lived, nullptr);
  prv_spawn(1, "watch", 2, prv_watcher, nullptr);
  pbl_test_kernel_run();
  cl_assert_equal_s(s_trace, "sw");
}

static void prv_stats_entry(void *arg) {
  pbl_thread_sleep(PBL_TICKS(3));
  struct pbl_thread_stats stats[4];
  uint32_t total;
  size_t n = pbl_thread_stats_snapshot(stats, 4, &total);
  cl_assert_equal_i(n, 2);
  cl_assert_equal_i(total, 3);
  struct pbl_thread_stack_info info;
  pbl_thread_stack_info(pbl_thread_current(), &info);
  // On the POSIX arch the thread runs on a pthread stack, so the kernel
  // stack is untouched and the high water is the whole buffer.
  cl_assert(info.high_water > 0 && info.high_water <= STACK);
  cl_assert_equal_i(info.start, (uintptr_t)s_stacks[0]);
  pbl_test_kernel_stop();
}

void test_kernel__stats_and_stack_info(void) {
  prv_spawn(0, "stats", 2, prv_stats_entry, nullptr);
  pbl_test_kernel_run();
}

static void prv_prio_raiser(void *arg) {
  struct pbl_thread *other = arg;
  prv_trace('a');
  pbl_thread_prio_set(other, 4); // other now outranks us and runs at once
  prv_trace('b');
  pbl_test_kernel_stop();
}

static void prv_raised(void *arg) {
  prv_trace('R');
  cl_assert_equal_i(pbl_thread_prio_get(pbl_thread_current()), 4);
}

void test_kernel__prio_set_preempts(void) {
  struct pbl_thread *other = prv_spawn(0, "other", 1, prv_raised, nullptr);
  prv_spawn(1, "raiser", 3, prv_prio_raiser, other);
  pbl_test_kernel_run();
  cl_assert_equal_s(s_trace, "aRb");
}

static void prv_sched_locked(void *arg) {
  prv_trace('a');
  pbl_sched_lock();
  pbl_sem_give(&s_sem); // would wake the higher-priority waiter
  prv_trace('b');
  cl_assert(!pbl_kernel_is_running() && pbl_sched_is_locked());
  pbl_sched_unlock(); // the switch happens here
  prv_trace('c');
  pbl_test_kernel_stop();
}

static void prv_sched_waiter(void *arg) {
  pbl_sem_take(&s_sem, PBL_FOREVER);
  prv_trace('W');
}

void test_kernel__sched_lock_defers_switch(void) {
  pbl_sem_init(&s_sem, s_sem.initial, s_sem.limit);
  prv_spawn(0, "waiter", 3, prv_sched_waiter, nullptr);
  prv_spawn(1, "locker", 2, prv_sched_locked, nullptr);
  pbl_test_kernel_run();
  cl_assert_equal_s(s_trace, "abWc");
}

// ---- timers past 32 bits ----------------------------------------------------

static void prv_timer_long(void *arg) {
  // Would read as already expired with a signed 32-bit comparison.
  const pbl_tick_t delay = 0x80000010U;
  pbl_timer_start(&s_timer, PBL_TICKS(delay));
  cl_assert_equal_i(pbl_timer_remaining(&s_timer), delay);
  cl_assert_equal_i(pbl_sem_take(&s_sem, PBL_TICKS(10)), -EAGAIN);
  cl_assert(pbl_timer_is_active(&s_timer));
  cl_assert_equal_i(pbl_timer_remaining(&s_timer), delay - 10);
  pbl_timer_stop(&s_timer);
  cl_assert_equal_i(pbl_timer_remaining(&s_timer), 0);
  pbl_test_kernel_stop();
}

void test_kernel__timer_longer_than_half_the_tick_range(void) {
  pbl_sem_init(&s_sem, 0, 1);
  pbl_timer_init(&s_timer, prv_timer_fn);
  prv_spawn(0, "waiter", 2, prv_timer_long, nullptr);
  pbl_test_kernel_run();
}

// ---- work queues ------------------------------------------------------------

static struct pbl_workq s_wq;
static struct pbl_work s_works[3];
static struct pbl_delayed_work s_dwork;
static uint64_t s_dwork_ran_at;
static int s_dwork_runs;
static PBL_SEM_DEFINE(s_work_gate, 0, 1);

static void prv_work_trace(struct pbl_work *work) {
  prv_trace('0' + (int)(work - s_works));
}

static void prv_work_blocking(struct pbl_work *work) {
  prv_trace('r');
  pbl_sem_take(&s_work_gate, PBL_FOREVER);
  prv_trace('R');
}

static void prv_dwork_fn(struct pbl_work *work) {
  s_dwork_ran_at = pbl_uptime_ticks();
  s_dwork_runs++;
}

static void prv_spawn_q(int prio) {
  pbl_workq_init(&s_wq);
  prv_spawn(7, "workq", prio, pbl_workq_thread, &s_wq);
}

static void prv_fifo_and_coalesce(void *arg) {
  cl_assert(pbl_work_submit(&s_wq, &s_works[1]));
  cl_assert(pbl_work_submit(&s_wq, &s_works[0]));
  cl_assert(!pbl_work_submit(&s_wq, &s_works[1]));
  cl_assert(pbl_work_is_pending(&s_works[1]));
  pbl_workq_flush(&s_wq);
  cl_assert(!pbl_work_is_pending(&s_works[1]));
  pbl_test_kernel_stop();
}

void test_kernel__work_runs_in_order_and_coalesces(void) {
  for (int i = 0; i < 3; i++) {
    pbl_work_init(&s_works[i], prv_work_trace);
  }
  prv_spawn_q(1);
  prv_spawn(0, "main", 2, prv_fifo_and_coalesce, nullptr);
  pbl_test_kernel_run();
  cl_assert_equal_s(s_trace, "10");
}

static void prv_isr_drain(void *arg) {
  // The queue must not need one wake-up per item, however many pile up in one ISR.
  pbl_test_isr_enter();
  for (int i = 0; i < 3; i++) {
    pbl_work_submit(&s_wq, &s_works[i]);
  }
  pbl_test_isr_exit();
  cl_assert_equal_s(s_trace, "012");
  pbl_test_kernel_stop();
}

void test_kernel__work_from_isr_drains_on_one_wake(void) {
  for (int i = 0; i < 3; i++) {
    pbl_work_init(&s_works[i], prv_work_trace);
  }
  prv_spawn_q(3);
  prv_spawn(0, "main", 2, prv_isr_drain, nullptr);
  pbl_test_kernel_run();
}

static void prv_work_requeue(struct pbl_work *work) {
  prv_trace('x');
  if (strlen(s_trace) < 3) {
    cl_assert(pbl_work_submit(&s_wq, work));
  }
}

static void prv_requeue_main(void *arg) {
  pbl_work_submit(&s_wq, &s_works[0]);
  while (strlen(s_trace) < 3) {
    pbl_thread_sleep(PBL_TICKS(1));
  }
  pbl_test_kernel_stop();
}

void test_kernel__work_resubmits_itself(void) {
  pbl_work_init(&s_works[0], prv_work_requeue);
  prv_spawn_q(3);
  prv_spawn(0, "main", 2, prv_requeue_main, nullptr);
  pbl_test_kernel_run();
  cl_assert_equal_s(s_trace, "xxx");
}

static void prv_cancel_main(void *arg) {
  pbl_work_submit(&s_wq, &s_works[0]);
  pbl_work_submit(&s_wq, &s_works[1]);
  pbl_work_submit(&s_wq, &s_works[2]);
  cl_assert(pbl_work_cancel(&s_works[1]));
  cl_assert(!pbl_work_cancel(&s_works[1]));
  cl_assert(pbl_work_cancel(&s_works[2]));
  // The tail moved: this must still link up.
  pbl_work_submit(&s_wq, &s_works[2]);
  pbl_workq_flush(&s_wq);
  pbl_test_kernel_stop();
}

void test_kernel__work_cancel_removes_from_queue(void) {
  for (int i = 0; i < 3; i++) {
    pbl_work_init(&s_works[i], prv_work_trace);
  }
  prv_spawn_q(1);
  prv_spawn(0, "main", 2, prv_cancel_main, nullptr);
  pbl_test_kernel_run();
  cl_assert_equal_s(s_trace, "02");
}

static void prv_cancel_sync_gate(void *arg) {
  pbl_thread_sleep(PBL_TICKS(5));
  prv_trace('g');
  pbl_sem_give(&s_work_gate);
}

static void prv_cancel_sync_main(void *arg) {
  pbl_work_submit(&s_wq, &s_works[0]);
  pbl_thread_sleep(PBL_TICKS(1));
  cl_assert(pbl_work_is_running(&s_works[0]));
  cl_assert(!pbl_work_cancel(&s_works[0]));
  cl_assert(!pbl_work_cancel_sync(&s_works[0]));
  cl_assert(!pbl_work_is_running(&s_works[0]));
  prv_trace('c');
  pbl_test_kernel_stop();
}

void test_kernel__work_cancel_sync_waits_for_running(void) {
  pbl_sem_init(&s_work_gate, 0, 1);
  pbl_work_init(&s_works[0], prv_work_blocking);
  prv_spawn_q(1);
  prv_spawn(0, "main", 2, prv_cancel_sync_main, nullptr);
  prv_spawn(1, "gate", 3, prv_cancel_sync_gate, nullptr);
  pbl_test_kernel_run();
  cl_assert_equal_s(s_trace, "rgRc");
}

static void prv_flush_main(void *arg) {
  cl_assert(!pbl_work_flush(&s_works[0]));
  pbl_work_submit(&s_wq, &s_works[0]);
  pbl_work_submit(&s_wq, &s_works[1]);
  cl_assert(pbl_work_flush(&s_works[0]));
  // Back at a higher priority right after 0, before 1 got to run.
  prv_trace('f');
  pbl_workq_flush(&s_wq);
  pbl_test_kernel_stop();
}

void test_kernel__work_flush_waits_for_the_item_only(void) {
  for (int i = 0; i < 3; i++) {
    pbl_work_init(&s_works[i], prv_work_trace);
  }
  prv_spawn_q(1);
  prv_spawn(0, "main", 2, prv_flush_main, nullptr);
  pbl_test_kernel_run();
  cl_assert_equal_s(s_trace, "0f1");
}

static void prv_dwork_main(void *arg) {
  cl_assert(pbl_delayed_work_schedule(&s_wq, &s_dwork, PBL_TICKS(5)));
  cl_assert(pbl_delayed_work_is_pending(&s_dwork));
  cl_assert_equal_i(pbl_delayed_work_remaining(&s_dwork), 5);
  // Schedule keeps the running delay, reschedule replaces it.
  cl_assert(!pbl_delayed_work_schedule(&s_wq, &s_dwork, PBL_TICKS(1)));
  pbl_thread_sleep(PBL_TICKS(3));
  cl_assert(pbl_delayed_work_reschedule(&s_wq, &s_dwork, PBL_TICKS(4)));
  pbl_thread_sleep(PBL_TICKS(10));
  cl_assert_equal_i(s_dwork_runs, 1);
  cl_assert_equal_i(s_dwork_ran_at, 7);
  cl_assert(!pbl_delayed_work_is_pending(&s_dwork));

  pbl_delayed_work_schedule(&s_wq, &s_dwork, PBL_TICKS(2));
  cl_assert(pbl_delayed_work_cancel(&s_dwork));
  pbl_thread_sleep(PBL_TICKS(5));
  cl_assert_equal_i(s_dwork_runs, 1);

  cl_assert(!pbl_delayed_work_reschedule(&s_wq, &s_dwork, PBL_NO_WAIT));
  pbl_thread_sleep(PBL_TICKS(1));
  cl_assert_equal_i(s_dwork_runs, 2);
  pbl_test_kernel_stop();
}

void test_kernel__delayed_work(void) {
  s_dwork_runs = 0;
  pbl_delayed_work_init(&s_dwork, prv_dwork_fn);
  prv_spawn_q(3);
  prv_spawn(0, "main", 2, prv_dwork_main, nullptr);
  pbl_test_kernel_run();
}

static void prv_dwork_self_rearm(struct pbl_work *work) {
  s_dwork_runs++;
  pbl_sem_take(&s_work_gate, PBL_FOREVER);
  pbl_delayed_work_schedule(&s_wq, &s_dwork, PBL_TICKS(1));
}

static void prv_dwork_sync_gate(void *arg) {
  pbl_thread_sleep(PBL_TICKS(5));
  pbl_sem_give(&s_work_gate);
}

static void prv_dwork_sync_main(void *arg) {
  pbl_delayed_work_schedule(&s_wq, &s_dwork, PBL_NO_WAIT);
  pbl_thread_sleep(PBL_TICKS(1));
  cl_assert_equal_i(s_dwork_runs, 1);
  // Returns only once the function has run and its re-arm is undone.
  pbl_delayed_work_cancel_sync(&s_dwork);
  cl_assert(!pbl_delayed_work_is_pending(&s_dwork));
  pbl_thread_sleep(PBL_TICKS(5));
  cl_assert_equal_i(s_dwork_runs, 1);
  pbl_test_kernel_stop();
}

void test_kernel__delayed_work_cancel_sync_beats_self_rearm(void) {
  s_dwork_runs = 0;
  pbl_sem_init(&s_work_gate, 0, 1);
  pbl_delayed_work_init(&s_dwork, prv_dwork_self_rearm);
  prv_spawn_q(1);
  prv_spawn(0, "main", 2, prv_dwork_sync_main, nullptr);
  prv_spawn(1, "gate", 3, prv_dwork_sync_gate, nullptr);
  pbl_test_kernel_run();
}

static void prv_dwork_stale_main(void *arg) {
  // An ISR preempting the tick can land between a timer's expiry and its callback.
  pbl_delayed_work_schedule(&s_wq, &s_dwork, PBL_TICKS(2));
  pbl_delayed_work_cancel(&s_dwork);
  pbl_delayed_work_expired(&s_dwork.timer);
  cl_assert(!pbl_delayed_work_is_pending(&s_dwork));
  pbl_thread_sleep(PBL_TICKS(1));
  cl_assert_equal_i(s_dwork_runs, 0);

  pbl_delayed_work_schedule(&s_wq, &s_dwork, PBL_TICKS(2));
  pbl_delayed_work_expired(&s_dwork.timer);
  cl_assert_equal_i(pbl_delayed_work_remaining(&s_dwork), 2);
  pbl_thread_sleep(PBL_TICKS(1));
  cl_assert_equal_i(s_dwork_runs, 0);
  pbl_thread_sleep(PBL_TICKS(2));
  cl_assert_equal_i(s_dwork_runs, 1);
  pbl_test_kernel_stop();
}

void test_kernel__delayed_work_ignores_a_stale_expiry(void) {
  s_dwork_runs = 0;
  pbl_delayed_work_init(&s_dwork, prv_dwork_fn);
  prv_spawn_q(3);
  prv_spawn(0, "main", 2, prv_dwork_stale_main, nullptr);
  pbl_test_kernel_run();
}

static PBL_WORK_POOL_DEFINE(s_pool, 2);

static void prv_call_record(void *data) {
  prv_trace((char)(uintptr_t)data);
}

static void prv_call_requeue(void *data) {
  prv_trace('a');
  // The caller's slot is already free, and the queue's own thread must not wait.
  cl_assert_equal_i(pbl_work_call(&s_wq, &s_pool, prv_call_record, (void *)'b', PBL_FOREVER), 0);
  cl_assert_equal_i(pbl_work_call(&s_wq, &s_pool, prv_call_record, (void *)'c', PBL_FOREVER), 0);
  cl_assert_equal_i(pbl_work_call(&s_wq, &s_pool, prv_call_record, (void *)'z', PBL_FOREVER),
                    -EBUSY);
}

static void prv_call_main(void *arg) {
  // Payloads stay distinct, unlike with a single work item.
  cl_assert_equal_i(pbl_work_call(&s_wq, &s_pool, prv_call_record, (void *)'1', PBL_NO_WAIT), 0);
  cl_assert_equal_i(pbl_work_call(&s_wq, &s_pool, prv_call_record, (void *)'2', PBL_NO_WAIT), 0);
  cl_assert_equal_i(pbl_work_pool_num_free(&s_pool), 0);
  cl_assert_equal_i(pbl_work_call(&s_wq, &s_pool, prv_call_record, (void *)'z', PBL_NO_WAIT),
                    -EBUSY);
  // The lower priority queue runs while this waits, which frees a slot.
  cl_assert_equal_i(pbl_work_call(&s_wq, &s_pool, prv_call_record, (void *)'3', PBL_FOREVER), 0);
  pbl_workq_flush(&s_wq);
  cl_assert_equal_i(pbl_work_pool_num_free(&s_pool), 2);
  cl_assert_equal_i(pbl_work_call(&s_wq, &s_pool, prv_call_requeue, nullptr, PBL_FOREVER), 0);
  pbl_workq_flush(&s_wq);
  pbl_workq_flush(&s_wq);
  cl_assert_equal_i(pbl_work_pool_num_free(&s_pool), 2);
  pbl_test_kernel_stop();
}

void test_kernel__work_call_pool(void) {
  static struct pbl_work_call slots[2];
  pbl_work_pool_init(&s_pool, slots, 2);
  prv_spawn_q(1);
  prv_spawn(0, "main", 2, prv_call_main, nullptr);
  pbl_test_kernel_run();
  cl_assert_equal_s(s_trace, "123abc");
}

static void prv_call_timeout_main(void *arg) {
  // No thread serves the queue, so nothing frees the slot.
  cl_assert_equal_i(pbl_work_call(&s_wq, &s_pool, prv_call_record, (void *)'1', PBL_NO_WAIT), 0);
  cl_assert_equal_i(pbl_work_call(&s_wq, &s_pool, prv_call_record, (void *)'2', PBL_NO_WAIT), 0);
  cl_assert_equal_i(pbl_work_call(&s_wq, &s_pool, prv_call_record, (void *)'z', PBL_TICKS(2)),
                    -EAGAIN);
  cl_assert_equal_i(pbl_uptime_ticks(), 2);
  pbl_test_kernel_stop();
}

void test_kernel__work_call_times_out(void) {
  static struct pbl_work_call slots[2];
  pbl_work_pool_init(&s_pool, slots, 2);
  pbl_workq_init(&s_wq);
  prv_spawn(0, "main", 2, prv_call_timeout_main, nullptr);
  pbl_test_kernel_run();
}

static void prv_ops_wait(struct pbl_workq *q, bool waiting) {
  prv_trace(waiting ? 'w' : 'W');
}

static const struct pbl_workq_ops s_ops = {
  .wait = prv_ops_wait,
};

static void prv_is_current(struct pbl_work *work) {
  cl_assert(pbl_workq_is_current(&s_wq));
  prv_trace('c');
}

static void prv_ops_late_main(void *arg) {
  // The queue is idle and blocked by now, past where it would call a hook.
  cl_assert_equal_s(s_trace, "");
  cl_assert(pbl_workq_get_thread(&s_wq) != nullptr);
  cl_assert(!pbl_workq_is_current(&s_wq));
  pbl_workq_set_ops(&s_wq, &s_ops);
  pbl_thread_sleep(PBL_TICKS(1));
  cl_assert_equal_s(s_trace, "Ww");
  pbl_work_submit(&s_wq, &s_works[0]);
  pbl_workq_flush(&s_wq);
  pbl_test_kernel_stop();
}

void test_kernel__workq_ops_can_be_set_while_idle(void) {
  pbl_work_init(&s_works[0], prv_is_current);
  prv_spawn_q(3);
  prv_spawn(0, "main", 2, prv_ops_late_main, nullptr);
  pbl_test_kernel_run();
  cl_assert(strstr(s_trace, "WwWcw") == s_trace);
}

void test_kernel__ms_to_ticks_saturates(void) {
  cl_assert_equal_i(pbl_ms_to_ticks(1000), PBL_TICK_HZ);
  cl_assert_equal_i(pbl_ms_to_ticks(UINT32_MAX), PBL_TICK_FOREVER - 1U);
}

// ---- periodic delayed work --------------------------------------------------

static uint64_t s_periodic_at[4];
static int s_periodic_runs;
static pbl_tick_t s_periodic_busy;

static void prv_periodic_fn(struct pbl_work *work) {
  s_periodic_at[s_periodic_runs++] = pbl_uptime_ticks();
  if (s_periodic_busy > 0U) {
    pbl_thread_sleep(PBL_TICKS(s_periodic_busy));
  }
  if (s_periodic_runs < (int)ARRAY_LENGTH(s_periodic_at)) {
    pbl_delayed_work_schedule_next(&s_dwork, 10);
  }
}

static void prv_periodic_main(void *arg) {
  const uint64_t start = pbl_uptime_ticks();

  pbl_delayed_work_reschedule(&s_wq, &s_dwork, PBL_TICKS(10));
  pbl_thread_sleep(PBL_TICKS(45));
  cl_assert_equal_i(s_periodic_runs, 4);
  for (int i = 0; i < 4; i++) {
    cl_assert_equal_i(s_periodic_at[i], start + (uint64_t)(i + 1) * 10U);
  }
  pbl_test_kernel_stop();
}

void test_kernel__delayed_work_schedule_next_does_not_drift(void) {
  s_periodic_runs = 0;
  s_periodic_busy = 3;
  pbl_delayed_work_init(&s_dwork, prv_periodic_fn);
  prv_spawn_q(3);
  prv_spawn(0, "main", 2, prv_periodic_main, nullptr);
  pbl_test_kernel_run();
}

static void prv_periodic_overrun_main(void *arg) {
  pbl_delayed_work_reschedule(&s_wq, &s_dwork, PBL_TICKS(10));
  pbl_thread_sleep(PBL_TICKS(120));
  // No burst for the missed periods: each run is a tick after the one before returned.
  cl_assert_equal_i(s_periodic_runs, 4);
  for (int i = 1; i < 4; i++) {
    cl_assert_equal_i(s_periodic_at[i] - s_periodic_at[i - 1], 26);
  }
  pbl_test_kernel_stop();
}

void test_kernel__delayed_work_schedule_next_skips_missed_periods(void) {
  s_periodic_runs = 0;
  s_periodic_busy = 25;
  pbl_delayed_work_init(&s_dwork, prv_periodic_fn);
  prv_spawn_q(3);
  prv_spawn(0, "main", 2, prv_periodic_overrun_main, nullptr);
  pbl_test_kernel_run();
}

// ---- boosted work -----------------------------------------------------------

static pbl_prio_t s_prio_seen[3];

static void prv_record_prio(struct pbl_work *work) {
  s_prio_seen[work - s_works] = pbl_thread_prio_get(pbl_workq_get_thread(&s_wq));
}

static void prv_boost_main(void *arg) {
  pbl_workq_set_boost_prio(&s_wq, 4);
  pbl_work_submit(&s_wq, &s_works[0]);
  pbl_workq_flush(&s_wq);
  // Raised from submission, which runs it ahead of this thread, and lowered once it returns.
  cl_assert(pbl_work_submit_boosted(&s_wq, &s_works[1]));
  pbl_workq_flush(&s_wq);
  cl_assert_equal_i(pbl_thread_prio_get(pbl_workq_get_thread(&s_wq)), 1);
  // Canceling a boosted item lowers it too.
  pbl_test_isr_enter();
  pbl_work_submit_boosted(&s_wq, &s_works[2]);
  cl_assert(pbl_work_cancel(&s_works[2]));
  pbl_test_isr_exit();
  cl_assert_equal_i(pbl_thread_prio_get(pbl_workq_get_thread(&s_wq)), 1);
  cl_assert_equal_i(s_prio_seen[0], 1);
  cl_assert_equal_i(s_prio_seen[1], 4);
  pbl_test_kernel_stop();
}

void test_kernel__boosted_work_raises_the_queue_until_it_ran(void) {
  for (int i = 0; i < 3; i++) {
    pbl_work_init(&s_works[i], prv_record_prio);
  }
  prv_spawn_q(1);
  prv_spawn(0, "main", 2, prv_boost_main, nullptr);
  pbl_test_kernel_run();
}

static void prv_boost_early_main(void *arg) {
  // The queue's thread, of a lower priority, has yet to run.
  pbl_workq_set_boost_prio(&s_wq, 4);
  cl_assert(pbl_work_submit_boosted(&s_wq, &s_works[0]));
  pbl_workq_flush(&s_wq);
  cl_assert_equal_i(s_prio_seen[0], 4);
  cl_assert_equal_i(pbl_thread_prio_get(pbl_workq_get_thread(&s_wq)), 1);
  pbl_test_kernel_stop();
}

void test_kernel__boosted_work_submitted_before_the_queue_thread_runs(void) {
  pbl_work_init(&s_works[0], prv_record_prio);
  prv_spawn_q(1);
  prv_spawn(0, "main", 2, prv_boost_early_main, nullptr);
  pbl_test_kernel_run();
}

// ---- wall clock work --------------------------------------------------------

#define T0        1700000000U // A whole minute: 22:13:20 UTC is not, 22:14:00 below is.
#define MINUTE_T0 (T0 + 40U)

static struct pbl_wallclock_work s_wcwork;
static uint64_t s_wc_ran_ms[8];
static int s_wc_runs;

static void prv_wc_fn(struct pbl_work *work) {
  cl_assert(work == &s_wcwork.work);
  s_wc_ran_ms[s_wc_runs++] = pbl_realtime_ms();
}

static void prv_wc_minutes(void *arg) {
  // 30 s before a minute.
  pbl_realtime_set(MINUTE_T0 - 30U);
  pbl_wallclock_work_start(&s_wq, &s_wcwork, 60, 1);
  cl_assert(pbl_wallclock_work_is_running(&s_wcwork));
  pbl_thread_sleep(PBL_MSEC(29 * 1000));
  cl_assert_equal_i(s_wc_runs, 0);
  pbl_thread_sleep(PBL_MSEC(2 * 1000));
  cl_assert_equal_i(s_wc_runs, 1);
  // On the boundary, and not before it.
  cl_assert(s_wc_ran_ms[0] >= (uint64_t)MINUTE_T0 * 1000U);
  cl_assert(s_wc_ran_ms[0] < (uint64_t)MINUTE_T0 * 1000U + 20U);
  pbl_thread_sleep(PBL_MSEC(60 * 1000));
  cl_assert_equal_i(s_wc_runs, 2);
  cl_assert(s_wc_ran_ms[1] >= (uint64_t)(MINUTE_T0 + 60U) * 1000U);
  cl_assert(pbl_wallclock_work_stop(&s_wcwork));
  pbl_thread_sleep(PBL_MSEC(120 * 1000));
  cl_assert_equal_i(s_wc_runs, 2);
  pbl_test_kernel_stop();
}

void test_kernel__wallclock_work_runs_on_minute_boundaries(void) {
  s_wc_runs = 0;
  pbl_wallclock_work_init(&s_wcwork, prv_wc_fn);
  prv_spawn_q(3);
  prv_spawn(0, "main", 2, prv_wc_minutes, nullptr);
  pbl_test_kernel_run();
}

static void prv_wc_clock_set(void *arg) {
  pbl_realtime_set(MINUTE_T0 + 10U);
  pbl_wallclock_work_start(&s_wq, &s_wcwork, 60, 1);
  // Within the same minute: nothing to do yet.
  pbl_realtime_set(MINUTE_T0 + 20U);
  pbl_thread_sleep(PBL_TICKS(2));
  cl_assert_equal_i(s_wc_runs, 0);
  // Into another minute: as if the minute had changed.
  pbl_realtime_set(MINUTE_T0 + 3600U + 5U);
  pbl_thread_sleep(PBL_TICKS(2));
  cl_assert_equal_i(s_wc_runs, 1);
  // Back in time: the next run is at the next minute of the new time, not an hour later.
  pbl_realtime_set(MINUTE_T0 + 50U);
  pbl_thread_sleep(PBL_TICKS(2));
  cl_assert_equal_i(s_wc_runs, 2);
  pbl_thread_sleep(PBL_MSEC(11 * 1000));
  cl_assert_equal_i(s_wc_runs, 3);
  cl_assert(s_wc_ran_ms[2] >= (uint64_t)(MINUTE_T0 + 60U) * 1000U);
  pbl_wallclock_work_stop(&s_wcwork);
  pbl_test_kernel_stop();
}

void test_kernel__wallclock_work_follows_the_clock(void) {
  s_wc_runs = 0;
  pbl_wallclock_work_init(&s_wcwork, prv_wc_fn);
  prv_spawn_q(3);
  prv_spawn(0, "main", 2, prv_wc_clock_set, nullptr);
  pbl_test_kernel_run();
}

static struct pbl_timer s_wc_set_timer;

static void prv_wc_set_fn(struct pbl_timer *timer) {
  pbl_realtime_set(MINUTE_T0 + 30U);
}

static void prv_wc_clock_set_when_due(void *arg) {
  pbl_realtime_set(MINUTE_T0);
  // Expires first on the tick the item is due at, so the clock is set before the item's timer runs.
  pbl_timer_start(&s_wc_set_timer, PBL_TICKS(PBL_TICK_HZ));
  pbl_wallclock_work_start(&s_wq, &s_wcwork, 1, 1);
  pbl_thread_sleep(PBL_TICKS(PBL_TICK_HZ + 2));
  cl_assert_equal_i(s_wc_runs, 1);
  // And it keeps running, from the new time.
  pbl_thread_sleep(PBL_MSEC(1000));
  cl_assert_equal_i(s_wc_runs, 2);
  cl_assert_equal_i(s_wc_ran_ms[1] / 1000U, MINUTE_T0 + 31U);
  pbl_wallclock_work_stop(&s_wcwork);
  pbl_test_kernel_stop();
}

void test_kernel__wallclock_work_runs_when_the_clock_is_set_as_it_is_due(void) {
  s_wc_runs = 0;
  pbl_wallclock_work_init(&s_wcwork, prv_wc_fn);
  pbl_timer_init(&s_wc_set_timer, prv_wc_set_fn);
  prv_spawn_q(3);
  prv_spawn(0, "main", 2, prv_wc_clock_set_when_due, nullptr);
  pbl_test_kernel_run();
}

static void prv_wc_every(void *arg) {
  pbl_realtime_set(MINUTE_T0);
  pbl_thread_sleep(PBL_MSEC(300));
  // Every 5 s: the 5th second boundary from now, then every 5.
  pbl_wallclock_work_start(&s_wq, &s_wcwork, 1, 5);
  pbl_thread_sleep(PBL_MSEC(16 * 1000));
  cl_assert_equal_i(s_wc_runs, 3);
  for (int i = 0; i < 3; i++) {
    cl_assert_equal_i(s_wc_ran_ms[i] / 1000U, MINUTE_T0 + 5U * (i + 1));
  }
  pbl_wallclock_work_stop(&s_wcwork);
  pbl_test_kernel_stop();
}

void test_kernel__wallclock_work_counts_boundaries(void) {
  s_wc_runs = 0;
  pbl_wallclock_work_init(&s_wcwork, prv_wc_fn);
  prv_spawn_q(3);
  prv_spawn(0, "main", 2, prv_wc_every, nullptr);
  pbl_test_kernel_run();
}
