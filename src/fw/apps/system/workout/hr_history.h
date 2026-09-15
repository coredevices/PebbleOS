/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define WORKOUT_HR_HISTORY_SECONDS 60

typedef struct WorkoutHrSample {
  int32_t elapsed_s;
  uint16_t bpm;
} WorkoutHrSample;

typedef struct WorkoutHrHistory {
  WorkoutHrSample samples[WORKOUT_HR_HISTORY_SECONDS];
  uint8_t next;
  uint8_t count;
} WorkoutHrHistory;

static inline const WorkoutHrSample *workout_hr_history_get(const WorkoutHrHistory *history,
                                                            unsigned index) {
  if (!history || index >= history->count) {
    return NULL;
  }
  return &history->samples[(history->next + WORKOUT_HR_HISTORY_SECONDS - history->count + index) %
                           WORKOUT_HR_HISTORY_SECONDS];
}

static inline void workout_hr_history_add(WorkoutHrHistory *history, int32_t elapsed_s, int bpm) {
  if (elapsed_s < 0) {
    return;
  }
  const WorkoutHrSample sample = {.elapsed_s = elapsed_s,
                                  .bpm = bpm > 0 && bpm <= UINT16_MAX ? bpm : 0};
  if (history->count) {
    const int last = (history->next + WORKOUT_HR_HISTORY_SECONDS - 1) % WORKOUT_HR_HISTORY_SECONDS;
    if (elapsed_s == history->samples[last].elapsed_s) {
      history->samples[last] = sample;
      return;
    }
    if (elapsed_s < history->samples[last].elapsed_s) {
      history->count = 0;
      history->next = 0;
    }
  }
  history->samples[history->next] = sample;
  history->next = (history->next + 1) % WORKOUT_HR_HISTORY_SECONDS;
  if (history->count < WORKOUT_HR_HISTORY_SECONDS) {
    history->count++;
  }
}

static inline bool workout_hr_history_range(const WorkoutHrHistory *history, int32_t elapsed_s,
                                            int *minimum, int *maximum) {
  int low = UINT16_MAX;
  int high = 0;
  for (unsigned i = 0; history && i < history->count; i++) {
    const WorkoutHrSample *sample = workout_hr_history_get(history, i);
    const int age = elapsed_s - sample->elapsed_s;
    if (!sample->bpm || age < 0 || age >= WORKOUT_HR_HISTORY_SECONDS) {
      continue;
    }
    if (sample->bpm < low) {
      low = sample->bpm;
    }
    if (sample->bpm > high) {
      high = sample->bpm;
    }
  }
  if (!high) {
    *minimum = 0;
    *maximum = 10;
    return false;
  }
  // Round the scale to tens, keeping constant readings away from the axes.
  *minimum = low / 10 * 10;
  *maximum = (high + 9) / 10 * 10;
  if (*minimum == *maximum) {
    *minimum -= 10;
    *maximum += 10;
  }
  return true;
}
