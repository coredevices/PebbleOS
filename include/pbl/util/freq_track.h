/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

/**
 * @defgroup util_freq_track Frequency tracking
 * @ingroup util
 * @brief Estimate the frequency of a drifting oscillator from noisy gated measurements.
 *
 * A scalar Kalman filter on the frequency: the state random walks with density @c rw and each
 * measurement, a gate of @c T seconds, reads it with white frequency noise @c wfm / sqrt(T) plus
 * @c wander, the short-term fluctuation that a gate samples at one instant. All three are
 * fractional, measured for the oscillator at hand. A measurement further than
 * @c reject_sigma standard deviations from the prediction is dropped, at most @c max_rejects in a
 * row; then the filter restarts from the next one.
 *
 * @code{.c}
 * static const struct pbl_freq_track_params s_params = {
 *   .wfm = 7.2e-6, .wander = 8e-6, .rw = 1.1e-6, .reject_sigma = 5.0, .max_rejects = 3,
 * };
 * struct pbl_freq_track ft;
 *
 * pbl_freq_track_init(&ft, &s_params);
 * pbl_freq_track_update(&ft, measured_hz, gate_s, pbl_uptime_ms());
 * double hz = pbl_freq_track_get(&ft);
 * @endcode
 * @{
 */

/** @brief Noise model of an oscillator and its measurements. */
struct pbl_freq_track_params {
  /** White frequency noise, fractional times square root of seconds. */
  double wfm;
  /** Short-term wander a gate samples at one instant, fractional. */
  double wander;
  /** Frequency random walk density, fractional per square root of seconds. */
  double rw;
  /** Rejection threshold, in standard deviations of the innovation. */
  double reject_sigma;
  /** Consecutive rejections before restarting. */
  uint8_t max_rejects;
};

/** @brief Tracker state. */
struct pbl_freq_track {
  /** @cond INTERNAL_HIDDEN */
  const struct pbl_freq_track_params *params;
  double f;
  double p;
  uint64_t t_ms;
  uint8_t rejects;
  /** @endcond */
};

/**
 * @brief Initialize a tracker with no estimate.
 *
 * @param ft Tracker.
 * @param params Noise model; must stay valid.
 */
void pbl_freq_track_init(struct pbl_freq_track *ft, const struct pbl_freq_track_params *params);

/**
 * @brief Start from a known frequency, e.g. one kept across a reset.
 *
 * @param ft Tracker.
 * @param hz Frequency.
 * @param gate_s Gate length the frequency is as good as, in seconds.
 * @param t_ms Current time, in milliseconds.
 */
void pbl_freq_track_seed(struct pbl_freq_track *ft, double hz, double gate_s, uint64_t t_ms);

/**
 * @brief Feed a measurement.
 *
 * @param ft Tracker.
 * @param hz Measured frequency.
 * @param gate_s Gate length, in seconds.
 * @param t_ms Time of the measurement, in milliseconds, not before the previous one.
 * @return 0 if used, @c -ERANGE if rejected.
 */
int pbl_freq_track_update(struct pbl_freq_track *ft, double hz, double gate_s, uint64_t t_ms);

/**
 * @brief Get the estimate.
 *
 * @param ft Tracker.
 * @return Frequency, or 0 before the first measurement or seed.
 */
double pbl_freq_track_get(const struct pbl_freq_track *ft);

/**
 * @brief Get the standard deviation of the estimate.
 *
 * @param ft Tracker.
 * @return Fractional standard deviation, or 0 before the first measurement or seed.
 */
double pbl_freq_track_get_dev(const struct pbl_freq_track *ft);

/** @} */
