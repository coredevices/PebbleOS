/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/accel.h>
#include <pbl/drivers/rtc.h>

// A watch lying still, face up. No FIFO: samples are only peeked.

static uint32_t s_sampling_interval_us = 40000;

void accel_init(void) {
}

void accel_set_rotated(bool rotated) {
}

uint32_t accel_set_sampling_interval(uint32_t interval_us) {
  s_sampling_interval_us = interval_us;
  return s_sampling_interval_us;
}

uint32_t accel_get_sampling_interval(void) {
  return s_sampling_interval_us;
}

uint32_t accel_get_max_num_samples(void) {
  return 0;
}

void accel_set_num_samples(uint32_t num_samples) {
}

int accel_peek(AccelDriverSample *data) {
  time_t s;
  uint16_t ms;
  rtc_get_time_ms(&s, &ms);
  *data = (AccelDriverSample){
    .timestamp_us = ((uint64_t)s * 1000 + ms) * 1000,
    .x = 0,
    .y = 0,
    .z = -1000,
  };
  return 0;
}

void accel_enable_shake_detection(bool on) {
}

bool accel_get_shake_detection_enabled(void) {
  return false;
}

void accel_enable_double_tap_detection(bool on) {
}

bool accel_get_double_tap_detection_enabled(void) {
  return false;
}

void accel_set_shake_sensitivity_high(bool sensitivity_high) {
}

void accel_set_shake_sensitivity_percent(uint8_t percent) {
}
