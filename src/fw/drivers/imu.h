/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

void imu_init(void);

//! Put all IMU devices into normal mode
void imu_power_up(void);
//! Put all IMU devices into low-power mode
void imu_power_down(void);

bool imu_self_test(void);

bool imu_sanity_check(void);
