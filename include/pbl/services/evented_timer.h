/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <kernel/pebble_tasks.h>

/**
 * @defgroup services_evented_timer Evented timers
 * @ingroup services
 * @brief Timers whose callbacks run on the registering task's event loop.
 *
 * Callbacks run as events on the task that registered the timer (KernelMain, App or Worker), so
 * they need no locking against the rest of that task's code. Timers are backed by new timers;
 * when they fire, a callback event is posted to the owning task. A one-shot timer is freed right
 * before its callback runs.
 *
 * @code{.c}
 * static EventedTimerID s_timer = EVENTED_TIMER_INVALID_ID;
 *
 * static void prv_timeout(void *data) {
 *   s_timer = EVENTED_TIMER_INVALID_ID;
 *   // runs on the task that registered the timer
 * }
 *
 * // Start, or push back if already running.
 * s_timer = evented_timer_register_or_reschedule(s_timer, 500, prv_timeout, NULL);
 *
 * // Stop it.
 * evented_timer_cancel(s_timer);
 * s_timer = EVENTED_TIMER_INVALID_ID;
 * @endcode
 * @{
 */

/** @brief Evented timer handle. */
typedef uintptr_t EventedTimerID;
/** @brief Invalid timer handle. */
#define EVENTED_TIMER_INVALID_ID 0

/**
 * @brief Timer callback, run on the task that registered the timer.
 *
 * @param data Data given when registering the timer.
 */
typedef void (*EventedTimerCallback)(void *data);

/** @brief Initialize the evented timer service, once at startup. */
void evented_timer_init(void);

/**
 * @brief Cancel all timers of a task, without calling their callbacks.
 *
 * Called by the kernel on KernelMain when a process exits.
 *
 * @param task Task whose timers are cancelled.
 */
void evented_timer_clear_process_timers(PebbleTask task);

/**
 * @brief Start a timer.
 *
 * Must be called from KernelMain, App or Worker.
 *
 * @param timeout_ms Delay in milliseconds; 0 is treated as 1.
 * @param repeating true to fire every @p timeout_ms until cancelled.
 * @param callback Callback to run.
 * @param callback_data Data passed to @p callback.
 * @return Timer handle.
 */
EventedTimerID evented_timer_register(uint32_t timeout_ms, bool repeating,
                                      EventedTimerCallback callback, void *callback_data);

/**
 * @brief Restart a pending timer with a new timeout.
 *
 * Must be called from the task that registered the timer.
 *
 * @param timer Timer to reschedule.
 * @param new_timeout_ms New delay in milliseconds, from now; 0 is treated as 1.
 * @return true on success, false if the timer does not exist or has already fired.
 */
bool evented_timer_reschedule(EventedTimerID timer, uint32_t new_timeout_ms);

/**
 * @brief Reschedule a timer, or register a new one-shot timer if that is not possible.
 *
 * @param timer_id Timer to reschedule, or @ref EVENTED_TIMER_INVALID_ID.
 * @param timeout_ms Delay in milliseconds.
 * @param callback Callback of the new timer.
 * @param data Data passed to @p callback by the new timer.
 * @return @p timer_id if it was rescheduled, else the handle of the new timer.
 */
EventedTimerID evented_timer_register_or_reschedule(EventedTimerID timer_id, uint32_t timeout_ms,
                                                    EventedTimerCallback callback, void *data);

/**
 * @brief Cancel a timer.
 *
 * The callback will not run, even if the timer has already fired. No-op if @p timer is
 * @ref EVENTED_TIMER_INVALID_ID or no longer exists.
 *
 * @param timer Timer to cancel.
 */
void evented_timer_cancel(EventedTimerID timer);

/**
 * @brief Check whether a timer exists.
 *
 * @param timer Timer to check.
 * @return true if the timer is pending or its callback has not run yet.
 */
bool evented_timer_exists(EventedTimerID timer);

/**
 * @brief Check whether a timer belongs to the current task.
 *
 * @param timer Existing timer; asserts otherwise.
 * @return true if the timer was registered by the current task.
 */
bool evented_timer_is_current_task(EventedTimerID timer);

/** @brief Forget all timers, without freeing them. Only for unit tests. */
void evented_timer_reset(void);

/**
 * @brief Get the callback data of a timer.
 *
 * @param timer Timer.
 * @return Data given when registering the timer, or NULL if the timer does not exist.
 */
void *evented_timer_get_data(EventedTimerID timer);

/** @} */
