/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <math.h>

#include <pbl/util/freq_track.h>

#include <clar.h>

#define F0     9658.0
#define GATE_S (2000.0 / F0)

static const struct pbl_freq_track_params s_params = {
  .wfm = 7.2e-6,
  .wander = 8e-6,
  .rw = 1.1e-6,
  .reject_sigma = 5.0,
  .max_rejects = 3,
};

static struct pbl_freq_track s_ft;

void test_freq_track__initialize(void) {
  pbl_freq_track_init(&s_ft, &s_params);
}

void test_freq_track__first_measurement_seeds(void) {
  cl_assert(pbl_freq_track_get(&s_ft) == 0.0);
  cl_assert_equal_i(pbl_freq_track_update(&s_ft, F0, GATE_S, 0U), 0);
  cl_assert(pbl_freq_track_get(&s_ft) == F0);
}

// Gain and posterior deviation converge to the Riccati steady state: K = 0.379 and 10.9 ppm for
// a 2000-cycle gate once a minute.
void test_freq_track__steady_state(void) {
  uint64_t t_ms = 0U;
  double before;
  double k;

  for (int i = 0; i < 100; i++, t_ms += 60000U) {
    cl_assert_equal_i(pbl_freq_track_update(&s_ft, F0, GATE_S, t_ms), 0);
  }
  cl_assert(fabs(pbl_freq_track_get_dev(&s_ft) - 10.9e-6) < 0.1e-6);

  before = pbl_freq_track_get(&s_ft);
  cl_assert_equal_i(pbl_freq_track_update(&s_ft, F0 * (1.0 + 10e-6), GATE_S, t_ms), 0);
  k = (pbl_freq_track_get(&s_ft) - before) / (F0 * 10e-6);
  cl_assert(fabs(k - 0.379) < 0.002);
}

void test_freq_track__rejects_then_restarts(void) {
  const double far = F0 * (1.0 + 500e-6);
  uint64_t t_ms = 0U;

  for (int i = 0; i < 10; i++, t_ms += 60000U) {
    pbl_freq_track_update(&s_ft, F0, GATE_S, t_ms);
  }

  for (int i = 0; i < 3; i++, t_ms += 60000U) {
    cl_assert_equal_i(pbl_freq_track_update(&s_ft, far, GATE_S, t_ms), -ERANGE);
    cl_assert(pbl_freq_track_get(&s_ft) == F0);
  }
  cl_assert_equal_i(pbl_freq_track_update(&s_ft, far, GATE_S, t_ms), 0);
  cl_assert(pbl_freq_track_get(&s_ft) == far);
}

void test_freq_track__isolated_outlier(void) {
  uint64_t t_ms = 0U;

  for (int i = 0; i < 10; i++, t_ms += 60000U) {
    pbl_freq_track_update(&s_ft, F0, GATE_S, t_ms);
  }
  cl_assert_equal_i(pbl_freq_track_update(&s_ft, F0 * 1.001, GATE_S, t_ms), -ERANGE);
  t_ms += 60000U;
  cl_assert_equal_i(pbl_freq_track_update(&s_ft, F0, GATE_S, t_ms), 0);
  cl_assert(pbl_freq_track_get(&s_ft) == F0);
}

void test_freq_track__seed(void) {
  pbl_freq_track_seed(&s_ft, F0, GATE_S, 1000U);
  cl_assert(pbl_freq_track_get(&s_ft) == F0);
  cl_assert_equal_i(pbl_freq_track_update(&s_ft, F0 * (1.0 + 20e-6), GATE_S, 61000U), 0);
  cl_assert(pbl_freq_track_get(&s_ft) > F0);
  cl_assert(pbl_freq_track_get(&s_ft) < F0 * (1.0 + 20e-6));
}
