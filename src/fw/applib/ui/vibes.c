/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/ui/vibes.h"

#include "syscall/syscall.h"
#include "system/logging.h"
#include "util/size.h"

#define PATTERN_FROM_DURATIONS(pat, array) (pat) = (VibePattern){ .durations = (array), .num_segments = ARRAY_LENGTH((array)) }

static const uint32_t SHORT_PULSE_DURATIONS[] = { 250 };
static const uint32_t LONG_PULSE_DURATIONS[] = { 500 };
static const uint32_t DOUBLE_PULSE_DURATIONS[] = { 100, 100, 100 };

void vibes_short_pulse(void) {
  VibePattern pat;
  PATTERN_FROM_DURATIONS(pat, SHORT_PULSE_DURATIONS);
  vibes_enqueue_custom_pattern(pat);
}

void vibes_long_pulse(void) {
  VibePattern pat;
  PATTERN_FROM_DURATIONS(pat, LONG_PULSE_DURATIONS);
  vibes_enqueue_custom_pattern(pat);
}

void vibes_double_pulse(void) {
  VibePattern pat;
  PATTERN_FROM_DURATIONS(pat, DOUBLE_PULSE_DURATIONS);
  vibes_enqueue_custom_pattern(pat);
}

void vibes_cancel(void) {
  sys_vibe_pattern_clear();
}

void vibes_enqueue_custom_pattern(VibePattern pattern) {
  if (pattern.durations == NULL) {
    PBL_LOG_ERR("tried to enqueue a null pattern");
    return;
  }

  bool on = true;
  for (uint32_t i = 0; i < pattern.num_segments; ++i) {
    sys_vibe_pattern_enqueue_step(pattern.durations[i], on);
    on = !on;
  }

  sys_vibe_pattern_trigger_start();
}

void vibes_enqueue_custom_pattern_with_amplitudes(VibePatternWithAmplitudes pattern) {
  if (pattern.durations == NULL || pattern.amplitudes == NULL) {
    PBL_LOG_ERR("tried to enqueue a null pattern");
    return;
  }

  for (uint32_t i = 0; i < pattern.num_segments; ++i) {
    uint32_t amp = pattern.amplitudes[i];
    int32_t strength = (int32_t)(amp > 100 ? 100 : amp);
    sys_vibe_pattern_enqueue_step_raw(pattern.durations[i], strength);
  }

  sys_vibe_pattern_trigger_start();
}

