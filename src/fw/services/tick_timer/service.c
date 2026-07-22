/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/tick_timer.h"

#include "kernel/events.h"
#include <pbl/drivers/rtc.h>
#include "pbl/services/regular_timer.h"
#include "process_management/app_manager.h"
#include <pbl/logging/logging.h>
#include "system/passert.h"
#include "util/time/time.h"

PBL_LOG_MODULE_DEFINE(service_tick_timer, CONFIG_SERVICE_TICK_TIMER_LOG_LEVEL);

static uint16_t s_num_subscribers;

// Per-task flag for whether that task needs second-resolution ticks, plus the
// running count of tasks that do. When nobody needs seconds we only broadcast
// the tick when the minute changes, so minute-granularity subscribers (the
// common watchface, the status bar) let the app/worker tasks stay asleep for
// the rest of the minute. The 1 Hz regular timer still fires (it also feeds the
// watchdogs), but we skip event_put() on the in-between seconds.
static bool s_task_needs_seconds[NumPebbleTask];
static uint16_t s_seconds_subscribers;

//! Epoch seconds of the most recently broadcast tick, used to detect minute
//! rollover in a way that tolerates regular-timer drift.
static time_t s_last_published;

static void prv_publish_tick(time_t now) {
  s_last_published = now;
  PebbleEvent e = {
    .type = PEBBLE_TICK_EVENT,
    .clock_tick.tick_time = now,
  };
  event_put(&e);
}

static void timer_tick_event_publisher(void* data) {
  const time_t now = rtc_get_time();
  if (s_seconds_subscribers == 0 &&
      (now / SECONDS_PER_MINUTE) == (s_last_published / SECONDS_PER_MINUTE)) {
    // No second-resolution subscribers and still within the same minute as the
    // last tick: nothing for minute-granularity subscribers to do.
    return;
  }
  prv_publish_tick(now);
}

static RegularTimerInfo s_tick_timer_info = {
  .cb = &timer_tick_event_publisher
};

void tick_timer_add_subscriber(PebbleTask task) {
  ++s_num_subscribers;
  if (s_num_subscribers == 1) {
    PBL_LOG_DBG("starting tick timer");
    regular_timer_add_seconds_callback(&s_tick_timer_info);
  }
  // Give the new subscriber a prompt baseline tick instead of making it wait up
  // to a full minute for the next boundary.
  prv_publish_tick(rtc_get_time());
}

void tick_timer_remove_subscriber(PebbleTask task) {
  PBL_ASSERTN(s_num_subscribers > 0);
  // Drop any second-resolution request this task still held.
  tick_timer_set_seconds_subscribed(task, false);
  --s_num_subscribers;
  if (s_num_subscribers == 0) {
    PBL_LOG_DBG("stopping tick timer");
    regular_timer_remove_callback(&s_tick_timer_info);
  }
}

void tick_timer_set_seconds_subscribed(PebbleTask task, bool needs_seconds) {
  PBL_ASSERTN(task < NumPebbleTask);
  if (s_task_needs_seconds[task] == needs_seconds) {
    return;
  }
  s_task_needs_seconds[task] = needs_seconds;
  if (needs_seconds) {
    ++s_seconds_subscribers;
  } else {
    PBL_ASSERTN(s_seconds_subscribers > 0);
    --s_seconds_subscribers;
  }
}
