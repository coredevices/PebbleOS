/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <kernel/pebble_tasks.h>

/**
 * @defgroup services_sys_work Background work queue
 * @ingroup services
 * @brief Callbacks with a payload on the system work queues, and the policy around them.
 *
 * The kernel runs @ref pbl_sys_workq and @ref pbl_sys_workq_high. This service adds what is the
 * system's own: a watchdog on both, throttling the app while the background queue starves, a slot
 * pool apps cannot exhaust for the system, and what to do when a queue is full. New code with a
 * fixed payload should embed a work item instead, with the kernel work API.
 *
 * Callbacks run one at a time in FIFO order. Long ones on the background queue must call
 * sys_work_watchdog_feed() regularly. The enqueue variants differ in what happens when the
 * background queue is full:
 * - sys_work_add() waits, then resets the system if still full (apps wait forever on separate
 *   slots),
 * - sys_work_add_from_isr() does not wait and resets the system,
 * - the sys_work_try_add() variants do not wait and return false.
 *
 * @code{.c}
 * static void prv_save_cb(void *data) {
 *   // Runs on SysWorkq.
 * }
 *
 * sys_work_add(prv_save_cb, ctx);
 * @endcode
 * @{
 */

/** @brief Attach the watchdog and hooks to the system work queues. */
void sys_work_init(void);

/**
 * @brief Feed the SysWorkq watchdog.
 *
 * Call it regularly from callbacks that take a while to run.
 */
void sys_work_watchdog_feed(void);

/**
 * @brief Background work queue callback.
 *
 * @param data Context given when queueing the callback.
 */
typedef void (*SysWorkCallback)(void *data);

/**
 * @brief Queue a callback from an ISR.
 *
 * Does not wait. A full queue resets the system.
 *
 * @param cb Callback to run on SysWorkq.
 * @param data Context passed to @p cb.
 * @return True if queued, false if callbacks are not accepted (not initialized or blocked).
 */
bool sys_work_add_from_isr(SysWorkCallback cb, void *data);

/**
 * @brief Queue a callback without waiting, dropping it if the queue is full.
 *
 * Callable from task or ISR context, including with interrupts locked. Never resets on failure.
 * Only use it when losing the callback is tolerable or the caller can retry later.
 *
 * @param cb Callback to run on SysWorkq.
 * @param data Context passed to @p cb.
 * @return True if queued, false if the queue is full or callbacks are not accepted.
 */
bool sys_work_try_add(SysWorkCallback cb, void *data);

/**
 * @brief Queue a short, time sensitive callback on @ref pbl_sys_workq_high.
 *
 * Waits up to 50 ticks for a free slot.
 *
 * @param cb Callback.
 * @param data Context passed to @p cb.
 * @return True if queued, false if no slot became free.
 */
bool sys_work_add_high(SysWorkCallback cb, void *data);

/**
 * @brief Queue a short, time sensitive callback on @ref pbl_sys_workq_high from an ISR.
 *
 * The callback is dropped if no slot is free.
 *
 * @param cb Callback.
 * @param data Context passed to @p cb.
 */
void sys_work_add_high_from_isr(SysWorkCallback cb, void *data);

/**
 * @brief Queue a droppable callback that raises SysWorkq priority until it has run.
 *
 * The priority is raised while the callback is pending or running, and released after it
 * returns. Nothing is retained if the callback is dropped.
 *
 * @param cb Callback to run on SysWorkq.
 * @param data Context passed to @p cb.
 * @return True if queued, false if the queue is full or callbacks are not accepted.
 */
bool sys_work_try_add_raised(SysWorkCallback cb, void *data);

/**
 * @brief Queue a callback from task context.
 *
 * From the app task, waits for space as long as needed. From other tasks, waits up to 3 seconds,
 * then resets the system.
 *
 * @param cb Callback to run on SysWorkq.
 * @param data Context passed to @p cb.
 * @return True if queued, false if callbacks are not accepted (not initialized or blocked).
 */
bool sys_work_add(SysWorkCallback cb, void *data);

/**
 * @brief Block or unblock new callbacks.
 *
 * @param block True to reject new callbacks, false to accept them.
 */
void sys_work_block(bool block);

/**
 * @brief Get the free space of the queue used by the calling task.
 *
 * @return Number of callbacks that can be queued before the queue is full.
 */
uint32_t sys_work_get_available_space(void);

/**
 * @brief Get the callback being run, for debugging.
 *
 * @return Running callback, or NULL when idle.
 */
void *sys_work_get_current_callback(void);

/**
 * @brief Acquire or release a reference raising SysWorkq priority.
 *
 * While at least one reference is held, SysWorkq runs at the priority of KernelMain.
 *
 * @param is_raised True to acquire a reference, false to release one. Calls must be balanced.
 */
void sys_work_enable_raised_priority(bool is_raised);

/**
 * @brief Check whether SysWorkq is ready to run.
 *
 * @return True if ready, false if blocked (e.g. on a mutex or its queue).
 */
bool sys_work_is_ready_to_run(void);

/** @} */
