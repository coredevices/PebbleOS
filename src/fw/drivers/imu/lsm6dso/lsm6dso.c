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
#include "kernel/util/sleep.h"
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
static void prv_lsm6dso_configure_double_tap(bool enable);
static void prv_lsm6dso_interrupt_handler(bool *should_context_switch);
static void prv_lsm6dso_process_interrupts(void);
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

// Maximum allowed sampling interval (i.e., slowest rate, in microseconds)
#define LSM6DSO_EVENT_MAX_INTERVAL_US 2398

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

void accel_enable_shake_detection(bool on) {}

bool accel_get_shake_detection_enabled(void) {
  return false;
}

void accel_enable_double_tap_detection(bool on) {
  PBL_LOG(LOG_LEVEL_DEBUG, "LSM6DSO: %s double tap detection.", on ? "Enabling" : "Disabling");
  s_lsm6dso_state_target.double_tap_detection_enabled = on;
  prv_lsm6dso_chase_target_state();
}

bool accel_get_double_tap_detection_enabled(void) {
  return s_lsm6dso_state.double_tap_detection_enabled;
}

void accel_set_shake_sensitivity_high(bool sensitivity_high) {}

// HAL context implementations

static int32_t prv_lsm6dso_read(void *handle, uint8_t reg_addr, uint8_t *buffer,
                                uint16_t read_size) {
  i2c_use(I2C_LSM6D);
  bool result = i2c_write_block(I2C_LSM6D, 1, &reg_addr);
  if (result) result = i2c_read_block(I2C_LSM6D, read_size, buffer);
  i2c_release(I2C_LSM6D);
  return result ? 0 : -1;
}

static int32_t prv_lsm6dso_write(void *handle, uint8_t reg_addr, const uint8_t *buffer,
                                 uint16_t write_size) {
  i2c_use(I2C_LSM6D);
  uint8_t d[write_size + 1];
  d[0] = reg_addr;
  memcpy(&d[1], buffer, write_size);
  bool result = i2c_write_block(I2C_LSM6D, write_size + 1, d);
  i2c_release(I2C_LSM6D);
  return result ? 0 : -1;
}

static void prv_lsm6dso_mdelay(uint32_t ms) { psleep(ms); }

// Initialization

//! Initialize the LSM6DSO sensor and configure it to a powered down state.
//! This function should be called once at system startup to prepare the sensor.
static void prv_lsm6dso_init(void) {
  if (s_lsm6dso_initialized) {
    return;
  }

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
  if (s_lsm6dso_state_target.shake_detection_enabled != s_lsm6dso_state.shake_detection_enabled) {
    s_lsm6dso_state.shake_detection_enabled = s_lsm6dso_state_target.shake_detection_enabled;
    update_interrupts = true;
  }

  // Update shake sensitivity
  if (s_lsm6dso_state_target.shake_sensitivity_high != s_lsm6dso_state.shake_sensitivity_high) {
    // TODO: Update the shake sensitivity thresholds.
    s_lsm6dso_state.shake_sensitivity_high = s_lsm6dso_state_target.shake_sensitivity_high;
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
  int1_routes.drdy_xl = s_lsm6dso_state.num_samples > 0;
  // TODO: Handle batching of samples using FIFO when num_samples > 1
  int1_routes.double_tap = s_lsm6dso_state.double_tap_detection_enabled;
  int1_routes.fsm1 = s_lsm6dso_state.shake_detection_enabled;

  if (lsm6dso_pin_int1_route_set(&lsm6dso_ctx, int1_routes)) {
    PBL_LOG(LOG_LEVEL_ERROR, "LSM6DSO: Failed to configure interrupts");
  }
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
  lsm6dso_all_sources_get(&lsm6dso_ctx, &all_sources);

  if (s_lsm6dso_state.num_samples > 0 && all_sources.drdy_xl) {
    prv_lsm6dso_read_samples();
  }

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
    interval_us = MIN(interval_us, LSM6DSO_EVENT_MAX_INTERVAL_US);
  }

  odr_xl_interval_t odr_interval = prv_get_odr_for_interval(interval_us);
  lsm6dso_xl_data_rate_set(&lsm6dso_ctx, odr_interval.odr);

  PBL_LOG(LOG_LEVEL_DEBUG, "LSM6DSO: Set sampling interval to %lu us (requested %lu us)",
          s_lsm6dso_state.sampling_interval_us, interval_us);
  return odr_interval.interval_us;
}

// Accelerometer sample reading (and reporting)

static void prv_lsm6dso_read_samples(void) {
  // TODO: Properly implement FIFO buffer.
  AccelDriverSample sample;
  prv_lsm6dso_read_sample(&sample);
}

static uint8_t prv_lsm6dso_read_sample(AccelDriverSample *data) {
  if (!s_lsm6dso_initialized) {
    PBL_LOG(LOG_LEVEL_ERROR, "LSM6DSO: Not initialized, cannot read sample");
    return -1;
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
