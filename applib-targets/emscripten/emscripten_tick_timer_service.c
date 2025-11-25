/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/app_logging.h"
#include "applib/tick_timer_service_private.h"

#include <emscripten/emscripten.h>
#include <stdio.h>
#include <time.h>

time_t time(time_t *time);

static TickTimerServiceState s_state;

static void prv_schedule_next_update(void);

static void prv_do_update(void *data) {
  if (!s_state.handler) {
    return;
  }

  // The data pointer value is used to pass a boolean value directly:
  const bool is_update_due_to_time_change = (uintptr_t)data;

  struct tm currtime;
  time_t t = time(NULL);
  localtime_r(&t, &currtime);

  TimeUnits units_changed;
  if (is_update_due_to_time_change) {
    units_changed = (SECOND_UNIT | MINUTE_UNIT | HOUR_UNIT | DAY_UNIT | YEAR_UNIT);
  } else {
    prv_schedule_next_update();
    units_changed = 0;
    if (!s_state.first_tick) {
      if (s_state.last_time.tm_sec != currtime.tm_sec) {
        units_changed |= SECOND_UNIT;
      }
      if (s_state.last_time.tm_min != currtime.tm_min) {
        units_changed |= MINUTE_UNIT;
      }
      if (s_state.last_time.tm_hour != currtime.tm_hour) {
        units_changed |= HOUR_UNIT;
      }
      if (s_state.last_time.tm_mday != currtime.tm_mday) {
        units_changed |= DAY_UNIT;
      }
      if (s_state.last_time.tm_mon != currtime.tm_mon) {
        units_changed |= MONTH_UNIT;
      }
      if (s_state.last_time.tm_year != currtime.tm_year) {
        units_changed |= YEAR_UNIT;
      }
    }
  }

  s_state.last_time = currtime;
  s_state.first_tick = false;

  if ((s_state.tick_units & units_changed) || (units_changed == 0)) {
    s_state.handler(&currtime, units_changed);
  }
}

static void prv_schedule_next_update(void) {
  // Schedule this to fire again at the top of the next second
  const int ms_into_s = EM_ASM_INT_V({
      return Date.now().getMilliseconds;
    });
  const double wait_ms = 1000 - ms_into_s;
  const bool is_update_due_to_time_change = false;
  emscripten_async_call(prv_do_update, (void *)(uintptr_t)is_update_due_to_time_change, wait_ms);
}

void tick_timer_service_handle_time_change(void) {
  const bool is_update_due_to_time_change = true;
  prv_do_update((void *)(uintptr_t)is_update_due_to_time_change);
}

void tick_timer_service_subscribe(TimeUnits tick_units, TickHandler handler) {
  const bool first = (s_state.handler == NULL);
  s_state = (TickTimerServiceState) {
    .handler = handler,
    .tick_units = tick_units,
    .first_tick = true,
  };

  if (first && handler != NULL) {
    prv_schedule_next_update();
  }
}

void emx_tick_timer_service_init(void) {
  s_state = (TickTimerServiceState){};
}
