/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

typedef struct RTCCalibConfig {
  uint32_t sign;
  uint32_t units;
} RTCCalibConfig;

//! Calculate the appropriate coarse calibration config given the measured and target frequencies
//! (in mHz)
RTCCalibConfig rtc_calibration_get_config(uint32_t frequency, uint32_t target);

void rtc_calibration_init_timer(void);
