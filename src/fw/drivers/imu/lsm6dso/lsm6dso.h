/* SPDX-FileCopyrightText: 2025 Matthew Wardrop */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

//! Bitmask values describing high level driver state for diagnostics.
enum {
  LSM6DSO_STATE_FLAG_INITIALIZED = 1u << 0,
  LSM6DSO_STATE_FLAG_ENABLED = 1u << 1,
  LSM6DSO_STATE_FLAG_RUNNING = 1u << 2,
  LSM6DSO_STATE_FLAG_HEALTH_OK = 1u << 3,
  LSM6DSO_STATE_FLAG_SAMPLE_VALID = 1u << 4,
};

typedef struct {
  int16_t last_sample_mg[3];
  uint32_t last_sample_age_ms;
  uint32_t last_successful_read_age_ms;
  uint32_t last_interrupt_age_ms;
  uint32_t last_wake_event_age_ms;
  uint32_t last_double_tap_age_ms;
  uint32_t i2c_error_count;
  uint32_t consecutive_error_count;
  uint32_t watchdog_event_count;
  uint32_t recovery_success_count;
  uint32_t state_flags;
  uint32_t interrupt_count;
  uint32_t wake_event_count;
  uint32_t double_tap_event_count;
} Lsm6dsoDiagnostics;

//! Initialize the LSM6DSO accelerometer driver.
void lsm6dso_init(void);

//! Enter normal mode for the LSM6DSO accelerometer.
void lsm6dso_power_up(void);

//! Enter low-power mode for the LSM6DSO accelerometer.
void lsm6dso_power_down(void);

//! Retrieve a snapshot of sensor diagnostics for telemetry.
void lsm6dso_get_diagnostics(Lsm6dsoDiagnostics *diagnostics);

//! Board configuration for the LSM6DSO gyroscope (CONFIG_GYRO_LSM6DSO).
//! Maps board axes (indexed by IMUCoordinateAxis) to hardware axes, mirroring
//! the accelerometer's axes_offsets/axes_inverts configuration.
typedef struct {
  uint8_t axis_map[3];
  int8_t axis_dir[3];
} Lsm6dsoGyroConfig;

//! Provided by the board when CONFIG_GYRO_LSM6DSO is enabled.
extern const Lsm6dsoGyroConfig *const LSM6DSO_GYRO;