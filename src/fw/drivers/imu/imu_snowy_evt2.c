/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/imu/bmi160/bmi160.h"
#include "drivers/imu/mag3110/mag3110.h"

#include "system/logging.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>


void imu_init(void) {
  bmi160_init();
  bmi160_set_accel_power_mode(BMI160_Accel_Mode_Normal);

  mag3110_init();
}

void imu_power_up(void) {
  // Unused in snowy as PMIC turns on everything
}

void imu_power_down(void) {
  // Unused in snowy as PMIC turns off everything
}
