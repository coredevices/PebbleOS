/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <pbl/util/units.h>
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

/** @brief Size of a timer duration description buffer. */
#define TIMER_DESCRIPTION_SIZE 16

/** @brief Unique ID of a running timer. */
typedef int32_t TimerId;

/** @brief Invalid timer ID, returned on failure. */
#define TIMER_INVALID_ID (-1)

/** @brief A running timer, as returned by timer_get_timers(). */
typedef struct TimerInfo {
  /** Timer ID. */
  TimerId id;
  /** Original duration, in seconds. */
  uint32_t duration_s;
  /** Seconds left until the timer fires. */
  uint32_t remaining_s;
  /** Original duration formatted for display, e.g. "5:00" or "1:00:00". */
  char description[TIMER_DESCRIPTION_SIZE];
} TimerInfo;

/** @brief Format a timer duration for display, e.g. "5:00" or "1:00:00". */
static inline void timer_format_duration(char *buffer, size_t buffer_size, uint32_t duration_s) {
  const uint32_t hours = duration_s / PBL_SEC_PER_HOUR;
  const uint32_t minutes = (duration_s % PBL_SEC_PER_HOUR) / PBL_SEC_PER_MIN;
  const uint32_t seconds = duration_s % PBL_SEC_PER_MIN;
  if (hours > 0) {
    snprintf(buffer, buffer_size, "%u:%02u:%02u", (unsigned int)hours, (unsigned int)minutes,
             (unsigned int)seconds);
  } else {
    snprintf(buffer, buffer_size, "%u:%02u", (unsigned int)minutes, (unsigned int)seconds);
  }
}

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

/**
 * @brief Copy an app's running timers, soonest expiry first.
 *
 * Timers being cancelled are not returned.
 *
 * @param owner App that owns the timers.
 * @param[out] out Buffer for the timers.
 * @param max Maximum number of timers to write.
 * @return Number of timers written.
 */
uint8_t timer_get_timers(const Uuid *owner, TimerInfo *out, uint8_t max);

/** @} // group services_timers */
