/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "services/common/new_timer/new_timer.h"

//! @internal
//! Event window is (in seconds) a reserved amount of time each wakeup_event receives
//! in which other wakeup events cannot be scheduled
#define WAKEUP_EVENT_WINDOW 60 
//! @internal
//! Number of wakeup events allowed per application (UUID)
#define MAX_WAKEUP_EVENTS_PER_APP 8
//! @internal
//! Reduced event window or gap for catching up on missed events due to a time change
//! or the service being disabled by the system (Power saving mode).
#define WAKEUP_CATCHUP_WINDOW (WAKEUP_EVENT_WINDOW / 2)

//! WakeupId is an identifier for a wakeup event
typedef int32_t WakeupId;

//! WakeupInfo is used to pass the wakeup event id and reason
//! to the application that requested the wakeup event
typedef struct {
  WakeupId wakeup_id;     //!< Identifier (Timestamp) of the wakeup event
  int32_t wakeup_reason;  //!< App provided reason for the wakeup event
} WakeupInfo;

//! @internal
//! This function initializes the wakeup service.  
//! Triggers a popup notification for any apps that missed a 
//! wakeup_event while the Pebble was off and specified 
//! notify_if_missed while scheduling the event.
//! Deletes all expired wakeup_events from "wakeup" settings_file and
//! schedules the next wakeup_event using a new_timer
void wakeup_init(void);

//! @internal
//! This function enables and disables the wakeup service.
void wakeup_enable(bool enabled);

//! @internal
//! This function enables unit testing of the current wakeup event
TimerID wakeup_get_current(void);

//! @internal
//! This function is used for testing and gets the next scheduled wakeup id
WakeupId wakeup_get_next_scheduled(void);

//! @internal
//! This function is used for migrating wakeup events after a timezone set
void wakeup_migrate_timezone(int utc_diff);

//! @internal
////! This function is used for updating wakeup events after a time change
void wakeup_handle_clock_change(void);
