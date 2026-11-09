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

/** @cond INTERNAL_HIDDEN */
#define PBL_MOUNT_MATRIX_COL_(x, y, z) ((x) != 0 ? 0 : (y) != 0 ? 1 : 2)
/** @endcond */

/**
 * @brief Sensor axis feeding each watch axis, from the rows of a devicetree mount-matrix.
 */
#define PBL_MOUNT_MATRIX_MAP(xx, xy, xz, yx, yy, yz, zx, zy, zz) \
  {[AXIS_X] = PBL_MOUNT_MATRIX_COL_(xx, xy, xz),                 \
   [AXIS_Y] = PBL_MOUNT_MATRIX_COL_(yx, yy, yz),                 \
   [AXIS_Z] = PBL_MOUNT_MATRIX_COL_(zx, zy, zz)}

/**
 * @brief Direction of each watch axis, from the rows of a devicetree mount-matrix.
 */
#define PBL_MOUNT_MATRIX_DIR(xx, xy, xz, yx, yy, yz, zx, zy, zz) \
  {[AXIS_X] = (xx) + (xy) + (xz), [AXIS_Y] = (yx) + (yy) + (yz), [AXIS_Z] = (zx) + (zy) + (zz)}

/** @} */
