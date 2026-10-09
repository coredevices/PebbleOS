/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

/**
 * @defgroup services_new_timer New timer
 * @ingroup services
 * @brief Timers and deferred work run on the high priority NewTimer task.
 *
 * NewTimer runs at the highest task priority and executes timer callbacks and work deferred from
 * interrupts by drivers. Callbacks must be short: anything long should be handed to another task,
 * e.g. with an evented timer or a system task callback. Timers are a wrapper around the kernel
 * task timers.
 *
 * @code{.c}
 * static TimerID s_timer;
 *
 * static void timeout_cb(void *data) {
 *   // Runs on NewTimer.
 * }
 *
 * s_timer = new_timer_create();
 * new_timer_start(s_timer, 500, timeout_cb, NULL, TIMER_START_FLAG_REPEATING);
 * // ...
 * new_timer_stop(s_timer);
 * new_timer_delete(s_timer);
 * @endcode
 * @{
 */

/**
 * @brief Timer callback, run on the NewTimer task.
 *
 * @param data Data passed to new_timer_start().
 */
typedef void (*NewTimerCallback)(void *data);

/** @brief Timer handle; ids are used instead of pointers to avoid use-after-free. */
typedef uint32_t TimerID;
/** @brief Invalid timer id, never returned by new_timer_create(). */
#define TIMER_INVALID_ID 0

/** @brief Re-arm the timer with the same timeout after each expiry. */
#define TIMER_START_FLAG_REPEATING 0x01
/**
 * @brief Fail if the callback is executing.
 *
 * Neither schedule the timer nor wait; new_timer_start() returns false. Useful when the callback
 * may be blocked on a semaphore owned by the task issuing the start.
 */
#define TIMER_START_FLAG_FAIL_IF_EXECUTING 0x02
/** @brief Fail if the timer is already scheduled, instead of rescheduling it. */
#define TIMER_START_FLAG_FAIL_IF_SCHEDULED 0x04

/**
 * @brief Create a timer, initially stopped.
 *
 * Timers come from a fixed pool; running out of timers asserts.
 *
 * @return Non-zero timer id.
 */
TimerID new_timer_create(void);

/**
 * @brief Schedule a timer.
 *
 * A timer that is already scheduled is rescheduled for the new time, unless
 * @ref TIMER_START_FLAG_FAIL_IF_SCHEDULED is given.
 *
 * @param timer Timer id.
 * @param timeout_ms Timeout in milliseconds.
 * @param cb Callback.
 * @param cb_data Data passed to @p cb.
 * @param flags Zero or more TIMER_START_FLAG_ values.
 * @return true on success; false is only returned when one of the FAIL_IF flags applies.
 */
bool new_timer_start(TimerID timer, uint32_t timeout_ms, NewTimerCallback cb, void *cb_data,
                     uint32_t flags);

/**
 * @brief Stop a timer.
 *
 * Safe to call on a timer that is not scheduled. A repeating timer does not run again even if
 * its callback is executing.
 *
 * @param timer Timer id.
 * @return false if the callback is executing, true otherwise.
 */
bool new_timer_stop(TimerID timer);

/**
 * @brief Check whether a timer is scheduled.
 *
 * @param timer Timer id.
 * @param[out] expire_ms_p If not NULL, milliseconds until the timer fires; only valid when
 * scheduled.
 * @return true if the timer is scheduled.
 */
bool new_timer_scheduled(TimerID timer, uint32_t *expire_ms_p);

/**
 * @brief Delete a timer, stopping it first.
 *
 * If the callback is executing, the timer is freed after it returns.
 *
 * @param timer Timer id.
 */
void new_timer_delete(TimerID timer);

/**
 * @brief Get the callback being executed, for the watchdog.
 *
 * @return Running timer or work callback, or NULL.
 */
void *new_timer_debug_get_current_callback(void);

/**
 * @brief Work callback, run on the NewTimer task.
 *
 * @param data Data passed when queuing the work.
 */
typedef void (*NewTimerWorkCallback)(void *data);

/**
 * @brief Queue work on the NewTimer task from an ISR.
 *
 * Used to handle time sensitive hardware events. The work is dropped if the queue is full.
 *
 * @param cb Work callback.
 * @param data Data passed to @p cb.
 */
void new_timer_add_work_callback_from_isr(NewTimerWorkCallback cb, void *data);

/**
 * @brief Queue work on the NewTimer task.
 *
 * Waits up to 50 ticks for space in the queue.
 *
 * @param cb Work callback.
 * @param data Data passed to @p cb.
 * @return true if queued, false if the queue stayed full.
 */
bool new_timer_add_work_callback(NewTimerWorkCallback cb, void *data);

/** @} */
