/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/normal/blob_db/pin_db.h"

#define TIMELINE_EVENT_DELTA_INFINITE (INT32_MAX)

typedef enum TimelineEventService {
  TimelineEventService_Calendar,
  TimelineEventService_Peek,

  TimelineEventServiceCount,
} TimelineEventService;

//! Called before filtering and updating begins. A double pointer user context is passed that may
//! be used to setup state necessary for filtering and updating.
//! @param context Double pointer to a user context.
typedef void (*TimelineEventWillUpdateCallback)(void **context);

//! Called for every timeline event header for filter. Events that are meant to be considered for
//! being passed to the update callback should return true. The event with the earliest timestamp
//! will be passed to the update callback. This means only one event is passed along.
//! @param header The header of the timeline event for consideration.
//! @param context Double pointer to a user context. The same double pointer is passed to the
//! update callback, so the user context pointer can be set or unset to affect the update callback.
//! @return true to consider the passed event as the one to update with, false otherwise
typedef bool (*TimelineEventFilterCallback)(SerializedTimelineItemHeader *header, void **context);

//! Called after filtering and after updating. A double pointer user context is passed that may
//! have been setup other callbacks. This callback can be used to teardown any such state that was
//! setup.
//! @param context Double pointer to a user context.
typedef void (*TimelineEventDidUpdateCallback)(void **context);

//! Called when more than one item passed into filtering. Determines which item should remain.
//! @param new_header The serialized header of the item that newly passed filtering.
//! @param old_header The serialized header of the item that already passed filtering.
//! @param context Double pointer to a user context.
//! @return < 0 to replace with `new_header`, > 0 to keep old_header, or 0 to use either one.
typedef int (*TimelineEventComparator)(SerializedTimelineItemHeader *new_header,
                                       SerializedTimelineItemHeader *old_header,
                                       void **context);

//! Called with the nearest filtered event if any. If there was no filtered event, the update
//! callback will be called with NULL. The update callback can optionally return a timeout until
//! the next time it would like to filter again. The shortest among all the non-zero timeouts
//! returned by all event events and the time until event start and event end in milliseconds
//! will be used as the timeout until the next filtering.
//! @param item The timeline item that is either next or current.
//! @param context Double pointer to a user context. The same double pointer user context in the
//! filter callback is passed, so the user context pointer can be set during filtering for use in
//! this callback.
//! @return a custom timeout or zero if no special timeout is requested
typedef uint32_t (*TimelineEventUpdateCallback)(TimelineItem *item, void **context);

typedef struct TimelineEventImpl {
  TimelineEventWillUpdateCallback will_update;
  TimelineEventFilterCallback filter;
  TimelineEventComparator comparator;
  TimelineEventUpdateCallback update;
  TimelineEventDidUpdateCallback did_update;
} TimelineEventImpl;

typedef const TimelineEventImpl *(*TimelineEventImplGetter)(void);

//! Initialize the timeline event service. Timeline items can be inserted before initialization and
//! the timeline event service will also take those new events into consideration. This also
//! initializes all registered specialized timeline event services. See `s_services` in ./event.c.
void timeline_event_init(void);

//! Deinit the timeline event service
//! @note Used for factory resetting
void timeline_event_deinit(void);

//! Should be called whenever a pin is added / deleted / changed. This makes sure the service is in
//! sync with the current set of pins and not acting on stale data.
void timeline_event_handle_blobdb_event(void);

//! Refresh the timeline event services.
void timeline_event_refresh(void);

//! Whether the event is all day.
//! @param common The common header of the event.
//! @return true if the event is all day, false otherwise.
bool timeline_event_is_all_day(CommonTimelineItemHeader *common);

//! Whether the event is ongoing.
//! @param now The current time in seconds since the epoch.
//! @param event_start The start of the event in seconds since the epoch.
//! @param event_duration_m The duration of the event in minutes.
//! @return true if the event is ongoing, false otherwise.
bool timeline_event_is_ongoing(time_t now, time_t event_start, int event_duration_m);

//! Whether the timeline event is between a time range specified relative to now.
//! @note Only the timestamp is compared against, the duration of the event is not considered.
//! @param common The common header of the event.
//! @param now The current time since the epoch
//! @param delta_start_s The delta seconds to apply to now to obtain the start time that the event
//! can be within. Passing in TIMELINE_EVENT_DELTA_INFINITE means any past event.
//! @param delta_end_s The delta seconds to apply to now to obtain the end time that the event
//! can be within. Passing in TIMELINE_EVENT_DELTA_INFINITE means any future event.
//! @return true if the event is within the specified time range, false otherwise.
bool timeline_event_starts_within(CommonTimelineItemHeader *common, time_t now,
                                  int delta_start_s, int delta_end_s);
