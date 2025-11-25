/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>

#include "system/logging.h"
#include "drivers/imu/lis3dh/lis3dh.h"
#include "drivers/imu/mag3110/mag3110.h"


void imu_init(void) {
  // Init accelerometer
  lis3dh_init();
  // Init magnetometer
  mag3110_init();
}

void imu_power_up(void) {
  lis3dh_power_up();
}

void imu_power_down(void) {
  lis3dh_power_down();
}
