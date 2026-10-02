/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "pbl/services/new_timer/new_timer.h"

/**
 * @defgroup services_wakeup Wakeup
 * @ingroup services
 * @brief Scheduled app launches.
 *
 * Kernel side of the app wakeup API. Events are stored in the "wakeup" settings file and the
 * next one is scheduled with a new_timer. When an event fires, its app is launched with
 * @c APP_LAUNCH_WAKEUP, or receives a @c PEBBLE_WAKEUP_EVENT if already running. Events missed
 * while the watch was off can be reported with a popup.
 * @{
 */

/**
 * @brief Time reserved around each event, in seconds, in which no other event can be scheduled.
 */
#define WAKEUP_EVENT_WINDOW 60
/** @brief Maximum number of events per app. */
#define MAX_WAKEUP_EVENTS_PER_APP 8
/**
 * @brief Reduced gap, in seconds, between events caught up after a time change or after the
 * service was disabled (e.g. low power mode).
 */
#define WAKEUP_CATCHUP_WINDOW (WAKEUP_EVENT_WINDOW / 2)

/** @brief WakeupId is an identifier for a wakeup event */
typedef int32_t WakeupId;

/** @brief Wakeup event passed to the app it launches. */
typedef struct {
  /** Event identifier, its scheduling timestamp. */
  WakeupId wakeup_id;
  /** Reason given by the app when scheduling the event. */
  int32_t wakeup_reason;
} WakeupInfo;

/**
 * @brief Initialize the service.
 *
 * Deletes expired events, shows a popup for apps that missed an event while the watch was off
 * and asked to be notified, and schedules the next event.
 */
void wakeup_init(void);

/**
 * @brief Enable or disable the service.
 *
 * While disabled no event fires; pending ones are caught up when re-enabled.
 *
 * @param enabled True to enable.
 */
void wakeup_enable(bool enabled);

/**
 * @brief Get the timer used to schedule events, for tests.
 *
 * @return Timer identifier.
 */
TimerID wakeup_get_current(void);

/**
 * @brief Get the next scheduled event, for tests.
 *
 * @return Event identifier.
 */
WakeupId wakeup_get_next_scheduled(void);

/**
 * @brief Convert events scheduled in local time to UTC after the timezone was set.
 *
 * @param utc_diff Local time minus UTC, in seconds.
 */
void wakeup_migrate_timezone(int utc_diff);

/**
 * @brief Handle a significant time change (over 15 s, timezone or DST).
 *
 * Deletes past events, shows missed event popups and reschedules, on KernelBG.
 */
void wakeup_handle_significant_clock_change(void);

/**
 * @brief Handle any time change, including small RTC calibrations.
 *
 * Reschedules the next event without deleting events or showing popups.
 */
void wakeup_handle_clock_change(void);

/** @} */
