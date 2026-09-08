/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/ecompass.h"
#include "pbl/util/math.h"

#include "clar.h"
#include "stubs_language_ui.h"
#include "stubs_logging.h"
#include "stubs_pbl_malloc.h"
#include "stubs_serial.h"

#include <stdint.h>

typedef struct {
  int16_t raw_samples[4][3];
  int16_t sphere_fit_corr[3];
} SampleData;

static SampleData s_sample_data[6] = {
    [0] = {{{2779, -2079, -1309}, {2616, -2007, -1679}, {3179, -2119, -1329}, {3151, -1725, -1359}},
           {2979, -1954, -1600}},
    [1] = {{{3113, -1684, -1384}, {2770, -1627, -1577}, {2636, -1978, -1550}, {2824, -1709, -1969}},
           {3012, -1930, -1688}},
    [2] = {{
               {2854, -1748, -2000},
               {2636, -1847, -1619},
               {2812, -2137, -1388},
               {3326, -1995, -1372},
           },
           {3042, -1935, -1675}},
    [3] = {{
               {3348, -1963, -1391},
               {3208, -1615, -1511},
               {2814, -1584, -1758},
               {3001, -1840, -2066},
           },
           {2988, -1972, -1646}},
    [4] = {{{3054, -1881, -2082}, {2789, -1672, -1888}, {2664, -1863, -1500}, {3161, -1997, -1293}},
           {3029, -1927, -1675}},
    [5] = {{{3195, -1941, -1300}, {3183, -1615, -1482}, {2927, -1579, -1845}, {3064, -2022, -2094}},
           {3036 - 1947 - 1685}}};

static int16_t expected_final_solution[3] = {3017, -1948, -1668};

int32_t integer_sqrt(int64_t x) {
  if (x < 0) {
    return 0;
  }
  int64_t last_res = 0x3fff;
  uint16_t iterations = 0;
  while ((last_res > 0) && (iterations < 15)) {
    last_res = ((x / last_res) + last_res) / 2;
    iterations++;
  }
  return (last_res);
}

static void solution_and_estimate_match(int16_t *solution, int16_t *correction) {
  for (int i = 0; i < 3; i++) {
    int diff = ABS(solution[i] - correction[i]);
    cl_assert(diff < 2);
  }
}

void test_compass_cal__sphere_fit(void) {
  int num_entries = sizeof(s_sample_data) / sizeof(SampleData);

  ecomp_corr_reset();

  int16_t solution[3];
  int rv;
  for (int i = 0; i < num_entries; i++) {
    for (int j = 0; j < 4; j++) {
      rv = ecomp_corr_add_raw_mag_sample(s_sample_data[i].raw_samples[j], NULL, solution);
      if (j != 3) {
        // add the same sample twice to make sure close values are thrown away
        rv = ecomp_corr_add_raw_mag_sample(s_sample_data[i].raw_samples[j], NULL, solution);
        cl_assert_equal_i(rv, MagCalStatusNoSolution);
      }
    }
    cl_assert_equal_i(rv, ((num_entries - 1) == i) ? MagCalStatusNewLockedSolutionAvail
                                                   : MagCalStatusNewSolutionAvail);

    if (rv == MagCalStatusNewSolutionAvail) {
      solution_and_estimate_match(solution, s_sample_data[i].sphere_fit_corr);
      // should be avg of last 3 solutions
    } else if (rv == MagCalStatusNewLockedSolutionAvail) {
      solution_and_estimate_match(solution, expected_final_solution);
    }
  }
}

//////////////////////////////////////////////////////////////////////////////
// Synthetic motion helpers
//
// Raw samples lie on a sphere whose radius is the local field strength
// (1 count = 0.1 uT) centered on the hard-iron offset. Watch motion is
// emulated by interpolating between waypoint directions, mimicking the
// tilt coaching of the calibration UI, plus MMC5603NJ-level noise.

static uint32_t s_rand_state;

static int prv_noise(void) {
  // deterministic pseudo-noise in [-3, 3]
  s_rand_state = s_rand_state * 1103515245u + 12345u;
  return (int)((s_rand_state >> 16) % 7) - 3;
}

typedef struct {
  const int16_t *center;
  int32_t radius;  // 0 = keep interpolated point (coplanar path)
  const int32_t (*waypoints)[3];
  int n_waypoints;
  int steps_per_leg;
  int leg;
  int step;
} MotionSim;

static void prv_next_sample(MotionSim *sim, int16_t *sample) {
  const int32_t *a = sim->waypoints[sim->leg % sim->n_waypoints];
  const int32_t *b = sim->waypoints[(sim->leg + 1) % sim->n_waypoints];

  int32_t v[3];
  for (int i = 0; i < 3; i++) {
    v[i] = a[i] + ((b[i] - a[i]) * sim->step) / sim->steps_per_leg;
  }

  if (sim->radius != 0) {
    // project the interpolated point back onto the sphere
    int32_t norm = integer_sqrt((int64_t)v[0] * v[0] + (int64_t)v[1] * v[1] + (int64_t)v[2] * v[2]);
    for (int i = 0; i < 3; i++) {
      v[i] = (v[i] * sim->radius) / norm;
    }
  }

  for (int i = 0; i < 3; i++) {
    sample[i] = sim->center[i] + v[i] + prv_noise();
  }

  if (++sim->step >= sim->steps_per_leg) {
    sim->step = 0;
    sim->leg++;
  }
}

// Directions within a 50 degree cap around +z on a radius-228 sphere
// (~22.8 uT, South Atlantic Anomaly level). Max pairwise chord is ~349,
// below the strict 370 point-to-point gate.
static const int32_t s_weak_waypoints[][3] = {
    {0, 0, 228},                        // pole
    {175, 0, 147},                      // rim, azimuth 0
    {-87, 151, 147},                    // rim, azimuth 120
    {0, 0, 228},     {-87, -151, 147},  // rim, azimuth 240
    {87, 151, 147},                     // rim, azimuth 60
    {0, 0, 228},     {-175, 0, 147},    // rim, azimuth 180
    {87, -151, 147},                    // rim, azimuth 300
};

// Directions within a 70 degree cap around +z on a radius-470 sphere (47 uT)
static const int32_t s_normal_waypoints[][3] = {
    {0, 0, 470},                          // pole
    {442, 0, 161},                        // rim, azimuth 0
    {-221, 383, 161},                     // rim, azimuth 120
    {0, 0, 470},      {-221, -383, 161},  // rim, azimuth 240
    {221, 383, 161},                      // rim, azimuth 60
    {0, 0, 470},      {-442, 0, 161},     // rim, azimuth 180
    {221, -383, 161},                     // rim, azimuth 300
};

// Coplanar ring (z = 147 slice of the weak-field sphere)
static const int32_t s_ring_waypoints[][3] = {
    {175, 0, 147},  {87, 151, 147},   {-87, 151, 147},
    {-175, 0, 147}, {-87, -151, 147}, {87, -151, 147},
};

// In a weak geomagnetic field all samples sit on a sphere of radius ~228, so
// the historical fixed gates were unsatisfiable: point-to-point > 370 needs
// ~108 degrees of tilt separation and point-to-line > 370 is geometrically
// impossible at this radius (max ~ R + sqrt(R^2 - 185^2) < 370). The old
// fallback never triggered without a point-to-point pass, so calibration
// never completed. Progressive relaxation must converge here.
void test_compass_cal__weak_field_converges(void) {
  static const int16_t center[3] = {-1250, 830, 1980};

  ecomp_corr_reset();
  s_rand_state = 0x12345678;

  MotionSim sim = {
      .center = center,
      .radius = 228,
      .waypoints = s_weak_waypoints,
      .n_waypoints = sizeof(s_weak_waypoints) / sizeof(s_weak_waypoints[0]),
      .steps_per_leg = 12,
  };

  int16_t solution[3] = {0};
  int locked_at = -1;
  for (int n = 0; n < 6000; n++) {
    int16_t sample[3];
    prv_next_sample(&sim, sample);
    int rv = ecomp_corr_add_raw_mag_sample(sample, NULL, solution);
    if (n < 300) {
      // the first window still runs the strict gates: no solution possible,
      // which is what the old fixed-threshold code produced indefinitely
      cl_assert_equal_i(rv, MagCalStatusNoSolution);
    }
    if (rv == MagCalStatusNewLockedSolutionAvail) {
      locked_at = n;
      break;
    }
  }

  cl_assert(locked_at > 0);
  for (int i = 0; i < 3; i++) {
    cl_assert(ABS(solution[i] - center[i]) <= 30);
  }
}

// In a normal field the strict gates are satisfiable and calibration must
// still lock quickly, before any threshold relaxation kicks in
void test_compass_cal__normal_field_converges_quickly(void) {
  static const int16_t center[3] = {2400, -1700, -900};

  ecomp_corr_reset();
  s_rand_state = 0x87654321;

  MotionSim sim = {
      .center = center,
      .radius = 470,
      .waypoints = s_normal_waypoints,
      .n_waypoints = sizeof(s_normal_waypoints) / sizeof(s_normal_waypoints[0]),
      .steps_per_leg = 12,
  };

  int16_t solution[3] = {0};
  int locked_at = -1;
  for (int n = 0; n < 1200; n++) {
    int16_t sample[3];
    prv_next_sample(&sim, sample);
    int rv = ecomp_corr_add_raw_mag_sample(sample, NULL, solution);
    if (rv == MagCalStatusNewLockedSolutionAvail) {
      locked_at = n;
      break;
    }
  }

  cl_assert(locked_at > 0);
  cl_assert(locked_at < 300);  // locked within the first (strict) window
  for (int i = 0; i < 3; i++) {
    cl_assert(ABS(solution[i] - center[i]) <= 20);
  }
}

// Degenerate inputs must never produce a solution, even once the threshold
// has relaxed to its floor
void test_compass_cal__degenerate_sets_rejected(void) {
  static const int16_t center[3] = {-1250, 830, 1980};

  // stationary watch: spread is pure sensor noise
  ecomp_corr_reset();
  s_rand_state = 0xdeadbeef;
  int16_t solution[3];
  for (int n = 0; n < 2000; n++) {
    int16_t sample[3] = {
        (int16_t)(810 + prv_noise()),
        (int16_t)(-455 + prv_noise()),
        (int16_t)(620 + prv_noise()),
    };
    int rv = ecomp_corr_add_raw_mag_sample(sample, NULL, solution);
    cl_assert_equal_i(rv, MagCalStatusNoSolution);
  }

  // near-coplanar motion: point-to-plane gate must reject the fourth point
  ecomp_corr_reset();
  MotionSim sim = {
      .center = center,
      .radius = 0,  // stay in the ring plane
      .waypoints = s_ring_waypoints,
      .n_waypoints = sizeof(s_ring_waypoints) / sizeof(s_ring_waypoints[0]),
      .steps_per_leg = 12,
  };
  for (int n = 0; n < 3000; n++) {
    int16_t sample[3];
    prv_next_sample(&sim, sample);
    int rv = ecomp_corr_add_raw_mag_sample(sample, NULL, solution);
    cl_assert_equal_i(rv, MagCalStatusNoSolution);
  }
}
