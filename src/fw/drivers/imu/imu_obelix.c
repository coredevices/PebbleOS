/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "board/board.h"
#include "drivers/i2c.h"
#include "drivers/imu.h"
#include "drivers/imu/lis2dw12/lis2dw12.h"
#include "drivers/imu/mmc5603nj/mmc5603nj.h"

void imu_init(void) {
  // For now, manually power-down LSM6DSO
  i2c_use(I2C_LSM6DSO);
  // ODR=Power down
  (void)i2c_write_register(I2C_LSM6DSO, 0x10U, 0U);
  // All INT1 sources disabled
  (void)i2c_write_register(I2C_LSM6DSO, 0x0DU, 0U);
  i2c_release(I2C_LSM6DSO);

  lis2dw12_init();
  mmc5603nj_init();
}

void imu_power_up(void) {
  lis2dw12_power_up();
}

void imu_power_down(void) {
  lis2dw12_power_down();
}
