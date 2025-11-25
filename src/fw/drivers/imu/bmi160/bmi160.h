/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

typedef enum {
  BMI160_Accel_Mode_Suspend = 0b00,
  BMI160_Accel_Mode_Normal = 0b01,
  BMI160_Accel_Mode_Low = 0b10,
} BMI160AccelPowerMode;

typedef enum {
  BMI160_Gyro_Mode_Suspend = 0b00,
  BMI160_Gyro_Mode_Normal = 0b01,
  BMI160_Gyro_Mode_FastStartup = 0b11
} BMI160GyroPowerMode;

void bmi160_init(void);
bool bmi160_query_whoami(void);
void bmi160_set_accel_power_mode(BMI160AccelPowerMode mode);
void bmi160_set_gyro_power_mode(BMI160GyroPowerMode mode);
