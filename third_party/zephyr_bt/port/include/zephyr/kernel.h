/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// The subset of the Zephyr kernel API the Bluetooth host uses, implemented on
// the PebbleOS kernel (zbt_kernel.c). Every object can be defined statically
// and used without an init call, as in Zephyr.
//
// All work queues share one PebbleOS thread, so the system work queue and the
// host's RX work queue are the same thread: a synchronous HCI command sent from
// either runs the host's "on the system work queue" path instead of waiting on
// itself.

#pragma once

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <zephyr/sys/__assert.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/clock.h>
#include <zephyr/sys/dlist.h>
#include <zephyr/sys/sflist.h>
#include <zephyr/sys/slist.h>
#include <zephyr/sys/util.h>
#include <zephyr/toolchain.h>
#include <zephyr/types.h>

/* Threads */

struct arch_esf;

struct k_thread;
typedef struct k_thread *k_tid_t;
typedef uint8_t k_thread_stack_t;

#define K_PRIO_COOP(x)                   (-(CONFIG_NUM_COOP_PRIORITIES - (x)))
#define K_PRIO_PREEMPT(x)                (x)
#define K_LOWEST_APPLICATION_THREAD_PRIO 15

// Work queues run on the port's own thread, so the stacks the host declares
// for them are never used; keep them to a token size.
#define K_KERNEL_STACK_DEFINE(sym, size) k_thread_stack_t sym[1]
#define K_THREAD_STACK_DEFINE(sym, size) K_KERNEL_STACK_DEFINE(sym, size)
#define K_THREAD_STACK_SIZEOF(sym)       sizeof(sym)
#define K_KERNEL_STACK_SIZEOF(sym)       sizeof(sym)

k_tid_t k_current_get(void);
int k_thread_name_set(k_tid_t thread, const char *name);
int32_t k_sleep(k_timeout_t timeout);
void k_yield(void);
bool k_is_in_isr(void);
static inline bool k_is_pre_kernel(void) {
  return false;
}

void k_sched_lock(void);
void k_sched_unlock(void);

/* Time */

int64_t k_uptime_ticks(void);

static inline int64_t k_uptime_get(void) {
  return k_ticks_to_ms_floor64(k_uptime_ticks());
}

static inline uint32_t k_uptime_get_32(void) {
  return (uint32_t)k_uptime_get();
}

#define K_NO_WAIT    Z_TIMEOUT_NO_WAIT
#define K_FOREVER    Z_FOREVER
#define K_NSEC(t)    Z_TIMEOUT_NS(t)
#define K_USEC(t)    Z_TIMEOUT_US(t)
#define K_MSEC(ms)   Z_TIMEOUT_MS(ms)
#define K_SECONDS(s) K_MSEC((s) * MSEC_PER_SEC)
#define K_MINUTES(m) K_SECONDS((m) * 60)
#define K_TICKS(t)   Z_TIMEOUT_TICKS(t)

/* Fatal errors, routed to the port instead of crashing the watch */

void zbt_fatal(const char *file, int line);

#define k_panic() zbt_fatal(__FILE__, __LINE__)
#define k_oops()  zbt_fatal(__FILE__, __LINE__)

/* Interrupt locking */

unsigned int irq_lock(void);
void irq_unlock(unsigned int key);

struct k_spinlock {
  uint8_t unused;
};

typedef struct {
  unsigned int key;
} k_spinlock_key_t;

static inline k_spinlock_key_t k_spin_lock(struct k_spinlock *l) {
  ARG_UNUSED(l);
  return (k_spinlock_key_t){.key = irq_lock()};
}

static inline void k_spin_unlock(struct k_spinlock *l, k_spinlock_key_t key) {
  ARG_UNUSED(l);
  irq_unlock(key.key);
}

/* Wait lists: each blocked thread queues a waiter that carries its own
 * semaphore, so objects need no OS resources and can be defined statically.
 */

struct zbt_waiter;

struct zbt_wait {
  struct zbt_waiter *head;
};

#define Z_WAIT_INITIALIZER {.head = NULL}

/* Semaphores */

struct k_sem {
  struct zbt_wait wait;
  volatile unsigned int count;
  unsigned int limit;
};

#define K_SEM_MAX_LIMIT UINT_MAX

#define Z_SEM_INITIALIZER(obj, initial_count, count_limit) \
  {.wait = Z_WAIT_INITIALIZER, .count = (initial_count), .limit = (count_limit)}

#define K_SEM_DEFINE(name, initial_count, count_limit) \
  struct k_sem name = Z_SEM_INITIALIZER(name, initial_count, count_limit)

int k_sem_init(struct k_sem *sem, unsigned int initial_count, unsigned int limit);
int k_sem_take(struct k_sem *sem, k_timeout_t timeout);
void k_sem_give(struct k_sem *sem);
void k_sem_reset(struct k_sem *sem);

static inline unsigned int k_sem_count_get(struct k_sem *sem) {
  return sem->count;
}

/* Mutexes (recursive, as in Zephyr) */

struct k_mutex {
  struct zbt_wait wait;
  k_tid_t owner;
  unsigned int lock_count;
};

#define Z_MUTEX_INITIALIZER(obj) {.wait = Z_WAIT_INITIALIZER, .owner = NULL, .lock_count = 0}
#define K_MUTEX_DEFINE(name)     struct k_mutex name = Z_MUTEX_INITIALIZER(name)

int k_mutex_init(struct k_mutex *mutex);
int k_mutex_lock(struct k_mutex *mutex, k_timeout_t timeout);
int k_mutex_unlock(struct k_mutex *mutex);

/* Condition variables */

struct k_condvar {
  struct zbt_wait wait;
};

#define Z_CONDVAR_INITIALIZER(obj) {.wait = Z_WAIT_INITIALIZER}
#define K_CONDVAR_DEFINE(name)     struct k_condvar name = Z_CONDVAR_INITIALIZER(name)

int k_condvar_init(struct k_condvar *condvar);
int k_condvar_signal(struct k_condvar *condvar);
int k_condvar_broadcast(struct k_condvar *condvar);
int k_condvar_wait(struct k_condvar *condvar, struct k_mutex *mutex, k_timeout_t timeout);

/* Queues: items carry their link in their first word, as in Zephyr */

struct k_queue {
  sys_sflist_t data_q;
  struct zbt_wait wait;
};

#define Z_QUEUE_INITIALIZER(obj) \
  {.data_q = SYS_SFLIST_STATIC_INIT(&obj.data_q), .wait = Z_WAIT_INITIALIZER}

void k_queue_init(struct k_queue *queue);
void k_queue_append(struct k_queue *queue, void *data);
void k_queue_prepend(struct k_queue *queue, void *data);
void *k_queue_get(struct k_queue *queue, k_timeout_t timeout);
bool k_queue_remove(struct k_queue *queue, void *data);
int k_queue_is_empty(struct k_queue *queue);
void *k_queue_peek_head(struct k_queue *queue);
void *k_queue_peek_tail(struct k_queue *queue);

struct k_fifo {
  struct k_queue _queue;
};

#define Z_FIFO_INITIALIZER(obj) {._queue = Z_QUEUE_INITIALIZER(obj._queue)}
#define K_FIFO_DEFINE(name)     struct k_fifo name = Z_FIFO_INITIALIZER(name)

#define k_fifo_init(fifo)         k_queue_init(&(fifo)->_queue)
#define k_fifo_put(fifo, data)    k_queue_append(&(fifo)->_queue, data)
#define k_fifo_get(fifo, timeout) k_queue_get(&(fifo)->_queue, timeout)
#define k_fifo_is_empty(fifo)     k_queue_is_empty(&(fifo)->_queue)
#define k_fifo_peek_head(fifo)    k_queue_peek_head(&(fifo)->_queue)
#define k_fifo_peek_tail(fifo)    k_queue_peek_tail(&(fifo)->_queue)

struct k_lifo {
  struct k_queue _queue;
};

#define Z_LIFO_INITIALIZER(obj) {._queue = Z_QUEUE_INITIALIZER(obj._queue)}
#define K_LIFO_DEFINE(name)     struct k_lifo name = Z_LIFO_INITIALIZER(name)

#define k_lifo_init(lifo)         k_queue_init(&(lifo)->_queue)
#define k_lifo_put(lifo, data)    k_queue_prepend(&(lifo)->_queue, data)
#define k_lifo_get(lifo, timeout) k_queue_get(&(lifo)->_queue, timeout)

/* Memory slabs */

struct k_mem_slab {
  struct k_lifo free;
  char *buffer;
  size_t block_size;
  uint32_t num_blocks;
  uint32_t num_used;
  bool initialized;
};

#define Z_MEM_SLAB_INITIALIZER(_slab, _buffer, _block_size, _num_blocks) \
  {.free = Z_LIFO_INITIALIZER(_slab.free),                               \
   .buffer = (char *)(_buffer),                                          \
   .block_size = (_block_size),                                          \
   .num_blocks = (_num_blocks),                                          \
   .num_used = 0,                                                        \
   .initialized = false}

#define K_MEM_SLAB_DEFINE_STATIC_TYPE(name, type, num_blocks) \
  static type _k_mem_slab_buf_##name[num_blocks];             \
  static struct k_mem_slab name =                             \
      Z_MEM_SLAB_INITIALIZER(name, _k_mem_slab_buf_##name, sizeof(type), num_blocks)

#define K_MEM_SLAB_DEFINE_TYPE(name, type, num_blocks) \
  static type _k_mem_slab_buf_##name[num_blocks];      \
  struct k_mem_slab name =                             \
      Z_MEM_SLAB_INITIALIZER(name, _k_mem_slab_buf_##name, sizeof(type), num_blocks)

int k_mem_slab_alloc(struct k_mem_slab *slab, void **mem, k_timeout_t timeout);
void k_mem_slab_free(struct k_mem_slab *slab, void *mem);

static inline uint32_t k_mem_slab_num_used_get(struct k_mem_slab *slab) {
  return slab->num_used;
}

/* Heaps: backed by the PebbleOS kernel heap */

struct k_heap {
  size_t limit;
};

#define K_HEAP_DEFINE(name, bytes) struct k_heap name = {.limit = (bytes)}
#define K_HEAP_MEM_POOL_SIZE       0

void *k_heap_alloc(struct k_heap *h, size_t bytes, k_timeout_t timeout);
void *k_heap_aligned_alloc(struct k_heap *h, size_t align, size_t bytes, k_timeout_t timeout);
void k_heap_free(struct k_heap *h, void *mem);
void *k_malloc(size_t size);
void *k_calloc(size_t nmemb, size_t size);
void k_free(void *ptr);

/* Work items */

struct k_work;
struct k_work_q;
typedef void (*k_work_handler_t)(struct k_work *work);

enum {
  K_WORK_RUNNING_BIT = 0,
  K_WORK_CANCELING_BIT = 1,
  K_WORK_QUEUED_BIT = 2,
  K_WORK_DELAYED_BIT = 3,
  K_WORK_FLUSHING_BIT = 4,
  K_WORK_DELAYABLE_BIT = 8,
};

enum {
  K_WORK_RUNNING = BIT(K_WORK_RUNNING_BIT),
  K_WORK_CANCELING = BIT(K_WORK_CANCELING_BIT),
  K_WORK_QUEUED = BIT(K_WORK_QUEUED_BIT),
  K_WORK_DELAYED = BIT(K_WORK_DELAYED_BIT),
  K_WORK_FLUSHING = BIT(K_WORK_FLUSHING_BIT),
};

struct k_work {
  sys_snode_t node;
  k_work_handler_t handler;
  struct k_work_q *queue;
  volatile uint32_t flags;
};

struct k_work_delayable {
  struct k_work work;
  struct k_work_delayable *next_delayed;
  int64_t deadline;
  //! Queue last scheduled on; the host reads it.
  struct k_work_q *queue;
};

struct k_work_sync {
  uint8_t unused;
};

struct k_work_queue_config {
  const char *name;
  bool no_yield;
  bool essential;
  uint32_t work_timeout_ms;
};

struct k_work_q {
  k_tid_t thread_id;
  uint32_t flags;
};

#define Z_WORK_INITIALIZER(work_handler) {.handler = (work_handler), .flags = 0}
#define Z_WORK_DELAYABLE_INITIALIZER(work_handler)                  \
  {                                                                 \
    .work = {.handler = (work_handler), .flags = K_WORK_DELAYABLE } \
  }
#define K_WORK_DELAYABLE BIT(K_WORK_DELAYABLE_BIT)

#define K_WORK_DEFINE(work, work_handler) struct k_work work = Z_WORK_INITIALIZER(work_handler)
#define K_WORK_DELAYABLE_DEFINE(work, work_handler) \
  struct k_work_delayable work = Z_WORK_DELAYABLE_INITIALIZER(work_handler)

extern struct k_work_q k_sys_work_q;

void k_work_init(struct k_work *work, k_work_handler_t handler);
int k_work_busy_get(const struct k_work *work);
int k_work_submit_to_queue(struct k_work_q *queue, struct k_work *work);
int k_work_submit(struct k_work *work);
bool k_work_flush(struct k_work *work, struct k_work_sync *sync);
int k_work_cancel(struct k_work *work);
bool k_work_cancel_sync(struct k_work *work, struct k_work_sync *sync);

static inline bool k_work_is_pending(const struct k_work *work) {
  return k_work_busy_get(work) != 0;
}

void k_work_init_delayable(struct k_work_delayable *dwork, k_work_handler_t handler);
int k_work_delayable_busy_get(const struct k_work_delayable *dwork);
k_ticks_t k_work_delayable_remaining_get(const struct k_work_delayable *dwork);
int k_work_schedule_for_queue(struct k_work_q *queue, struct k_work_delayable *dwork,
                              k_timeout_t delay);
int k_work_schedule(struct k_work_delayable *dwork, k_timeout_t delay);
int k_work_reschedule_for_queue(struct k_work_q *queue, struct k_work_delayable *dwork,
                                k_timeout_t delay);
int k_work_reschedule(struct k_work_delayable *dwork, k_timeout_t delay);
int k_work_cancel_delayable(struct k_work_delayable *dwork);
bool k_work_cancel_delayable_sync(struct k_work_delayable *dwork, struct k_work_sync *sync);
bool k_work_flush_delayable(struct k_work_delayable *dwork, struct k_work_sync *sync);

static inline bool k_work_delayable_is_pending(const struct k_work_delayable *dwork) {
  return k_work_delayable_busy_get(dwork) != 0;
}

static inline struct k_work_delayable *k_work_delayable_from_work(struct k_work *work) {
  return CONTAINER_OF(work, struct k_work_delayable, work);
}

void k_work_queue_init(struct k_work_q *queue);
void k_work_queue_start(struct k_work_q *queue, k_thread_stack_t *stack, size_t stack_size,
                        int prio, const struct k_work_queue_config *cfg);

static inline k_tid_t k_work_queue_thread_get(struct k_work_q *queue) {
  return queue->thread_id;
}
