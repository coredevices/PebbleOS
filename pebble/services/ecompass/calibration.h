/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/** Direction bins, 6 cube faces of 3x3 cells. */
#define ECOMPASS_CAL_BINS 54U

/** A magnetometer calibration: corrected = soft * (raw - offset), in microtesla. */
struct ecompass_cal {
  /** Hard iron offset. */
  float offset[3];
  /** Soft iron correction, symmetric. */
  float soft[3][3];
  /** Field strength the corrected samples lie at. */
  float field;
  /** RMS of the relative deviation from @c field over the samples it was last checked on. */
  float error;
  /** Model: 4 parameters for hard iron only, 10 with soft iron, 0 for none. */
  uint8_t order;
};

/** Online calibrator: the latest sample per direction bin, fit as coverage grows. */
struct ecompass_calibrator {
  float bins[ECOMPASS_CAL_BINS][3];
  uint64_t filled;
  uint8_t num_filled;
  uint8_t pending;
  bool verified;
  bool stale;
  float min[3];
  float max[3];
  struct ecompass_cal cal;
};

/**
 * Start a calibrator.
 *
 * @param c Calibrator.
 * @param saved Calibration to start from, or NULL.
 */
void ecompass_cal_init(struct ecompass_calibrator *c, const struct ecompass_cal *saved);

/**
 * Drop the collected samples, keeping the calibration.
 *
 * @param c Calibrator.
 */
void ecompass_cal_reset_samples(struct ecompass_calibrator *c);

/**
 * Add a sample, refitting once enough new directions came in.
 *
 * @param c Calibrator.
 * @param m Raw sample in microtesla.
 * @return True if the calibration was replaced.
 */
bool ecompass_cal_add(struct ecompass_calibrator *c, const float m[3]);

/**
 * Get the relative deviation of a corrected sample from the field.
 *
 * @param cal Calibration, with a model.
 * @param m Raw sample in microtesla.
 * @return Deviation, 0 when on the field sphere.
 */
float ecompass_cal_deviation(const struct ecompass_cal *cal, const float m[3]);

/**
 * Correct a sample.
 *
 * @param cal Calibration.
 * @param m Raw sample in microtesla.
 * @param[out] out Corrected sample.
 */
void ecompass_cal_apply(const struct ecompass_cal *cal, const float m[3], float out[3]);
