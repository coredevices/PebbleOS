/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/services/battery/battery_monitor.h"

/**
 * @brief Represents an angle relative to get to a reference direction, e.g. (magnetic) north.
 *
 * The angle value is scaled linearly, such that a value of TRIG_MAX_ANGLE
 * corresponds to 360 degrees or 2 PI radians.
 * Thus, if heading towards north, north is 0, west is TRIG_MAX_ANGLE/4,
 * south is TRIG_MAX_ANGLE/2, and so on.
 */
typedef int32_t CompassHeading;

/** @brief Enum describing the current state of the Compass Service */
typedef enum {
  /** The Compass Service is unavailable. */
  CompassStatusUnavailable = -1,
  /**
   * Compass is calibrating: data is invalid and should not be used
   * Data will become valid once calibration is complete
   */
  CompassStatusDataInvalid = 0,
  /** Compass is calibrating: the data is valid but the calibration is still being refined */
  CompassStatusCalibrating,
  /** Compass data is valid and the calibration has completed */
  CompassStatusCalibrated
} CompassStatus;

/** @brief Structure containing a single heading towards magnetic and true north. */
typedef struct {
  /**
   * Measured angle that increases counter-clockwise from magnetic north
   * (use `int clockwise_heading = TRIG_MAX_ANGLE - heading_data.magnetic_heading;`
   * for example to find your heading clockwise from magnetic north).
   */
  CompassHeading magnetic_heading;
  /** Currently same value as magnetic_heading (reserved for future implementation). */
  CompassHeading true_heading;
  /** Indicates the current state of the Compass Service calibration. */
  CompassStatus compass_status;
  /** Currently always false (reserved for future implementation). */
  bool is_declination_valid;
} CompassHeadingData;

/**
 * @defgroup services_ecompass Compass
 * @ingroup services
 * @brief Magnetometer-based compass heading.
 *
 * While a process subscribes to @c PEBBLE_COMPASS_DATA_EVENT, the magnetometer and accelerometer
 * are sampled and a tilt-compensated magnetic heading is published with each event. Hard iron
 * correction is calibrated on the fly, first at a higher sampling rate for a few minutes;
 * calibration is suspended while the charger is plugged in and restarted once unplugged.
 * The heading types shared with the app SDK (CompassHeading, CompassStatus,
 * CompassHeadingData) are documented in the SDK.
 * @{
 */

/** @brief Register the compass with the event service. */
extern void ecompass_service_init(void);

/**
 * @brief Process a new magnetometer sample.
 *
 * Called from KernelMain when the magnetometer has data. Updates the calibration and posts a
 * @c PEBBLE_COMPASS_DATA_EVENT with the new heading.
 */
extern void ecompass_service_handle(void);

/**
 * @brief Handle a battery state change.
 *
 * Plugging the charger invalidates the calibration; unplugging it restarts calibration.
 *
 * @param new_state New battery state.
 */
extern void ecompass_handle_battery_state_change_event(PreciseBatteryChargeState new_state);

/** @brief Result of feeding a sample to the hard iron calibration. */
typedef enum {
  /** No new correction estimate available. */
  MagCalStatusNoSolution,
  /** Several fits close to the saved correction were found. */
  MagCalStatusSavedSampleMatch,
  /** A new correction estimate is available. */
  MagCalStatusNewSolutionAvail,
  /** Several fits close to one another were found; the correction is their average. */
  MagCalStatusNewLockedSolutionAvail
} MagCalStatus;

/**
 * @brief Feed a raw magnetometer sample to the hard iron calibration.
 *
 * Selects well-spread points among the samples and fits a sphere to them; its origin is the hard
 * iron correction.
 *
 * @param sample Raw sample, 3 axes.
 * @param saved_corr Previously saved correction, 3 axes, or NULL if none.
 * @param[out] new_corr Correction found, 3 axes; valid unless @ref MagCalStatusNoSolution is
 *                      returned.
 * @return Calibration status.
 */
extern MagCalStatus ecomp_corr_add_raw_mag_sample(int16_t *sample, int16_t *saved_corr,
                                                  int16_t *new_corr);

/**
 * @brief Drop the samples collected by ecomp_corr_add_raw_mag_sample() and reset its state.
 */
extern void ecomp_corr_reset(void);

/**
 * @brief Check whether the current task is subscribed to the compass.
 *
 * @return true if subscribed.
 */
bool sys_ecompass_service_subscribed(void);

/**
 * @brief Get the last published heading.
 *
 * @param[out] data Heading and calibration status.
 */
void sys_ecompass_get_last_heading(CompassHeadingData *data);

/** @} */
