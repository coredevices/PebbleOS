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
//! The TimerService lets an app start a countdown timer. When the timer
//! expires a popup shows the timer's duration and vibrates, whether or not
//! the app is still running. The system keeps up to 8 timers at once.
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
//! @ref TimerServiceCreateErrPermissionDenied. An app can only cancel the
//! timers it created. Timers live in memory only and do not survive a reset.
//!   @{

//! @brief Maximum timer duration, in seconds.
#define TIMER_SERVICE_MAX_DURATION_S (24 * 60 * 60)

//! @brief Invalid timer ID.
#define TIMER_SERVICE_INVALID_ID (-1)

//! @brief Unique ID of a running timer.
typedef int32_t TimerServiceId;

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

//! Cancel a timer.
//!
//! Only the app that created the timer can cancel it. Cancelling does not
//! require the `Timers` permission, so an app can always stop its own timers.
//!
//! @param id ID returned by timer_service_create().
//! @return true if the timer was cancelled, false if it was not found or
//!         belongs to another app.
bool timer_service_cancel(TimerServiceId id);

//! Cancel every timer the calling app created.
//!
//! @return Number of timers cancelled.
uint32_t timer_service_cancel_all(void);

//!   @} // group TimerService
//! @} // group Foundation
