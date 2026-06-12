/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

typedef void (*NewTimerCallback)(void *data);

typedef uint32_t TimerID;

#define TIMER_INVALID_ID 0

static TimerID s_next_timer_id = 1;

typedef struct {
  TimerID id;
  NewTimerCallback cb;
  void *cb_data;
  bool active;
} StubTimer;

#define MAX_STUB_TIMERS 4

static StubTimer s_timers[MAX_STUB_TIMERS];

static StubTimer *prv_find_timer(TimerID id) {
  for (int i = 0; i < MAX_STUB_TIMERS; i++) {
    if (s_timers[i].id == id) {
      return &s_timers[i];
    }
  }
  return NULL;
}

TimerID new_timer_create(void) {
  TimerID id = s_next_timer_id++;
  for (int i = 0; i < MAX_STUB_TIMERS; i++) {
    if (s_timers[i].id == TIMER_INVALID_ID) {
      s_timers[i] = (StubTimer){
        .id = id,
        .cb = NULL,
        .cb_data = NULL,
        .active = false,
      };
      return id;
    }
  }
  return TIMER_INVALID_ID;
}

bool new_timer_start(TimerID timer, uint32_t timeout_ms, NewTimerCallback cb, void *cb_data,
                     uint32_t flags) {
  StubTimer *t = prv_find_timer(timer);
  if (!t) return false;
  t->cb = cb;
  t->cb_data = cb_data;
  t->active = true;
  return true;
}

bool new_timer_stop(TimerID timer) {
  StubTimer *t = prv_find_timer(timer);
  if (!t) return false;
  t->active = false;
  return true;
}

bool new_timer_scheduled(TimerID timer, uint32_t *expire_ms_p) {
  StubTimer *t = prv_find_timer(timer);
  return t && t->active;
}

void new_timer_delete(TimerID timer) {
  StubTimer *t = prv_find_timer(timer);
  if (t) {
    t->id = TIMER_INVALID_ID;
    t->cb = NULL;
    t->cb_data = NULL;
    t->active = false;
  }
}

void* new_timer_debug_get_current_callback(void) {
  return NULL;
}

static void stub_timer_fire_all(void) {
  for (int i = 0; i < MAX_STUB_TIMERS; i++) {
    if (s_timers[i].active && s_timers[i].cb) {
      s_timers[i].cb(s_timers[i].cb_data);
    }
  }
}

static void stub_timer_reset_all(void) {
  for (int i = 0; i < MAX_STUB_TIMERS; i++) {
    s_timers[i] = (StubTimer){0};
  }
  s_next_timer_id = 1;
}