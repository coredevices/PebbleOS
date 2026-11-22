/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/services/battery/battery_monitor.h>

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
 * are sampled and a tilt-compensated magnetic heading is published with each event. The
 * magnetometer is calibrated on the fly: hard iron offset, then axis gains and full soft iron
 * correction as samples cover more orientations, fit to the latest sample of each direction and
 * kept across reboots. Sampling is faster for a few minutes while uncalibrated, and calibration
 * pauses while the charger is plugged in.
 * The heading types shared with the app SDK (CompassHeading, CompassStatus,
 * CompassHeadingData) are documented in the SDK.
 * @{
 */

/** @brief Register the compass with the event service. */
extern void ecompass_service_init(void);

/**
 * @brief Process a new magnetometer sample.
 *
 * Called from KernelMain when the magnetometer has data. Feeds the calibration and posts a
 * @c PEBBLE_COMPASS_DATA_EVENT with the new heading.
 */
extern void ecompass_service_handle(void);

/**
 * @brief Handle a battery state change.
 *
 * Calibration pauses while the charger is plugged, and starts collecting samples afresh once it
 * is unplugged, keeping the calibration until a better one is found.
 *
 * @param new_state New battery state.
 */
extern void ecompass_handle_battery_state_change_event(PreciseBatteryChargeState new_state);

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
