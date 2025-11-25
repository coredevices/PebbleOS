/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "services/normal/activity/activity.h"
#include "services/normal/activity/activity_private.h"
#include "services/normal/activity/activity_calculators.h"

#include "util/units.h"

#include <stdint.h>

// Fakes
static uint8_t s_age_years;
uint8_t activity_prefs_get_age_years(void) {
  return s_age_years;
}

static ActivityGender s_gender;
ActivityGender activity_prefs_get_gender(void) {
  return s_gender;
}

static uint16_t s_weight_dag;
uint16_t activity_prefs_get_weight_dag(void) {
  return s_weight_dag;
}

static uint16_t s_height_mm;
uint16_t activity_prefs_get_height_mm(void) {
  return s_height_mm;
}

typedef enum {
  Human_TallMale,
  Human_ShortMale,
  Human_TallFemale,
  Human_ShortFemale,
  Human_Count,
} Human;

typedef struct HumanPrefs {
  uint8_t age_years;
  ActivityGender gender;
  uint16_t weight_dag;
  uint16_t height_mm;
} HumanPrefs;

#define ACTIVITY_DEFAULT_HEIGHT_MM                1620    // 5'3.8"
// dag - decagram (10 g)
#define ACTIVITY_DEFAULT_WEIGHT_DAG               7539    // 166.2 lbs
#define ACTIVITY_DEFAULT_GENDER                   ActivityGenderFemale
#define ACTIVITY_DEFAULT_AGE_YEARS                30

static void prv_set_user(Human type) {
  const HumanPrefs types[Human_Count] = {
    [Human_TallMale] =    {30, ActivityGenderMale,   7539, 1900},
    [Human_ShortMale] =   {30, ActivityGenderMale,   4536, 1620},
    [Human_TallFemale] =  {30, ActivityGenderFemale, 7539, 1900},
    [Human_ShortFemale] = {30, ActivityGenderFemale, 4536, 1620},
  };

  s_age_years = types[type].age_years;
  s_gender = types[type].gender;
  s_weight_dag = types[type].weight_dag;
  s_height_mm = types[type].height_mm;
}

#define MM_PER_METER 1000
#define M_PER_KM 1000

// =============================================================================================
// Start of unit tests
void test_activity_calculators__initialize(void) {
}

// ---------------------------------------------------------------------------------------
void test_activity_calculators__cleanup(void) {
}

// ---------------------------------------------------------------------------------------
void test_activity_calculators__distance(void) {
  uint32_t steps;
  uint32_t time_ms;

  prv_set_user(Human_TallMale);

  const int walking_cadence_spm = 100;

  // Do an normal walk for 12 mins. An average person should cover ~1km
  const int walk_time = 12;
  steps = walking_cadence_spm * walk_time;
  time_ms = walk_time * SECONDS_PER_MINUTE * MS_PER_SECOND;
  int easy_walk_distance_m = activity_private_compute_distance_mm(steps, time_ms) / MM_PER_METER;
  cl_assert_within(easy_walk_distance_m, 900, 1100);

  // Walk for 12 mins again, but this time faster. More distance should be covered
  steps = (walking_cadence_spm * 1.2) * walk_time;
  time_ms = walk_time * SECONDS_PER_MINUTE * MS_PER_SECOND;
  int fast_walk_distance_m = activity_private_compute_distance_mm(steps, time_ms) / MM_PER_METER;
  cl_assert_gt(fast_walk_distance_m, easy_walk_distance_m);

  // Walk for a long time. People can walk at roughly 5km/h, so we should be close to 50km
  const int long_walk_time = 10 * MINUTES_PER_HOUR;
  steps = walking_cadence_spm * long_walk_time;
  time_ms = long_walk_time * SECONDS_PER_MINUTE * MS_PER_SECOND;
  int long_walk_distance_m = activity_private_compute_distance_mm(steps, time_ms) / MM_PER_METER;
  cl_assert_within(long_walk_distance_m, 48000, 52000);


  // A typical cadence is roughly 165 steps per minute
  const int running_cadence_spm = 165;

  // Running for 25 minutes should come out to roughly 5km
  const int run_time = 25;
  steps = running_cadence_spm * run_time;
  time_ms = run_time * SECONDS_PER_MINUTE * MS_PER_SECOND;
  int normal_run_distance_m = activity_private_compute_distance_mm(steps, time_ms) / MM_PER_METER;
  cl_assert_within(normal_run_distance_m, 4500, 5500);

  // Running for 25 minutes again, but this time faster
  steps = (running_cadence_spm * 1.15) * run_time;
  time_ms = run_time * SECONDS_PER_MINUTE * MS_PER_SECOND;
  int fast_run_distance_m = activity_private_compute_distance_mm(steps, time_ms) / MM_PER_METER;
  cl_assert_within(fast_run_distance_m, 6500, 7000);
  cl_assert_gt(fast_run_distance_m, normal_run_distance_m);

  // Run for 3.5 hours. This is a reasonable marathon time
  const int long_run_time = 3 * MINUTES_PER_HOUR + 30;
  steps = running_cadence_spm * long_run_time;
  time_ms = long_run_time * SECONDS_PER_MINUTE * MS_PER_SECOND;
  int long_run_distance_m = activity_private_compute_distance_mm(steps, time_ms) / MM_PER_METER;
  cl_assert_within(long_run_distance_m, 40000, 44000);

  // Now make a shorter guy run for the same time.
  // He should be in the same ballpark but cover less distance
  prv_set_user(Human_ShortMale);
  int short_guy_distance = activity_private_compute_distance_mm(steps, time_ms) / MM_PER_METER;
  cl_assert_within(short_guy_distance, 36000, 44000);
  cl_assert_gt(long_run_distance_m, short_guy_distance);

  // And finally throw in a specific value so that anyone who touches the function will have to
  // check up on the unit tests
  cl_assert_equal_i(short_guy_distance, 36845);
}

// ---------------------------------------------------------------------------------------
void test_activity_calculators__active_calories(void) {
  uint32_t distance_mm;
  uint32_t time_ms;

  prv_set_user(Human_ShortMale);

  // Walk 1km in 12 minutes
  distance_mm = 1 * M_PER_KM * MM_PER_METER;
  time_ms = 12 * SECONDS_PER_MINUTE * MS_PER_SECOND;
  int walk_calories = activity_private_compute_active_calories(distance_mm, time_ms) / ACTIVITY_CALORIES_PER_KCAL;
  cl_assert_within(walk_calories, 20, 25); // This seems a little low, but not un-reasonable

  // Run 1km in 5 minutes. This should burn more calories than walking
  distance_mm = 1 * M_PER_KM * MM_PER_METER;
  time_ms = 5 * SECONDS_PER_MINUTE * MS_PER_SECOND;
  int run_calories = activity_private_compute_active_calories(distance_mm, time_ms) / ACTIVITY_CALORIES_PER_KCAL;
  cl_assert_within(run_calories, 40, 60); // This also seems a little low, but not un-reasonable
  cl_assert_gt(run_calories, walk_calories);

  // Run 5km in 25 minutes
  distance_mm = 5 * M_PER_KM * MM_PER_METER;
  time_ms = 25 * SECONDS_PER_MINUTE * MS_PER_SECOND;
  int five_k_calories = activity_private_compute_active_calories(distance_mm, time_ms) / ACTIVITY_CALORIES_PER_KCAL;
  cl_assert_within(five_k_calories, 220, 250);
  cl_assert_gt(five_k_calories, run_calories);

  // PG: I went for the following run last night and my garmin watch said I burned 550 calories
  prv_set_user(Human_TallMale);
  distance_mm = 7 * M_PER_KM * MM_PER_METER;
  time_ms = 30 * SECONDS_PER_MINUTE * MS_PER_SECOND;
  int quick_run_calories = activity_private_compute_active_calories(distance_mm, time_ms) / ACTIVITY_CALORIES_PER_KCAL;
  cl_assert_within(quick_run_calories, 520, 580);

  // Run a marathon
  distance_mm = 42 * M_PER_KM * MM_PER_METER;
  time_ms = 3 * MINUTES_PER_HOUR * SECONDS_PER_MINUTE * MS_PER_SECOND;
  int long_run_calories = activity_private_compute_active_calories(distance_mm, time_ms) / ACTIVITY_CALORIES_PER_KCAL;
  cl_assert_within(long_run_calories, 3000, 3200);

  // And finally throw in a specific value so that anyone who touches the function will have to
  // check up on the unit tests
  cl_assert_equal_i(long_run_calories, 3172);
}

// ---------------------------------------------------------------------------------------
void test_activity_calculators__inactive_calories(void) {
  // People burn roughly 2000 (women) - 2400 (men) a day. This number includes active calories
  // so I would expect the values we get to be less than that. I don't know enough to make
  // better real world analogies though...

  uint32_t long_time_m = 24 * MINUTES_PER_HOUR;
  uint32_t short_time_m = 5;

  prv_set_user(Human_ShortMale);

  cl_assert_equal_i(1321, activity_private_compute_resting_calories(long_time_m) / ACTIVITY_CALORIES_PER_KCAL);
  cl_assert_equal_i(4, activity_private_compute_resting_calories(short_time_m) / ACTIVITY_CALORIES_PER_KCAL);

  prv_set_user(Human_TallMale);

  cl_assert_equal_i(1796, activity_private_compute_resting_calories(long_time_m)/ ACTIVITY_CALORIES_PER_KCAL);
  cl_assert_equal_i(6, activity_private_compute_resting_calories(short_time_m) / ACTIVITY_CALORIES_PER_KCAL);

  prv_set_user(Human_ShortFemale);

  cl_assert_equal_i(1155, activity_private_compute_resting_calories(long_time_m) / ACTIVITY_CALORIES_PER_KCAL);
  cl_assert_equal_i(4, activity_private_compute_resting_calories(short_time_m) / ACTIVITY_CALORIES_PER_KCAL);

  prv_set_user(Human_TallFemale);

  cl_assert_equal_i(1630, activity_private_compute_resting_calories(long_time_m) / ACTIVITY_CALORIES_PER_KCAL);
  cl_assert_equal_i(5, activity_private_compute_resting_calories(short_time_m) / ACTIVITY_CALORIES_PER_KCAL);

}
