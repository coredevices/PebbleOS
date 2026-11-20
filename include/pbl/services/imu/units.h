/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup services_imu IMU
 * @ingroup services
 * @brief IMU definitions shared by the motion sensor services.
 * @{
 */

/** @brief IMU coordinate axes, relative to the watch. */
typedef enum {
  /** The positive direction along the X axis goes toward the right of the watch. */
  AXIS_X = 0,
  /** The positive direction along the Y axis goes toward the top of the watch. */
  AXIS_Y = 1,
  /** The positive direction along the Z axis goes vertically out of the watchface. */
  AXIS_Z = 2,
} IMUCoordinateAxis;

/** @} */
