/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/services/evented_timer.h"
#include "pbl/kernel/compiler.h"

void PBL_WEAK evented_timer_init(void) {
}

void PBL_WEAK evented_timer_clear_process_timers(PebbleTask task) {
}

EventedTimerID PBL_WEAK evented_timer_register(uint32_t timeout_ms, bool repeating,
                                               EventedTimerCallback callback, void *callback_data) {
  return 0;
}

bool PBL_WEAK evented_timer_reschedule(EventedTimerID timer, uint32_t new_timeout_ms) {
  return true;
}

EventedTimerID PBL_WEAK evented_timer_register_or_reschedule(EventedTimerID timer_id,
                                                             uint32_t timeout_ms,
                                                             EventedTimerCallback callback,
                                                             void *data) {
  return 0;
}

void PBL_WEAK evented_timer_cancel(EventedTimerID timer) {
}

bool PBL_WEAK evented_timer_exists(EventedTimerID timer) {
  return true;
}

bool PBL_WEAK evented_timer_is_current_task(EventedTimerID timer) {
  return true;
}

void PBL_WEAK evented_timer_reset(void) {
}
