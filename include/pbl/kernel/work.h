/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>

#include <pbl/kernel/sem.h>
#include <pbl/kernel/timer.h>
#include <pbl/kernel/types.h>

/**
 * @defgroup kernel_work Work queues
 * @ingroup kernel
 * @brief Run functions on a kernel thread, deferred from interrupts or other threads.
 *
 * A work item is a caller-owned struct, usually embedded in the object it acts on. Submitting
 * allocates nothing and cannot fail: an item is queued at most once, so submitting a pending item
 * does nothing, and its function runs once for any number of submits. Items run one at a time in
 * submission order. An item may be resubmitted or freed by its own function.
 *
 * Delayed work adds a timer in front of the queue. Payloads that cannot share one item go through
 * pbl_work_call(), which takes its slots from a bounded pool.
 *
 * The kernel provides @ref pbl_sys_workq for background work and @ref pbl_sys_workq_high for short,
 * time sensitive work.
 *
 * Everything may be called from an ISR except where noted.
 *
 * @code{.c}
 * struct foo {
 *   struct pbl_delayed_work flush;
 *   ...
 * };
 *
 * static void prv_flush(struct pbl_work *work) {
 *   struct foo *foo = container_of(work, struct foo, flush.work);
 *   ...
 * }
 *
 * pbl_delayed_work_init(&foo->flush, prv_flush);
 * pbl_delayed_work_reschedule(&pbl_sys_workq, &foo->flush, PBL_MSEC(500));
 * ...
 * pbl_delayed_work_cancel_sync(&foo->flush);
 * @endcode
 * @{
 */

struct pbl_work;
struct pbl_workq;

/** @brief Work function, run on the queue's thread. */
typedef void (*pbl_work_fn_t)(struct pbl_work *work);

/** @brief Function run by pbl_work_call(). */
typedef void (*pbl_work_call_fn_t)(void *data);

/** @brief Work item. */
struct pbl_work {
  /** @cond INTERNAL_HIDDEN */
  struct pbl_work *next;
  struct pbl_workq *q;
  bool pending;
  bool armed;
  bool boosted;
  /** @endcond */
  /** Function. */
  pbl_work_fn_t fn;
};

/** @brief Work item submitted after a delay. */
struct pbl_delayed_work {
  /** Work item, passed to the function. */
  struct pbl_work work;
  /** @cond INTERNAL_HIDDEN */
  struct pbl_timer timer;
  /** @endcond */
};

/** @brief Work item submitted at wall clock boundaries. */
struct pbl_wallclock_work {
  /** Work item, passed to the function. */
  struct pbl_work work;
  /** @cond INTERNAL_HIDDEN */
  struct pbl_timer timer;
  struct pbl_workq *q;
  struct pbl_wallclock_work *next;
  uint64_t due_ms;
  uint32_t unit_ms;
  uint32_t every;
  bool running;
  /** @endcond */
};

/** @brief Optional hooks run on a queue's thread. */
struct pbl_workq_ops {
  /** Around blocking for work. */
  void (*wait)(struct pbl_workq *q, bool waiting);
  /** After each work item. */
  void (*done)(struct pbl_workq *q);
};

/** @brief Work queue. */
struct pbl_workq {
  /** @cond INTERNAL_HIDDEN */
  struct pbl_work *head;
  struct pbl_work *tail;
  const struct pbl_work *current;
  void *current_fn;
  struct pbl_thread *thread;
  const struct pbl_workq_ops *ops;
  struct pbl_sem wake;
  uint32_t boosts;
  pbl_prio_t boost_prio;
  pbl_prio_t unboosted_prio;
  /** @endcond */
};

/** @cond INTERNAL_HIDDEN */
struct pbl_work_call {
  struct pbl_work work;
  struct pbl_work_pool *pool;
  pbl_work_call_fn_t fn;
  void *data;
};

void pbl_delayed_work_expired(struct pbl_timer *timer);
/** @endcond */

/** @brief Slots for pbl_work_call(). */
struct pbl_work_pool {
  /** @cond INTERNAL_HIDDEN */
  struct pbl_work_call *slots;
  struct pbl_work_call *free_head;
  size_t num_slots;
  size_t num_used;
  struct pbl_sem free;
  /** @endcond */
};

/**
 * @brief Static initializer for a work item.
 *
 * @param _fn Function.
 */
#define PBL_WORK_INITIALIZER(_fn) {.fn = (_fn)}

/**
 * @brief Define a work item, usable without pbl_work_init().
 *
 * @param name Name of the variable.
 * @param fn Function.
 */
#define PBL_WORK_DEFINE(name, fn) struct pbl_work name = PBL_WORK_INITIALIZER(fn)

/**
 * @brief Static initializer for a delayed work item.
 *
 * @param _fn Function.
 */
#define PBL_DELAYED_WORK_INITIALIZER(_fn)                     \
  {                                                           \
    .work = PBL_WORK_INITIALIZER(_fn),                        \
    .timer = PBL_TIMER_INITIALIZER(pbl_delayed_work_expired), \
  }

/**
 * @brief Define a delayed work item, usable without pbl_delayed_work_init().
 *
 * @param name Name of the variable.
 * @param fn Function.
 */
#define PBL_DELAYED_WORK_DEFINE(name, fn) \
  struct pbl_delayed_work name = PBL_DELAYED_WORK_INITIALIZER(fn)

/** @brief Static initializer for a work queue. */
#define PBL_WORKQ_INITIALIZER {.wake = PBL_SEM_INITIALIZER(0, 1)}

/**
 * @brief Define a work queue, usable without pbl_workq_init().
 *
 * @param name Name of the variable.
 */
#define PBL_WORKQ_DEFINE(name) struct pbl_workq name = PBL_WORKQ_INITIALIZER

/**
 * @brief Define a pool for pbl_work_call(), usable without pbl_work_pool_init().
 *
 * @param name Name of the variable.
 * @param n Number of calls that can be pending at once.
 */
#define PBL_WORK_POOL_DEFINE(name, n)     \
  struct pbl_work_pool name = {           \
    .slots = (struct pbl_work_call[n]){}, \
    .num_slots = (n),                     \
    .free = PBL_SEM_INITIALIZER(n, n),    \
  }

/** @brief Queue for background work, which may block and take a while. */
extern struct pbl_workq pbl_sys_workq;

/** @brief Queue for short, time sensitive work, at the highest thread priority. */
extern struct pbl_workq pbl_sys_workq_high;

/**
 * @brief Initialize a work queue in dynamically allocated memory.
 *
 * @param[out] q Queue.
 */
void pbl_workq_init(struct pbl_workq *q);

/**
 * @brief Set the hooks of a queue, which take effect once it is next idle.
 *
 * @param q Queue.
 * @param ops Hooks, which must outlive the queue.
 */
void pbl_workq_set_ops(struct pbl_workq *q, const struct pbl_workq_ops *ops);

/**
 * @brief Thread entry serving a queue, with the queue as its argument.
 *
 * For queues other than the system ones, which the kernel starts at boot. The caller creates the
 * thread, which picks the queue's priority and stack. Work may be submitted before it starts.
 *
 * @param arg Queue.
 */
void pbl_workq_thread(void *arg);

/**
 * @brief Wait until everything submitted so far has run. Thread context only.
 *
 * Must not be called from the queue's own thread.
 *
 * @param q Queue.
 */
void pbl_workq_flush(struct pbl_workq *q);

/**
 * @brief Get the thread serving a queue.
 *
 * @param q Queue.
 * @return Its thread, NULL until it has one.
 */
struct pbl_thread *pbl_workq_get_thread(const struct pbl_workq *q);

/**
 * @brief Check whether the caller runs on a queue's thread.
 *
 * @param q Queue.
 * @return true on its thread, false elsewhere, including in an ISR.
 */
bool pbl_workq_is_current(const struct pbl_workq *q);

/**
 * @brief Get the function being run, for debugging.
 *
 * @param q Queue.
 * @return The work or call function being run, NULL when idle.
 */
void *pbl_workq_current_fn(const struct pbl_workq *q);

/**
 * @brief Initialize a work item.
 *
 * @param[out] work Work item.
 * @param fn Function.
 */
void pbl_work_init(struct pbl_work *work, pbl_work_fn_t fn);

/**
 * @brief Queue a work item.
 *
 * @param q Queue.
 * @param work Work item.
 * @return true if queued, false if it was already pending.
 */
bool pbl_work_submit(struct pbl_workq *q, struct pbl_work *work);

/**
 * @brief Set the priority a queue's thread runs at while it has boosted work.
 *
 * @param q Queue.
 * @param prio Priority, above the thread's own for the boost to have an effect.
 */
void pbl_workq_set_boost_prio(struct pbl_workq *q, pbl_prio_t prio);

/**
 * @brief Queue a work item that raises its queue's priority until it has run.
 *
 * For latency sensitive work on a low priority queue, such as reading a device after its
 * interrupt. The queue's thread runs at the priority set with pbl_workq_set_boost_prio() from now
 * until the function returns or the item is canceled, together with any other boosted item.
 * Submitting an item that is already pending boosts it.
 *
 * @param q Queue.
 * @param work Work item.
 * @return true if queued, false if it was already pending.
 */
bool pbl_work_submit_boosted(struct pbl_workq *q, struct pbl_work *work);

/**
 * @brief Remove a work item from its queue, without waiting for it if it is running.
 *
 * @param work Work item.
 * @return true if it was pending.
 */
bool pbl_work_cancel(struct pbl_work *work);

/**
 * @brief Remove a work item from its queue and wait for it to finish running. Thread context only.
 *
 * On return the function is not running, unless called by the item itself, and will not run
 * unless submitted again.
 *
 * @param work Work item.
 * @return true if it was pending.
 */
bool pbl_work_cancel_sync(struct pbl_work *work);

/**
 * @brief Wait for a work item to finish its last submission. Thread context only.
 *
 * Must not be called from the thread of the item's queue.
 *
 * @param work Work item.
 * @return true if it had to wait, false if the item was idle.
 */
bool pbl_work_flush(struct pbl_work *work);

/**
 * @brief Check whether a work item is queued.
 *
 * @param work Work item.
 * @return true from submission until its function starts or it is canceled.
 */
bool pbl_work_is_pending(const struct pbl_work *work);

/**
 * @brief Check whether a work item's function is running.
 *
 * @param work Work item.
 * @return true while the function runs.
 */
bool pbl_work_is_running(const struct pbl_work *work);

/**
 * @brief Initialize a delayed work item.
 *
 * @param[out] dwork Delayed work item.
 * @param fn Function.
 */
void pbl_delayed_work_init(struct pbl_delayed_work *dwork, pbl_work_fn_t fn);

/**
 * @brief Queue a delayed work item after a delay, unless it is already pending.
 *
 * @param q Queue.
 * @param dwork Delayed work item.
 * @param delay Delay, @ref PBL_NO_WAIT to queue now; must not be @ref PBL_FOREVER.
 * @return true if scheduled, false if it was already pending.
 */
bool pbl_delayed_work_schedule(struct pbl_workq *q, struct pbl_delayed_work *dwork,
                               pbl_timeout_t delay);

/**
 * @brief Queue a delayed work item after a delay, replacing any pending delay.
 *
 * @param q Queue.
 * @param dwork Delayed work item.
 * @param delay Delay, @ref PBL_NO_WAIT to queue now; must not be @ref PBL_FOREVER.
 * @return true if it was already pending.
 */
bool pbl_delayed_work_reschedule(struct pbl_workq *q, struct pbl_delayed_work *dwork,
                                 pbl_timeout_t delay);

/**
 * @brief Queue a delayed work item one period after its last expiry, so it does not drift.
 *
 * For a periodic item, from its own function. When that time has passed, the item is queued on the
 * next tick and later periods count from there, so missed periods do not pile up.
 *
 * @param dwork Delayed work item, which must have been scheduled before.
 * @param period Ticks since its last expiry.
 */
void pbl_delayed_work_schedule_next(struct pbl_delayed_work *dwork, pbl_tick_t period);

/**
 * @brief Stop a delayed work item, without waiting for it if it is running.
 *
 * @param dwork Delayed work item.
 * @return true if it was pending.
 */
bool pbl_delayed_work_cancel(struct pbl_delayed_work *dwork);

/**
 * @brief Stop a delayed work item and wait for it to finish running. Thread context only.
 *
 * Holds against an item that schedules itself again from its function.
 *
 * @param dwork Delayed work item.
 * @return true if it was pending.
 */
bool pbl_delayed_work_cancel_sync(struct pbl_delayed_work *dwork);

/**
 * @brief Check whether a delayed work item is pending.
 *
 * @param dwork Delayed work item.
 * @return true while its delay runs or it is queued.
 */
bool pbl_delayed_work_is_pending(const struct pbl_delayed_work *dwork);

/**
 * @brief Get the time left on a delayed work item.
 *
 * @param dwork Delayed work item.
 * @return Ticks until it is queued, 0 if its delay is not running.
 */
pbl_tick_t pbl_delayed_work_remaining(const struct pbl_delayed_work *dwork);

/**
 * @brief Initialize a pool in dynamically allocated memory.
 *
 * @param[out] pool Pool.
 * @param slots Storage for the slots, which must outlive the pool.
 * @param num_slots Number of calls that can be pending at once.
 */
void pbl_work_pool_init(struct pbl_work_pool *pool, struct pbl_work_call *slots, size_t num_slots);

/**
 * @brief Get the free slots of a pool.
 *
 * @param pool Pool.
 * @return Number of calls that can be queued without waiting.
 */
size_t pbl_work_pool_num_free(const struct pbl_work_pool *pool);

/**
 * @brief Run a function with a payload on a queue.
 *
 * For work that cannot share a work item because several payloads may be pending at once; prefer
 * a work item otherwise. The slot is released before @p fn runs, so @p fn may queue another call.
 *
 * The queue's own thread never waits, as only it can release the slots of its queue's calls.
 *
 * @param q Queue.
 * @param pool Pool to take the slot from.
 * @param fn Function.
 * @param data Argument of @p fn.
 * @param timeout How long to wait for a free slot; must be @ref PBL_NO_WAIT in an ISR.
 * @return 0 on success, -EBUSY if no slot was free without waiting, -EAGAIN on timeout.
 */
int pbl_work_call(struct pbl_workq *q, struct pbl_work_pool *pool, pbl_work_call_fn_t fn,
                  void *data, pbl_timeout_t timeout);

/**
 * @brief Initialize a wall clock work item.
 *
 * @param[out] w Wall clock work item.
 * @param fn Function, whose argument is @c &w->work.
 */
void pbl_wallclock_work_init(struct pbl_wallclock_work *w, pbl_work_fn_t fn);

/**
 * @brief Queue a work item at every @p every -th boundary of the wall clock's @p unit_s seconds.
 *
 * E.g. every minute, as the wall clock's minute changes, or every 5 seconds, as its second does.
 * The first is @p every boundaries from now. Items due at the same boundary expire together, so
 * they wake the system once.
 *
 * Setting the clock moves the boundaries with it. A change that lands in another unit counts as
 * one boundary, so a minute item runs at once when the clock moves to another minute. Starting a
 * running item restarts it.
 *
 * @param q Queue.
 * @param w Wall clock work item.
 * @param unit_s Unit in seconds, e.g. 1 or 60.
 * @param every Number of boundaries between runs.
 */
void pbl_wallclock_work_start(struct pbl_workq *q, struct pbl_wallclock_work *w, uint32_t unit_s,
                              uint32_t every);

/**
 * @brief Stop a wall clock work item, and remove it from its queue if it is pending.
 *
 * Does not wait for a function that is running.
 *
 * @param w Wall clock work item.
 * @return true if it was running.
 */
bool pbl_wallclock_work_stop(struct pbl_wallclock_work *w);

/**
 * @brief Check whether a wall clock work item is started.
 *
 * @param w Wall clock work item.
 * @return true from pbl_wallclock_work_start() until pbl_wallclock_work_stop().
 */
bool pbl_wallclock_work_is_running(const struct pbl_wallclock_work *w);

/**
 * @brief Run a function with a payload on @ref pbl_sys_workq.
 *
 * For work that cannot embed a work item. Thread context only. A thread with its own slots, see
 * @ref pbl_thread_attr, waits for one; the queue's own thread puts what does not fit on the heap;
 * any other thread waits a while, then stops the system, as a queue that stays full is stuck.
 *
 * @param fn Function.
 * @param data Argument of @p fn.
 * @return true if queued, false while calls are blocked.
 */
bool pbl_sys_work_call(pbl_work_call_fn_t fn, void *data);

/**
 * @brief Like pbl_sys_work_call(), from an ISR: stops the system at once if no slot is free.
 *
 * @param fn Function.
 * @param data Argument of @p fn.
 * @return true if queued, false while calls are blocked.
 */
bool pbl_sys_work_call_from_isr(pbl_work_call_fn_t fn, void *data);

/**
 * @brief Like pbl_sys_work_call(), but never waits nor stops the system. Callable from an ISR.
 *
 * @param fn Function.
 * @param data Argument of @p fn.
 * @return true if queued, false if no slot was free or calls are blocked.
 */
bool pbl_sys_work_try_call(pbl_work_call_fn_t fn, void *data);

/**
 * @brief Run a short, time sensitive function with a payload on @ref pbl_sys_workq_high.
 *
 * Thread context only; waits a little for a free slot.
 *
 * @param fn Function.
 * @param data Argument of @p fn.
 * @return true if queued, false if no slot became free.
 */
bool pbl_sys_work_call_high(pbl_work_call_fn_t fn, void *data);

/**
 * @brief Like pbl_sys_work_call_high(), from an ISR: the call is dropped if no slot is free.
 *
 * @param fn Function.
 * @param data Argument of @p fn.
 */
void pbl_sys_work_call_high_from_isr(pbl_work_call_fn_t fn, void *data);

/**
 * @brief Refuse new calls to @ref pbl_sys_workq, e.g. while the system shuts down.
 *
 * @param block true to refuse them, false to accept them again.
 */
void pbl_sys_work_block(bool block);

/**
 * @brief Get how many calls the caller can make to @ref pbl_sys_workq without waiting.
 *
 * @return Free slots, the caller's own if it has some.
 */
size_t pbl_sys_work_num_free(void);

/**
 * @brief Get the function running on @ref pbl_sys_workq, for debugging.
 *
 * @return The function, including one behind a call the queue made to itself, NULL when idle.
 */
void *pbl_sys_work_current_fn(void);

/** @} */
