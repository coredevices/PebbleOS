/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "calendar.h"

#include "drivers/rtc.h"
#include "kernel/event_loop.h"
#include "kernel/events.h"
#include "kernel/pbl_malloc.h"
#include "kernel/pebble_tasks.h"
#include "services/common/system_task.h"
#include "services/normal/blob_db/pin_db.h"
#include "system/logging.h"
#include "system/status_codes.h"
#include "util/time/time.h"

static bool s_event_ongoing = false;

static void prv_put_calendar_event(void) {
  PebbleEvent event = {
    .type = PEBBLE_CALENDAR_EVENT,
    .calendar = {
      .is_event_ongoing = s_event_ongoing,
    },
  };
  event_put(&event);
}

static bool prv_calendar_filter(SerializedTimelineItemHeader *header, void **context) {
  const time_t now = rtc_get_time();
  return ((header->common.layout == LayoutIdCalendar) &&
          !timeline_event_is_all_day(&header->common) &&
          (timeline_event_is_ongoing(now, header->common.timestamp, header->common.duration) ||
           timeline_event_starts_within(&header->common, now, 0, TIMELINE_EVENT_DELTA_INFINITE)));
}

static uint32_t prv_calendar_update(TimelineItem *item, void **context) {
  s_event_ongoing = item ? timeline_event_is_ongoing(rtc_get_time(), item->header.timestamp,
                                                     item->header.duration)
                         : false;
  prv_put_calendar_event();
  return 0;
}

const TimelineEventImpl *calendar_get_event_service(void) {
  static const TimelineEventImpl s_event_impl = {
    .filter = prv_calendar_filter,
    .update = prv_calendar_update,
  };
  return &s_event_impl;
}

bool calendar_event_is_ongoing(void) {
  return s_event_ongoing;
}
