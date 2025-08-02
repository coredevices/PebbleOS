/*
 * Copyright 2025 Matthew Wardrop
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
 *
 */

#include "drivers/accel.h"
#include "lsm6dso_reg.h"

#include "lsm6dso.h"

// LSM6DSO configuration entrypoints

void lsm6dso_init(void) {}

void lsm6dso_power_up(void) {}

void lsm6dso_power_down(void) {}

// accel.h implementation

const AccelDriverInfo ACCEL_DRIVER_INFO = {
    .sample_interval_max = 1000000, // 1 second
    .sample_interval_low_power = 100000, // 100 ms
    .sample_interval_ui = 250000, // 250 ms
    .sample_interval_game = 20000, // 20 ms
    .sample_interval_min = 1000, // 1 ms
};

uint32_t accel_set_sampling_interval(uint32_t interval_us) {
  return 0;
}

uint32_t accel_get_sampling_interval(void) {
  return 0;
}

void accel_set_num_samples(uint32_t num_samples) {}

int accel_peek(AccelDriverSample *data) {
  data->timestamp_us = 0;
  data->x = 0;
  data->y = 0;
  data->z = 0;
  return 0;
}

void accel_enable_shake_detection(bool on) {}

bool accel_get_shake_detection_enabled(void) {
  return false;
}

void accel_enable_double_tap_detection(bool on) {}

bool accel_get_double_tap_detection_enabled(void) {
  return false;
}

void accel_set_shake_sensitivity_high(bool sensitivity_high) {}
