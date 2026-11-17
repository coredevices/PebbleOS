/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#include <pbl/kraepelin/kraepelin.h>
#include <pbl/util/units.h>
#include <pbl/vanhees/vanhees.h>

#include <clar.h>

#define EPOCH_SAMPLES  (5 * PBL_VANHEES_SAMPLE_HZ)
#define EPOCHS_PER_MIN 12
#define MINUTE_SAMPLES (EPOCHS_PER_MIN * EPOCH_SAMPLES)
// Minute updates before the first verdict
#define WARM_UP_MINUTES (PBL_VANHEES_TIME_THRESHOLD_M + PBL_VANHEES_LAG_M)

// M_PI is not part of standard C
#define PI 3.14159265358979323846

static struct pbl_vanhees *s_state;
static uint32_t s_noise;

// Small deterministic noise, within +/- 8 mG
static int16_t prv_noise(void) {
  s_noise = (s_noise * 1103515245) + 12345;
  return (int16_t)((s_noise >> 16) % 17) - 8;
}

// 1 G with the Z axis at z_deg to the horizontal plane, rotated by xy_deg around it
static struct pbl_vanhees_sample prv_sample(double z_deg, double xy_deg) {
  const double z_rad = z_deg * PI / 180;
  const double xy_rad = xy_deg * PI / 180;
  return (struct pbl_vanhees_sample){
    .x = (int16_t)lround(1000 * cos(z_rad) * cos(xy_rad)) + prv_noise(),
    .y = (int16_t)lround(1000 * cos(z_rad) * sin(xy_rad)) + prv_noise(),
    .z = (int16_t)lround(1000 * sin(z_rad)) + prv_noise(),
  };
}

static void prv_feed_epochs(int num_epochs, double z_deg, double xy_deg) {
  for (int i = 0; i < num_epochs * EPOCH_SAMPLES; i++) {
    const struct pbl_vanhees_sample sample = prv_sample(z_deg, xy_deg);
    pbl_vanhees_analyze_samples(s_state, &sample, 1);
  }
}

// Feed a still minute and return its update: -1 without verdict, else whether inactive
static int prv_minute(double z_deg) {
  prv_feed_epochs(EPOCHS_PER_MIN, z_deg, 0);
  bool inactive = true;
  if (!pbl_vanhees_minute_update(s_state, &inactive)) {
    cl_assert(!inactive);
    return -1;
  }
  return inactive;
}

static void prv_warm_up(double z_deg) {
  for (int i = 0; i < WARM_UP_MINUTES - 1; i++) {
    cl_assert_equal_i(prv_minute(z_deg), -1);
  }
  cl_assert_equal_i(prv_minute(z_deg), 1);
}

void test_vanhees__initialize(void) {
  s_noise = 1;
  s_state = malloc(pbl_vanhees_state_size());
  pbl_vanhees_init(s_state);
}

void test_vanhees__cleanup(void) {
  free(s_state);
  s_state = nullptr;
}

void test_vanhees__still_is_inactive(void) {
  prv_warm_up(30);
  for (int i = 0; i < 60; i++) {
    cl_assert_equal_i(prv_minute(30), 1);
  }
}

void test_vanhees__angle_threshold(void) {
  prv_warm_up(30);

  // Changes below the threshold go unnoticed, in both directions
  double z_deg = 30;
  for (int i = 0; i < 20; i++) {
    z_deg += (i % 2) ? -4 : 4;
    cl_assert_equal_i(prv_minute(z_deg), 1);
  }

  // A change above it makes the minute it happens in active, and only that one
  const int k_lag = PBL_VANHEES_LAG_M;
  for (int i = 0; i < 20; i++) {
    cl_assert_equal_i(prv_minute((i < 10) ? 30 : 36), (i == 10 + k_lag) ? 0 : 1);
  }
}

void test_vanhees__face_up_to_face_down(void) {
  prv_warm_up(60);
  for (int i = 0; i < 20; i++) {
    cl_assert_equal_i(prv_minute((i < 10) ? 60 : -60), (i == 10 + PBL_VANHEES_LAG_M) ? 0 : 1);
  }
}

// Only the angle of the Z axis counts
void test_vanhees__rotation_around_z_is_ignored(void) {
  prv_warm_up(20);
  for (int i = 0; i < 20; i++) {
    prv_feed_epochs(EPOCHS_PER_MIN, 20, i * 40);
    bool inactive;
    cl_assert(pbl_vanhees_minute_update(s_state, &inactive));
    cl_assert(inactive);
  }
}

void test_vanhees__frequent_changes_are_active(void) {
  prv_warm_up(0);

  // A posture change every 2 minutes
  int num_inactive = 0;
  for (int i = 0; i < 60; i++) {
    const int result = prv_minute(((i / 2) % 2) ? 10 : 0);
    cl_assert(result != -1);
    if (i >= WARM_UP_MINUTES) {
      num_inactive += result;
    }
  }
  cl_assert_equal_i(num_inactive, 0);
}

void test_vanhees__bouts_between_changes(void) {
  prv_warm_up(0);

  // A posture change every 7 minutes: the 6 minutes without one are a bout
  const int k_period_m = 7;
  for (int i = 0; i < 10 * k_period_m; i++) {
    const int result = prv_minute(((i / k_period_m) % 2) ? 10 : 0);
    if (i < WARM_UP_MINUTES) {
      continue;
    }
    const int scored_minute = i - PBL_VANHEES_LAG_M;
    cl_assert_equal_i(result, (scored_minute % k_period_m) ? 1 : 0);
  }
}

// A change every 4 minutes leaves 4 minute runs, shorter than a bout
void test_vanhees__short_runs_are_active(void) {
  prv_warm_up(0);
  for (int i = 0; i < 40; i++) {
    const int result = prv_minute(((i / 4) % 2) ? 10 : 0);
    if (i >= WARM_UP_MINUTES) {
      cl_assert_equal_i(result, 0);
    }
  }
}

// The median ignores movement that lasts less than half an epoch
void test_vanhees__short_movement_is_ignored(void) {
  prv_warm_up(10);
  for (int i = 0; i < 20; i++) {
    for (int epoch = 0; epoch < EPOCHS_PER_MIN; epoch++) {
      for (int j = 0; j < EPOCH_SAMPLES; j++) {
        const bool moving = (epoch % 2) && (j < EPOCH_SAMPLES / 3);
        const struct pbl_vanhees_sample sample = prv_sample(moving ? 70 : 10, 0);
        pbl_vanhees_analyze_samples(s_state, &sample, 1);
      }
    }
    bool inactive;
    cl_assert(pbl_vanhees_minute_update(s_state, &inactive));
    cl_assert(inactive);
  }
}

void test_vanhees__batches_and_minute_jitter(void) {
  // Samples arrive in batches that do not line up with epochs or minutes
  int minutes = 0;
  int pending = 0;
  for (int i = 0; minutes < 30; i++) {
    struct pbl_vanhees_sample batch[32];
    const int num = 20 + (i % 13);
    for (int j = 0; j < num; j++) {
      batch[j] = prv_sample(45, 0);
    }
    pbl_vanhees_analyze_samples(s_state, batch, num);
    pending += num;
    if (pending >= MINUTE_SAMPLES) {
      pending -= MINUTE_SAMPLES;
      bool inactive;
      const bool valid = pbl_vanhees_minute_update(s_state, &inactive);
      // The first minute may end an epoch short
      if (minutes >= WARM_UP_MINUTES) {
        cl_assert(valid);
        cl_assert(inactive);
      }
      minutes++;
    }
  }
}

void test_vanhees__gap_discards_history(void) {
  prv_warm_up(30);

  // A minute without samples
  bool inactive;
  cl_assert(!pbl_vanhees_minute_update(s_state, &inactive));
  cl_assert(!inactive);

  // A posture change across the gap does not count
  prv_warm_up(-30);
}

void test_vanhees__saturated_samples(void) {
  for (int i = 0; i < WARM_UP_MINUTES + 5; i++) {
    for (int j = 0; j < MINUTE_SAMPLES; j++) {
      const struct pbl_vanhees_sample sample = {INT16_MAX, INT16_MIN, INT16_MIN};
      pbl_vanhees_analyze_samples(s_state, &sample, 1);
    }
    bool inactive;
    const bool valid = pbl_vanhees_minute_update(s_state, &inactive);
    cl_assert_equal_b(valid, i >= WARM_UP_MINUTES - 1);
    cl_assert_equal_b(inactive, valid);
  }
}

// ---------------------------------------------------------------------------------------
// A night through both libraries, the way the activity service runs them
#define NIGHT_START_UTC 1787618400
// VMC far too high for the Kraepelin sleep score
#define NIGHT_VMC 2000

static time_t s_sleep_start_utc;
static uint32_t s_sleep_len_m;
static int s_num_sleep_sessions;

static void prv_session_cb(void *context, enum pbl_kraepelin_activity activity_type,
                           time_t start_utc, uint32_t len_sec, bool ongoing, bool delete,
                           uint32_t steps, uint32_t resting_calories, uint32_t active_calories,
                           uint32_t distance_mm) {
  if ((activity_type != PBL_KRAEPELIN_ACTIVITY_SLEEP) || ongoing) {
    return;
  }
  cl_assert(!delete);
  s_sleep_start_utc = start_utc;
  s_sleep_len_m = len_sec / PBL_SEC_PER_MIN;
  s_num_sleep_sessions++;
}

// Feed a minute at z_deg to both libraries
static void prv_night_minute(struct pbl_kraepelin *k_state, time_t *now, double z_deg,
                             bool van_hees) {
  prv_feed_epochs(EPOCHS_PER_MIN, z_deg, 0);
  *now += PBL_SEC_PER_MIN;

  bool inactive;
  pbl_vanhees_minute_update(s_state, &inactive);
  if (van_hees) {
    pbl_kraepelin_set_sleep_minute(k_state, inactive);
  }
  // The orientation keeps changing, or the watch would look not worn
  const uint8_t orientation = ((*now / PBL_SEC_PER_MIN) % 2) ? 0x35 : 0x47;
  pbl_kraepelin_activities_update(k_state, *now, 0 /*steps*/, NIGHT_VMC, orientation,
                                  false /*definitely_not_worn*/, 0 /*resting_calories*/,
                                  0 /*active_calories*/, 0 /*distance_mm*/, false /*shutting_down*/,
                                  prv_session_cb, nullptr);
}

static void prv_night(bool van_hees) {
  struct pbl_kraepelin *k_state = calloc(1, pbl_kraepelin_state_size());
  pbl_kraepelin_init(k_state, nullptr, nullptr);
  s_num_sleep_sessions = 0;
  time_t now = NIGHT_START_UTC;

  // An evening awake: the wrist moves every minute
  const int k_evening_m = 30;
  for (int i = 0; i < k_evening_m; i++) {
    prv_night_minute(k_state, &now, (i % 2) ? 40 : 0, van_hees);
  }
  // 6 hours asleep, turning over every 45 minutes
  const int k_asleep_m = 6 * PBL_MIN_PER_HOUR;
  for (int i = 0; i < k_asleep_m; i++) {
    prv_night_minute(k_state, &now, ((i / 45) % 2) ? -50 : 20, van_hees);
  }
  // A morning awake
  for (int i = 0; i < 40; i++) {
    prv_night_minute(k_state, &now, (i % 2) ? 40 : 0, van_hees);
  }
  free(k_state);

  if (!van_hees) {
    cl_assert_equal_i(s_num_sleep_sessions, 0);
    return;
  }
  cl_assert_equal_i(s_num_sleep_sessions, 1);
  const int start_m = (s_sleep_start_utc - NIGHT_START_UTC) / PBL_SEC_PER_MIN;
  cl_assert(abs(start_m - k_evening_m) <= 2);
  cl_assert(abs((int)s_sleep_len_m - k_asleep_m) <= 2);
}

void test_vanhees__night_with_kraepelin_sessions(void) {
  prv_night(true /*van_hees*/);
}

void test_vanhees__night_without_verdicts(void) {
  prv_night(false /*van_hees*/);
}
