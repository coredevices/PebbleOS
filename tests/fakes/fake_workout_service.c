/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "fake_workout_service.h"

#include <string.h>

static bool s_is_paused;
static bool s_is_ongoing;
static int32_t s_steps;
static int32_t s_duration_s;
static int32_t s_distance_m;
static int32_t s_current_bpm;
static int32_t s_current_hr_zone;
static int32_t s_active_kcal;
static int32_t s_avg_bpm;
static int32_t s_hr_samples;
static int32_t s_spo2;
static int32_t s_spo2_min;
static bool s_spo2_measuring;
static int32_t s_hr_zone_time_s[HRZoneCount];

void workout_service_set_hr_zone_time(const int32_t *hr_zone_time_s) {
  memcpy(s_hr_zone_time_s, hr_zone_time_s, sizeof(s_hr_zone_time_s));
}

void workout_service_set_spo2(int32_t last_pct, int32_t min_pct, bool measuring) {
  s_spo2 = last_pct;
  s_spo2_min = min_pct;
  s_spo2_measuring = measuring;
}

void workout_service_set_avg_hr(int32_t bpm) {
  s_avg_bpm = bpm;
}

void workout_service_set_hr_samples(int32_t samples) {
  s_hr_samples = samples;
}

void workout_service_set_active_kcalories(int32_t active) {
  s_active_kcal = active;
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
  if (!s_is_ongoing) {
    return false;
  }
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

bool workout_service_get_current_workout_metrics(WorkoutServiceMetrics *metrics_out) {
  if (!s_is_ongoing) {
    return false;
  }
  *metrics_out = (WorkoutServiceMetrics){
    .steps = s_steps,
    .duration_s = s_duration_s,
    .distance_m = s_distance_m,
    .current_bpm = s_current_bpm,
    .current_hr_zone = s_current_hr_zone,
    .hr_samples = s_hr_samples,
    .avg_bpm = s_avg_bpm,
    .active_kcal = s_active_kcal,
    .spo2_pct = s_spo2,
    .spo2_min_pct = s_spo2_min,
    .spo2_measuring = s_spo2_measuring,
  };
  memcpy(metrics_out->hr_zone_time_s, s_hr_zone_time_s, sizeof(s_hr_zone_time_s));
  return true;
}
