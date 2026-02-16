/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "tick_timer.h"

#include "kernel/events.h"
#include "drivers/rtc.h"
#include "services/common/regular_timer.h"
#include "process_management/app_manager.h"
#include "system/logging.h"
#include "system/passert.h"
#include "kernel/util/sleep.h"

static uint16_t s_num_subscribers;
static time_t s_last_tick_seconds = -1;

static void timer_tick_event_publisher(void* data) {
  time_t seconds;
  uint16_t ms;

  rtc_get_time_ms(&seconds, &ms);

  // If we haven't advanced to the next second yet, sleep until we do.
  // This prevents missing second updates when FreeRTOS timer and RTC drift
  // relative to each other. If this happens too often, we may want to calibrate
  // more often...
  if ((s_last_tick_seconds != -1) && (seconds == s_last_tick_seconds)) {
    do {
      PBL_LOG_WRN("Sleeping until next second (remaining ms: %d)", 1000 - ms);
      psleep(1000 - ms);
      rtc_get_time_ms(&seconds, &ms);
    } while (seconds == s_last_tick_seconds);
  }

  s_last_tick_seconds = seconds;

  PebbleEvent e = {
    .type = PEBBLE_TICK_EVENT,
    .clock_tick.tick_time = seconds,
  };

  event_put(&e);
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
}

void tick_timer_remove_subscriber(PebbleTask task) {
  PBL_ASSERTN(s_num_subscribers > 0);
  --s_num_subscribers;
  if (s_num_subscribers == 0) {
    PBL_LOG_DBG("stopping tick timer");
    regular_timer_remove_callback(&s_tick_timer_info);
  }
}
