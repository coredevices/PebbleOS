/* SPDX-FileCopyrightText: 2026 Dave Bortz */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

//! Valid gyroscope sampling rates, in Hz. The hardware runs at the nearest
//! native rate at or above the requested one (e.g. 104 Hz for
//! GYRO_SAMPLING_100HZ) and samples are subsampled down to the requested rate.
typedef enum {
  //! 25 HZ sampling rate
  GYRO_SAMPLING_25HZ = 25,
  //! 50 HZ sampling rate
  GYRO_SAMPLING_50HZ = 50,
  //! 100 HZ sampling rate [Default]
  GYRO_SAMPLING_100HZ = 100,
  //! 200 HZ sampling rate
  GYRO_SAMPLING_200HZ = 200,
} GyroSamplingRate;

//! A single gyroscope sample for all three axes, in millidegrees per second
typedef struct __attribute__((__packed__)) {
  //! angular velocity around the x axis
  int32_t x;
  //! angular velocity around the y axis
  int32_t y;
  //! angular velocity around the z axis
  int32_t z;
} GyroRawData;

//! A single gyroscope sample for all three axes including timestamp
typedef struct __attribute__((__packed__)) GyroData {
  //! angular velocity around the x axis, in millidegrees per second
  int32_t x;
  //! angular velocity around the y axis, in millidegrees per second
  int32_t y;
  //! angular velocity around the z axis, in millidegrees per second
  int32_t z;

  //! timestamp, in milliseconds
  uint64_t timestamp;
} GyroData;
