/*
 * Copyright 2025 Core Devices LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "voice_agc.h"

#include "util/math.h"

#include <limits.h>
#include <string.h>
#include <system/passert.h>

#define AGC_Q_BITS (12)
#define AGC_Q_UNIT (1 << AGC_Q_BITS)
#define AGC_TARGET_LEVEL (6000U)
#define AGC_SILENCE_LEVEL (200U)
#define AGC_MIN_GAIN_Q12 (AGC_Q_UNIT / 4)          // 0.25x
#define AGC_MAX_GAIN_Q12 (AGC_Q_UNIT * 8)          // 8x
#define AGC_DEFAULT_GAIN_Q12 (AGC_Q_UNIT)
#define AGC_LEVEL_SMOOTHING (8U)
#define AGC_ATTACK_FRAMES (4)
#define AGC_RELEASE_FRAMES (16)
#define AGC_SILENCE_HANG_FRAMES (12)

static int32_t prv_agc_adjust_gain(int32_t current_gain_q12, int32_t desired_gain_q12) {
  if (desired_gain_q12 > current_gain_q12) {
    const int32_t delta = desired_gain_q12 - current_gain_q12;
    int32_t step = MAX(1, (delta + (AGC_ATTACK_FRAMES - 1)) / AGC_ATTACK_FRAMES);
    step = MIN(step, delta);
    return current_gain_q12 + step;
  }

  const int32_t delta = current_gain_q12 - desired_gain_q12;
  int32_t step = MAX(1, (delta + (AGC_RELEASE_FRAMES - 1)) / AGC_RELEASE_FRAMES);
  step = MIN(step, delta);
  return current_gain_q12 - step;
}

void voice_agc_init(VoiceAgcState *agc) {
  PBL_ASSERTN(agc != NULL);

  memset(agc, 0, sizeof(*agc));
  agc->enabled = true;
  agc->gain_q12 = AGC_DEFAULT_GAIN_Q12;
  agc->last_applied_gain_q12 = AGC_DEFAULT_GAIN_Q12;
  agc->smoothed_level = AGC_TARGET_LEVEL;
  agc->silence_run = 0;
}

void voice_agc_process_frame(VoiceAgcState *agc, int16_t *samples, size_t sample_count) {
  PBL_ASSERTN(agc != NULL && agc->enabled && sample_count > 0 && samples != NULL);

  uint64_t sum_abs = 0;
  int32_t peak = 0;
  for (size_t i = 0; i < sample_count; i++) {
    const int32_t sample = samples[i];
    const int32_t abs_sample = sample >= 0 ? sample : -sample;
    sum_abs += (uint32_t)abs_sample;
    peak = MAX(peak, abs_sample);
  }

  const uint32_t instant_level = sample_count > 0 ? (uint32_t)(sum_abs / sample_count) : 0U;
  const uint32_t bounded_level = MAX(instant_level, 1U);

  if (agc->smoothed_level == 0) {
    agc->smoothed_level = bounded_level;
  } else {
    const uint32_t weight = AGC_LEVEL_SMOOTHING;
    const uint64_t acc = (uint64_t)agc->smoothed_level * (weight - 1U) + bounded_level;
    agc->smoothed_level = (uint32_t)((acc + (weight / 2U)) / weight);
  }

  if (instant_level <= AGC_SILENCE_LEVEL) {
    if (agc->silence_run < AGC_SILENCE_HANG_FRAMES) {
      agc->silence_run++;
    }
  } else {
    agc->silence_run = 0;
  }

  int32_t desired_gain_q12 = AGC_DEFAULT_GAIN_Q12;
  if (instant_level > AGC_SILENCE_LEVEL || agc->silence_run < AGC_SILENCE_HANG_FRAMES) {
    const uint32_t effective_level = MAX(agc->smoothed_level, AGC_SILENCE_LEVEL);
    const uint64_t numerator = (uint64_t)AGC_TARGET_LEVEL << AGC_Q_BITS;
    desired_gain_q12 = (int32_t)(numerator / effective_level);
  }

  desired_gain_q12 = CLIP(desired_gain_q12, AGC_MIN_GAIN_Q12, AGC_MAX_GAIN_Q12);
  agc->gain_q12 = prv_agc_adjust_gain(agc->gain_q12, desired_gain_q12);
  agc->gain_q12 = CLIP(agc->gain_q12, AGC_MIN_GAIN_Q12, AGC_MAX_GAIN_Q12);

  int32_t applied_gain_q12 = agc->gain_q12;
  if (peak > 0) {
    const int64_t max_gain_from_peak = ((int64_t)INT16_MAX << AGC_Q_BITS) / peak;
    int32_t safe_gain_q12 = (int32_t)MIN(max_gain_from_peak, (int64_t)AGC_MAX_GAIN_Q12);
    if (safe_gain_q12 < AGC_MIN_GAIN_Q12) {
      safe_gain_q12 = AGC_MIN_GAIN_Q12;
    }
    applied_gain_q12 = MIN(applied_gain_q12, safe_gain_q12);
  }

  applied_gain_q12 = CLIP(applied_gain_q12, 0, AGC_MAX_GAIN_Q12);

  for (size_t i = 0; i < sample_count; i++) {
    int64_t scaled = (int64_t)samples[i] * (int64_t)applied_gain_q12;
    scaled = (scaled + (int64_t)(1 << (AGC_Q_BITS - 1))) >> AGC_Q_BITS;
    scaled = CLIP(scaled, (int64_t)INT16_MIN, (int64_t)INT16_MAX);
    samples[i] = (int16_t)scaled;
  }

  agc->last_applied_gain_q12 = applied_gain_q12;
}
