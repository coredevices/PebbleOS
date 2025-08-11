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
#include "kernel/util/sleep.h"
#include "system/logging.h"
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
static void prv_lsm6dso_interrupt_handler(bool *should_context_switch);
static void prv_lsm6dso_process_interrupts(void);

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
    // TODO: Configure the double tap thresholds.
    s_lsm6dso_state.double_tap_detection_enabled =
        s_lsm6dso_state_target.double_tap_detection_enabled;
    update_interrupts = true;
  }

  // Update sampling interval
  if (update_interrupts ||
      s_lsm6dso_state_target.sampling_interval_us != s_lsm6dso_state.sampling_interval_us) {
    // TODO: Set the sampling interval to the closest supported ODR to that requested.
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
}
