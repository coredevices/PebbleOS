/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/common/evented_timer.h"
#include "util/attributes.h"

void WEAK evented_timer_init(void) {}

void WEAK evented_timer_clear_process_timers(PebbleTask task) {}

EventedTimerID WEAK evented_timer_register(uint32_t timeout_ms, bool repeating,
                                           EventedTimerCallback callback, void* callback_data) {
  return 0;
}

bool WEAK evented_timer_reschedule(EventedTimerID timer, uint32_t new_timeout_ms) {
  return true;
}

EventedTimerID WEAK evented_timer_register_or_reschedule(
    EventedTimerID timer_id, uint32_t timeout_ms, EventedTimerCallback callback, void *data) {
  return 0;
}

void WEAK evented_timer_cancel(EventedTimerID timer) {}

bool WEAK evented_timer_exists(EventedTimerID timer) {
  return true;
}

bool WEAK evented_timer_is_current_task(EventedTimerID timer) {
  return true;
}

void WEAK evented_timer_reset(void) {}
