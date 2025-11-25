/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "system/logging.h"
#include "drivers/imu/bmi160/bmi160.h"
#include "drivers/mag.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>


void imu_init(void) {
  bmi160_init();
  bmi160_set_accel_power_mode(BMI160_Accel_Mode_Normal);
}

void imu_power_up(void) {
  // Unused in snowy as PMIC turns on everything
}

void imu_power_down(void) {
  // Unused in snowy as PMIC turns off everything
}

// We don't actually support the mag at all on snowy_evt, as we tried a more complicated
// arrangement where the mag was a slave of the bmi160 chip. We never fully implemented it, and
// as we abandoned that approach there's no point in ever implementing it. Below are a bunch of
// stubs that do nothing on this board.

void mag_use(void) {
}

void mag_start_sampling(void) {
}

void mag_release(void) {
}

MagReadStatus mag_read_data(MagData *data) {
  return MagReadCommunicationFail;
}

bool mag_change_sample_rate(MagSampleRate rate) {
  return false;
}

