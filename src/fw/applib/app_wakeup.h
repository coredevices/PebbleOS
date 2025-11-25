/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once


#include <stdbool.h>
#include <stdint.h>

#include "util/time/time.h"
#include "services/normal/wakeup.h"

//! @addtogroup Foundation
//! @{
//!   @addtogroup Wakeup
//!   \brief Allows applications to schedule to be launched even if they are not running.
//!   @{


//! The type of function which can be called when a wakeup event occurs.  
//! The arguments will be the id of the wakeup event that occurred, 
//! as well as the scheduled cookie provided to \ref wakeup_schedule.
typedef void (*WakeupHandler)(WakeupId wakeup_id, int32_t cookie);

//! Registers a WakeupHandler to be called when wakeup events occur.
//! @param handler The callback that gets called when the wakeup event occurs
void app_wakeup_service_subscribe(WakeupHandler handler);

//! Registers a wakeup event that triggers a callback at the specified time.
//! Applications may only schedule up to 8 wakeup events.
//! Wakeup events are given a 1 minute duration window, in that no application may schedule a 
//! wakeup event with 1 minute of a currently scheduled wakeup event.
//! @param timestamp The requested time (UTC) for the wakeup event to occur
//! @param cookie The application specific reason for the wakeup event
//! @param notify_if_missed On powering on Pebble, will alert user when 
//! notifications were missed due to Pebble being off.
//! @return negative values indicate errors (StatusCode)
//! E_RANGE if the event cannot be scheduled due to another event in that period.
//! E_INVALID_ARGUMENT if the time requested is in the past.
//! E_OUT_OF_RESOURCES if the application has already scheduled all 8 wakeup events.
//! E_INTERNAL if a system error occurred during scheduling.
WakeupId app_wakeup_schedule(time_t timestamp, int32_t cookie, bool notify_if_missed);

//! Cancels a wakeup event.
//! @param wakeup_id Wakeup event to cancel
void app_wakeup_cancel(WakeupId wakeup_id);

//! Cancels all wakeup event for the app.
void app_wakeup_cancel_all(void);

//! Retrieves the wakeup event info for an app that was launched
//! by a wakeup_event (ie. \ref launch_reason() === APP_LAUNCH_WAKEUP)
//! so that an app may display information regarding the wakeup event
//! @param wakeup_id WakeupId for the wakeup event that caused the app to wakeup
//! @param cookie App provided reason for the wakeup event
//! @return True if app was launched due to a wakeup event, false otherwise
bool app_wakeup_get_launch_event(WakeupId *wakeup_id, int32_t *cookie);

//! Checks if the current WakeupId is still scheduled and therefore valid
//! @param wakeup_id Wakeup event to query for validity and scheduled time
//! @param timestamp Optionally points to an address of a time_t variable to
//! store the time that the wakeup event is scheduled to occur.
//! (The time is in UTC, but local time when \ref clock_is_timezone_set
//! returns false).
//! You may pass NULL instead if you do not need it.
//! @return True if WakeupId is still scheduled, false if it doesn't exist or has
//! already occurred
bool app_wakeup_query(WakeupId wakeup_id, time_t *timestamp);

//!   @} // group Wakeup
//! @} // group Foundation

