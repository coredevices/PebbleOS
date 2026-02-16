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
static bool s_vibe_on = false;
static int s_vibe_ctl_count = 0;
void vibe_ctl(bool on) {
  s_vibe_on = on;
  s_vibe_ctl_count++;
}

static int8_t s_last_strength_set = 0;
static int s_strength_set_count = 0;
void vibe_set_strength(int8_t strength) {
  s_last_strength_set = strength;
  s_strength_set_count++;
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
  s_last_strength_set = 0;
  s_strength_set_count = 0;
  s_vibe_on = false;
  s_vibe_ctl_count = 0;
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

void test_vibe__custom_pattern_with_amplitudes(void) {
  const uint32_t durations[] = { 200, 100, 400 };
  const uint32_t amplitudes[] = { 80, 50, 20 };
  const VibePatternWithAmplitudes pattern = {
    .durations = durations,
    .amplitudes = amplitudes,
    .num_segments = ARRAY_LENGTH(durations),
  };
  vibes_enqueue_custom_pattern_with_amplitudes(pattern);
  prv_run_vibes();
  // Pattern engine turns motor off via vibe_ctl(false) at end
  cl_assert_equal_b(s_vibe_on, false);
  // All 3 segments set strength (no alternation — every segment has an amplitude)
  cl_assert(s_strength_set_count >= 3);
}

void test_vibe__custom_pattern_with_amplitudes_clamped(void) {
  const uint32_t durations[] = { 100 };
  const uint32_t amplitudes[] = { 200 };  // exceeds 100, should be clamped
  const VibePatternWithAmplitudes pattern = {
    .durations = durations,
    .amplitudes = amplitudes,
    .num_segments = ARRAY_LENGTH(durations),
  };
  vibes_enqueue_custom_pattern_with_amplitudes(pattern);
  prv_run_vibes();
  cl_assert_equal_b(s_vibe_on, false);
  // Clamped to 100, verify it was set
  cl_assert_equal_i(s_last_strength_set, 100);
}

void test_vibe__custom_pattern_with_null_amplitudes(void) {
  const uint32_t durations[] = { 100 };
  const VibePatternWithAmplitudes pattern = {
    .durations = durations,
    .amplitudes = NULL,
    .num_segments = ARRAY_LENGTH(durations),
  };
  // Should return without crashing (early return on null amplitudes)
  vibes_enqueue_custom_pattern_with_amplitudes(pattern);
  cl_assert_equal_i(s_strength_set_count, 0);
  cl_assert_equal_i(s_vibe_ctl_count, 0);
}

void test_vibe__custom_pattern_with_amplitudes_null_durations(void) {
  const uint32_t amplitudes[] = { 80 };
  const VibePatternWithAmplitudes pattern = {
    .durations = NULL,
    .amplitudes = amplitudes,
    .num_segments = 1,
  };
  // Should return without crashing (early return on null durations)
  vibes_enqueue_custom_pattern_with_amplitudes(pattern);
  cl_assert_equal_i(s_strength_set_count, 0);
  cl_assert_equal_i(s_vibe_ctl_count, 0);
}

void test_vibe__custom_pattern_with_amplitudes_single(void) {
  const uint32_t durations[] = { 300 };
  const uint32_t amplitudes[] = { 50 };
  const VibePatternWithAmplitudes pattern = {
    .durations = durations,
    .amplitudes = amplitudes,
    .num_segments = 1,
  };
  vibes_enqueue_custom_pattern_with_amplitudes(pattern);
  prv_run_vibes();
  cl_assert_equal_b(s_vibe_on, false);
  cl_assert_equal_i(s_last_strength_set, 50);
}

void test_vibe__custom_pattern_with_zero_amplitude(void) {
  const uint32_t durations[] = { 200, 100, 300 };
  const uint32_t amplitudes[] = { 0, 0, 100 };
  const VibePatternWithAmplitudes pattern = {
    .durations = durations,
    .amplitudes = amplitudes,
    .num_segments = ARRAY_LENGTH(durations),
  };
  vibes_enqueue_custom_pattern_with_amplitudes(pattern);
  prv_run_vibes();
  cl_assert_equal_b(s_vibe_on, false);
  // Amplitude 0 segments use vibe_ctl(false), not vibe_set_strength(0),
  // so only the non-zero segment (100) calls vibe_set_strength
  cl_assert(s_strength_set_count >= 1);
  // Last segment had amplitude 100
  cl_assert_equal_i(s_last_strength_set, 100);
}

void test_vibe__custom_pattern_with_amplitudes_verifies_strength(void) {
  const uint32_t durations[] = { 100, 50, 100 };
  const uint32_t amplitudes[] = { 75, 50, 25 };
  const VibePatternWithAmplitudes pattern = {
    .durations = durations,
    .amplitudes = amplitudes,
    .num_segments = ARRAY_LENGTH(durations),
  };
  vibes_enqueue_custom_pattern_with_amplitudes(pattern);
  prv_run_vibes();
  // vibe_set_strength called for all 3 segments (75, 50, 25)
  cl_assert(s_strength_set_count >= 3);
  // Last strength set was 25 (third segment)
  cl_assert_equal_i(s_last_strength_set, 25);
  // Motor is off after pattern completes
  cl_assert_equal_b(s_vibe_on, false);
}

void test_vibe__custom_pattern_ramp_down(void) {
  const uint32_t durations[] = { 200, 200, 200, 200 };
  const uint32_t amplitudes[] = { 100, 75, 50, 25 };
  const VibePatternWithAmplitudes pattern = {
    .durations = durations,
    .amplitudes = amplitudes,
    .num_segments = ARRAY_LENGTH(durations),
  };
  vibes_enqueue_custom_pattern_with_amplitudes(pattern);
  prv_run_vibes();
  // All 4 segments set strength
  cl_assert(s_strength_set_count >= 4);
  // Last strength was 25
  cl_assert_equal_i(s_last_strength_set, 25);
  // Motor is off after pattern completes
  cl_assert_equal_b(s_vibe_on, false);
}
