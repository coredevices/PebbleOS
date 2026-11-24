/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

//! @addtogroup Foundation
//! @{
//!   @addtogroup TimerService
//! \brief Disposable countdown timers.
//!
//! The TimerService lets an app start a countdown timer, list every running
//! timer, and cancel timers. When the timer expires a popup shows the timer's
//! duration and vibrates, whether or not the app is still running. The system
//! keeps up to @ref TIMER_SERVICE_MAX_TIMERS timers at once.
//!
//! Creating a timer requires the `Timers` permission. Declare it in the
//! `pebble` block of `package.json`:
//!
//! ```json
//! "usesPermissions": ["Timers"]
//! ```
//!
//! The phone app shows the permission at install time and syncs the user's
//! decision to the watch. Apps without the permission get
//! @ref TimerServiceCreateErrPermissionDenied. The permission also lets an
//! app list every running timer and cancel any timer by ID;
//! timer_service_cancel_all() only stops the calling app's timers and works
//! without the permission. Timers live in memory only and do not survive a
//! reset.
//!   @{

//! @brief Maximum timer duration, in seconds.
#define TIMER_SERVICE_MAX_DURATION_S (24 * 60 * 60)

//! @brief Maximum number of timers that can run at once.
#define TIMER_SERVICE_MAX_TIMERS 8

//! @brief Size of a timer duration description buffer.
#define TIMER_SERVICE_DESCRIPTION_SIZE 16

//! @brief Invalid timer ID.
#define TIMER_SERVICE_INVALID_ID (-1)

//! @brief Unique ID of a running timer.
typedef int32_t TimerServiceId;

//! @brief A running timer, as returned by timer_service_get_timers().
typedef struct TimerServiceInfo {
  //! Timer ID.
  TimerServiceId id;
  //! Original duration, in seconds.
  uint32_t duration_s;
  //! Seconds left until the timer fires.
  uint32_t remaining_s;
  //! Original duration formatted for display, e.g. "5:00" or "1:00:00".
  char description[TIMER_SERVICE_DESCRIPTION_SIZE];
} TimerServiceInfo;

//! @brief Result of timer_service_create().
typedef enum TimerServiceCreateResult {
  //! The timer was started.
  TimerServiceCreateOk = 0,
  //! The app does not have the `Timers` permission.
  TimerServiceCreateErrPermissionDenied,
  //! The duration is zero or longer than @ref TIMER_SERVICE_MAX_DURATION_S.
  TimerServiceCreateErrInvalidArgument,
  //! All timer slots are in use.
  TimerServiceCreateErrNoSpace,
  //! The platform does not support timers.
  TimerServiceCreateErrUnavailable,
  //! The timer could not be started.
  TimerServiceCreateErrInternal,
} TimerServiceCreateResult;

//! Check whether a timer slot is free.
//!
//! This only answers the capacity question. It does not check the `Timers`
//! permission, and the result is advisory: timer_service_create() must still
//! handle @ref TimerServiceCreateErrNoSpace.
//!
//! @return true if at least one timer slot is available.
bool timer_service_can_create(void);

//! Start a countdown timer.
//!
//! @param duration_s Duration, in seconds, between 1 and
//!        @ref TIMER_SERVICE_MAX_DURATION_S.
//! @param[out] id_out Set to the ID of the new timer on success. Set to
//!             @ref TIMER_SERVICE_INVALID_ID on failure.
//! @return The result of the operation.
TimerServiceCreateResult timer_service_create(uint32_t duration_s, TimerServiceId *id_out);

//! Copy every running timer into a buffer, soonest expiry first.
//!
//! The app needs the `Timers` permission. This is how an app discovers
//! timers created earlier, including timers created by other apps, for
//! example after being relaunched. Timers being cancelled are not returned.
//! Each entry's @ref TimerServiceInfo::description is the original duration
//! formatted for display, which the app can show to help the wearer pick the
//! timer to cancel.
//!
//! @param[out] timers Buffer for the timers.
//! @param max_timers Maximum number of timers to write, usually
//!        @ref TIMER_SERVICE_MAX_TIMERS.
//! @return Number of timers written.
uint8_t timer_service_get_timers(TimerServiceInfo *timers, uint8_t max_timers);

//! Cancel a timer.
//!
//! The app needs the `Timers` permission. Any app with the permission can
//! cancel any timer, including one created by another app.
//!
//! @param id ID returned by timer_service_create() or
//!        timer_service_get_timers().
//! @return true if the timer was cancelled, false if it was not found.
bool timer_service_cancel(TimerServiceId id);

//! Cancel every timer the calling app created.
//!
//! This only affects the calling app's timers and does not need the `Timers`
//! permission, so an app can always clean up after itself. Use
//! timer_service_cancel() with IDs from timer_service_get_timers() to stop
//! another app's timer.
//!
//! @return Number of timers cancelled.
uint32_t timer_service_cancel_all(void);

//!   @} // group TimerService
//! @} // group Foundation
