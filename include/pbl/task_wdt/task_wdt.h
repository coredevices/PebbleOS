/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/kernel/thread.h>

/**
 * @defgroup task_wdt Task watchdog
 * @ingroup subsys
 * @brief Software watchdog that catches stuck threads and turns them into core dumps.
 *
 * Each channel watches one thread that has to keep proving it makes progress by feeding the
 * channel within its timeout, unless it is waiting for work (see pbl_task_wdt_set_waiting()). A
 * thread at the highest priority checks the channels every @c CONFIG_TASK_WDT_CHECK_PERIOD_MS and
 * feeds the hardware watchdog. A channel that expires is
 * logged and reported with pbl_task_wdt_stalled(), and its callback gets a chance to recover the
 * thread; once it stays expired for @c CONFIG_TASK_WDT_GRACE_MS after the check that found it
 * expired, pbl_task_wdt_expired() resets the system (with @c CONFIG_WATCHDOG; otherwise it only
 * logs). The pool holds @c CONFIG_TASK_WDT_CHANNELS channels.
 *
 * pbl_task_wdt_stalled(), pbl_task_wdt_recovered() and pbl_task_wdt_expired() have defaults an
 * application overrides to record what was stuck and to reset its own way.
 *
 * @code{.c}
 * static void prv_worker(void *arg) {
 *   int ch = pbl_task_wdt_add(NULL, CONFIG_TASK_WDT_TIMEOUT_MS, NULL, NULL);
 *
 *   while (running) {
 *     wait_for_work();
 *     do_work();
 *     pbl_task_wdt_feed(ch);
 *   }
 *
 *   pbl_task_wdt_delete(ch);
 * }
 * @endcode
 * @{
 */

/**
 * @brief Callback of an expired channel.
 *
 * Runs on the watchdog thread on every check while the channel stays expired, before the system
 * resets. It may try to unblock the watched thread.
 *
 * @param channel_id Expired channel.
 * @param user_data Data given to pbl_task_wdt_add().
 * @return Pointer naming the work the thread was running, reported with pbl_task_wdt_stalled(),
 * or NULL.
 */
typedef void *(*pbl_task_wdt_callback_t)(int channel_id, void *user_data);

/** @brief Expired channels, as reported with pbl_task_wdt_stalled(). */
struct pbl_task_wdt_stall {
  /** Channels fed in time, one bit per channel. */
  uint8_t fed_mask;
  /** Channels in use, one bit per channel. */
  uint8_t active_mask;
  /** Program counter of the highest-priority stuck thread, the most likely culprit. */
  uintptr_t pc;
  /** Link register of that thread. */
  uintptr_t lr;
  /** What its channel callback returned, or NULL. */
  void *work;
};

/**
 * @brief Report expired channels.
 *
 * Called on the watchdog thread on every check that finds channels expired. The default does
 * nothing.
 *
 * @param stall What is stuck.
 */
void pbl_task_wdt_stalled(const struct pbl_task_wdt_stall *stall);

/**
 * @brief Report that the channels found expired have all been fed again.
 *
 * Called on the watchdog thread. The default does nothing.
 */
void pbl_task_wdt_recovered(void);

/**
 * @brief Reset the system: channels stayed expired for the grace period.
 *
 * Called on the watchdog thread with interrupts locked. The default panics.
 */
[[noreturn]] void pbl_task_wdt_expired(void);

/**
 * @brief Start the watchdog thread.
 *
 * Every channel added so far starts with a full timeout.
 */
void pbl_task_wdt_init(void);

/**
 * @brief Add a channel.
 *
 * The thread must delete the channel before it exits.
 *
 * @param thread Thread to watch, NULL for the calling thread.
 * @param timeout_ms Maximum time between feeds, in milliseconds.
 * @param callback Callback when the channel expires, or NULL.
 * @param user_data Data passed to @p callback.
 * @return Channel id, or -ENOMEM when every channel is in use.
 */
int pbl_task_wdt_add(struct pbl_thread *thread, uint32_t timeout_ms,
                     pbl_task_wdt_callback_t callback, void *user_data);

/**
 * @brief Delete a channel.
 *
 * @param channel_id Channel.
 * @retval 0 Success.
 * @retval -EINVAL The channel is not in use.
 */
int pbl_task_wdt_delete(int channel_id);

/**
 * @brief Feed a channel, restarting its timeout.
 *
 * @param channel_id Channel.
 * @retval 0 Success.
 * @retval -EINVAL The channel is not in use.
 */
int pbl_task_wdt_feed(int channel_id);

/**
 * @brief Feed the channels of the calling thread, if it has any.
 */
void pbl_task_wdt_feed_self(void);

/**
 * @brief Feed the channels of a thread.
 *
 * @param thread Thread; NULL is a no-op.
 */
void pbl_task_wdt_feed_thread(struct pbl_thread *thread);

/**
 * @brief Feed every channel.
 *
 * For long operations that hold locks other watched threads wait on, such as flash erases.
 */
void pbl_task_wdt_feed_all(void);

/**
 * @brief Mark the calling thread as waiting for work, or as busy again.
 *
 * A thread blocked waiting for work cannot be stuck, so its channels do not expire while it
 * waits; going back to busy restarts their timeouts. A thread that only blocks to wait for work
 * then needs no periodic feeding. Once woken, a waiting thread that does not get to run is
 * starved: its timeouts restart from the check that first finds it runnable.
 *
 * @code{.c}
 * while (running) {
 *   pbl_task_wdt_set_waiting(true);
 *   wait_for_work();
 *   pbl_task_wdt_set_waiting(false);
 *   do_work();
 * }
 * @endcode
 *
 * @param waiting True before blocking for work, false once there is work to do.
 */
void pbl_task_wdt_set_waiting(bool waiting);

struct pbl_workq;

/**
 * @brief Add a channel watching the thread of a work queue.
 *
 * The thread waits while the queue is idle, and its channel is fed after each item, so it expires
 * when an item runs for too long or the queue does not get to run. This takes the queue's hooks.
 *
 * @param q Queue, whose thread must exist.
 * @param timeout_ms Maximum time between items, in milliseconds.
 * @param callback Callback when the channel expires, or NULL to name the function being run.
 * @param user_data Data passed to @p callback.
 * @return Channel id, -EINVAL when the queue has no thread yet, or -ENOMEM when every channel is
 *         in use.
 */
int pbl_task_wdt_add_workq(struct pbl_workq *q, uint32_t timeout_ms,
                           pbl_task_wdt_callback_t callback, void *user_data);

/**
 * @brief Keep every channel fed for a while, for phases where stalls are expected.
 *
 * A new call replaces the suspension in progress.
 *
 * @param timeout_ms Duration in milliseconds, or 0 to last until pbl_task_wdt_resume().
 */
void pbl_task_wdt_suspend(uint32_t timeout_ms);

/**
 * @brief End a suspension; every channel restarts with a full timeout.
 */
void pbl_task_wdt_resume(void);

/** @} */
