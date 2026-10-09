/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup services_activity_hr_util Heart rate zones
 * @ingroup services_activity
 * @brief Heart rate zone classification using the user's heart rate preferences.
 *
 * @code{.c}
 * if (hr_util_get_hr_zone(bpm) >= HRZone_Zone2) {
 *   // vigorous effort
 * }
 * @endcode
 * @{
 */

/** @brief Heart rate zone. */
typedef enum HRZone {
  /** Below the zone 1 threshold. */
  HRZone_Zone0,
  /** From the zone 1 threshold. */
  HRZone_Zone1,
  /** From the zone 2 threshold. */
  HRZone_Zone2,
  /** From the zone 3 threshold. */
  HRZone_Zone3,

  /** Number of zones. */
  HRZoneCount,
  /** Highest zone. */
  HRZone_Max = HRZone_Zone3,
} HRZone;

/**
 * @brief Get the zone of a heart rate.
 *
 * @param bpm Heart rate, in beats per minute.
 * @return Zone, from the thresholds in the heart rate preferences.
 */
HRZone hr_util_get_hr_zone(int bpm);

/**
 * @brief Check whether a heart rate is elevated.
 *
 * @param bpm Heart rate, in beats per minute.
 * @return true if @p bpm is at or above the elevated heart rate preference.
 */
bool hr_util_is_elevated(int bpm);

/** @} */
