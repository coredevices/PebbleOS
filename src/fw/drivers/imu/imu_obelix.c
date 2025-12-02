/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/imu.h"
#include "drivers/imu/lis2dw12/lis2dw12.h"
#include "drivers/imu/mmc5603nj/mmc5603nj.h"

void imu_init(void) {
  lis2dw12_init();
  mmc5603nj_init();
}

void imu_power_up(void) {
  lis2dw12_power_up();
}

void imu_power_down(void) {
  lis2dw12_power_down();
}
