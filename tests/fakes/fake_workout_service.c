/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "fake_workout_service.h"

static bool s_is_paused;
static bool s_is_ongoing;
static int32_t s_steps;
static int32_t s_duration_s;
static int32_t s_distance_m;
static int32_t s_current_bpm;
static int32_t s_current_hr_zone;
static int32_t s_active_kcal;
static int32_t s_avg_bpm;
static int32_t s_spo2;
static int32_t s_spo2_min;
static bool s_spo2_measuring;

void workout_service_set_spo2(int32_t last_pct, int32_t min_pct, bool measuring) {
  s_spo2 = last_pct;
  s_spo2_min = min_pct;
  s_spo2_measuring = measuring;
}

bool workout_service_get_spo2(int32_t *last_pct_out, int32_t *min_pct_out, bool *measuring_out) {
  if (last_pct_out) {
    *last_pct_out = s_spo2;
  }
  if (min_pct_out) {
    *min_pct_out = s_spo2_min;
  }
  if (measuring_out) {
    *measuring_out = s_spo2_measuring;
  }
  return s_is_ongoing;
}

void workout_service_set_avg_hr(int32_t bpm) {
  s_avg_bpm = bpm;
}

bool workout_service_get_avg_hr(int32_t *bpm) {
  *bpm = s_avg_bpm;
  return true;
}

void workout_service_set_active_kcalories(int32_t active) {
  s_active_kcal = active;
}

void workout_service_get_active_kcalories(int32_t *active) {
  *active = s_active_kcal;
}

bool workout_service_is_workout_ongoing(void) {
  return s_is_ongoing;
}

bool workout_service_start_workout(ActivitySessionType type) {
  s_is_ongoing = true;
  return true;
}

bool workout_service_pause_workout(bool should_be_paused) {
  s_is_paused = should_be_paused;
  return true;
}

bool workout_service_stop_workout(void) {
  s_is_ongoing = false;
  return true;
}

bool workout_service_is_paused(void) {
  return s_is_paused;
}

bool workout_service_get_current_workout_type(ActivitySessionType *type_out) {
  return false;
}

bool workout_service_get_current_workout_info(int32_t *steps_out, int32_t *duration_s_out,
                                              int32_t *distance_m_out, int32_t *current_bpm_out,
                                              HRZone *current_hr_zone_out) {
  *steps_out = s_steps;
  *duration_s_out = s_duration_s;
  *distance_m_out = s_distance_m;
  *current_bpm_out = s_current_bpm;
  *current_hr_zone_out = s_current_hr_zone;

  return true;
}

bool workout_service_set_current_workout_info(int32_t steps, int32_t duration_s, int32_t distance_m,
                                              int32_t current_bpm, HRZone current_hr_zone) {
  s_steps = steps;
  s_duration_s = duration_s;
  s_distance_m = distance_m;
  s_current_bpm = current_bpm;
  s_current_hr_zone = current_hr_zone;

  return true;
}
