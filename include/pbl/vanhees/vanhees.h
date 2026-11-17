/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include <stdint.h>

/**
 * @defgroup vanhees van Hees
 * @ingroup lib
 * @brief Sleep classification from the wrist angle (van Hees et al., 2015).
 *
 * The algorithm processes accelerometer samples at @ref PBL_VANHEES_SAMPLE_HZ in 5 second epochs.
 * Each epoch gives the angle of the Z axis (perpendicular to the skin) to the horizontal plane,
 * computed from the per-axis median. A posture change is a difference of more than
 * @ref PBL_VANHEES_ANGLE_THRESHOLD_DEG between successive epochs, and a sustained inactivity bout
 * is a period of at least @ref PBL_VANHEES_TIME_THRESHOLD_M minutes without one.
 *
 * It only classifies minutes: building sleep sessions out of them is left to the caller.
 *
 * The caller owns the state, of size pbl_vanhees_state_size():
 *
 * @code{.c}
 * struct pbl_vanhees *state = malloc(pbl_vanhees_state_size());
 * pbl_vanhees_init(state);
 *
 * // For every batch of 25 Hz samples
 * pbl_vanhees_analyze_samples(state, samples, num_samples);
 *
 * // Once a minute
 * bool inactive;
 * if (pbl_vanhees_minute_update(state, &inactive)) {
 *   // The minute that started PBL_VANHEES_LAG_M + 1 minutes ago was inactive, or not
 * }
 *
 * free(state);
 * @endcode
 * @{
 */

/** @brief Accelerometer sampling rate expected by the algorithm, in Hz. */
#define PBL_VANHEES_SAMPLE_HZ 25

/** @brief A larger angle difference between successive epochs is a posture change, in degrees. */
#define PBL_VANHEES_ANGLE_THRESHOLD_DEG 5

/** @brief Minimum time without posture change of a sustained inactivity bout, in minutes. */
#define PBL_VANHEES_TIME_THRESHOLD_M 5

/**
 * @brief Minutes between the end of a minute and its verdict.
 *
 * A minute at the very start of a bout is only known to be in one once the rest of the bout has
 * been seen.
 */
#define PBL_VANHEES_LAG_M (PBL_VANHEES_TIME_THRESHOLD_M - 1)

/** @brief Accelerometer sample, in mG. */
struct pbl_vanhees_sample {
  int16_t x;
  int16_t y;
  int16_t z;
};

/** @brief Opaque algorithm state. */
struct pbl_vanhees;

/**
 * @brief Get the size of the algorithm state.
 *
 * @return Size of the state, in bytes.
 */
uint32_t pbl_vanhees_state_size(void);

/**
 * @brief Initialize the algorithm state.
 *
 * @param state State, of pbl_vanhees_state_size() bytes.
 */
void pbl_vanhees_init(struct pbl_vanhees *state);

/**
 * @brief Analyze accelerometer samples.
 *
 * Samples are buffered until a full epoch is available.
 *
 * @param state State passed to pbl_vanhees_init().
 * @param samples Samples, in mG, at @ref PBL_VANHEES_SAMPLE_HZ.
 * @param num_samples Number of samples in @p samples.
 */
void pbl_vanhees_analyze_samples(struct pbl_vanhees *state,
                                 const struct pbl_vanhees_sample *samples, uint32_t num_samples);

/**
 * @brief Classify the minute that ended @ref PBL_VANHEES_LAG_M minutes ago.
 *
 * Called once a minute. A minute without samples discards the history.
 *
 * @param state State passed to pbl_vanhees_init().
 * @param[out] inactive true if the whole minute lies in a sustained inactivity bout.
 * @return true if there is enough history for a verdict, false otherwise (@p inactive is then
 * false).
 */
bool pbl_vanhees_minute_update(struct pbl_vanhees *state, bool *inactive);

/** @} */
