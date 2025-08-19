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
#include "drivers/i2c.h"
#include "drivers/exti.h"
#include "drivers/rtc.h"
#include "drivers/vibe.h"
#include "kernel/util/sleep.h"
#include "services/common/vibe_pattern.h"
#include "system/logging.h"
#include "util/math.h"
#include "lsm6dso_reg.h"

#include "lsm6dso.h"

// Forward declaration of private functions defined below public functions
static int32_t prv_lsm6dso_read(void *handle, uint8_t reg_addr, uint8_t *buffer,
                                uint16_t read_size);
static int32_t prv_lsm6dso_write(void *handle, uint8_t reg_addr, const uint8_t *buffer,
                                 uint16_t write_size);
static void prv_lsm6dso_mdelay(uint32_t ms);
static void prv_lsm6dso_init(void);
static void prv_lsm6dso_chase_target_state(void);
static void prv_lsm6dso_configure_interrupts(void);
static lsm6dso_bdr_xl_t prv_get_fifo_batch_rate(uint32_t interval_us);
static void prv_lsm6dso_configure_fifo(bool enable);
static void prv_lsm6dso_configure_double_tap(bool enable);
static void prv_lsm6dso_configure_shake(bool enable, bool sensitivity_high);
static void prv_lsm6dso_interrupt_handler(bool *should_context_switch);
static void prv_lsm6dso_process_interrupts(void);
static bool prv_is_vibing(void);
static bool prv_lsm6dso_health_check(void);
static void prv_lsm6dso_attempt_recovery(void);
typedef struct {
  lsm6dso_odr_xl_t odr;
  uint32_t interval_us;
} odr_xl_interval_t;
static odr_xl_interval_t prv_get_odr_for_interval(uint32_t interval_us);
static int32_t prv_lsm6dso_set_sampling_interval(uint32_t interval_us);
static void prv_lsm6dso_read_samples(void);
static uint8_t prv_lsm6dso_read_sample(AccelDriverSample *data);
typedef enum {
  X_AXIS = 0,
  Y_AXIS = 1,
  Z_AXIS = 2,
} axis_t;
static int16_t prv_get_axis_projection_mg(axis_t axis, int16_t *raw_vector);
static uint64_t prv_get_timestamp_ms(void);

// HAL context for LSM6DSO
stmdev_ctx_t lsm6dso_ctx = {
    .write_reg = prv_lsm6dso_write,
    .read_reg = prv_lsm6dso_read,
    .mdelay = prv_lsm6dso_mdelay,
};

// Toplevel module state

static bool s_lsm6dso_initialized = false;
static bool s_lsm6dso_enabled = true;
static bool s_lsm6dso_running = false;
typedef struct {
  uint32_t sampling_interval_us;
  uint32_t num_samples;
  bool shake_detection_enabled;
  bool shake_sensitivity_high;
  bool double_tap_detection_enabled;
} lsm6dso_state_t;
lsm6dso_state_t s_lsm6dso_state = {0};
lsm6dso_state_t s_lsm6dso_state_target = {0};
static bool s_interrupts_pending = false;
static uint32_t s_tap_threshold = BOARD_CONFIG_ACCEL.accel_config.double_tap_threshold / 1250;
static bool s_fifo_in_use = false;  // true when we have enabled FIFO batching
static uint32_t s_last_vibe_detected = 0;

// Error tracking and recovery
static uint32_t s_i2c_error_count = 0;
static uint32_t s_last_successful_read_ms = 0;
static uint32_t s_consecutive_read_failures = 0;
static bool s_sensor_health_ok = true;

// Maximum FIFO watermark supported by hardware (diff_fifo is 10 bits -> 0..1023)
#define LSM6DSO_FIFO_MAX_WATERMARK 1023

// Maximum allowed sampling interval for tap detection (i.e., slowest rate, in microseconds)
#define LSM6DSO_TAP_DETECTION_MAX_INTERVAL_US 2398

// Delay after detecting a vibe before shake/tap interrupts should be processed again
#define LSM6DSO_VIBE_COOLDOWN_MS 50

// Error recovery thresholds
#define LSM6DSO_MAX_CONSECUTIVE_FAILURES 3
#define LSM6DSO_HEALTH_CHECK_INTERVAL_MS 5000  // Check sensor health every 5 seconds
#define LSM6DSO_MAX_SILENT_PERIOD_MS 30000     // Max time without successful read

// LSM6DSO configuration entrypoints

void lsm6dso_init(void) {
  // Initialize the LSM6DSO sensor to a powered down state.
  prv_lsm6dso_init();
}

void lsm6dso_power_up(void) {
  PBL_LOG(LOG_LEVEL_DEBUG, "LSM6DSO: Powering up accelerometer");
  s_lsm6dso_enabled = true;
  prv_lsm6dso_chase_target_state();
}

void lsm6dso_power_down(void) {
  PBL_LOG(LOG_LEVEL_DEBUG, "LSM6DSO: Powering down accelerometer");
  s_lsm6dso_enabled = false;
  prv_lsm6dso_chase_target_state();
}

// accel.h implementation

const AccelDriverInfo ACCEL_DRIVER_INFO = {
    .sample_interval_max = 625000,       // 1.6 Hz
    .sample_interval_low_power = 80000,  // 12.5Hz
    .sample_interval_ui = 80000,         // 12.5Hz
    .sample_interval_game = 19231,       // 52Hz
    .sample_interval_min = 150,          // 6667Hz
};

uint32_t accel_set_sampling_interval(uint32_t interval_us) {
  PBL_LOG(LOG_LEVEL_DEBUG, "LSM6DSO: Requesting update of sampling interval to %lu us",
          interval_us);
  s_lsm6dso_state_target.sampling_interval_us = interval_us;
  prv_lsm6dso_chase_target_state();
  return s_lsm6dso_state.sampling_interval_us;
}

uint32_t accel_get_sampling_interval(void) { return s_lsm6dso_state.sampling_interval_us; }

void accel_set_num_samples(uint32_t num_samples) {
  PBL_LOG(LOG_LEVEL_DEBUG, "LSM6DSO: Setting number of samples to %lu", num_samples);
  s_lsm6dso_state_target.num_samples = num_samples;
  prv_lsm6dso_chase_target_state();
}

int accel_peek(AccelDriverSample *data) { return prv_lsm6dso_read_sample(data); }

void accel_enable_shake_detection(bool on) {
  PBL_LOG(LOG_LEVEL_DEBUG, "LSM6DSO: %s shake detection.", on ? "Enabling" : "Disabling");
  s_lsm6dso_state_target.shake_detection_enabled = on;
  prv_lsm6dso_chase_target_state();
}

bool accel_get_shake_detection_enabled(void) { return s_lsm6dso_state.shake_detection_enabled; }

void accel_enable_double_tap_detection(bool on) {
  PBL_LOG(LOG_LEVEL_DEBUG, "LSM6DSO: %s double tap detection.", on ? "Enabling" : "Disabling");
  s_lsm6dso_state_target.double_tap_detection_enabled = on;
  prv_lsm6dso_chase_target_state();
}

bool accel_get_double_tap_detection_enabled(void) {
  return s_lsm6dso_state.double_tap_detection_enabled;
}

void accel_set_shake_sensitivity_high(bool sensitivity_high) {
  PBL_LOG(LOG_LEVEL_DEBUG, "LSM6DSO: Setting shake sensitivity to %s.",
          sensitivity_high ? "high" : "normal");
  s_lsm6dso_state_target.shake_sensitivity_high = sensitivity_high;
  prv_lsm6dso_chase_target_state();
}

// HAL context implementations

static int32_t prv_lsm6dso_read(void *handle, uint8_t reg_addr, uint8_t *buffer,
                                uint16_t read_size) {
  i2c_use(I2C_LSM6D);
  bool result = i2c_write_block(I2C_LSM6D, 1, &reg_addr);
  if (result) result = i2c_read_block(I2C_LSM6D, read_size, buffer);
  i2c_release(I2C_LSM6D);
  
  if (!result) {
    s_i2c_error_count++;
    s_consecutive_read_failures++;
    PBL_LOG(LOG_LEVEL_ERROR, "LSM6DSO: I2C read failed (reg=0x%02x, count=%lu)", 
            reg_addr, s_consecutive_read_failures);
    if (s_consecutive_read_failures >= LSM6DSO_MAX_CONSECUTIVE_FAILURES) {
      s_sensor_health_ok = false;
      PBL_LOG(LOG_LEVEL_ERROR, "LSM6DSO: Sensor health degraded after %lu failures", 
              s_consecutive_read_failures);
    }
    return -1;
  } else {
    s_consecutive_read_failures = 0;
    s_last_successful_read_ms = prv_get_timestamp_ms();
    s_sensor_health_ok = true;
  }
  
  return 0;
}

static int32_t prv_lsm6dso_write(void *handle, uint8_t reg_addr, const uint8_t *buffer,
                                 uint16_t write_size) {
  i2c_use(I2C_LSM6D);
  uint8_t d[write_size + 1];
  d[0] = reg_addr;
  memcpy(&d[1], buffer, write_size);
  bool result = i2c_write_block(I2C_LSM6D, write_size + 1, d);
  i2c_release(I2C_LSM6D);
  
  if (!result) {
    s_i2c_error_count++;
    PBL_LOG(LOG_LEVEL_ERROR, "LSM6DSO: I2C write failed (reg=0x%02x)", reg_addr);
    return -1;
  }
  
  return 0;
}

static void prv_lsm6dso_mdelay(uint32_t ms) { psleep(ms); }

// Initialization

//! Initialize the LSM6DSO sensor and configure it to a powered down state.
//! This function should be called once at system startup to prepare the sensor.
static void prv_lsm6dso_init(void) {
  if (s_lsm6dso_initialized) {
    return;
  }

  // Initialize error tracking
  s_i2c_error_count = 0;
  s_consecutive_read_failures = 0;
  s_last_successful_read_ms = 0;
  s_sensor_health_ok = true;

  // Verify sensor is present and functioning
  uint8_t whoami;
  if (lsm6dso_device_id_get(&lsm6dso_ctx, &whoami)) {
    PBL_LOG(LOG_LEVEL_ERROR, "LSM6DSO: Failed to read WHO_AM_I register");
    return;
  }
  if (whoami != LSM6DSO_ID) {
    PBL_LOG(LOG_LEVEL_ERROR,
            "LSM6DSO: Sensor not detected or malfunctioning (WHO_AM_I=0x%02x, expecting 0x%02x)",
            whoami, LSM6DSO_ID);
    return;
  }

  // Reset sensor to known state
  if (lsm6dso_reset_set(&lsm6dso_ctx, PROPERTY_ENABLE)) {
    PBL_LOG(LOG_LEVEL_ERROR, "LSM6DSO: Failed to reset sensor");
    return;
  }
  uint8_t rst;
  do {  // Wait for reset to complete
    lsm6dso_reset_get(&lsm6dso_ctx, &rst);
  } while (rst);

  // Disable I3C interface
  if (lsm6dso_i3c_disable_set(&lsm6dso_ctx, LSM6DSO_I3C_DISABLE)) {
    PBL_LOG(LOG_LEVEL_ERROR, "LSM6DSO: Failed to disable I3C interface");
    return;
  }

  // Enable Block Data Update
  if (lsm6dso_block_data_update_set(&lsm6dso_ctx, PROPERTY_ENABLE)) {
    PBL_LOG(LOG_LEVEL_ERROR, "LSM6DSO: Failed to enable block data update");
    return;
  }

  // Enable Auto Increment
  if (lsm6dso_auto_increment_set(&lsm6dso_ctx, PROPERTY_ENABLE)) {
    PBL_LOG(LOG_LEVEL_ERROR, "LSM6DSO: Failed to enable auto increment");
    return;
  }

  // Set FIFO mode to bypass (will be reconfigured as necessary later)
  if (lsm6dso_fifo_mode_set(&lsm6dso_ctx, LSM6DSO_BYPASS_MODE)) {
    PBL_LOG(LOG_LEVEL_ERROR, "LSM6DSO: Failed to set FIFO mode to bypass");
    return;
  }

  // Set default full scale
  if (lsm6dso_xl_full_scale_set(&lsm6dso_ctx, LSM6DSO_4g)) {
    PBL_LOG(LOG_LEVEL_ERROR, "LSM6DSO: Failed to set accelerometer full scale");
    return;
  }
  if (lsm6dso_gy_full_scale_set(&lsm6dso_ctx, LSM6DSO_250dps)) {
    PBL_LOG(LOG_LEVEL_ERROR, "LSM6DSO: Failed to set gyroscope full scale");
    return;
  }

  // Set output rate to zero (disabling sensors)
  if (lsm6dso_xl_data_rate_set(&lsm6dso_ctx, LSM6DSO_XL_ODR_OFF)) {
    PBL_LOG(LOG_LEVEL_ERROR, "LSM6DSO: Failed to set accelerometer ODR");
    return;
  }
  if (lsm6dso_gy_data_rate_set(&lsm6dso_ctx, LSM6DSO_GY_ODR_OFF)) {
    PBL_LOG(LOG_LEVEL_ERROR, "LSM6DSO: Failed to set gyroscope ODR");
    return;
  }

  // Configure interrupts
  // Note that we only configure on interrupt pin for now, since not all devices
  // have enough channels for two (and it is not in any case neccessary).
  exti_configure_pin(BOARD_CONFIG_ACCEL.accel_ints[0], ExtiTrigger_Rising,
                     prv_lsm6dso_interrupt_handler);

  // Since we are using only one interrupt pin, it is important that we set
  // these to pulsed so that if we miss an interrupt due to timing issues we do
  // not miss subsequent ones.
  if (lsm6dso_data_ready_mode_set(&lsm6dso_ctx, LSM6DSO_DRDY_PULSED)) {
    PBL_LOG(LOG_LEVEL_ERROR, "LSM6DSO: Failed to set data ready mode");
    return;
  }
  if (lsm6dso_int_notification_set(&lsm6dso_ctx, LSM6DSO_ALL_INT_PULSED)) {
    PBL_LOG(LOG_LEVEL_ERROR, "LSM6DSO: Failed to configure interrupt notification");
    return;
  }

  s_lsm6dso_initialized = true;
  PBL_LOG(LOG_LEVEL_DEBUG, "LSM6DSO: Initialization complete");
}

//! Synchronize the LSM6DSO state with the desired target state.
static void prv_lsm6dso_chase_target_state(void) {
  if (!s_lsm6dso_initialized) {
    PBL_LOG(LOG_LEVEL_ERROR, "LSM6DSO: Cannot chase target state before initialization");
    return;
  }

  bool update_interrupts = false;

  // Check whether we should be spinning up the accelerometer
  bool should_be_running = s_lsm6dso_state_target.sampling_interval_us > 0 ||
                           s_lsm6dso_state_target.num_samples > 0 ||
                           s_lsm6dso_state_target.shake_detection_enabled ||
                           s_lsm6dso_state_target.double_tap_detection_enabled;

  if (!should_be_running || !s_lsm6dso_enabled) {
    if (s_lsm6dso_running) {
      PBL_LOG(LOG_LEVEL_DEBUG, "LSM6DSO: Stopping accelerometer");
      lsm6dso_xl_data_rate_set(&lsm6dso_ctx, LSM6DSO_XL_ODR_OFF);
      s_lsm6dso_running = false;
      s_lsm6dso_state = (lsm6dso_state_t){0};
      prv_lsm6dso_configure_interrupts();
    }
    return;
  } else if (!s_lsm6dso_running) {
    s_lsm6dso_running = true;
    update_interrupts = true;
  }

  // Update number of samples
  if (s_lsm6dso_state_target.num_samples != s_lsm6dso_state.num_samples) {
    s_lsm6dso_state.num_samples = s_lsm6dso_state_target.num_samples;
    update_interrupts = true;
  }

  // Update shake detection
  if (s_lsm6dso_state_target.shake_detection_enabled != s_lsm6dso_state.shake_detection_enabled ||
      s_lsm6dso_state_target.shake_sensitivity_high != s_lsm6dso_state.shake_sensitivity_high) {
    s_lsm6dso_state.shake_detection_enabled = s_lsm6dso_state_target.shake_detection_enabled;
    s_lsm6dso_state.shake_sensitivity_high = s_lsm6dso_state_target.shake_sensitivity_high;
    prv_lsm6dso_configure_shake(s_lsm6dso_state.shake_detection_enabled,
                                s_lsm6dso_state.shake_sensitivity_high);
    update_interrupts = true;
  }

  // Update double tap detection
  if (s_lsm6dso_state_target.double_tap_detection_enabled !=
      s_lsm6dso_state.double_tap_detection_enabled) {
    prv_lsm6dso_configure_double_tap(s_lsm6dso_state_target.double_tap_detection_enabled);
    s_lsm6dso_state.double_tap_detection_enabled =
        s_lsm6dso_state_target.double_tap_detection_enabled;
    update_interrupts = true;
  }

  // Update sampling interval
  if (update_interrupts ||
      s_lsm6dso_state_target.sampling_interval_us != s_lsm6dso_state.sampling_interval_us) {
    s_lsm6dso_state.sampling_interval_us =
        prv_lsm6dso_set_sampling_interval(s_lsm6dso_state_target.sampling_interval_us);
  }

  // Update interrupts if necessary
  if (update_interrupts) {
    prv_lsm6dso_configure_interrupts();
  }

  s_lsm6dso_state_target = s_lsm6dso_state;  // Reset target state to current state

  PBL_LOG(LOG_LEVEL_DEBUG,
          "LSM6DSO: Reached target state: sampling_interval_us=%lu, num_samples=%lu, "
          "shake_detection_enabled=%d, shake_high_sensitivity=%d, double_tap_detection_enabled=%d",
          s_lsm6dso_state.sampling_interval_us, s_lsm6dso_state.num_samples,
          s_lsm6dso_state.shake_detection_enabled, s_lsm6dso_state.shake_sensitivity_high,
          s_lsm6dso_state.double_tap_detection_enabled);
}

static void prv_lsm6dso_configure_interrupts(void) {
  if (s_lsm6dso_enabled &&
      (s_lsm6dso_state.num_samples || s_lsm6dso_state.shake_detection_enabled ||
       s_lsm6dso_state.double_tap_detection_enabled)) {
    exti_enable(BOARD_CONFIG_ACCEL.accel_ints[0]);
  } else {
    exti_disable(BOARD_CONFIG_ACCEL.accel_ints[0]);
    return;
  }

  lsm6dso_pin_int1_route_t int1_routes = {0};
  bool use_fifo = s_lsm6dso_state.num_samples > 1;  // batching requested
  if (use_fifo) {
    int1_routes.fifo_th = 1;  // watermark interrupt
    int1_routes.drdy_xl = 0;
    prv_lsm6dso_configure_fifo(true);
  } else {
    int1_routes.drdy_xl = s_lsm6dso_state.num_samples > 0;  // single-sample mode
    int1_routes.fifo_th = 0;
    prv_lsm6dso_configure_fifo(false);
  }
  int1_routes.double_tap = s_lsm6dso_state.double_tap_detection_enabled;
  int1_routes.wake_up = s_lsm6dso_state.shake_detection_enabled;  // use wake-up (any-motion)

  if (lsm6dso_pin_int1_route_set(&lsm6dso_ctx, int1_routes)) {
    PBL_LOG(LOG_LEVEL_ERROR, "LSM6DSO: Failed to configure interrupts");
  }
}

// Map output data rate (interval) to FIFO batching rate enum
static lsm6dso_bdr_xl_t prv_get_fifo_batch_rate(uint32_t interval_us) {
  if (interval_us >= 625000) return LSM6DSO_XL_BATCHED_AT_6Hz5;  // lowest supported batching
  if (interval_us >= 80000) return LSM6DSO_XL_BATCHED_AT_12Hz5;
  if (interval_us >= 38462) return LSM6DSO_XL_BATCHED_AT_26Hz;
  if (interval_us >= 19231) return LSM6DSO_XL_BATCHED_AT_52Hz;
  if (interval_us >= 9615) return LSM6DSO_XL_BATCHED_AT_104Hz;
  if (interval_us >= 4808) return LSM6DSO_XL_BATCHED_AT_208Hz;
  if (interval_us >= 2398) return LSM6DSO_XL_BATCHED_AT_417Hz;
  if (interval_us >= 1200) return LSM6DSO_XL_BATCHED_AT_833Hz;
  if (interval_us >= 600) return LSM6DSO_XL_BATCHED_AT_1667Hz;
  if (interval_us >= 300) return LSM6DSO_XL_BATCHED_AT_3333Hz;
  return LSM6DSO_XL_BATCHED_AT_6667Hz;
}

static void prv_lsm6dso_configure_fifo(bool enable) {
  if (enable == s_fifo_in_use) {
    return;  // nothing to do
  }

  if (enable) {
    // Program FIFO watermark and batch rate (only accelerometer)
    uint32_t watermark = s_lsm6dso_state.num_samples;
    if (watermark == 0) watermark = 1;  // safety
    if (watermark > LSM6DSO_FIFO_MAX_WATERMARK) watermark = LSM6DSO_FIFO_MAX_WATERMARK;
    if (lsm6dso_fifo_watermark_set(&lsm6dso_ctx, (uint16_t)watermark)) {
      PBL_LOG(LOG_LEVEL_ERROR, "LSM6DSO: Failed to set FIFO watermark");
    }
    // Enable accelerometer batching at (approx) current ODR
    lsm6dso_bdr_xl_t batch_rate = prv_get_fifo_batch_rate(s_lsm6dso_state.sampling_interval_us);
    if (lsm6dso_fifo_xl_batch_set(&lsm6dso_ctx, batch_rate)) {
      PBL_LOG(LOG_LEVEL_ERROR, "LSM6DSO: Failed to set FIFO batch rate");
    }
    // Disable gyro batching
    lsm6dso_fifo_gy_batch_set(&lsm6dso_ctx, LSM6DSO_GY_NOT_BATCHED);
    // Put FIFO in stream mode so we keep collecting samples and get periodic watermark interrupts
    if (lsm6dso_fifo_mode_set(&lsm6dso_ctx, LSM6DSO_STREAM_MODE)) {
      PBL_LOG(LOG_LEVEL_ERROR, "LSM6DSO: Failed to enable FIFO stream mode");
    }
  } else {
    // Disable batching & return to bypass
    lsm6dso_fifo_xl_batch_set(&lsm6dso_ctx, LSM6DSO_XL_NOT_BATCHED);
    lsm6dso_fifo_gy_batch_set(&lsm6dso_ctx, LSM6DSO_GY_NOT_BATCHED);
    if (lsm6dso_fifo_mode_set(&lsm6dso_ctx, LSM6DSO_BYPASS_MODE)) {
      PBL_LOG(LOG_LEVEL_ERROR, "LSM6DSO: Failed to disable FIFO");
    }
  }

  s_fifo_in_use = enable;
  PBL_LOG(LOG_LEVEL_DEBUG, "LSM6DSO: FIFO %s (wm=%lu)", enable ? "enabled" : "disabled",
          (unsigned long)s_lsm6dso_state.num_samples);
}

void prv_lsm6dso_configure_double_tap(bool enable) {
  if (enable) {
    // Configure tap detection parameters
    lsm6dso_tap_threshold_x_set(&lsm6dso_ctx, s_tap_threshold);  // Adjust threshold as needed
    lsm6dso_tap_threshold_y_set(&lsm6dso_ctx, s_tap_threshold);
    lsm6dso_tap_threshold_z_set(&lsm6dso_ctx, s_tap_threshold);

    // Enable tap detection on all axes
    lsm6dso_tap_detection_on_x_set(&lsm6dso_ctx, PROPERTY_ENABLE);
    lsm6dso_tap_detection_on_y_set(&lsm6dso_ctx, PROPERTY_ENABLE);
    lsm6dso_tap_detection_on_z_set(&lsm6dso_ctx, PROPERTY_ENABLE);

    // Configure tap timing
    uint8_t tap_shock = BOARD_CONFIG_ACCEL.accel_config.tap_shock;
    uint8_t tap_quiet = BOARD_CONFIG_ACCEL.accel_config.tap_quiet;
    uint8_t tap_dur = BOARD_CONFIG_ACCEL.accel_config.tap_dur;

    lsm6dso_tap_shock_set(&lsm6dso_ctx, tap_shock);  // Shock duration
    lsm6dso_tap_quiet_set(&lsm6dso_ctx, tap_quiet);  // Quiet period
    lsm6dso_tap_dur_set(&lsm6dso_ctx, tap_dur);      // Double tap window

    // Enable double tap recognition
    lsm6dso_tap_mode_set(&lsm6dso_ctx, LSM6DSO_BOTH_SINGLE_DOUBLE);
  } else {
    // Disable tap detection
    lsm6dso_tap_detection_on_x_set(&lsm6dso_ctx, PROPERTY_DISABLE);
    lsm6dso_tap_detection_on_y_set(&lsm6dso_ctx, PROPERTY_DISABLE);
    lsm6dso_tap_detection_on_z_set(&lsm6dso_ctx, PROPERTY_DISABLE);
  }
}

// Configure wake-up (any-motion) for shake detection using wake-up threshold & duration.
static void prv_lsm6dso_configure_shake(bool enable, bool sensitivity_high) {
  if (!enable) {
    // Disable wake-up related routing by clearing threshold
    lsm6dso_wkup_threshold_set(&lsm6dso_ctx, 0);
    return;
  }

  // Select slope filter (not high-pass) for wake-up detection
  lsm6dso_xl_hp_path_internal_set(&lsm6dso_ctx, LSM6DSO_USE_SLOPE);

  // Weight of threshold: use FS/64 for finer resolution when high sensitivity
  lsm6dso_wkup_ths_weight_set(&lsm6dso_ctx,
                              sensitivity_high ? LSM6DSO_LSb_FS_DIV_256 : LSM6DSO_LSb_FS_DIV_64);

  // Duration: increase a bit to reduce spurious triggers
  lsm6dso_wkup_dur_set(&lsm6dso_ctx, sensitivity_high ? 0 : 1);

  // Threshold: derive from board config; clamp into 0..63
  uint32_t raw_high = BOARD_CONFIG_ACCEL.accel_config.shake_thresholds[AccelThresholdHigh];
  uint32_t raw_low = BOARD_CONFIG_ACCEL.accel_config.shake_thresholds[AccelThresholdLow];
  uint32_t raw = sensitivity_high ? raw_high : raw_low;
  // Increase sensitivity: scale threshold down (halve). Ensure at least 2 to avoid noise storms.
  raw = (raw + 1) / 2;     // divide by 2 rounding up
  if (raw > 63) raw = 63;  // lsm6dso wk_ths is 6 bits
  // Sanity fallback if 0 (avoid constant triggers) choose very low but non-zero
  if (raw == 0) raw = 2;
  lsm6dso_wkup_threshold_set(&lsm6dso_ctx, (uint8_t)raw);
}

static void prv_lsm6dso_interrupt_handler(bool *should_context_switch) {
  if (s_interrupts_pending) {  // avoid flooding the kernel queue
    return;
  }
  s_interrupts_pending = true;
  accel_offload_work_from_isr(prv_lsm6dso_process_interrupts, should_context_switch);
}

static void prv_lsm6dso_process_interrupts(void) {
  s_interrupts_pending = false;
  lsm6dso_all_sources_t all_sources;
  
  if (lsm6dso_all_sources_get(&lsm6dso_ctx, &all_sources) != 0) {
    PBL_LOG(LOG_LEVEL_ERROR, "LSM6DSO: Failed to read interrupt sources");
    // If we can't read interrupt sources, the sensor may be unresponsive
    s_consecutive_read_failures++;
    if (s_consecutive_read_failures >= LSM6DSO_MAX_CONSECUTIVE_FAILURES) {
      s_sensor_health_ok = false;
      PBL_LOG(LOG_LEVEL_WARNING, "LSM6DSO: Interrupt processing failed, sensor health degraded");
    }
    return;
  }
  
  // Reset failure count on successful read
  s_consecutive_read_failures = 0;

  // Collect accelerometer samples if requested
  if (s_lsm6dso_state.num_samples > 0 && all_sources.drdy_xl) {
    prv_lsm6dso_read_samples();
  } else if (s_lsm6dso_state.num_samples > 1 &&
             (all_sources.fifo_th || all_sources.fifo_full || all_sources.fifo_ovr)) {
    prv_lsm6dso_read_samples();
  }

  // If currently vibing, any additional events should be ignored (they are
  // likely spurious).
  if (prv_is_vibing()) {
    return;
  }

  // Process double tap events
  if (all_sources.double_tap) {
    PBL_LOG(LOG_LEVEL_DEBUG, "LSM6DSO: Double tap interrupt triggered");
    // Handle double tap detection
    axis_t axis;
    if (all_sources.tap_x) {
      axis = X_AXIS;
    } else if (all_sources.tap_y) {
      axis = Y_AXIS;
    } else if (all_sources.tap_z) {
      axis = Z_AXIS;
    } else {
      PBL_LOG(LOG_LEVEL_DEBUG, "LSM6DSO: No tap axis detected");
      return;  // No valid tap detected
    }

    uint8_t axis_offset = BOARD_CONFIG_ACCEL.accel_config.axes_offsets[axis];
    uint8_t axis_direction = (BOARD_CONFIG_ACCEL.accel_config.axes_inverts[axis] ? -1 : 1) *
                             (all_sources.tap_sign ? -1 : 1);

    PBL_LOG(LOG_LEVEL_DEBUG, "LSM6DSO: Double tap interrupt triggered; axis=%d, direction=%d",
            axis_offset, axis_direction);
    accel_cb_double_tap_detected(axis_offset, axis_direction);
  }

  // Wake-up (any-motion) event -> treat as shake. Axis & direction derived from wake_up_src.
  if (s_lsm6dso_state.shake_detection_enabled && all_sources.wake_up) {
    lsm6dso_wake_up_src_t wake_src;
    if (lsm6dso_read_reg(&lsm6dso_ctx, LSM6DSO_WAKE_UP_SRC, (uint8_t *)&wake_src, 1) == 0) {
      IMUCoordinateAxis axis = AXIS_X;
      int32_t direction = 1;  // LSM6DSO does not give sign directly for wake-up; approximate via
                              // sign of latest sample on axis
      // Determine which axis triggered: order X,Y,Z
      const AccelConfig *cfg = &BOARD_CONFIG_ACCEL.accel_config;
      if (wake_src.x_wu) {
        axis = AXIS_X;
      } else if (wake_src.y_wu) {
        axis = AXIS_Y;
      } else if (wake_src.z_wu) {
        axis = AXIS_Z;
      }
      // Read current sample to infer direction
      int16_t accel_raw[3];
      if (lsm6dso_acceleration_raw_get(&lsm6dso_ctx, accel_raw) == 0) {
        int16_t val = accel_raw[cfg->axes_offsets[axis]];
        bool invert = cfg->axes_inverts[axis];
        direction = (val >= 0 ? 1 : -1) * (invert ? -1 : 1);
      }
      PBL_LOG(LOG_LEVEL_DEBUG, "LSM6DSO: Shake detected; axis=%d, direction=%lu", axis, direction);
      accel_cb_shake_detected(axis, direction);
    }
  }
}

static bool prv_is_vibing(void) {
  if (vibes_get_vibe_strength() != VIBE_STRENGTH_OFF) {
    s_last_vibe_detected = prv_get_timestamp_ms();
    return true;
  }
  if (s_last_vibe_detected > 0) {
    if (prv_get_timestamp_ms() - s_last_vibe_detected < LSM6DSO_VIBE_COOLDOWN_MS) {
      return true;
    } else {
      s_last_vibe_detected = 0;  // reset if cooldown expired
    }
  }
  return false;
}

// Sampling interval configuration

static odr_xl_interval_t prv_get_odr_for_interval(uint32_t interval_us) {
  if (interval_us >= 625000) return (odr_xl_interval_t){LSM6DSO_XL_ODR_1Hz6, 625000};
  if (interval_us >= 80000) return (odr_xl_interval_t){LSM6DSO_XL_ODR_12Hz5, 80000};
  if (interval_us >= 38462) return (odr_xl_interval_t){LSM6DSO_XL_ODR_26Hz, 38462};
  if (interval_us >= 19231) return (odr_xl_interval_t){LSM6DSO_XL_ODR_52Hz, 19231};
  if (interval_us >= 9615) return (odr_xl_interval_t){LSM6DSO_XL_ODR_104Hz, 9615};
  if (interval_us >= 4808) return (odr_xl_interval_t){LSM6DSO_XL_ODR_208Hz, 4808};
  if (interval_us >= 2398) return (odr_xl_interval_t){LSM6DSO_XL_ODR_417Hz, 2398};
  if (interval_us >= 1200) return (odr_xl_interval_t){LSM6DSO_XL_ODR_833Hz, 1200};
  if (interval_us >= 600) return (odr_xl_interval_t){LSM6DSO_XL_ODR_1667Hz, 600};
  if (interval_us >= 300) return (odr_xl_interval_t){LSM6DSO_XL_ODR_3333Hz, 300};
  return (odr_xl_interval_t){LSM6DSO_XL_ODR_6667Hz, 150};
}

static int32_t prv_lsm6dso_set_sampling_interval(uint32_t interval_us) {
  if (!s_lsm6dso_initialized) {
    PBL_LOG(LOG_LEVEL_ERROR, "LSM6DSO: Not initialized, cannot set sampling interval");
    return -1;
  }

  if (s_lsm6dso_state.double_tap_detection_enabled) {
    interval_us = MIN(interval_us, LSM6DSO_TAP_DETECTION_MAX_INTERVAL_US);
  }

  odr_xl_interval_t odr_interval = prv_get_odr_for_interval(interval_us);
  lsm6dso_xl_data_rate_set(&lsm6dso_ctx, odr_interval.odr);

  PBL_LOG(LOG_LEVEL_DEBUG, "LSM6DSO: Set sampling interval to %lu us (requested %lu us)",
          s_lsm6dso_state.sampling_interval_us, interval_us);
  return odr_interval.interval_us;
}

// Accelerometer sample reading (and reporting)

static void prv_lsm6dso_read_samples(void) {
  if (s_lsm6dso_state.num_samples <= 1 || !s_fifo_in_use) {
    // Single sample path
    AccelDriverSample sample;
    prv_lsm6dso_read_sample(&sample);
    return;
  }

  // Drain FIFO
  uint16_t fifo_level = 0;
  if (lsm6dso_fifo_data_level_get(&lsm6dso_ctx, &fifo_level) != 0) {
    PBL_LOG(LOG_LEVEL_ERROR, "LSM6DSO: Failed to read FIFO level");
    return;
  }
  if (fifo_level == 0) {
    return;  // nothing to do
  }

  const uint64_t now_us = prv_get_timestamp_ms() * 1000ULL;
  const uint32_t interval_us = s_lsm6dso_state.sampling_interval_us ?: 1000;  // avoid div by zero

  for (uint16_t i = 0; i < fifo_level; ++i) {
    lsm6dso_fifo_tag_t tag;
    if (lsm6dso_fifo_sensor_tag_get(&lsm6dso_ctx, &tag) != 0) {
      PBL_LOG(LOG_LEVEL_ERROR, "LSM6DSO: Failed to read FIFO tag");
      break;
    }

    uint8_t raw_bytes[6];
    if (lsm6dso_read_reg(&lsm6dso_ctx, LSM6DSO_FIFO_DATA_OUT_X_L, raw_bytes, sizeof(raw_bytes)) !=
        0) {
      PBL_LOG(LOG_LEVEL_ERROR, "LSM6DSO: Failed to read FIFO sample (%u/%u)", i, fifo_level);
      break;
    }

    if (tag != LSM6DSO_XL_NC_TAG && tag != LSM6DSO_XL_NC_T_1_TAG && tag != LSM6DSO_XL_NC_T_2_TAG &&
        tag != LSM6DSO_XL_2XC_TAG && tag != LSM6DSO_XL_3XC_TAG) {
      // Not an accelerometer sample (e.g., gyro/timestamp/config), ignore
      continue;
    }

    int16_t raw_vector[3];
    raw_vector[0] = (int16_t)((raw_bytes[1] << 8) | raw_bytes[0]);
    raw_vector[1] = (int16_t)((raw_bytes[3] << 8) | raw_bytes[2]);
    raw_vector[2] = (int16_t)((raw_bytes[5] << 8) | raw_bytes[4]);

    AccelDriverSample sample = {0};
    sample.x = prv_get_axis_projection_mg(X_AXIS, raw_vector);
    sample.y = prv_get_axis_projection_mg(Y_AXIS, raw_vector);
    sample.z = prv_get_axis_projection_mg(Z_AXIS, raw_vector);
    // Approximate timestamp: assume fifo_level contiguous samples ending now
    uint32_t sample_index_from_end = (fifo_level - 1) - i;  // 0 for newest
    sample.timestamp_us = now_us - (sample_index_from_end * (uint64_t)interval_us);
    accel_cb_new_sample(&sample);
  }
}

static uint8_t prv_lsm6dso_read_sample(AccelDriverSample *data) {
  if (!s_lsm6dso_initialized) {
    PBL_LOG(LOG_LEVEL_ERROR, "LSM6DSO: Not initialized, cannot read sample");
    return -1;
  }

  // Check sensor health periodically
  static uint32_t s_last_health_check_ms = 0;
  uint32_t now_ms = prv_get_timestamp_ms();
  if (now_ms - s_last_health_check_ms > LSM6DSO_HEALTH_CHECK_INTERVAL_MS) {
    s_last_health_check_ms = now_ms;
    if (!prv_lsm6dso_health_check()) {
      if (s_sensor_health_ok) {
        // Health just degraded, attempt recovery
        prv_lsm6dso_attempt_recovery();
      }
      s_sensor_health_ok = false;
      return -1;
    }
  }

  // TODO: Handle case when accelerometer is not enabled or running (by briefly
  // enabling it.

  int16_t accel_raw[3];
  if (lsm6dso_acceleration_raw_get(&lsm6dso_ctx, accel_raw) != 0) {
    PBL_LOG(LOG_LEVEL_ERROR, "LSM6DSO: Failed to read accelerometer data");
    return -1;
  }

  data->x = prv_get_axis_projection_mg(X_AXIS, accel_raw);
  data->y = prv_get_axis_projection_mg(Y_AXIS, accel_raw);
  data->z = prv_get_axis_projection_mg(Z_AXIS, accel_raw);
  data->timestamp_us = prv_get_timestamp_ms() * 1000;

  if (s_lsm6dso_state.num_samples > 0) {
    accel_cb_new_sample(data);
  }

  return 0;
}

static int16_t prv_get_axis_projection_mg(axis_t axis, int16_t *raw_vector) {
  uint8_t axis_offset = BOARD_CONFIG_ACCEL.accel_config.axes_offsets[axis];
  int axis_direction = BOARD_CONFIG_ACCEL.accel_config.axes_inverts[axis] ? -1 : 1;

  return lsm6dso_from_fs4_to_mg(raw_vector[axis_offset] * axis_direction);
}

static uint64_t prv_get_timestamp_ms(void) {
  time_t time_s;
  uint16_t time_ms;
  rtc_get_time_ms(&time_s, &time_ms);
  return (((uint64_t)time_s) * 1000 + time_ms);
}

// Helper to grab one raw sample set (blocking) and convert to mg (board axis adjusted)
static int prv_get_sample_mg(int16_t out_mg[3]) {
  int16_t raw[3];
  if (lsm6dso_acceleration_raw_get(&lsm6dso_ctx, raw) != 0) {
    return -1;
  }
  out_mg[0] = prv_get_axis_projection_mg(X_AXIS, raw);
  out_mg[1] = prv_get_axis_projection_mg(Y_AXIS, raw);
  out_mg[2] = prv_get_axis_projection_mg(Z_AXIS, raw);
  return 0;
}

// Self-test implementation
//
// Reference: LSM6DSO datasheet / application notes. Procedure (simplified):
// 1. Configure XL @ 52Hz, FS=4g. Collect a small set of samples (ST disabled).
// 2. Enable self-test (positive) and wait for output to settle. Collect samples.
// 3. Compute absolute delta per axis (ON - OFF) in mg and compare against threshold.
// 4. Disable self-test and restore previous configuration.
//
// We only enforce a minimum delta (lower bound) which indicates the internal actuation worked.
// Chosen conservative thresholds (mg) based on typical min values from datasheet; may be tuned.
//

bool accel_run_selftest(void) {
  if (!s_lsm6dso_initialized) {
    // Attempt init if not already done
    prv_lsm6dso_init();
    if (!s_lsm6dso_initialized) {
      return false;
    }
  }

  // Save current target/current state so we can restore later
  const lsm6dso_state_t saved_state = s_lsm6dso_state;
  const lsm6dso_state_t saved_target = s_lsm6dso_state_target;
  const bool saved_enabled = s_lsm6dso_enabled;

  // Ensure accelerometer enabled & running at known configuration
  s_lsm6dso_enabled = true;
  s_lsm6dso_state_target.sampling_interval_us = 19231;  // ~52Hz per mapping
  s_lsm6dso_state_target.num_samples = 0;               // disable callbacks during test
  s_lsm6dso_state_target.shake_detection_enabled = false;
  s_lsm6dso_state_target.double_tap_detection_enabled = false;
  prv_lsm6dso_chase_target_state();

  // Force FS=4g (required for mg conversion helper used elsewhere)
  lsm6dso_xl_full_scale_set(&lsm6dso_ctx, LSM6DSO_4g);

  // Collect baseline (self-test disabled)
  (void)lsm6dso_xl_self_test_set(&lsm6dso_ctx, LSM6DSO_XL_ST_DISABLE);
  psleep(100);  // allow settling
  int32_t sum_off[3] = {0};
  const int kNumSamples = 5;
  int collected = 0;
  for (int i = 0; i < kNumSamples; ++i) {
    int16_t mg[3];
    if (prv_get_sample_mg(mg) != 0) {
      break;
    }
    sum_off[0] += mg[0];
    sum_off[1] += mg[1];
    sum_off[2] += mg[2];
    ++collected;
    psleep(20);  // ~1 sample period @52Hz (19ms)
  }
  if (collected == 0) {
    // restore state
    s_lsm6dso_state = saved_state;
    s_lsm6dso_state_target = saved_target;
    s_lsm6dso_enabled = saved_enabled;
    prv_lsm6dso_chase_target_state();
    return false;
  }
  int32_t avg_off[3] = {sum_off[0] / collected, sum_off[1] / collected, sum_off[2] / collected};

  // Enable positive self-test stimulus
  if (lsm6dso_xl_self_test_set(&lsm6dso_ctx, LSM6DSO_XL_ST_POSITIVE) != 0) {
    // restore and fail
    s_lsm6dso_state = saved_state;
    s_lsm6dso_state_target = saved_target;
    s_lsm6dso_enabled = saved_enabled;
    prv_lsm6dso_chase_target_state();
    return false;
  }
  psleep(100);  // settling per app note

  int32_t sum_on[3] = {0};
  collected = 0;
  for (int i = 0; i < kNumSamples; ++i) {
    int16_t mg[3];
    if (prv_get_sample_mg(mg) != 0) {
      break;
    }
    sum_on[0] += mg[0];
    sum_on[1] += mg[1];
    sum_on[2] += mg[2];
    ++collected;
    psleep(20);
  }
  int32_t avg_on[3] = {0};
  if (collected > 0) {
    avg_on[0] = sum_on[0] / collected;
    avg_on[1] = sum_on[1] / collected;
    avg_on[2] = sum_on[2] / collected;
  }

  // Disable self-test
  lsm6dso_xl_self_test_set(&lsm6dso_ctx, LSM6DSO_XL_ST_DISABLE);

  // Thresholds (mg) - conservative lower bounds
  const int kMinDeltaXY_mg = 90;  // datasheet min typically ~90 mg
  const int kMinDeltaZ_mg = 90;   // Z similar / slightly different; keep same for simplicity

  bool pass = true;
  int32_t delta_x = ABS(avg_on[0] - avg_off[0]);
  int32_t delta_y = ABS(avg_on[1] - avg_off[1]);
  int32_t delta_z = ABS(avg_on[2] - avg_off[2]);
  if (delta_x < kMinDeltaXY_mg) {
    pass = false;
  }
  if (delta_y < kMinDeltaXY_mg) {
    pass = false;
  }
  if (delta_z < kMinDeltaZ_mg) {
    pass = false;
  }

  PBL_LOG(LOG_LEVEL_DEBUG,
          "LSM6DSO: Self-test deltas mg X=%" PRId32 " Y=%" PRId32 " Z=%" PRId32
          " (min XY=%d Z=%d) => %s",
          delta_x, delta_y, delta_z, kMinDeltaXY_mg, kMinDeltaZ_mg, pass ? "PASS" : "FAIL");

  // Restore previous configuration (best-effort)
  s_lsm6dso_state = saved_state;
  s_lsm6dso_state_target = saved_target;
  s_lsm6dso_enabled = saved_enabled;
  prv_lsm6dso_chase_target_state();

  return pass;
}

// Health monitoring and recovery functions

//! Check if the sensor is responding properly by attempting to read WHO_AM_I
static bool prv_lsm6dso_health_check(void) {
  if (!s_lsm6dso_initialized) {
    return false;
  }
  
  // Check if we haven't had a successful read in too long
  uint32_t now_ms = prv_get_timestamp_ms();
  if (s_last_successful_read_ms > 0 && 
      (now_ms - s_last_successful_read_ms) > LSM6DSO_MAX_SILENT_PERIOD_MS) {
    PBL_LOG(LOG_LEVEL_ERROR, "LSM6DSO: No successful reads in %lu ms", 
            now_ms - s_last_successful_read_ms);
    return false;
  }
  
  // Try to read WHO_AM_I register as a health check
  uint8_t whoami;
  if (lsm6dso_device_id_get(&lsm6dso_ctx, &whoami) != 0) {
    PBL_LOG(LOG_LEVEL_ERROR, "LSM6DSO: Health check failed - cannot read WHO_AM_I");
    return false;
  }
  
  if (whoami != LSM6DSO_ID) {
    PBL_LOG(LOG_LEVEL_ERROR, "LSM6DSO: Health check failed - invalid WHO_AM_I (0x%02x)", whoami);
    return false;
  }
  
  return true;
}

//! Attempt to recover from sensor communication failure
static void prv_lsm6dso_attempt_recovery(void) {
  PBL_LOG(LOG_LEVEL_WARNING, "LSM6DSO: Attempting sensor recovery...");
  
  // Save current state
  bool was_enabled = s_lsm6dso_enabled;
  lsm6dso_state_t saved_target = s_lsm6dso_state_target;
  
  // Power down and reinitialize
  s_lsm6dso_enabled = false;
  s_lsm6dso_running = false;
  s_lsm6dso_initialized = false;
  
  // Clear error counters
  s_consecutive_read_failures = 0;
  s_i2c_error_count = 0;
  
  // Wait a bit for sensor to stabilize
  psleep(100);
  
  // Re-initialize sensor
  prv_lsm6dso_init();
  
  if (s_lsm6dso_initialized) {
    // Restore previous state
    s_lsm6dso_enabled = was_enabled;
    s_lsm6dso_state_target = saved_target;
    
    if (s_lsm6dso_enabled) {
      prv_lsm6dso_chase_target_state();
    }
    
    PBL_LOG(LOG_LEVEL_INFO, "LSM6DSO: Recovery successful");
  } else {
    PBL_LOG(LOG_LEVEL_ERROR, "LSM6DSO: Recovery failed - sensor still unresponsive");
  }
}
