/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/imu.h"
#include "drivers/imu/lsm6dso/lsm6dso.h"
#include "drivers/imu/lis2dw12/lis2dw12.h"
#include "drivers/imu/mmc5603nj/mmc5603nj.h"

void imu_init(void) {
#ifdef IMU_USE_LIS2DW12
  lis2dw12_init();
#else
  lsm6dso_init();
#endif
  mmc5603nj_init();
}

void imu_power_up(void) {
#ifdef IMU_USE_LIS2DW12
  lis2dw12_power_up();
#else
  lsm6dso_power_up();
#endif
}

void imu_power_down(void) {
#ifdef IMU_USE_LIS2DW12
  lis2dw12_power_down();
#else
  lsm6dso_power_down();
#endif
}
