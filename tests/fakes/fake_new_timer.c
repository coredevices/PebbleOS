/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "fake_new_timer.h"
#include "fake_pbl_malloc.h"

#include "services/common/new_timer/new_timer.h"
#include "util/list.h"
#include "drivers/rtc.h"
#include "system/passert.h"
#include <stdio.h>

// =============================================================================================
// Variables (defined here so they can be shared across translation units)

ListNode *s_running_timers = NULL;
ListNode *s_idle_timers = NULL;

// Timer ID counter - needs to be visible for reset
int s_stub_next_timer_id = 1;

// Call counters
int s_num_new_timer_create_calls = 0;
int s_num_new_timer_start_calls = 0;
int s_num_new_timer_stop_calls = 0;
int s_num_new_timer_delete_calls = 0;
int s_num_new_timer_schedule_calls = 0;

// Last parameters
TimerID s_new_timer_start_param_timer_id;
uint32_t s_new_timer_start_param_timeout_ms;
NewTimerCallback s_new_timer_start_param_cb;
void * s_new_timer_start_param_cb_data;

// =============================================================================================
// External implementations of new_timer functions

TimerID new_timer_create(void) {
  s_num_new_timer_create_calls++;
  return stub_new_timer_create();
}

bool new_timer_start(TimerID timer_id, uint32_t timeout_ms, NewTimerCallback cb, void *cb_data,
                     uint32_t flags) {
  s_num_new_timer_start_calls++;
  s_new_timer_start_param_timer_id = timer_id;
  s_new_timer_start_param_timeout_ms = timeout_ms;
  s_new_timer_start_param_cb = cb;
  s_new_timer_start_param_cb_data = cb_data;
  return stub_new_timer_start(timer_id, timeout_ms, cb, cb_data, flags);
}

bool new_timer_stop(TimerID timer_id) {
  s_num_new_timer_stop_calls++;
  return stub_new_timer_stop(timer_id);
}

void new_timer_delete(TimerID timer_id) {
  s_num_new_timer_delete_calls++;
  stub_new_timer_delete(timer_id);
}

bool new_timer_scheduled(TimerID timer, uint32_t *expire_ms_p) {
  s_num_new_timer_schedule_calls++;
  return stub_new_timer_is_scheduled(timer);
}
