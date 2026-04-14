/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "tick_timer.h"

#include "kernel/events.h"
#include "drivers/rtc.h"
#include "services/common/regular_timer.h"
#include "process_management/app_manager.h"
#include "system/logging.h"
#include "system/passert.h"

// FIXME: Move to Kconfig
#ifdef MICRO_FAMILY_SF32LB52
#define USE_RTC_SECOND_TICK
#endif

static uint16_t s_num_subscribers;

static void timer_tick_event_publisher(void* data) {
  PebbleEvent e = {
    .type = PEBBLE_TICK_EVENT,
    .clock_tick.tick_time = rtc_get_time(),
  };

#ifdef USE_RTC_SECOND_TICK
  event_put_isr(&e);
#else
  event_put(&e);
#endif
}

#ifndef USE_RTC_SECOND_TICK
static RegularTimerInfo s_tick_timer_info = {
  .cb = &timer_tick_event_publisher
};
#endif

void tick_timer_add_subscriber(PebbleTask task) {
  ++s_num_subscribers;
  if (s_num_subscribers == 1) {
    PBL_LOG_DBG("starting tick timer");
#ifdef USE_RTC_SECOND_TICK
    rtc_second_tick_subscribe(timer_tick_event_publisher, NULL);
#else
    regular_timer_add_seconds_callback(&s_tick_timer_info);
#endif
  }
}

void tick_timer_remove_subscriber(PebbleTask task) {
  PBL_ASSERTN(s_num_subscribers > 0);
  --s_num_subscribers;
  if (s_num_subscribers == 0) {
    PBL_LOG_DBG("stopping tick timer");
#ifdef USE_RTC_SECOND_TICK
    rtc_second_tick_unsubscribe();
#else
    regular_timer_remove_callback(&s_tick_timer_info);
#endif
  }
}
