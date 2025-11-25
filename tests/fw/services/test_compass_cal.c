/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "services/common/ecompass.h"
#include "util/math.h"

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
  [0] = {
    {
      { 2779, -2079, -1309 },
      { 2616, -2007, -1679 },
      { 3179, -2119, -1329 },
      { 3151, -1725, -1359 }
    },
    { 2979, -1954, -1600 }
  },
  [1] = {
    {
      { 3113, -1684, -1384 },
      { 2770, -1627, -1577 },
      { 2636, -1978, -1550 },
      { 2824, -1709, -1969 }
    },
    { 3012, -1930, -1688 }
  },
  [2] = {
    {
      { 2854, -1748, -2000 },
      { 2636, -1847, -1619 },
      { 2812, -2137, -1388 },
      { 3326, -1995, -1372 },
    },
    { 3042, -1935, -1675 }
  },
  [3] = {
    {
      { 3348, -1963, -1391 },
      { 3208, -1615, -1511 },
      { 2814, -1584, -1758 },
      { 3001, -1840, -2066 },
    },
    { 2988, -1972, -1646 }
  },
  [4] = {
    {
      { 3054, -1881, -2082 },
      { 2789, -1672, -1888 },
      { 2664, -1863, -1500 },
      { 3161, -1997, -1293 }
    },
    { 3029, -1927, -1675 }
  },
  [5] = {
    {
      { 3195, -1941, -1300 },
      { 3183, -1615, -1482 },
      { 2927, -1579, -1845 },
      { 3064, -2022, -2094 }
    },
    { 3036 -1947 -1685 }
  }
};

static int16_t expected_final_solution[3] = { 3017, -1948, -1668 };

void test_analytics__initialize(void) {
}

void test_analytics__cleanup(void) {
}

int32_t integer_sqrt(int64_t x) {
  if (x < 0) {
    return 0;
  }
  int64_t last_res = 0x3fff;
  uint16_t iterations = 0;
  while ((last_res > 0) && (iterations < 15)) {
    last_res = ((x / last_res) + last_res)/2;
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

  int16_t solution[3];
  int rv;
  for (int i = 0; i < num_entries; i++) {
    for (int j = 0; j < 4; j++) {
      rv = ecomp_corr_add_raw_mag_sample(s_sample_data[i].raw_samples[j],
          NULL, solution);
      if (j != 3) {
        // add the same sample twice to make sure close values are thrown away
        rv = ecomp_corr_add_raw_mag_sample(s_sample_data[i].raw_samples[j],
            NULL, solution);
        cl_assert_equal_i(rv, MagCalStatusNoSolution);
      }
    }
    cl_assert_equal_i(rv, ((num_entries - 1) == i) ?
        MagCalStatusNewLockedSolutionAvail : MagCalStatusNewSolutionAvail);

    if (rv == MagCalStatusNewSolutionAvail) {
      solution_and_estimate_match(solution, s_sample_data[i].sphere_fit_corr);
    // should be avg of last 3 solutions
    } else if (rv == MagCalStatusNewLockedSolutionAvail) {
      solution_and_estimate_match(solution, expected_final_solution);
    }
  }
}

  

  
