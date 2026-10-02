/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "event.h"
#include "pbl/util/units.h"

/**
 * @defgroup services_timeline_peek Timeline Peek events
 * @ingroup services_timeline
 * @brief Selects the event shown by Timeline Peek.
 *
 * Posts a @c PEBBLE_TIMELINE_PEEK_EVENT with the next or current event and its
 * TimelinePeekTimeType whenever that changes.
 * @{
 */

/** @brief Default time before an event starts at which Timeline Peek shows it; configurable. */
#define TIMELINE_PEEK_DEFAULT_SHOW_BEFORE_TIME_S (10 * PBL_SEC_PER_MIN)

/**
 * @brief Time after an event starts at which Timeline Peek hides it, unless it is persistent; not
 * user configurable.
 */
#define TIMELINE_PEEK_HIDE_AFTER_TIME_S (10 * PBL_SEC_PER_MIN)

/** @brief Relation between now and the event of a Timeline Peek event. */
typedef enum TimelinePeekTimeType {
  /** No event. */
  TimelinePeekTimeType_None = 0,
  /** The event is next but not imminent: more than the show-before time away. */
  TimelinePeekTimeType_SomeTimeNext,
  /** The event starts within the show-before time and should be shown. */
  TimelinePeekTimeType_ShowWillStart,
  /**
   * The event started less than @ref TIMELINE_PEEK_HIDE_AFTER_TIME_S ago, or is persistent, and
   * should be shown.
   */
  TimelinePeekTimeType_ShowStarted,
  /** The event is ongoing and started at least @ref TIMELINE_PEEK_HIDE_AFTER_TIME_S ago. */
  TimelinePeekTimeType_WillEnd,
} TimelinePeekTimeType;

/**
 * @brief Get the timeline event service callbacks of Timeline Peek.
 *
 * @return Callbacks.
 */
const TimelineEventImpl *timeline_peek_get_event_service(void);

/**
 * @brief Set how long before an event starts Timeline Peek shows it, and refresh.
 *
 * @param before_time_s Time in seconds.
 */
void timeline_peek_set_show_before_time(unsigned int before_time_s);

/** @} */
