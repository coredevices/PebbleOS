/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// Zephyr kernel API for the Bluetooth host, on the PebbleOS kernel.
//
// Blocking objects keep a list of waiters under the interrupt lock; each
// waiter is on the blocked thread's stack and carries its own semaphore. All
// work queues run on one thread (zbt_executor), which also handles delayed
// work by sleeping until the earliest deadline.

#include "zbt_port.h"

#include <string.h>

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/debug.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/sched.h>
#include <pbl/kernel/sem.h>
#include <pbl/kernel/thread.h>
#include <pbl/kernel/time.h>
#include <pbl/kernel/types.h>
#include <pbl/util/rand32.h>

#include <kernel/pbl_malloc.h>
#include <zbt/zbt.h>
#include <zbt_link.h>
#include <zephyr/kernel.h>

/* Time ---------------------------------------------------------------------*/

// Zephyr ticks are milliseconds (CONFIG_SYS_CLOCK_TICKS_PER_SEC is 1000).
int64_t k_uptime_ticks(void) {
  // Extend the 32-bit kernel tick count to 64 bits.
  static uint32_t s_last;
  static uint32_t s_high;

  pbl_irq_lock();
  uint32_t now = pbl_uptime_ticks();
  if (now < s_last) {
    s_high++;
  }
  s_last = now;
  int64_t ticks = ((int64_t)s_high << 32) | now;
  pbl_irq_unlock();
  return ticks * 1000 / PBL_TICK_HZ;
}

static pbl_timeout_t prv_pbl_timeout(k_timeout_t timeout) {
  if (K_TIMEOUT_EQ(timeout, K_FOREVER)) {
    return PBL_FOREVER;
  }
  if (timeout.ticks <= 0) {
    return PBL_NO_WAIT;
  }
  int64_t ms = timeout.ticks;
  if (ms > (int64_t)(UINT32_MAX / 2)) {
    ms = UINT32_MAX / 2;
  }
  return PBL_MSEC((uint32_t)ms);
}

k_timepoint_t sys_timepoint_calc(k_timeout_t timeout) {
  if (K_TIMEOUT_EQ(timeout, K_FOREVER)) {
    return (k_timepoint_t){.tick = UINT64_MAX};
  }
  if (timeout.ticks <= 0) {
    return (k_timepoint_t){.tick = 0};
  }
  return (k_timepoint_t){.tick = (uint64_t)k_uptime_ticks() + (uint64_t)timeout.ticks};
}

k_timeout_t sys_timepoint_timeout(k_timepoint_t timepoint) {
  if (timepoint.tick == UINT64_MAX) {
    return K_FOREVER;
  }
  if (timepoint.tick == 0) {
    return K_NO_WAIT;
  }
  uint64_t now = (uint64_t)k_uptime_ticks();
  return now >= timepoint.tick ? K_NO_WAIT : K_TICKS((k_ticks_t)(timepoint.tick - now));
}

/* Threads ------------------------------------------------------------------*/

k_tid_t k_current_get(void) {
  return (k_tid_t)pbl_thread_current();
}

int k_thread_name_set(k_tid_t thread, const char *name) {
  return 0;
}

static void prv_check_can_block(void *caller);

int32_t k_sleep(k_timeout_t timeout) {
  prv_check_can_block(PBL_RETURN_ADDRESS(0));
  pbl_thread_sleep(prv_pbl_timeout(timeout));
  return 0;
}

void k_yield(void) {
  pbl_thread_yield();
}

bool k_is_in_isr(void) {
  return pbl_in_isr();
}

void k_sched_lock(void) {
  pbl_sched_lock();
}

void k_sched_unlock(void) {
  pbl_sched_unlock();
}

unsigned int irq_lock(void) {
  pbl_irq_lock();
  return 0;
}

void irq_unlock(unsigned int key) {
  pbl_irq_unlock();
}

/* Wait lists ---------------------------------------------------------------*/

// Marks a waiter in use, to catch one left in a wait list past its wait (diagnostic).
#define WAITER_MAGIC 0x5A17E2u

struct zbt_waiter {
  struct zbt_waiter *next;
  struct pbl_sem sem;
  k_tid_t thread;
  bool woken;
  void *item;
  uint32_t magic;
};

// A wait must not start with the interrupt lock held from outside: PebbleOS's lock is global,
// so the thread would not be switched out while it waits (Zephyr's is per thread). Reported
// with the caller, as is a waiter found in a wait list after its wait ended.
static void prv_check_can_block(void *caller) {
  if (pbl_irq_is_locked()) {
    zbt_ctl_diag(ZBT_DIAG_BLOCK_LOCKED, (uint32_t)(uintptr_t)caller, 0, 0);
  }
}

static void prv_waiter_init(struct zbt_waiter *w) {
  w->magic = WAITER_MAGIC;
  w->next = NULL;
  pbl_sem_init(&w->sem, 0, 1);
  w->thread = k_current_get();
  w->woken = false;
  w->item = NULL;
}

static void prv_waiter_done(struct zbt_waiter *w) {
  w->magic = 0;
  pbl_sem_deinit(&w->sem);
}

// Called with the interrupt lock held.
static void prv_wait_append(struct zbt_wait *wait, struct zbt_waiter *w) {
  struct zbt_waiter **p = &wait->head;
  while (*p != NULL) {
    p = &(*p)->next;
  }
  *p = w;
}

// Called with the interrupt lock held.
static void prv_wait_remove(struct zbt_wait *wait, struct zbt_waiter *w) {
  for (struct zbt_waiter **p = &wait->head; *p != NULL; p = &(*p)->next) {
    if (*p == w) {
      *p = w->next;
      return;
    }
  }
}

// Called with the interrupt lock held. Hands @p item to the first waiter.
static bool prv_wake_one(struct zbt_wait *wait, void *item) {
  struct zbt_waiter *w = wait->head;
  if (w == NULL) {
    return false;
  }
  if (w->magic != WAITER_MAGIC) {
    zbt_ctl_diag(ZBT_DIAG_STALE_WAITER, (uint32_t)(uintptr_t)wait, (uint32_t)(uintptr_t)w,
                 (uint32_t)(uintptr_t)PBL_RETURN_ADDRESS(0));
  }
  wait->head = w->next;
  w->woken = true;
  w->item = item;
  pbl_sem_give(&w->sem);
  return true;
}

// Called with the interrupt lock held, which is released while blocked and
// held again on return. Returns 0 when woken, -EAGAIN on timeout.
static int prv_block(struct zbt_wait *wait, struct zbt_waiter *w, k_timeout_t timeout,
                     void *caller) {
  prv_wait_append(wait, w);
  pbl_irq_unlock();
  prv_check_can_block(caller);
  int rc = pbl_sem_take(&w->sem, prv_pbl_timeout(timeout));
  pbl_irq_lock();
  if (rc != 0 && !w->woken) {
    prv_wait_remove(wait, w);
    return -EAGAIN;
  }
  return 0;
}

/* Semaphores ---------------------------------------------------------------*/

int k_sem_init(struct k_sem *sem, unsigned int initial_count, unsigned int limit) {
  sem->wait.head = NULL;
  sem->count = initial_count;
  sem->limit = limit;
  return 0;
}

int k_sem_take(struct k_sem *sem, k_timeout_t timeout) {
  int rc = 0;

  pbl_irq_lock();
  if (sem->count > 0) {
    sem->count--;
  } else if (K_TIMEOUT_EQ(timeout, K_NO_WAIT)) {
    rc = -EBUSY;
  } else {
    struct zbt_waiter w;
    prv_waiter_init(&w);
    rc = prv_block(&sem->wait, &w, timeout, PBL_RETURN_ADDRESS(0));
    prv_waiter_done(&w);
  }
  pbl_irq_unlock();
  return rc;
}

void k_sem_give(struct k_sem *sem) {
  pbl_irq_lock();
  if (!prv_wake_one(&sem->wait, NULL) && sem->count < sem->limit) {
    sem->count++;
  }
  pbl_irq_unlock();
}

void k_sem_reset(struct k_sem *sem) {
  pbl_irq_lock();
  sem->count = 0;
  pbl_irq_unlock();
}

/* Mutexes ------------------------------------------------------------------*/

int k_mutex_init(struct k_mutex *mutex) {
  mutex->wait.head = NULL;
  mutex->owner = NULL;
  mutex->lock_count = 0;
  return 0;
}

int k_mutex_lock(struct k_mutex *mutex, k_timeout_t timeout) {
  k_tid_t self = k_current_get();
  int rc = 0;

  pbl_irq_lock();
  if (mutex->lock_count == 0 || mutex->owner == self) {
    mutex->owner = self;
    mutex->lock_count++;
  } else if (K_TIMEOUT_EQ(timeout, K_NO_WAIT)) {
    rc = -EBUSY;
  } else {
    struct zbt_waiter w;
    prv_waiter_init(&w);
    // The unlocking thread hands ownership over before waking us.
    rc = prv_block(&mutex->wait, &w, timeout, PBL_RETURN_ADDRESS(0));
    prv_waiter_done(&w);
  }
  pbl_irq_unlock();
  return rc;
}

int k_mutex_unlock(struct k_mutex *mutex) {
  int rc = 0;

  pbl_irq_lock();
  if (mutex->lock_count == 0 || mutex->owner != k_current_get()) {
    rc = -EPERM;
  } else if (--mutex->lock_count == 0) {
    struct zbt_waiter *w = mutex->wait.head;
    if (w != NULL) {
      // Hand over to the first waiter.
      mutex->owner = w->thread;
      mutex->lock_count = 1;
      prv_wake_one(&mutex->wait, NULL);
    } else {
      mutex->owner = NULL;
    }
  }
  pbl_irq_unlock();
  return rc;
}

/* Condition variables ------------------------------------------------------*/

int k_condvar_init(struct k_condvar *condvar) {
  condvar->wait.head = NULL;
  return 0;
}

int k_condvar_signal(struct k_condvar *condvar) {
  pbl_irq_lock();
  prv_wake_one(&condvar->wait, NULL);
  pbl_irq_unlock();
  return 0;
}

int k_condvar_broadcast(struct k_condvar *condvar) {
  int n = 0;
  pbl_irq_lock();
  while (prv_wake_one(&condvar->wait, NULL)) {
    n++;
  }
  pbl_irq_unlock();
  return n;
}

int k_condvar_wait(struct k_condvar *condvar, struct k_mutex *mutex, k_timeout_t timeout) {
  struct zbt_waiter w;
  prv_waiter_init(&w);

  pbl_irq_lock();
  prv_wait_append(&condvar->wait, &w);
  pbl_irq_unlock();

  k_mutex_unlock(mutex);
  prv_check_can_block(PBL_RETURN_ADDRESS(0));
  int rc = pbl_sem_take(&w.sem, prv_pbl_timeout(timeout));

  pbl_irq_lock();
  if (rc != 0 && !w.woken) {
    prv_wait_remove(&condvar->wait, &w);
    rc = -EAGAIN;
  } else {
    rc = 0;
  }
  pbl_irq_unlock();
  prv_waiter_done(&w);

  k_mutex_lock(mutex, K_FOREVER);
  return rc;
}

/* Queues -------------------------------------------------------------------*/

void k_queue_init(struct k_queue *queue) {
  sys_sflist_init(&queue->data_q);
  queue->wait.head = NULL;
}

static void prv_queue_insert(struct k_queue *queue, void *data, bool append) {
  pbl_irq_lock();
  if (!prv_wake_one(&queue->wait, data)) {
    sys_sfnode_init((sys_sfnode_t *)data, 0);
    if (append) {
      sys_sflist_append(&queue->data_q, (sys_sfnode_t *)data);
    } else {
      sys_sflist_prepend(&queue->data_q, (sys_sfnode_t *)data);
    }
  }
  pbl_irq_unlock();
}

void k_queue_append(struct k_queue *queue, void *data) {
  prv_queue_insert(queue, data, true);
}

void k_queue_prepend(struct k_queue *queue, void *data) {
  prv_queue_insert(queue, data, false);
}

void *k_queue_get(struct k_queue *queue, k_timeout_t timeout) {
  void *data = NULL;

  pbl_irq_lock();
  if (!sys_sflist_is_empty(&queue->data_q)) {
    data = sys_sflist_get_not_empty(&queue->data_q);
  } else if (!K_TIMEOUT_EQ(timeout, K_NO_WAIT)) {
    struct zbt_waiter w;
    prv_waiter_init(&w);
    if (prv_block(&queue->wait, &w, timeout, PBL_RETURN_ADDRESS(0)) == 0) {
      data = w.item;
    }
    prv_waiter_done(&w);
  }
  pbl_irq_unlock();
  return data;
}

bool k_queue_remove(struct k_queue *queue, void *data) {
  pbl_irq_lock();
  bool found = sys_sflist_find_and_remove(&queue->data_q, (sys_sfnode_t *)data);
  pbl_irq_unlock();
  return found;
}

int k_queue_is_empty(struct k_queue *queue) {
  return sys_sflist_is_empty(&queue->data_q);
}

void *k_queue_peek_head(struct k_queue *queue) {
  return sys_sflist_peek_head(&queue->data_q);
}

void *k_queue_peek_tail(struct k_queue *queue) {
  return sys_sflist_peek_tail(&queue->data_q);
}

/* Memory slabs and heaps ---------------------------------------------------*/

int k_mem_slab_alloc(struct k_mem_slab *slab, void **mem, k_timeout_t timeout) {
  pbl_irq_lock();
  if (!slab->initialized) {
    slab->initialized = true;
    for (uint32_t i = 0; i < slab->num_blocks; i++) {
      void *block = slab->buffer + i * slab->block_size;
      sys_sfnode_init((sys_sfnode_t *)block, 0);
      sys_sflist_append(&slab->free._queue.data_q, (sys_sfnode_t *)block);
    }
  }
  pbl_irq_unlock();

  *mem = k_lifo_get(&slab->free, timeout);
  if (*mem == NULL) {
    return K_TIMEOUT_EQ(timeout, K_NO_WAIT) ? -ENOMEM : -EAGAIN;
  }
  pbl_irq_lock();
  slab->num_used++;
  pbl_irq_unlock();
  return 0;
}

void k_mem_slab_free(struct k_mem_slab *slab, void *mem) {
  pbl_irq_lock();
  slab->num_used--;
  pbl_irq_unlock();
  k_lifo_put(&slab->free, mem);
}

void *k_heap_alloc(struct k_heap *h, size_t bytes, k_timeout_t timeout) {
  return kernel_malloc(bytes);
}

void *k_heap_aligned_alloc(struct k_heap *h, size_t align, size_t bytes, k_timeout_t timeout) {
  // The kernel heap aligns to 8 bytes, which covers every request the host makes.
  if (align > 8) {
    return NULL;
  }
  return kernel_malloc(bytes);
}

void k_heap_free(struct k_heap *h, void *mem) {
  kernel_free(mem);
}

void *k_malloc(size_t size) {
  return kernel_malloc(size);
}

void *k_calloc(size_t nmemb, size_t size) {
  return kernel_zalloc(nmemb * size);
}

void k_free(void *ptr) {
  kernel_free(ptr);
}

/* Work queues --------------------------------------------------------------*/

struct k_work_q k_sys_work_q;

static sys_slist_t s_ready;                // queued work, in order
static struct k_work_delayable *s_delayed; // delayed work, by deadline
static struct k_sem s_exec_sem = Z_SEM_INITIALIZER(s_exec_sem, 0, 1);
static struct k_condvar s_work_done = Z_CONDVAR_INITIALIZER(s_work_done);
static struct k_mutex s_work_done_mutex = Z_MUTEX_INITIALIZER(s_work_done_mutex);
// On the control side's struct zbt_host_threads, in the kernel's RAM.
static struct pbl_thread *s_exec_thread;
static bool s_exec_started;
static bool s_exec_quit;

#define BUSY_MASK \
  (K_WORK_RUNNING | K_WORK_CANCELING | K_WORK_QUEUED | K_WORK_DELAYED | K_WORK_FLUSHING)

void k_work_init(struct k_work *work, k_work_handler_t handler) {
  memset(work, 0, sizeof(*work));
  work->handler = handler;
}

void k_work_init_delayable(struct k_work_delayable *dwork, k_work_handler_t handler) {
  memset(dwork, 0, sizeof(*dwork));
  dwork->work.handler = handler;
  dwork->work.flags = K_WORK_DELAYABLE;
}

int k_work_busy_get(const struct k_work *work) {
  return work->flags & BUSY_MASK;
}

int k_work_delayable_busy_get(const struct k_work_delayable *dwork) {
  return k_work_busy_get(&dwork->work);
}

// Called with the interrupt lock held.
static void prv_delayed_remove(struct k_work_delayable *dwork) {
  for (struct k_work_delayable **p = &s_delayed; *p != NULL; p = &(*p)->next_delayed) {
    if (*p == dwork) {
      *p = dwork->next_delayed;
      dwork->next_delayed = NULL;
      return;
    }
  }
}

// Called with the interrupt lock held.
static void prv_delayed_insert(struct k_work_delayable *dwork) {
  struct k_work_delayable **p = &s_delayed;
  while (*p != NULL && (*p)->deadline <= dwork->deadline) {
    p = &(*p)->next_delayed;
  }
  dwork->next_delayed = *p;
  *p = dwork;
}

static void prv_exec_kick(void) {
  k_sem_give(&s_exec_sem);
}

// Called with the interrupt lock held. Returns Zephyr's submit result.
static int prv_submit_locked(struct k_work_q *queue, struct k_work *work) {
  if (work->flags & K_WORK_CANCELING) {
    return -EBUSY;
  }
  if (work->flags & K_WORK_QUEUED) {
    return 0;
  }
  work->flags |= K_WORK_QUEUED;
  work->queue = queue;
  if (work->flags & K_WORK_DELAYABLE) {
    k_work_delayable_from_work(work)->queue = queue;
  }
  sys_slist_append(&s_ready, &work->node);
  return (work->flags & K_WORK_RUNNING) ? 2 : 1;
}

int k_work_submit_to_queue(struct k_work_q *queue, struct k_work *work) {
  pbl_irq_lock();
  int rc = prv_submit_locked(queue, work);
  pbl_irq_unlock();
  if (rc > 0) {
    prv_exec_kick();
  }
  return rc;
}

int k_work_submit(struct k_work *work) {
  return k_work_submit_to_queue(&k_sys_work_q, work);
}

// Called with the interrupt lock held. Removes pending (not running) state and
// returns the remaining busy flags.
static int prv_cancel_locked(struct k_work *work) {
  if (work->flags & K_WORK_QUEUED) {
    sys_slist_find_and_remove(&s_ready, &work->node);
    work->flags &= ~K_WORK_QUEUED;
  }
  if (work->flags & K_WORK_DELAYED) {
    prv_delayed_remove(k_work_delayable_from_work(work));
    work->flags &= ~K_WORK_DELAYED;
  }
  return work->flags & BUSY_MASK;
}

int k_work_cancel(struct k_work *work) {
  pbl_irq_lock();
  int rc = prv_cancel_locked(work);
  pbl_irq_unlock();
  return rc;
}

int k_work_cancel_delayable(struct k_work_delayable *dwork) {
  return k_work_cancel(&dwork->work);
}

// Waits until @p work is neither queued nor running. Must not be called from
// the executor for work it is running.
static void prv_wait_idle(struct k_work *work, uint32_t mask) {
  k_mutex_lock(&s_work_done_mutex, K_FOREVER);
  while (work->flags & mask) {
    k_condvar_wait(&s_work_done, &s_work_done_mutex, K_MSEC(100));
  }
  k_mutex_unlock(&s_work_done_mutex);
}

bool k_work_cancel_sync(struct k_work *work, struct k_work_sync *sync) {
  pbl_irq_lock();
  bool was_busy = (work->flags & BUSY_MASK) != 0;
  prv_cancel_locked(work);
  bool running = (work->flags & K_WORK_RUNNING) != 0;
  pbl_irq_unlock();

  if (running && k_current_get() != k_sys_work_q.thread_id) {
    prv_wait_idle(work, K_WORK_RUNNING);
  }
  return was_busy;
}

bool k_work_cancel_delayable_sync(struct k_work_delayable *dwork, struct k_work_sync *sync) {
  return k_work_cancel_sync(&dwork->work, sync);
}

bool k_work_flush(struct k_work *work, struct k_work_sync *sync) {
  bool busy = (k_work_busy_get(work) & (K_WORK_QUEUED | K_WORK_RUNNING)) != 0;
  if (busy && k_current_get() != k_sys_work_q.thread_id) {
    prv_wait_idle(work, K_WORK_QUEUED | K_WORK_RUNNING);
  }
  return busy;
}

bool k_work_flush_delayable(struct k_work_delayable *dwork, struct k_work_sync *sync) {
  pbl_irq_lock();
  if (dwork->work.flags & K_WORK_DELAYED) {
    // Run it now.
    prv_delayed_remove(dwork);
    dwork->work.flags &= ~K_WORK_DELAYED;
    prv_submit_locked(dwork->work.queue, &dwork->work);
  }
  pbl_irq_unlock();
  prv_exec_kick();
  return k_work_flush(&dwork->work, sync);
}

k_ticks_t k_work_delayable_remaining_get(const struct k_work_delayable *dwork) {
  if (!(dwork->work.flags & K_WORK_DELAYED)) {
    return 0;
  }
  int64_t left = dwork->deadline - k_uptime_ticks();
  return left > 0 ? left : 0;
}

// Called with the interrupt lock held.
static int prv_schedule_locked(struct k_work_q *queue, struct k_work_delayable *dwork,
                               k_timeout_t delay) {
  struct k_work *work = &dwork->work;
  if (K_TIMEOUT_EQ(delay, K_NO_WAIT) || delay.ticks <= 0) {
    return prv_submit_locked(queue, work);
  }
  if (K_TIMEOUT_EQ(delay, K_FOREVER)) {
    return 0;
  }
  work->queue = queue;
  dwork->queue = queue;
  work->flags |= K_WORK_DELAYED;
  dwork->deadline = k_uptime_ticks() + delay.ticks;
  prv_delayed_insert(dwork);
  return 1;
}

int k_work_schedule_for_queue(struct k_work_q *queue, struct k_work_delayable *dwork,
                              k_timeout_t delay) {
  int rc = 0;
  pbl_irq_lock();
  if (!(dwork->work.flags & (K_WORK_QUEUED | K_WORK_DELAYED))) {
    rc = prv_schedule_locked(queue, dwork, delay);
  }
  pbl_irq_unlock();
  prv_exec_kick();
  return rc;
}

int k_work_schedule(struct k_work_delayable *dwork, k_timeout_t delay) {
  return k_work_schedule_for_queue(&k_sys_work_q, dwork, delay);
}

int k_work_reschedule_for_queue(struct k_work_q *queue, struct k_work_delayable *dwork,
                                k_timeout_t delay) {
  pbl_irq_lock();
  if (dwork->work.flags & K_WORK_DELAYED) {
    prv_delayed_remove(dwork);
    dwork->work.flags &= ~K_WORK_DELAYED;
  }
  int rc = prv_schedule_locked(queue, dwork, delay);
  pbl_irq_unlock();
  prv_exec_kick();
  return rc;
}

int k_work_reschedule(struct k_work_delayable *dwork, k_timeout_t delay) {
  return k_work_reschedule_for_queue(&k_sys_work_q, dwork, delay);
}

void k_work_queue_init(struct k_work_q *queue) {
  queue->thread_id = NULL;
  queue->flags = 0;
}

void k_work_queue_start(struct k_work_q *queue, k_thread_stack_t *stack, size_t stack_size,
                        int prio, const struct k_work_queue_config *cfg) {
  // Every queue shares the executor thread.
  queue->thread_id = k_sys_work_q.thread_id;
}

// Runs the next due work item, or returns how long to sleep.
static k_timeout_t prv_exec_step(void) {
  pbl_irq_lock();
  int64_t now = k_uptime_ticks();
  while (s_delayed != NULL && s_delayed->deadline <= now) {
    struct k_work_delayable *dwork = s_delayed;
    s_delayed = dwork->next_delayed;
    dwork->next_delayed = NULL;
    dwork->work.flags &= ~K_WORK_DELAYED;
    prv_submit_locked(dwork->work.queue, &dwork->work);
  }

  sys_snode_t *node = sys_slist_get(&s_ready);
  if (node == NULL) {
    k_timeout_t sleep = s_delayed != NULL ? K_TICKS(s_delayed->deadline - now) : K_FOREVER;
    pbl_irq_unlock();
    return sleep;
  }

  struct k_work *work = CONTAINER_OF(node, struct k_work, node);
  work->flags &= ~K_WORK_QUEUED;
  work->flags |= K_WORK_RUNNING;
  pbl_irq_unlock();

  work->handler(work);

  pbl_irq_lock();
  work->flags &= ~K_WORK_RUNNING;
  pbl_irq_unlock();
  k_condvar_broadcast(&s_work_done);
  return K_NO_WAIT;
}

static uint32_t s_exec_loops;

static void prv_exec_main(void *arg) {
  while (!s_exec_quit) {
    s_exec_loops++;
    k_timeout_t sleep = prv_exec_step();
    if (!K_TIMEOUT_EQ(sleep, K_NO_WAIT)) {
      (void)k_sem_take(&s_exec_sem, sleep);
    }
  }
}

void zbt_executor_quit(void) {
  s_exec_quit = true;
  k_sem_give(&s_exec_sem);
}

static void prv_set_regions(struct pbl_thread_attr *attr, const MpuRegion *const *regions) {
  for (int i = 0; regions != NULL && i < PBL_THREAD_MAX_MEM_REGIONS; i++) {
    attr->regions[i] = regions[i];
  }
}

void zbt_executor_start(struct zbt_host_threads *threads, const MpuRegion *const *regions) {
  if (s_exec_started) {
    return;
  }
  s_exec_started = true;

  struct pbl_thread_attr attr = {
    .name = "ZephyrBT",
    .entry = prv_exec_main,
    .prio = CONFIG_ZBT_EXECUTOR_PRIO,
    .privileged = true,
    .stack = threads->exec_stack,
    .stack_size = sizeof(threads->exec_stack),
  };
  prv_set_regions(&attr, regions);
  // The thread id must be known before any work runs on it.
  s_exec_thread = &threads->exec;
  k_sys_work_q.thread_id = (k_tid_t)s_exec_thread;
  if (pbl_thread_create(s_exec_thread, &attr) != 0) {
    k_sys_work_q.thread_id = NULL;
    s_exec_started = false;
  }
}

bool zbt_executor_is_current(void) {
  return s_exec_started && k_current_get() == k_sys_work_q.thread_id;
}

/* HCI input thread ---------------------------------------------------------*/

// Packets from the HCI router's receive thread are handed to bt_recv() here:
// the router thread only copies them into host buffers, and the Zephyr thread
// cannot take them itself, as it waits for command completions.

static struct pbl_thread *s_rx_thread;
static struct k_fifo s_rx_fifo;
static bool s_rx_started;
// Queued last to end the thread.
static sys_sfnode_t s_rx_quit_node;

static uint32_t s_rx_items;

static void prv_rx_main(void *arg) {
  zbt_rx_handler_t handler = arg;
  while (true) {
    void *item = k_fifo_get(&s_rx_fifo, K_FOREVER);
    if (item == &s_rx_quit_node) {
      return;
    }
    s_rx_items++;
    handler(item);
  }
}

void zbt_rx_thread_start(zbt_rx_handler_t handler, struct zbt_host_threads *threads,
                         const MpuRegion *const *regions) {
  k_fifo_init(&s_rx_fifo);
  struct pbl_thread_attr attr = {
    .name = "ZephyrRX",
    .entry = prv_rx_main,
    .arg = handler,
    .prio = CONFIG_ZBT_EXECUTOR_PRIO,
    .privileged = true,
    .stack = threads->rx_stack,
    .stack_size = sizeof(threads->rx_stack),
  };
  prv_set_regions(&attr, regions);
  s_rx_thread = &threads->rx;
  s_rx_started = pbl_thread_create(s_rx_thread, &attr) == 0;
}

void zbt_rx_thread_put(void *buf) {
  k_fifo_put(&s_rx_fifo, buf);
}

void zbt_rx_thread_quit(void) {
  if (s_rx_started) {
    k_fifo_put(&s_rx_fifo, &s_rx_quit_node);
  }
}

bool zbt_rx_thread_is_current(void) {
  return s_rx_started && pbl_thread_current() == s_rx_thread;
}

/* Thread status --------------------------------------------------------------*/

static bool prv_thread_gone(bool started, const struct pbl_thread *t) {
  return !started || pbl_thread_state(t) == PBL_THREAD_DEAD;
}

void zbt_threads_counts(uint32_t *exec_loops, uint32_t *rx_items) {
  *exec_loops = s_exec_loops;
  *rx_items = s_rx_items;
}

bool zbt_threads_stopped(void) {
  return prv_thread_gone(s_exec_started, s_exec_thread) &&
         prv_thread_gone(s_rx_started, s_rx_thread);
}

static void prv_stack(bool started, const struct pbl_thread *t, struct zbt_stack *out) {
  struct pbl_thread_stack_info info = {0};
  if (started) {
    pbl_thread_stack_info(t, &info);
  }
  out->size = info.size;
  out->used = info.size - info.high_water;
}

void zbt_threads_stacks(struct zbt_stack *exec, struct zbt_stack *rx) {
  prv_stack(s_exec_started, s_exec_thread, exec);
  prv_stack(s_rx_started, s_rx_thread, rx);
}

/* Randomness ---------------------------------------------------------------*/

void sys_rand_get(void *dst, size_t len) {
  uint8_t *p = dst;
  while (len > 0) {
    uint32_t r = pbl_rand32();
    size_t n = len < sizeof(r) ? len : sizeof(r);
    memcpy(p, &r, n);
    p += n;
    len -= n;
  }
}

int sys_csrand_get(void *dst, size_t len) {
  sys_rand_get(dst, len);
  return 0;
}
