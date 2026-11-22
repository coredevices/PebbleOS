/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#include <pbl/pdm/pdm.h>

// A second-order sigma-delta modulator stands in for the microphone

#define FULL_SCALE 32767.0
#define PI         3.14159265358979323846

static struct pbl_pdm_filter s_filter;

static struct {
  double i1;
  double i2;
  double phase;
} s_mod;

static uint8_t prv_pdm_byte(double freq, double amplitude, double rate) {
  uint8_t byte = 0U;

  for (int i = 0; i < 8; i++) {
    double x = amplitude * sin(s_mod.phase);
    double y = (s_mod.i2 >= 0.0) ? 1.0 : -1.0;

    s_mod.phase += 2.0 * PI * freq / rate;
    s_mod.i1 += x - y;
    s_mod.i2 += s_mod.i1 - y;
    byte = (uint8_t)((byte << 1) | (y > 0.0 ? 1U : 0U));
  }

  return byte;
}

// RMS of the output after the filters settle, and how often it crosses zero upwards
static void prv_measure(uint32_t decimation, uint32_t out_rate, double freq, double amplitude,
                        double *rms, unsigned int *crossings) {
  const double pdm_rate = (double)out_rate * decimation;
  const size_t bytes = decimation / 8U;
  double sum_sq = 0.0;
  size_t counted = 0U;
  int16_t prev = 0;

  memset(&s_mod, 0, sizeof(s_mod));
  *crossings = 0U;

  // One second: the first half settles, the second is measured
  for (uint32_t n = 0U; n < out_rate; n++) {
    uint8_t pdm[16];
    int16_t pcm;

    for (size_t i = 0U; i < bytes; i++) {
      pdm[i] = prv_pdm_byte(freq, amplitude, pdm_rate);
    }

    cl_assert_equal_i(pbl_pdm_filter_run(&s_filter, pdm, bytes, 1U, &pcm, 1U), 1);

    if (n >= out_rate / 2U) {
      sum_sq += (double)pcm * pcm;
      counted++;
      if (prev < 0 && pcm >= 0) {
        (*crossings)++;
      }
    }
    prev = pcm;
  }

  *rms = sqrt(sum_sq / counted);
}

void test_pdm__initialize(void) {
  memset(&s_filter, 0, sizeof(s_filter));
}

void test_pdm__rejects_other_decimations(void) {
  cl_assert_equal_i(pbl_pdm_filter_init(&s_filter, 32U, 0U), -EINVAL);
  cl_assert_equal_i(pbl_pdm_filter_init(&s_filter, 64U, 0U), 0);
  cl_assert_equal_i(pbl_pdm_filter_init(&s_filter, 128U, 0U), 0);
}

void test_pdm__sine_at_16k(void) {
  double rms;
  unsigned int crossings;

  cl_assert_equal_i(pbl_pdm_filter_init(&s_filter, 64U, 0U), 0);
  prv_measure(64U, 16000U, 1000.0, 0.5, &rms, &crossings);

  // Half of full scale, within 0.5 dB
  cl_assert(fabs(20.0 * log10(rms / (0.5 * FULL_SCALE / sqrt(2.0)))) < 0.5);
  cl_assert_equal_i(crossings, 500);
}

void test_pdm__sine_at_8k(void) {
  double rms;
  unsigned int crossings;

  cl_assert_equal_i(pbl_pdm_filter_init(&s_filter, 128U, 0U), 0);
  prv_measure(128U, 8000U, 1000.0, 0.5, &rms, &crossings);

  cl_assert(fabs(20.0 * log10(rms / (0.5 * FULL_SCALE / sqrt(2.0)))) < 0.5);
  cl_assert_equal_i(crossings, 500);
}

void test_pdm__stops_above_the_band(void) {
  double rms;
  unsigned int crossings;

  // 10 kHz at a 16 kHz rate would alias to 6 kHz
  cl_assert_equal_i(pbl_pdm_filter_init(&s_filter, 64U, 0U), 0);
  prv_measure(64U, 16000U, 10000.0, 0.5, &rms, &crossings);

  cl_assert(20.0 * log10(rms / (0.5 * FULL_SCALE / sqrt(2.0))) < -60.0);
}

void test_pdm__removes_the_offset(void) {
  double rms;
  unsigned int crossings;

  // A constant input is an offset: nothing of it is left once settled
  cl_assert_equal_i(pbl_pdm_filter_init(&s_filter, 64U, 0U), 0);
  prv_measure(64U, 16000U, 0.0, 0.0, &rms, &crossings);
  cl_assert(rms < 4.0);

  memset(&s_mod, 0, sizeof(s_mod));
  s_mod.phase = PI / 2.0;
  cl_assert_equal_i(pbl_pdm_filter_init(&s_filter, 64U, 0U), 0);
  prv_measure(64U, 16000U, 1e-9, 0.3, &rms, &crossings);
  cl_assert(rms < 16.0);
}

void test_pdm__silent_while_settling(void) {
  uint8_t pdm[8];
  int16_t pcm[1];
  size_t silent = 0U;

  cl_assert_equal_i(pbl_pdm_filter_init(&s_filter, 64U, 100U), 0);
  memset(&s_mod, 0, sizeof(s_mod));
  for (size_t n = 0U; n < 200U; n++) {
    for (size_t i = 0U; i < sizeof(pdm); i++) {
      pdm[i] = prv_pdm_byte(1000.0, 0.5, 1024000.0);
    }
    cl_assert_equal_i(pbl_pdm_filter_run(&s_filter, pdm, sizeof(pdm), 1U, pcm, 1U), 1);
    if (pcm[0] != 0 && silent == 0U) {
      silent = n;
    }
  }

  // The taps fill in 16 samples, then 100 settle
  cl_assert_equal_i(silent, 116);
}

void test_pdm__gain(void) {
  double rms0;
  double rms12;
  unsigned int crossings;

  cl_assert_equal_i(pbl_pdm_filter_init(&s_filter, 64U, 0U), 0);
  prv_measure(64U, 16000U, 1000.0, 0.05, &rms0, &crossings);

  cl_assert_equal_i(pbl_pdm_filter_init(&s_filter, 64U, 0U), 0);
  pbl_pdm_filter_set_gain(&s_filter, 12000);
  prv_measure(64U, 16000U, 1000.0, 0.05, &rms12, &crossings);

  cl_assert(fabs(20.0 * log10(rms12 / rms0) - 12.0) < 0.2);
}
