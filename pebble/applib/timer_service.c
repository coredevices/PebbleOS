/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "timer_service.h"

#include <pbl/services/timers/timer.h>

#include <syscall/syscall.h>

_Static_assert(TIMER_MAX_DURATION_S == TIMER_SERVICE_MAX_DURATION_S, "max duration mismatch");
_Static_assert(TIMER_MAX_COUNT == TIMER_SERVICE_MAX_TIMERS, "max timer count mismatch");
_Static_assert(TIMER_DESCRIPTION_SIZE == TIMER_SERVICE_DESCRIPTION_SIZE,
               "description size mismatch");
_Static_assert(sizeof(TimerId) == sizeof(TimerServiceId), "TimerServiceId size mismatch");
_Static_assert(TIMER_INVALID_ID == TIMER_SERVICE_INVALID_ID, "invalid ID mismatch");

bool timer_service_can_create(void) {
  return sys_timer_can_create();
}

TimerServiceCreateResult timer_service_create(uint32_t duration_s, TimerServiceId *id_out) {
  if (!id_out) {
    return TimerServiceCreateErrInvalidArgument;
  }

  *id_out = TIMER_SERVICE_INVALID_ID;
  return sys_timer_create(duration_s, id_out);
}

bool timer_service_cancel(TimerServiceId id) {
  return sys_timer_cancel(id);
}

uint32_t timer_service_cancel_all(void) {
  return sys_timer_cancel_all();
}

uint8_t timer_service_get_timers(TimerServiceInfo *timers, uint8_t max_timers) {
  if (!timers || max_timers == 0) {
    return 0;
  }
  return sys_timer_get_timers(timers, max_timers);
}
