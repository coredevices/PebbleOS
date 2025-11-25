/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/imu.h"
#include "drivers/imu/mag3110/mag3110.h"
#include "drivers/mag.h"
#include "drivers/qemu/qemu_accel.h"

void imu_init(void) {
  qemu_accel_init();
#if CAPABILITY_HAS_MAGNETOMETER
  mag3110_init();
#endif
}

void imu_power_up(void) {
}

void imu_power_down(void) {
}

bool gyro_run_selftest(void) {
  return true;
}
