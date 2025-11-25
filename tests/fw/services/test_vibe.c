/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "fake_events.h"
#include "fake_new_timer.h"
#include "fake_pbl_malloc.h"
#include "fake_pebble_tasks.h"
#include "fake_rtc.h"

#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_analytics.h"

#include "services/common/vibe_pattern.h"
#include "applib/ui/vibes.h"
#include "util/size.h"

//declarations
bool sys_vibe_pattern_enqueue_step_raw(uint32_t step_duration_ms, int32_t strength);
bool sys_vibe_pattern_enqueue_step(uint32_t step_duration_ms, bool on);
void sys_vibe_pattern_trigger_start(void);
void sys_vibe_pattern_clear(void);
void sys_vibe_history_start_collecting(void);
void sys_vibe_history_stop_collecting(void);
bool sys_vibe_history_was_vibrating(uint64_t time_search);
int32_t sys_vibe_get_vibe_strength(void);

//stub
void vibe_ctl(bool on) {
}
void vibe_set_strength(int8_t strength) {
}

//helpers
static uint64_t prv_get_current_time() {
  time_t s;
  uint16_t ms;
  rtc_get_time_ms(&s, &ms);
  return ((uint64_t)s * 1000) + ms;
}

static void prv_run_vibes() {
  TimerID timer = stub_new_timer_get_next();
  while (timer != TIMER_INVALID_ID) {
    fake_rtc_increment_time_ms(stub_new_timer_timeout(timer));
    stub_new_timer_fire(timer);
    timer = stub_new_timer_get_next();
  }
}

static bool prv_confirm_history(const VibePattern pattern, int64_t start_time) {
  int64_t time = start_time;
  bool enabled = true;
  for (size_t i = 0; i < pattern.num_segments; i++) {
    for (int64_t time_offset = 1; time_offset < pattern.durations[i]; time_offset += 1) {
      if (sys_vibe_history_was_vibrating(time + time_offset) != enabled) {
        return false;
      }
    }
    time += pattern.durations[i];
    enabled = !enabled;
  }
  return true;
}

//unit test code
void test_vibe__initialize(void) {
  vibes_init();
  fake_rtc_init(0, 100);
}


void test_vibe__cleanup(void) {
}

void test_vibe__check_vibe_history(void) {
  // test builtin vibe
  sys_vibe_history_start_collecting();
  vibes_long_pulse();
  prv_run_vibes();
  cl_assert(sys_vibe_history_was_vibrating(prv_get_current_time() - 1));
  sys_vibe_history_stop_collecting();

  // test custom vibe
  const uint32_t custom_pattern_durations[] = { 10, 12, 100, 123, 25, 5 };
  const VibePattern custom_pattern = (VibePattern) {
    .durations = custom_pattern_durations,
    .num_segments = ARRAY_LENGTH(custom_pattern_durations)
  };
  uint64_t time_start = prv_get_current_time();
  sys_vibe_history_start_collecting();
  vibes_enqueue_custom_pattern(custom_pattern);
  prv_run_vibes();
  cl_assert(prv_confirm_history(custom_pattern, time_start));
  sys_vibe_history_stop_collecting();
}

void test_vibe__check_vibe_history_multiple(void) {
  const uint32_t custom_pattern_durations_1[] = { 10, 12, 100, 123, 25, 5 };
  const uint32_t custom_pattern_durations_2[] = { 24, 50, 130, 112, 52, 9 };
  const VibePattern custom_pattern_1 = (VibePattern) {
    .durations = custom_pattern_durations_1,
    .num_segments = ARRAY_LENGTH(custom_pattern_durations_1)
  };
  const VibePattern custom_pattern_2 = (VibePattern) {
    .durations = custom_pattern_durations_2,
    .num_segments = ARRAY_LENGTH(custom_pattern_durations_2)
  };

  sys_vibe_history_start_collecting();
  uint64_t time_start_1 = prv_get_current_time();
  vibes_enqueue_custom_pattern(custom_pattern_1);
  prv_run_vibes();
  uint64_t time_start_2 = prv_get_current_time();
  vibes_enqueue_custom_pattern(custom_pattern_2);
  prv_run_vibes();
  cl_assert(prv_confirm_history(custom_pattern_1, time_start_1));
  cl_assert(prv_confirm_history(custom_pattern_2, time_start_2));
  sys_vibe_history_stop_collecting();
}
