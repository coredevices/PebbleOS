/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// The Zephyr kernel API of the zephyr_bt port, on the real PebbleOS kernel
// (POSIX arch, virtual time).

#include "zbt_port.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/kernel.h>

#include <clar.h>
#include <kernel_test.h>
#include <zbt/zbt.h>
#include <zbt_link.h>
#include <zephyr/kernel.h>

static struct zbt_host_threads s_host_threads;
#include <zephyr/sys/atomic.h>

// The port's diagnostics: a wait with the interrupt lock held, a stale waiter.
void zbt_ctl_diag(uint16_t kind, uint32_t a, uint32_t b, uint32_t c) {
  cl_fail("zbt diagnostic");
}

[[noreturn]] void pbl_kernel_assert_failed(const char *filename, int line) {
  fprintf(stderr, "kernel assert at %s:%d\n", filename, line);
  abort();
}

void *kernel_malloc(size_t bytes) {
  return malloc(bytes);
}

void *kernel_zalloc(size_t bytes) {
  return calloc(1, bytes);
}

void kernel_free(void *ptr) {
  free(ptr);
}

uint32_t pbl_rand32(void) {
  return 4;
}

#define STACK 4096
static uint8_t s_stacks[4][STACK] PBL_ALIGNED(8);
static struct pbl_thread s_threads[4];

static char s_trace[128];

static void prv_trace(char c) {
  size_t n = strlen(s_trace);
  if (n + 1 < sizeof(s_trace)) {
    s_trace[n] = c;
    s_trace[n + 1] = '\0';
  }
}

static void prv_spawn(int i, pbl_prio_t prio, void (*entry)(void *), void *arg) {
  struct pbl_thread_attr attr = {
    .name = "t",
    .entry = entry,
    .arg = arg,
    .prio = prio,
    .privileged = true,
    .stack = s_stacks[i],
    .stack_size = STACK,
  };
  cl_assert_equal_i(pbl_thread_create(&s_threads[i], &attr), 0);
}

void test_zbt_kernel__initialize(void) {
  s_trace[0] = '\0';
  memset(s_threads, 0, sizeof(s_threads));
}

// ---- atomics -------------------------------------------------------------------

void test_zbt_kernel__atomics(void) {
  atomic_t a = ATOMIC_INIT(5);
  cl_assert_equal_i(atomic_add(&a, 3), 5);
  cl_assert_equal_i(atomic_sub(&a, 1), 8);
  cl_assert_equal_i(atomic_inc(&a), 7);
  cl_assert_equal_i(atomic_dec(&a), 8);
  cl_assert(atomic_cas(&a, 7, 1));
  cl_assert(!atomic_cas(&a, 7, 2));
  cl_assert_equal_i(atomic_or(&a, 6), 1);
  cl_assert_equal_i(atomic_and(&a, 3), 7);
  cl_assert_equal_i(atomic_xor(&a, 1), 3);
  cl_assert_equal_i(atomic_nand(&a, 2), 2);
  cl_assert_equal_i(atomic_get(&a), ~2);
  cl_assert_equal_i(atomic_clear(&a), ~2);

  ATOMIC_DEFINE(bits, 40);
  atomic_clear(&bits[0]);
  atomic_clear(&bits[1]);
  cl_assert(!atomic_test_and_set_bit(bits, 35));
  cl_assert(atomic_test_and_set_bit(bits, 35));
  cl_assert(atomic_test_bit(bits, 35));
  cl_assert(atomic_test_and_clear_bit(bits, 35));
  cl_assert(!atomic_test_bit(bits, 35));

  int x;
  atomic_ptr_t p = ATOMIC_PTR_INIT(NULL);
  cl_assert(atomic_ptr_cas(&p, NULL, &x));
  cl_assert(atomic_ptr_get(&p) == &x);
  cl_assert(atomic_ptr_set(&p, NULL) == &x);
  cl_assert(atomic_ptr_clear(&p) == NULL);
}

// ---- semaphores ---------------------------------------------------------------

static K_SEM_DEFINE(s_sem, 0, 1);

static void prv_sem_taker(void *arg) {
  int64_t start = k_uptime_get();
  cl_assert_equal_i(k_sem_take(&s_sem, K_NO_WAIT), -EBUSY);
  cl_assert_equal_i(k_sem_take(&s_sem, K_MSEC(20)), -EAGAIN);
  int64_t waited = k_uptime_get() - start;
  cl_assert(waited >= 19 && waited <= 22);
  prv_trace('t');
  cl_assert_equal_i(k_sem_take(&s_sem, K_FOREVER), 0);
  prv_trace('T');
  pbl_test_kernel_stop();
}

static void prv_sem_giver(void *arg) {
  k_sleep(K_MSEC(30));
  prv_trace('g');
  k_sem_give(&s_sem);
  prv_trace('G');
}

void test_zbt_kernel__semaphore_timeout_and_handoff(void) {
  k_sem_init(&s_sem, 0, 1);
  prv_spawn(0, 3, prv_sem_taker, NULL);
  prv_spawn(1, 2, prv_sem_giver, NULL);
  pbl_test_kernel_run();
  // The higher-priority taker runs as soon as the giver gives.
  cl_assert_equal_s(s_trace, "tgT");
}

void test_zbt_kernel__semaphore_counts_to_limit(void) {
  k_sem_init(&s_sem, 0, 2);
  for (int i = 0; i < 5; i++) {
    k_sem_give(&s_sem);
  }
  cl_assert_equal_i(k_sem_count_get(&s_sem), 2);
  cl_assert_equal_i(k_sem_take(&s_sem, K_NO_WAIT), 0);
  cl_assert_equal_i(k_sem_take(&s_sem, K_NO_WAIT), 0);
  cl_assert_equal_i(k_sem_take(&s_sem, K_NO_WAIT), -EBUSY);
}

// ---- queues -------------------------------------------------------------------

struct item {
  void *reserved; // k_queue link
  char tag;
};

static K_FIFO_DEFINE(s_fifo);
static struct item s_items[4] = {{.tag = 'a'}, {.tag = 'b'}, {.tag = 'c'}, {.tag = 'd'}};

static void prv_fifo_getter(void *arg) {
  // Blocks first; the first item is handed over, the rest queue up.
  struct item *it = k_fifo_get(&s_fifo, K_FOREVER);
  prv_trace(it->tag);
  for (int i = 0; i < 2; i++) {
    it = k_fifo_get(&s_fifo, K_NO_WAIT);
    cl_assert(it != NULL);
    prv_trace(it->tag);
  }
  cl_assert(k_fifo_get(&s_fifo, K_MSEC(5)) == NULL);
  prv_trace('-');
  pbl_test_kernel_stop();
}

static void prv_fifo_putter(void *arg) {
  k_sleep(K_MSEC(1));
  prv_trace('p');
  k_fifo_put(&s_fifo, &s_items[0]);
  k_fifo_put(&s_fifo, &s_items[1]);
  k_fifo_put(&s_fifo, &s_items[2]);
}

void test_zbt_kernel__fifo_blocks_and_keeps_order(void) {
  k_fifo_init(&s_fifo);
  prv_spawn(0, 2, prv_fifo_getter, NULL);
  prv_spawn(1, 3, prv_fifo_putter, NULL);
  pbl_test_kernel_run();
  cl_assert_equal_s(s_trace, "pabc-");
}

void test_zbt_kernel__lifo_peek_and_remove(void) {
  struct k_lifo lifo;
  k_lifo_init(&lifo);
  k_lifo_put(&lifo, &s_items[0]);
  k_lifo_put(&lifo, &s_items[1]);
  k_lifo_put(&lifo, &s_items[2]);
  cl_assert(k_queue_peek_head(&lifo._queue) == &s_items[2]);
  cl_assert(k_queue_remove(&lifo._queue, &s_items[1]));
  cl_assert(!k_queue_remove(&lifo._queue, &s_items[3]));
  cl_assert(k_lifo_get(&lifo, K_NO_WAIT) == &s_items[2]);
  cl_assert(k_lifo_get(&lifo, K_NO_WAIT) == &s_items[0]);
  cl_assert(k_lifo_get(&lifo, K_NO_WAIT) == NULL);
  cl_assert(k_queue_is_empty(&lifo._queue));
}

K_MEM_SLAB_DEFINE_STATIC_TYPE(s_slab, struct item, 2);

void test_zbt_kernel__mem_slab(void) {
  void *a;
  void *b;
  void *c;
  cl_assert_equal_i(k_mem_slab_alloc(&s_slab, &a, K_NO_WAIT), 0);
  cl_assert_equal_i(k_mem_slab_alloc(&s_slab, &b, K_NO_WAIT), 0);
  cl_assert(a != b);
  cl_assert_equal_i(k_mem_slab_alloc(&s_slab, &c, K_NO_WAIT), -ENOMEM);
  cl_assert_equal_i(k_mem_slab_num_used_get(&s_slab), 2);
  k_mem_slab_free(&s_slab, a);
  cl_assert_equal_i(k_mem_slab_alloc(&s_slab, &c, K_NO_WAIT), 0);
  cl_assert(c == a);
  k_mem_slab_free(&s_slab, b);
  k_mem_slab_free(&s_slab, c);
}

// ---- mutexes ------------------------------------------------------------------

static K_MUTEX_DEFINE(s_mutex);

static void prv_mutex_owner(void *arg) {
  cl_assert_equal_i(k_mutex_lock(&s_mutex, K_FOREVER), 0);
  cl_assert_equal_i(k_mutex_lock(&s_mutex, K_FOREVER), 0); // recursive
  prv_trace('L');
  k_sleep(K_MSEC(10));
  cl_assert_equal_i(k_mutex_unlock(&s_mutex), 0);
  prv_trace('u');
  k_sleep(K_MSEC(10));
  cl_assert_equal_i(k_mutex_unlock(&s_mutex), 0);
  prv_trace('U');
}

static void prv_mutex_waiter(void *arg) {
  k_sleep(K_MSEC(1));
  cl_assert_equal_i(k_mutex_lock(&s_mutex, K_NO_WAIT), -EBUSY);
  cl_assert_equal_i(k_mutex_lock(&s_mutex, K_MSEC(5)), -EAGAIN);
  prv_trace('w');
  cl_assert_equal_i(k_mutex_lock(&s_mutex, K_FOREVER), 0);
  prv_trace('W');
  cl_assert_equal_i(k_mutex_unlock(&s_mutex), 0);
  // Not the owner any more.
  cl_assert_equal_i(k_mutex_unlock(&s_mutex), -EPERM);
  pbl_test_kernel_stop();
}

void test_zbt_kernel__mutex_recursion_and_handover(void) {
  k_mutex_init(&s_mutex);
  prv_spawn(0, 2, prv_mutex_owner, NULL);
  prv_spawn(1, 3, prv_mutex_waiter, NULL);
  pbl_test_kernel_run();
  // The higher priority waiter runs as soon as the last unlock hands over.
  cl_assert_equal_s(s_trace, "LwuW");
}

// ---- work queues --------------------------------------------------------------
//
// The executor thread lives for the whole run, so all work queue checks share
// one kernel run.

static int s_runs;
static k_tid_t s_ran_on;
static int64_t s_ran_at;
static bool s_slow_done;

static void prv_count_handler(struct k_work *work) {
  s_runs++;
  s_ran_on = k_current_get();
  s_ran_at = k_uptime_get();
}

static void prv_slow_handler(struct k_work *work) {
  k_sleep(K_MSEC(5));
  s_slow_done = true;
}

static void prv_rx_item(void *item) {
  prv_trace(((struct item *)item)->tag);
}

static void prv_work_controller(void *arg) {
  struct k_work work;
  struct k_work_delayable dwork;
  struct k_work_sync sync;
  int64_t t0;

  zbt_executor_start(&s_host_threads, NULL);
  cl_assert(k_sys_work_q.thread_id != NULL);

  // Submit runs once on the executor; a second submit while queued is a no-op.
  k_work_init(&work, prv_count_handler);
  cl_assert_equal_i(k_work_submit(&work), 1);
  cl_assert_equal_i(k_work_submit(&work), 0);
  cl_assert(k_work_busy_get(&work) & K_WORK_QUEUED);
  k_sleep(K_MSEC(1));
  cl_assert_equal_i(s_runs, 1);
  cl_assert(s_ran_on == k_sys_work_q.thread_id);
  cl_assert(!zbt_executor_is_current());
  cl_assert_equal_i(k_work_busy_get(&work), 0);

  // Delayed work runs after its delay.
  s_runs = 0;
  k_work_init_delayable(&dwork, prv_count_handler);
  t0 = k_uptime_get();
  cl_assert_equal_i(k_work_schedule(&dwork, K_MSEC(10)), 1);
  cl_assert_equal_i(k_work_schedule(&dwork, K_MSEC(50)), 0); // already scheduled
  cl_assert(k_work_delayable_busy_get(&dwork) & K_WORK_DELAYED);
  cl_assert(k_work_delayable_remaining_get(&dwork) > 0);
  k_sleep(K_MSEC(5));
  cl_assert_equal_i(s_runs, 0);
  k_sleep(K_MSEC(10));
  cl_assert_equal_i(s_runs, 1);
  cl_assert(s_ran_at - t0 >= 9 && s_ran_at - t0 <= 12);

  // Cancelled before its deadline, it never runs.
  s_runs = 0;
  cl_assert_equal_i(k_work_schedule(&dwork, K_MSEC(10)), 1);
  k_sleep(K_MSEC(3));
  cl_assert_equal_i(k_work_cancel_delayable(&dwork), 0);
  k_sleep(K_MSEC(20));
  cl_assert_equal_i(s_runs, 0);

  // Rescheduling moves the deadline.
  t0 = k_uptime_get();
  cl_assert_equal_i(k_work_schedule(&dwork, K_MSEC(10)), 1);
  k_sleep(K_MSEC(5));
  cl_assert_equal_i(k_work_reschedule(&dwork, K_MSEC(10)), 1);
  k_sleep(K_MSEC(8));
  cl_assert_equal_i(s_runs, 0);
  k_sleep(K_MSEC(5));
  cl_assert_equal_i(s_runs, 1);
  cl_assert(s_ran_at - t0 >= 14 && s_ran_at - t0 <= 17);

  // A zero delay submits at once.
  s_runs = 0;
  cl_assert_equal_i(k_work_reschedule(&dwork, K_NO_WAIT), 1);
  k_sleep(K_MSEC(1));
  cl_assert_equal_i(s_runs, 1);

  // cancel_sync waits for a running handler to finish.
  k_work_init(&work, prv_slow_handler);
  s_slow_done = false;
  k_work_submit(&work);
  k_sleep(K_MSEC(1)); // the handler is now sleeping inside
  cl_assert(k_work_busy_get(&work) & K_WORK_RUNNING);
  cl_assert(k_work_cancel_sync(&work, &sync));
  cl_assert(s_slow_done);
  cl_assert_equal_i(k_work_busy_get(&work), 0);

  // A pending item cancelled with cancel_sync does not run.
  s_runs = 0;
  k_work_init(&work, prv_count_handler);
  k_work_submit(&work);
  cl_assert(k_work_cancel_sync(&work, &sync));
  k_sleep(K_MSEC(2));
  cl_assert_equal_i(s_runs, 0);

  // Every queue shares the executor thread.
  struct k_work_q q2;
  k_work_queue_init(&q2);
  k_work_queue_start(&q2, NULL, 0, 0, NULL);
  cl_assert(k_work_queue_thread_get(&q2) == k_sys_work_q.thread_id);
  s_ran_on = NULL;
  cl_assert_equal_i(k_work_submit_to_queue(&q2, &work), 1);
  k_sleep(K_MSEC(1));
  cl_assert(s_ran_on == k_sys_work_q.thread_id);

  // The HCI input thread hands items over in order; both threads end on quit.
  zbt_rx_thread_start(prv_rx_item, &s_host_threads, NULL);
  zbt_rx_thread_put(&s_items[0]);
  zbt_rx_thread_put(&s_items[1]);
  k_sleep(K_MSEC(1));
  cl_assert_equal_s(s_trace, "ab");

  struct zbt_stack exec;
  struct zbt_stack rx;
  zbt_threads_stacks(&exec, &rx);
  cl_assert_equal_i(exec.size, CONFIG_ZBT_EXECUTOR_STACK_SIZE);
  cl_assert_equal_i(rx.size, CONFIG_ZBT_RX_STACK_SIZE);

  cl_assert(!zbt_threads_stopped());
  zbt_rx_thread_quit();
  zbt_executor_quit();
  k_sleep(K_MSEC(1));
  cl_assert(zbt_threads_stopped());

  pbl_test_kernel_stop();
}

void test_zbt_kernel__work_queues(void) {
  prv_spawn(0, 3, prv_work_controller, NULL);
  pbl_test_kernel_run();
}

// ---- condition variables ------------------------------------------------------

static K_CONDVAR_DEFINE(s_cv);
static int s_cv_flag;

static void prv_cv_waiter(void *arg) {
  k_mutex_lock(&s_mutex, K_FOREVER);
  cl_assert_equal_i(k_condvar_wait(&s_cv, &s_mutex, K_MSEC(5)), -EAGAIN);
  prv_trace('t');
  while (!s_cv_flag) {
    cl_assert_equal_i(k_condvar_wait(&s_cv, &s_mutex, K_FOREVER), 0);
  }
  prv_trace('W');
  k_mutex_unlock(&s_mutex);
  pbl_test_kernel_stop();
}

static void prv_cv_signaller(void *arg) {
  k_sleep(K_MSEC(10));
  k_mutex_lock(&s_mutex, K_FOREVER);
  s_cv_flag = 1;
  prv_trace('s');
  k_condvar_broadcast(&s_cv);
  k_mutex_unlock(&s_mutex);
}

void test_zbt_kernel__condvar(void) {
  k_mutex_init(&s_mutex);
  k_condvar_init(&s_cv);
  s_cv_flag = 0;
  prv_spawn(0, 3, prv_cv_waiter, NULL);
  prv_spawn(1, 2, prv_cv_signaller, NULL);
  pbl_test_kernel_run();
  cl_assert_equal_s(s_trace, "tsW");
}
