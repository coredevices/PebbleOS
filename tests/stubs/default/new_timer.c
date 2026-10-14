/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/services/new_timer/new_timer.h>

typedef void (*NewTimerCallback)(void *data);

typedef uint32_t TimerID;

PBL_WEAK TimerID new_timer_create(void) {
  return 1;
}

PBL_WEAK bool new_timer_start(TimerID timer, uint32_t timeout_ms, NewTimerCallback cb,
                              void *cb_data, uint32_t flags) {
  return true;
}

PBL_WEAK bool new_timer_stop(TimerID timer) {
  return true;
}

PBL_WEAK bool new_timer_scheduled(TimerID timer, uint32_t *expire_ms_p) {
  return true;
}

PBL_WEAK void new_timer_delete(TimerID timer) {
}

PBL_WEAK void *new_timer_debug_get_current_callback(void) {
  return nullptr;
}
