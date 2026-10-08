/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/services/blob_db/pin_db.h>

/**
 * @defgroup services_timeline_event Timeline events
 * @ingroup services_timeline
 * @brief Tracks the next or current timeline event for services that react to it.
 *
 * Each registered service (calendar, Timeline Peek) supplies a TimelineEventImpl. Whenever pins
 * change, a timer expires or a refresh is requested, the event service runs on KernelBG: it calls
 * every will_update callback, passes each pin header through every filter, keeps one item per
 * service, calls every update callback with it (or NULL), then every did_update callback. The next
 * run is scheduled at the earliest of the timeouts returned by the update callbacks and the start
 * or end of the selected item.
 * @{
 */

/** @brief Delta meaning "unbounded" for timeline_event_starts_within(). */
#define TIMELINE_EVENT_DELTA_INFINITE (INT32_MAX)

/** @brief Services using the timeline event service. */
typedef enum TimelineEventService {
  /** Calendar, for smart Do Not Disturb. */
  TimelineEventService_Calendar,
  /** Timeline Peek. */
  TimelineEventService_Peek,

  /** Number of services. */
  TimelineEventServiceCount,
} TimelineEventService;

/**
 * @brief Called before filtering and updating begins.
 *
 * @param context Pointer to the user context, which may be set up here for filtering and updating.
 */
typedef void (*TimelineEventWillUpdateCallback)(void **context);

/**
 * @brief Called for every pin header to filter.
 *
 * Of the headers accepted, only one, the earliest unless the comparator says otherwise, is passed
 * to the update callback.
 *
 * @param header Header of the pin under consideration.
 * @param context Pointer to the user context; the same pointer is passed to the update callback.
 * @return true to consider the pin for the update.
 */
typedef bool (*TimelineEventFilterCallback)(SerializedTimelineItemHeader *header, void **context);

/**
 * @brief Called after filtering and updating, to tear down state set up by other callbacks.
 *
 * @param context Pointer to the user context.
 */
typedef void (*TimelineEventDidUpdateCallback)(void **context);

/**
 * @brief Pick between two pins that passed filtering.
 *
 * @param new_header Header of the pin that newly passed filtering.
 * @param old_header Header of the pin that passed filtering earlier.
 * @param context Pointer to the user context.
 * @return Negative to replace @p old_header with @p new_header; otherwise the earlier of the two
 *         is kept.
 */
typedef int (*TimelineEventComparator)(SerializedTimelineItemHeader *new_header,
                                       SerializedTimelineItemHeader *old_header, void **context);

/**
 * @brief Called with the selected pin, or NULL if no pin passed filtering.
 *
 * @param item Next or current pin (header only), or NULL.
 * @param context Pointer to the user context, as set during filtering.
 * @return Milliseconds until the service wants to filter again, or 0 for no special timeout.
 */
typedef uint32_t (*TimelineEventUpdateCallback)(TimelineItem *item, void **context);

/** @brief Callbacks of a timeline event service. */
typedef struct TimelineEventImpl {
  /** Called before filtering, may be NULL. */
  TimelineEventWillUpdateCallback will_update;
  /** Filter. */
  TimelineEventFilterCallback filter;
  /** Comparator, may be NULL to always keep the earliest pin. */
  TimelineEventComparator comparator;
  /** Update. */
  TimelineEventUpdateCallback update;
  /** Called after updating, may be NULL. */
  TimelineEventDidUpdateCallback did_update;
} TimelineEventImpl;

/**
 * @brief Get the callbacks of a timeline event service.
 *
 * @return Callbacks.
 */
typedef const TimelineEventImpl *(*TimelineEventImplGetter)(void);

/**
 * @brief Initialize the timeline event service and all registered services.
 *
 * Runs asynchronously on KernelBG. Pins inserted before initialization are taken into account.
 */
void timeline_event_init(void);

/**
 * @brief Stop the timeline event service.
 *
 * @note Used for factory resetting.
 */
void timeline_event_deinit(void);

/**
 * @brief Resynchronize after a pin was added, deleted or changed.
 *
 * Keeps the services from acting on stale data.
 */
void timeline_event_handle_blobdb_event(void);

/** @brief Refresh the timeline event services. */
void timeline_event_refresh(void);

/**
 * @brief Check whether an event is all day.
 *
 * @param common Header of the event.
 * @return true if flagged all day or lasting 24 hours or more.
 */
bool timeline_event_is_all_day(CommonTimelineItemHeader *common);

/**
 * @brief Check whether an event is ongoing.
 *
 * @param now Current time, in seconds since the epoch.
 * @param event_start Start of the event, in seconds since the epoch.
 * @param event_duration_m Duration of the event, in minutes.
 * @return true if the event has started and not ended.
 */
bool timeline_event_is_ongoing(time_t now, time_t event_start, int event_duration_m);

/**
 * @brief Check whether a pin starts within a time range relative to now.
 *
 * @note Only the start is compared; the duration is not considered. Items other than pins never
 * match.
 *
 * @param common Header of the event.
 * @param now Current time, in seconds since the epoch.
 * @param delta_start_s Seconds added to @p now to get the start of the range (exclusive), or
 *                      @ref TIMELINE_EVENT_DELTA_INFINITE for any past event.
 * @param delta_end_s Seconds added to @p now to get the end of the range (exclusive), or
 *                    @ref TIMELINE_EVENT_DELTA_INFINITE for any future event.
 * @return true if the pin starts within the range.
 */
bool timeline_event_starts_within(CommonTimelineItemHeader *common, time_t now, int delta_start_s,
                                  int delta_end_s);

/** @} */
