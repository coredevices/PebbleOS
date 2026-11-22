/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <math.h>
#include <stdint.h>

#include <clar.h>
#include <services/ecompass/calibration.h>

#define FIELD 45.0f

static struct ecompass_calibrator s_cal;
static uint32_t s_seed;

static float prv_noise(float amplitude) {
  s_seed = s_seed * 1103515245U + 12345U;
  return amplitude * (((float)((s_seed >> 8) & 0xFFFF) / 32768.0f) - 1.0f);
}

// Directions spread over the sphere, i-th of n
static void prv_direction(uint32_t i, uint32_t n, float u[3]) {
  float z = 1.0f - 2.0f * ((float)i + 0.5f) / (float)n;
  float r = sqrtf(1.0f - z * z);
  float phi = (float)i * 2.39996323f;

  u[0] = r * cosf(phi);
  u[1] = r * sinf(phi);
  u[2] = z;
}

struct prv_model {
  float offset[3];
  float distortion[3][3];
};

static const struct prv_model s_hard_only = {
  .offset = {30.0f, -20.0f, 50.0f},
  .distortion = {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
};

static const struct prv_model s_soft = {
  .offset = {-12.0f, 41.0f, 7.0f},
  .distortion = {{1.12f, 0.06f, 0.0f}, {0.06f, 0.93f, 0.04f}, {0.0f, 0.04f, 1.03f}},
};

// What the magnetometer reads with the field along u
static void prv_raw(const struct prv_model *model, const float u[3], float noise, float m[3]) {
  for (int i = 0; i < 3; i++) {
    m[i] = model->offset[i] + prv_noise(noise);
    for (int j = 0; j < 3; j++) {
      m[i] += model->distortion[i][j] * FIELD * u[j];
    }
  }
}

// Orientations in an order that jumps around, as a moving wrist does
static void prv_feed(const struct prv_model *model, uint32_t n, float noise) {
  float u[3];
  float m[3];

  for (uint32_t i = 0U; i < n; i++) {
    prv_direction((i * 157U) % n, n, u);
    prv_raw(model, u, noise, m);
    ecompass_cal_add(&s_cal, m);
  }
}

// Largest relative deviation of corrected fresh samples from the field
static float prv_worst(const struct prv_model *model) {
  float worst = 0.0f;
  float u[3];
  float m[3];
  float out[3];

  for (uint32_t i = 0U; i < 200U; i++) {
    prv_direction(i * 7U + 3U, 1400U, u);
    prv_raw(model, u, 0.0f, m);
    ecompass_cal_apply(&s_cal.cal, m, out);
    float mag = sqrtf(out[0] * out[0] + out[1] * out[1] + out[2] * out[2]);
    worst = fmaxf(worst, fabsf(mag / s_cal.cal.field - 1.0f));
  }

  return worst;
}

void test_compass_cal__initialize(void) {
  s_seed = 1U;
  ecompass_cal_init(&s_cal, nullptr);
}

void test_compass_cal__a_hard_iron_offset_is_found(void) {
  prv_feed(&s_hard_only, 300U, 0.3f);

  cl_assert(s_cal.cal.order != 0U);
  for (int i = 0; i < 3; i++) {
    cl_assert(fabsf(s_cal.cal.offset[i] - s_hard_only.offset[i]) < 1.0f);
  }
  cl_assert(fabsf(s_cal.cal.field - FIELD) < 1.0f);
  cl_assert(s_cal.cal.error < 0.02f);
  cl_assert(prv_worst(&s_hard_only) < 0.02f);
}

void test_compass_cal__soft_iron_is_corrected(void) {
  prv_feed(&s_soft, 400U, 0.3f);

  cl_assert_equal_i(s_cal.cal.order, 10);
  for (int i = 0; i < 3; i++) {
    cl_assert(fabsf(s_cal.cal.offset[i] - s_soft.offset[i]) < 1.0f);
  }
  cl_assert(s_cal.cal.error < 0.02f);
  cl_assert(prv_worst(&s_soft) < 0.02f);
}

void test_compass_cal__a_hard_iron_fit_leaves_soft_iron_errors(void) {
  struct ecompass_cal hard = {
    .offset = {-12.0f, 41.0f, 7.0f},
    .soft = {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
    .field = FIELD,
    .error = 0.1f,
    .order = 4,
  };

  ecompass_cal_init(&s_cal, &hard);
  float before = prv_worst(&s_soft);
  prv_feed(&s_soft, 400U, 0.3f);

  cl_assert(before > 0.08f);
  cl_assert(prv_worst(&s_soft) < 0.02f);
}

void test_compass_cal__turning_in_a_plane_is_not_enough(void) {
  float m[3];

  for (uint32_t i = 0U; i < 500U; i++) {
    float phi = (float)i * 0.1f;
    const float u[3] = {cosf(phi), sinf(phi), 0.0f};
    prv_raw(&s_hard_only, u, 0.3f, m);
    ecompass_cal_add(&s_cal, m);
  }

  cl_assert_equal_i(s_cal.cal.order, 0);
}

void test_compass_cal__a_few_orientations_are_not_enough(void) {
  float u[3];
  float m[3];

  for (uint32_t i = 0U; i < 500U; i++) {
    prv_direction(i % 6U, 6U, u);
    prv_raw(&s_hard_only, u, 0.3f, m);
    ecompass_cal_add(&s_cal, m);
  }

  cl_assert_equal_i(s_cal.cal.order, 0);
}

void test_compass_cal__an_implausible_field_is_rejected(void) {
  const struct prv_model weak = {
    .offset = {10.0f, 10.0f, 10.0f},
    .distortion = {{0.2f, 0.0f, 0.0f}, {0.0f, 0.2f, 0.0f}, {0.0f, 0.0f, 0.2f}},
  };

  prv_feed(&weak, 300U, 0.05f);

  cl_assert_equal_i(s_cal.cal.order, 0);
}

void test_compass_cal__a_new_environment_replaces_the_calibration(void) {
  const struct prv_model moved = {
    .offset = {60.0f, 5.0f, -30.0f},
    .distortion = {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
  };

  prv_feed(&s_hard_only, 300U, 0.3f);
  prv_feed(&moved, 300U, 0.3f);
  prv_feed(&moved, 300U, 0.3f);

  for (int i = 0; i < 3; i++) {
    cl_assert(fabsf(s_cal.cal.offset[i] - moved.offset[i]) < 1.5f);
  }
}

void test_compass_cal__a_saved_calibration_is_checked_against_new_samples(void) {
  struct ecompass_cal saved;

  prv_feed(&s_hard_only, 300U, 0.3f);
  saved = s_cal.cal;
  ecompass_cal_init(&s_cal, &saved);

  cl_assert(!s_cal.verified);
  cl_assert_equal_i(s_cal.cal.order, saved.order);
  prv_feed(&s_hard_only, 300U, 0.3f);
  cl_assert(s_cal.verified);
  cl_assert(s_cal.cal.error < 0.02f);
}
