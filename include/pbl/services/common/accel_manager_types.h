/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

//! Valid accelerometer sampling rates, in Hz
typedef enum {
  //! 10 HZ sampling rate
  ACCEL_SAMPLING_10HZ = 10,
  //! 25 HZ sampling rate [Default]
  ACCEL_SAMPLING_25HZ = 25,
  //! 50 HZ sampling rate
  ACCEL_SAMPLING_50HZ = 50,
  //! 100 HZ sampling rate
  ACCEL_SAMPLING_100HZ = 100,
} AccelSamplingRate;

//! A single accelerometer sample for all three axes
typedef struct __attribute__((__packed__)) {
  //! acceleration along the x axis
  int16_t x;
  //! acceleration along the y axis
  int16_t y;
  //! acceleration along the z axis
  int16_t z;
} AccelRawData;

//! A single accelerometer sample for all three axes including timestamp and
//! vibration rumble status.
typedef struct __attribute__((__packed__)) AccelData {
  //! acceleration along the x axis
  int16_t x;
  //! acceleration along the y axis
  int16_t y;
  //! acceleration along the z axis
  int16_t z;

  //! true if the watch vibrated when this sample was collected
  bool did_vibrate;

  //! timestamp, in milliseconds
  uint64_t timestamp;
} AccelData;
