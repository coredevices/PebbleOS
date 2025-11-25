/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

//! @addtogroup Foundation
//! @{
//!   @addtogroup Timer
//!   \brief Can be used to execute some code at some point in the future.
//!   @{

//! An opaque handle to a timer
struct AppTimer;
typedef struct AppTimer AppTimer;

//! The type of function which can be called when a timer fires.  The argument will be the @p callback_data passed to
//! @ref app_timer_register().
typedef void (*AppTimerCallback)(void* data);

//! Registers a timer that ends up in callback being called some specified time in the future.
//! @param timeout_ms The expiry time in milliseconds from the current time
//! @param callback The callback that gets called at expiry time
//! @param callback_data The data that will be passed to callback
//! @return A pointer to an `AppTimer` that can be used to later reschedule or cancel this timer
AppTimer* app_timer_register(uint32_t timeout_ms, AppTimerCallback callback, void* callback_data);

//! @internal
//! Registers a timer that ends up in callback being called repeatedly at a specified interval
//! @param timeout_ms The interval time in milliseconds from the current time
//! @param callback The callback that gets called at every interval
//! @param callback_data The data that will be passed to callback
//! @return A pointer to an `AppTimer` that can be used to later reschedule or cancel this timer
AppTimer* app_timer_register_repeatable(uint32_t timeout_ms,
                                        AppTimerCallback callback,
                                        void* callback_data,
                                        bool repeating);

//! @internal
//! Get the data passed to the app timer
void *app_timer_get_data(AppTimer *timer);

//! Reschedules an already running timer for some point in the future.
//! @param timer_handle The timer to reschedule
//! @param new_timeout_ms The new expiry time in milliseconds from the current time
//! @return true if the timer was rescheduled, false if the timer has already elapsed
bool app_timer_reschedule(AppTimer *timer_handle, uint32_t new_timeout_ms);

//! Cancels an already registered timer.
//! Once cancelled the handle may no longer be used for any purpose.
void app_timer_cancel(AppTimer *timer_handle);

//!   @} // group Timer
//! @} // group Foundation

