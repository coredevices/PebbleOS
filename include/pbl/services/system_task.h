/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <kernel/pebble_tasks.h>

/**
 * @defgroup services_system_task System task
 * @ingroup services
 * @brief Low priority background task (KernelBG) running deferred work.
 *
 * ISRs and higher priority tasks marshal units of work onto KernelBG by queueing callbacks, which
 * run one at a time in FIFO order. Callbacks are covered by a task watchdog; long ones must call
 * system_task_watchdog_feed() regularly. When KernelBG starves, the app task is briefly throttled.
 *
 * The enqueue variants differ in what happens when the queue is full:
 * - system_task_add_callback() waits, then resets the system if still full (apps wait forever on
 *   a separate queue),
 * - system_task_add_callback_from_isr() does not wait and resets the system,
 * - the @c _droppable variants do not wait and return false.
 *
 * @code{.c}
 * static void prv_save_cb(void *data) {
 *   // Runs on KernelBG.
 * }
 *
 * system_task_add_callback(prv_save_cb, ctx);
 * @endcode
 * @{
 */

/** @brief Create the KernelBG task. */
void system_task_init(void);

/** @brief Set up the timers used by the system task, once timers are available. */
void system_task_timer_init(void);

/**
 * @brief Feed the KernelBG watchdog.
 *
 * Call it regularly from callbacks that take a while to run.
 */
void system_task_watchdog_feed(void);

/**
 * @brief System task callback.
 *
 * @param data Context given when queueing the callback.
 */
typedef void (*SystemTaskEventCallback)(void *data);

/**
 * @brief Queue a callback from an ISR.
 *
 * Does not wait. A full queue resets the system.
 *
 * @param cb Callback to run on KernelBG.
 * @param data Context passed to @p cb.
 * @return True if queued, false if callbacks are not accepted (not initialized or blocked).
 */
bool system_task_add_callback_from_isr(SystemTaskEventCallback cb, void *data);

/**
 * @brief Queue a callback without waiting, dropping it if the queue is full.
 *
 * Callable from task or ISR context, including with interrupts locked. Never resets on failure.
 * Only use it when losing the callback is tolerable or the caller can retry later.
 *
 * @param cb Callback to run on KernelBG.
 * @param data Context passed to @p cb.
 * @return True if queued, false if the queue is full or callbacks are not accepted.
 */
bool system_task_add_callback_droppable(SystemTaskEventCallback cb, void *data);

/**
 * @brief ISR flavour of system_task_add_callback_droppable().
 *
 * @param cb Callback to run on KernelBG.
 * @param data Context passed to @p cb.
 * @return True if queued, false if the queue is full or callbacks are not accepted.
 */
bool system_task_add_callback_from_isr_droppable(SystemTaskEventCallback cb, void *data);

/**
 * @brief Queue a droppable callback that raises KernelBG priority until it has run.
 *
 * The priority is raised while the callback is pending or running, and released after it
 * returns. Nothing is retained if the callback is dropped.
 *
 * @param cb Callback to run on KernelBG.
 * @param data Context passed to @p cb.
 * @return True if queued, false if the queue is full or callbacks are not accepted.
 */
bool system_task_add_callback_from_isr_droppable_raised(SystemTaskEventCallback cb, void *data);

/**
 * @brief Queue a callback from task context.
 *
 * From the app task, waits for space as long as needed. From other tasks, waits up to 3 seconds,
 * then resets the system.
 *
 * @param cb Callback to run on KernelBG.
 * @param data Context passed to @p cb.
 * @return True if queued, false if callbacks are not accepted (not initialized or blocked).
 */
bool system_task_add_callback(SystemTaskEventCallback cb, void *data);

/**
 * @brief Block or unblock new callbacks.
 *
 * @param block True to reject new callbacks, false to accept them.
 */
void system_task_block_callbacks(bool block);

/**
 * @brief Get the free space of the queue used by the calling task.
 *
 * @return Number of callbacks that can be queued before the queue is full.
 */
uint32_t system_task_get_available_space(void);

/**
 * @brief Get the callback being run, for debugging.
 *
 * @return Running callback, or NULL when idle.
 */
void *system_task_get_current_callback(void);

/**
 * @brief Acquire or release a reference raising KernelBG priority.
 *
 * While at least one reference is held, KernelBG runs at the priority of KernelMain.
 *
 * @param is_raised True to acquire a reference, false to release one. Calls must be balanced.
 */
void system_task_enable_raised_priority(bool is_raised);

/**
 * @brief Check whether KernelBG is ready to run.
 *
 * @return True if ready, false if blocked (e.g. on a mutex or its queue).
 */
bool system_task_is_ready_to_run(void);

/** @} */
