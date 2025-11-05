/*
 * Copyright 2025 Core Devices LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
