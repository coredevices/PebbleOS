/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "event.h"

/**
 * @defgroup services_timeline_calendar Calendar events
 * @ingroup services_timeline
 * @brief Tracks whether a calendar event is ongoing.
 *
 * Posts a @c PEBBLE_CALENDAR_EVENT telling whether one or more calendar events (non all-day
 * calendar pins) are ongoing. Not every event start or end produces one, but every transition
 * does.
 * @{
 */

/**
 * @brief Get the timeline event service callbacks of the calendar.
 *
 * @return Callbacks.
 */
const TimelineEventImpl *calendar_get_event_service(void);

/**
 * @brief Check whether a calendar event is ongoing, for smart Do Not Disturb.
 *
 * @return true if one is ongoing.
 */
bool calendar_event_is_ongoing(void);

#if UNITTEST
#include "pbl/services/new_timer/new_timer.h"
/** @brief Get the calendar timer. Unit tests only. */
TimerID get_calendar_timer_id(void);
/** @brief Set the calendar timer. Unit tests only. */
void set_calendar_timer_id(TimerID id);
#endif

/** @} */
