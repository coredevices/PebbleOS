/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "fake_pbl_malloc.h"

#include "services/common/new_timer/new_timer.h"
#include "util/list.h"
#include "drivers/rtc.h"
#include "system/passert.h"
#include <stdio.h>

// Structure of a timer
typedef struct StubTimer {
  //! Entry into either the running timers (s_running_timers) list or the idle timers
  //! (s_idle_timers)
  ListNode list_node;

  TimerID id; //<! ID assigned to this timer

  //! client provided callback function and argument
  NewTimerCallback cb;
  void* cb_data;

  //! The tick value when this timer will expire (in milliseconds). If the timer isn't currently
  //! running (scheduled) this value will be zero.
  uint32_t timeout_ms;

  //! True if this timer should automatically be rescheduled for period_ms from now
  bool repeating;
  uint32_t period_ms;

  //! True if this timer is currently having its callback executed.
  bool executing;

  //! Set by the delete function of client tries to delete a timer currently executing it's callback
  bool defer_delete;

} StubTimer;



// =============================================================================================
// Stubs
static ListNode *s_running_timers = NULL;
static ListNode *s_idle_timers = NULL;

// Call counters
static int s_num_new_timer_create_calls = 0;
static int s_num_new_timer_start_calls = 0;
static int s_num_new_timer_stop_calls = 0;
static int s_num_new_timer_delete_calls = 0;
static int s_num_new_timer_schedule_calls = 0;

// Last parameters
static TimerID s_new_timer_start_param_timer_id;
static uint32_t s_new_timer_start_param_timeout_ms;
static NewTimerCallback s_new_timer_start_param_cb;
static void * s_new_timer_start_param_cb_data;

// Debug utility
/*
static void prv_print_idle_list(char* title) {
  printf("%s IDLE LIST:\n", title);
  StubTimer *node = (StubTimer *) s_idle_timers;
  while (node) {
    StubTimer *next = (StubTimer *) list_get_next(&node->list_node);
    printf("  %p\n", node);
    if (node == next) {
      printf("RECURSIVE!!!\n");
      return;
    }
    node = next;
  }
  printf("\n");
}
*/


static bool prv_id_list_filter(ListNode* node, void* data) {
  StubTimer* timer = (StubTimer*)node;
  return timer->id == (uint32_t) data;
}

static StubTimer* prv_find_timer(TimerID timer_id)
{
  // Look for this timer in either the running or idle list
  ListNode* node = list_find(s_running_timers, prv_id_list_filter, (void*)(intptr_t)timer_id);
  if (!node) {
    node = list_find(s_idle_timers, prv_id_list_filter, (void*)(intptr_t)timer_id);
  }
  return (StubTimer *)node;
}

static int prv_timer_expire_compare_func(void* a, void* b) {
  return (((StubTimer*)b)->timeout_ms - ((StubTimer*)a)->timeout_ms);
}


static int stub_new_timer_create(void) {
  StubTimer *timer = (StubTimer *) kernel_malloc(sizeof(StubTimer));
  static int s_next_timer_id = 1;
  *timer = (StubTimer) {
    .id = s_next_timer_id++,
  };
  s_idle_timers = list_insert_before(s_idle_timers, &timer->list_node);
  return timer->id;
}

////////////////////////////////////
// Stub manipulation:
//
bool stub_new_timer_start(TimerID timer_id, uint32_t timeout_ms, NewTimerCallback cb, void *cb_data,
                          uint32_t flags) {
  StubTimer* timer = prv_find_timer(timer_id);

  // Remove it from its current list
  if (list_contains(s_running_timers, &timer->list_node)) {
    list_remove(&timer->list_node, &s_running_timers /* &head */, NULL /* &tail */);
  } else {
    list_remove(&timer->list_node, &s_idle_timers /* &head */, NULL /* &tail */);
  }

  // Set timer variables
  timer->cb = cb;
  timer->cb_data = cb_data;
  timer->timeout_ms = timeout_ms;
  timer->repeating = flags & TIMER_START_FLAG_REPEATING;
  timer->period_ms = timeout_ms;

  // Insert into sorted order in the running list
  s_running_timers = list_sorted_add(s_running_timers, &timer->list_node,
                                     prv_timer_expire_compare_func, true);
  return true;
}

bool stub_new_timer_stop(TimerID timer_id) {
  StubTimer* timer = prv_find_timer(timer_id);

  // Move it to the idle list if it's currently running
  if (list_contains(s_running_timers, &timer->list_node)) {
    list_remove(&timer->list_node, &s_running_timers /* &head */, NULL /* &tail */);
    s_idle_timers = list_insert_before(s_idle_timers, &timer->list_node);
  }

  // Clear the repeating flag so that if they call this method from a callback it won't get
  // rescheduled
  timer->repeating = false;
  timer->timeout_ms = 0;
  return !timer->executing;
}

void stub_new_timer_delete(TimerID timer_id) {
  StubTimer* timer = prv_find_timer(timer_id);

  // Automatically stop it if it it's not stopped already
  if (list_contains(s_running_timers, &timer->list_node)) {
    timer->timeout_ms = 0;
    list_remove(&timer->list_node, &s_running_timers /* &head */, NULL /* &tail */);
    s_idle_timers = list_insert_before(s_idle_timers, &timer->list_node);
  }
  timer->repeating = false; // In case it's currently executing, make sure we don't reschedule it

  if (!timer->executing) {
    list_remove(&timer->list_node, &s_idle_timers /* &head */, NULL /* &tail */);
    kernel_free(timer);
  } else {
    timer->defer_delete = true;
  }
}

bool stub_new_timer_is_scheduled(TimerID timer_id) {
  StubTimer* timer = prv_find_timer(timer_id);
  if (timer == NULL) {
    return false;
  }
  return list_contains(s_running_timers, &timer->list_node);
}

uint32_t stub_new_timer_timeout(TimerID timer_id) {
  StubTimer* timer = prv_find_timer(timer_id);
  if (timer == NULL) {
    return false;
  }
  return timer->timeout_ms;
}

// Mark the timer as executing. This prevents it from getting deleted. In the real implementation,
// it would get deleted after it's callback returned
void stub_new_timer_set_executing(TimerID timer_id, bool set) {
  StubTimer* timer = prv_find_timer(timer_id);
  PBL_ASSERTN(timer != NULL);
  timer->executing = true;
}

void * stub_new_timer_callback_data(TimerID timer_id) {
  StubTimer* timer = prv_find_timer(timer_id);
  if (timer == NULL) {
    return false;
  }
  return timer->cb_data;
}

bool stub_new_timer_fire(TimerID timer_id) {
  StubTimer* timer = prv_find_timer(timer_id);
  if (timer == NULL) {
    return false;
  }

  if (list_contains(s_running_timers, &timer->list_node)) {
    list_remove(&timer->list_node, &s_running_timers /* &head */, NULL /* &tail */);
    s_idle_timers = list_insert_before(s_idle_timers, &timer->list_node);
  } else {
    printf("WARNING: Attempted to fire a non-running timer\n");
    return false;
  }

  timer->timeout_ms = 0;
  timer->executing = true;
  timer->cb(timer->cb_data);
  timer->executing = false;

  if (timer->defer_delete) {
    // Timer was deleted from the callback, clean it up now:
    list_remove(&timer->list_node, &s_idle_timers /* &head */, NULL /* &tail */);
    kernel_free(timer);
    return true;
  }

  if (timer->repeating && timer->timeout_ms == 0) {
    stub_new_timer_start(timer_id, timer->period_ms, timer->cb, timer->cb_data,
                         TIMER_START_FLAG_REPEATING);
  }
  return true;
}

void stub_new_timer_cleanup(void) {
  StubTimer *node = (StubTimer *) s_running_timers;
  while (node) {
    StubTimer *next = (StubTimer *) list_get_next(&node->list_node);
    list_remove(&node->list_node, &s_running_timers, NULL);
    kernel_free(node);
    node = next;
  }
  PBL_ASSERTN(s_running_timers == NULL);

  node = (StubTimer *) s_idle_timers;
  while (node) {
    StubTimer *next = (StubTimer *) list_get_next(&node->list_node);
    list_remove(&node->list_node, &s_idle_timers, NULL);
    kernel_free(node);
    node = next;
  }
  PBL_ASSERTN(s_idle_timers == NULL);
}

TimerID stub_new_timer_get_next(void) {
  StubTimer *timer = (StubTimer *) list_get_head(s_running_timers);
  return timer ? timer->id : TIMER_INVALID_ID;
}

void stub_new_timer_invoke(int num_to_invoke) {
  TimerID timer = stub_new_timer_get_next();
  while (timer != TIMER_INVALID_ID && num_to_invoke--) {
    stub_new_timer_fire(timer);
    timer = stub_new_timer_get_next();
  }
}

// =============================================================================================
// Fakes

// Create a new timer
TimerID new_timer_create(void) {
  s_num_new_timer_create_calls++;
  return stub_new_timer_create();
}

// Start a timer
bool new_timer_start(TimerID timer_id, uint32_t timeout_ms, NewTimerCallback cb, void *cb_data,
                     uint32_t flags) {
  s_num_new_timer_start_calls++;
  s_new_timer_start_param_timer_id = timer_id;
  s_new_timer_start_param_timeout_ms = timeout_ms;
  s_new_timer_start_param_cb = cb;
  s_new_timer_start_param_cb_data = cb_data;
  return stub_new_timer_start(timer_id, timeout_ms, cb, cb_data, flags);
}

// Stop a timer
bool new_timer_stop(TimerID timer_id) {
  s_num_new_timer_stop_calls++;
  return stub_new_timer_stop(timer_id);
}

// Delete a timer
void new_timer_delete(TimerID timer_id) {
  s_num_new_timer_delete_calls++;
  stub_new_timer_delete(timer_id);
}

bool new_timer_scheduled(TimerID timer, uint32_t *expire_ms_p) {
  s_num_new_timer_schedule_calls++;
  return stub_new_timer_is_scheduled(timer);
}

