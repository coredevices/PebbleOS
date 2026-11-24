/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/util/uuid.h>

/**
 * @defgroup services_timers Timers
 * @ingroup services
 * @brief Disposable countdown timers created by apps.
 *
 * Timers live in memory only: they are gone after a reset. Each timer records
 * the UUID of the app that created it so only that app can cancel it. When a
 * timer expires a @c PEBBLE_TIMER_EVENT is put and the timer popup shows the
 * timer's duration.
 *
 * @{
 */

/** @brief Maximum timer duration, in seconds. */
#define TIMER_MAX_DURATION_S (24 * 60 * 60)

/** @brief Number of timers that can run at once. */
#define TIMER_MAX_COUNT 8

/** @brief Unique ID of a running timer. */
typedef int32_t TimerId;

/** @brief Invalid timer ID, returned on failure. */
#define TIMER_INVALID_ID (-1)

/** @brief Initialize the timer service. */
void timer_init(void);

/** @brief Check whether a timer slot is free. */
bool timer_can_create(void);

/**
 * @brief Start a countdown timer.
 *
 * @param duration_s Duration, in seconds.
 * @param owner App that owns the timer, which is the only one allowed to cancel it.
 * @return The timer ID, or @ref TIMER_INVALID_ID if the duration is invalid or no slot is free.
 */
TimerId timer_start(uint32_t duration_s, const Uuid *owner);

/**
 * @brief Cancel a timer.
 *
 * @param id Timer ID.
 * @param owner App that owns the timer.
 * @return true if the timer was cancelled, false if it was not found or belongs to another app.
 */
bool timer_cancel(TimerId id, const Uuid *owner);

/**
 * @brief Cancel every timer owned by an app.
 *
 * @param owner App that owns the timers.
 * @return Number of timers cancelled.
 */
uint32_t timer_cancel_all(const Uuid *owner);

/** @} // group services_timers */
