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
#include "kernel/util/delay.h"
#include "system/logging.h"

#include "lsm6dso_reg.h"
#include "lsm6dso.h"

// Forward declaration of private functions defined below public functions

static int32_t prv_lsm6dso_read(void *handle, uint8_t reg_addr, uint8_t *buffer,
                                uint16_t read_size);
static int32_t prv_lsm6dso_write(void *handle, uint8_t reg_addr, const uint8_t *buffer,
                                 uint16_t write_size);
static void prv_lsm6dso_register_exti(void);
static void prv_lsm6dso_interrupt_handler(bool *should_context_switch);
static void prv_lsm6dso_process_interrupts(void);
static void prv_lsm6dso_configure_interrupts(void);
typedef struct {
  lsm6dso_odr_xl_t odr;
  uint32_t interval_us;
} odr_xl_interval_t;
static odr_xl_interval_t prv_get_odr_for_interval(uint32_t interval_us);
int32_t prv_lsm6dso_set_sampling_interval(uint32_t interval_us);
static void prv_lsm6dso_read_samples(void);
static uint8_t prv_lsm6dso_read_sample(AccelDriverSample *data);
static void prv_delay_ms(uint32_t ms);
static uint64_t prv_get_timestamp_ms(void);

// Toplevel module state

static bool s_interrupts_pending = false;
static bool s_exti_configured = false;
static struct {
  bool initialized;
  uint32_t sampling_interval_us;
  uint32_t num_samples;
  bool shake_detection_enabled;
  bool shake_sensitivity_high;
  bool double_tap_detection_enabled;
} s_lsm6dso_state = {
    .initialized = false,
    .sampling_interval_us = 0,
    .num_samples = 0,
    .shake_detection_enabled = false,
    .shake_sensitivity_high = false,
    .double_tap_detection_enabled = false,
};

// STM library context for LSM6DSO

stmdev_ctx_t lsm6dso_ctx = {
    .write_reg = prv_lsm6dso_write,
    .read_reg = prv_lsm6dso_read,
    .mdelay = prv_delay_ms,
    .handle = NULL,    // Custom handle can be set if needed
    .priv_data = NULL  // Private data can be used for additional context
};

// LSM6DSO configuration entrypoints

void lsm6dso_init(void) {
  if (s_lsm6dso_state.initialized) {
    return;
  }
  PBL_LOG(LOG_LEVEL_DEBUG, "LSM6DSO: Initializing");

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

  // Set FIFO mode to bypass (this may be reconfigured later)
  if (lsm6dso_fifo_mode_set(&lsm6dso_ctx, LSM6DSO_BYPASS_MODE)) {
    PBL_LOG(LOG_LEVEL_ERROR, "LSM6DSO: Failed to set FIFO mode to bypass");
    return;
  }

  // Set default full scale
  if (lsm6dso_xl_full_scale_set(&lsm6dso_ctx, LSM6DSO_2g)) {
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

  // Configure external interrupt callbacks
  prv_lsm6dso_register_exti();
  prv_lsm6dso_configure_interrupts();

  s_lsm6dso_state.initialized = true;
  PBL_LOG(LOG_LEVEL_DEBUG, "LSM6D: Initialization complete");
}

void lsm6dso_power_up(void) {
  lsm6dso_xl_power_mode_set(&lsm6dso_ctx, LSM6DSO_LOW_NORMAL_POWER_MD);
}

void lsm6dso_power_down(void) {
  lsm6dso_xl_power_mode_set(&lsm6dso_ctx, LSM6DSO_ULTRA_LOW_POWER_MD);
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
  return prv_lsm6dso_set_sampling_interval(interval_us);
}

uint32_t accel_get_sampling_interval(void) { return s_lsm6dso_state.sampling_interval_us; }

void accel_set_num_samples(uint32_t num_samples) {
  s_lsm6dso_state.num_samples = num_samples;
  PBL_LOG(LOG_LEVEL_DEBUG, "LSM6DSO: Set number of samples to %lu", num_samples);

  if (s_lsm6dso_state.sampling_interval_us == 0) {
    prv_lsm6dso_set_sampling_interval(ACCEL_DRIVER_INFO.sample_interval_low_power);
  }
  prv_lsm6dso_configure_interrupts();
}

int accel_peek(AccelDriverSample *data) {
  // TODO: Handle automatically enabling if disabled.
  return prv_lsm6dso_read_sample(data);
}

void accel_enable_shake_detection(bool on) {
  s_lsm6dso_state.shake_detection_enabled = on;
  // TODO: Trigger necessary reconfiguration
}

bool accel_get_shake_detection_enabled(void) { return s_lsm6dso_state.shake_detection_enabled; }

void accel_enable_double_tap_detection(bool on) {
  s_lsm6dso_state.double_tap_detection_enabled = on;
  // TODO: Trigger necessary reconfiguration
}

bool accel_get_double_tap_detection_enabled(void) {
  return s_lsm6dso_state.double_tap_detection_enabled;
}

void accel_set_shake_sensitivity_high(bool sensitivity_high) {
  s_lsm6dso_state.shake_sensitivity_high = sensitivity_high;
  // TODO: Trigger necessary reconfiguration
}

// I2C read/write

static int32_t prv_lsm6dso_read(void *handle, uint8_t reg_addr, uint8_t *buffer,
                                uint16_t read_size) {
  i2c_use(I2C_LSM6D);
  bool result = i2c_write_block(I2C_LSM6D, 1, &reg_addr);
  if (result) result = i2c_read_block(I2C_LSM6D, read_size, buffer);
  i2c_release(I2C_LSM6D);
  return !result;
}

static int32_t prv_lsm6dso_write(void *handle, uint8_t reg_addr, const uint8_t *buffer,
                                 uint16_t write_size) {
  i2c_use(I2C_LSM6D);
  uint8_t d[write_size + 1];
  d[0] = reg_addr;
  memcpy(&d[1], buffer, write_size);
  bool result = i2c_write_block(I2C_LSM6D, write_size + 1, d);
  i2c_release(I2C_LSM6D);
  return !result;
}

// Interrupt handling

static void prv_lsm6dso_register_exti(void) {
  if (s_exti_configured) {
    return;
  }

  // Register the EXTI handler for accelerometer interrupts
  exti_configure_pin(BOARD_CONFIG_ACCEL.accel_ints[0], ExtiTrigger_Rising,
                     prv_lsm6dso_interrupt_handler);
  s_exti_configured = true;
}

static void prv_lsm6dso_interrupt_handler(bool *should_context_switch) {
  if (s_interrupts_pending) {
    return;
  }

  s_interrupts_pending = true;
  accel_offload_work_from_isr(prv_lsm6dso_process_interrupts, should_context_switch);
}

static void prv_lsm6dso_process_interrupts(void) {
  // Check what actually triggered the interrupt(s)
  lsm6dso_all_sources_t all_sources;
  lsm6dso_all_sources_get(&lsm6dso_ctx, &all_sources);

  if (all_sources.double_tap) {
    PBL_LOG(LOG_LEVEL_DEBUG, "LSM6DSO: Double tap interrupt triggered");
    // TODO
  }

  if (all_sources.sig_mot) {
    PBL_LOG(LOG_LEVEL_DEBUG, "LSM6DSO: Shake detection interrupt triggered");
    // TODO
  }

  if (all_sources.drdy_xl) {
    prv_lsm6dso_read_samples();
  }

  s_interrupts_pending = false;
}

static void prv_lsm6dso_configure_interrupts(void) {
  PBL_LOG(LOG_LEVEL_DEBUG,
          "LSM6DSO: Configuring interrupts: num_samples=%lu, shake_detection_enabled=%d, "
          "double_tap_detection_enabled=%d",
          s_lsm6dso_state.num_samples, s_lsm6dso_state.shake_detection_enabled,
          s_lsm6dso_state.double_tap_detection_enabled);

  lsm6dso_pin_int1_route_t int1_routes = {0};
  int1_routes.drdy_xl = s_lsm6dso_state.num_samples > 0;
  // TODO: int1_routes.fifo_th = s_lsm6dso_state.num_samples > 0;
  int1_routes.double_tap = s_lsm6dso_state.double_tap_detection_enabled;
  int1_routes.sig_mot = s_lsm6dso_state.shake_detection_enabled;

  if (lsm6dso_pin_int1_route_set(&lsm6dso_ctx, int1_routes)) {
    PBL_LOG(LOG_LEVEL_ERROR, "LSM6DSO: Failed to configure interrupts");
  }

  if (s_lsm6dso_state.num_samples || s_lsm6dso_state.shake_detection_enabled ||
      s_lsm6dso_state.double_tap_detection_enabled) {
    exti_enable(BOARD_CONFIG_ACCEL.accel_ints[0]);
  } else {
    exti_disable(BOARD_CONFIG_ACCEL.accel_ints[0]);
  }
}

// ODR rates helpers

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

int32_t prv_lsm6dso_set_sampling_interval(uint32_t interval_us) {
  if (!s_lsm6dso_state.initialized) {
    PBL_LOG(LOG_LEVEL_ERROR, "LSM6DSO: Not initialized, cannot set sampling interval");
    return -1;
  }

  odr_xl_interval_t odr_interval = prv_get_odr_for_interval(interval_us);
  lsm6dso_xl_data_rate_set(&lsm6dso_ctx, odr_interval.odr);
  s_lsm6dso_state.sampling_interval_us = odr_interval.interval_us;

  PBL_LOG(LOG_LEVEL_DEBUG, "LSM6DSO: Set sampling interval to %lu us (requested %lu us)",
          s_lsm6dso_state.sampling_interval_us, interval_us);
  return odr_interval.interval_us;
}

// Read accelerometer samples from the sensor and dispatch to callback

static void prv_lsm6dso_read_samples(void) {
  // TODO: Properly implement FIFO buffer.
  AccelDriverSample sample;
  prv_lsm6dso_read_sample(&sample);
}

static uint8_t prv_lsm6dso_read_sample(AccelDriverSample *data) {
  if (!s_lsm6dso_state.initialized) {
    return -1;  // Not initialized
    PBL_LOG(LOG_LEVEL_ERROR, "LSM6DSO: Not initialized, cannot read sample");
  }

  int16_t accel_raw[3];
  if (lsm6dso_acceleration_raw_get(&lsm6dso_ctx, accel_raw) != 0) {
    PBL_LOG(LOG_LEVEL_ERROR, "LSM6DSO: Failed to read accelerometer data");
    return -1;  // Read error
  }

  data->x = lsm6dso_from_fs2_to_mg(accel_raw[0]);
  data->y = lsm6dso_from_fs2_to_mg(accel_raw[1]);
  data->z = lsm6dso_from_fs2_to_mg(accel_raw[2]);
  data->timestamp_us = prv_get_timestamp_ms() * 1000;

  if (s_lsm6dso_state.num_samples > 0) {
    accel_cb_new_sample(data);
  }

  return 0;
}

// Other utility functions

static void prv_delay_ms(uint32_t ms) { delay_us(ms * 1000); }

static uint64_t prv_get_timestamp_ms(void) {
  time_t time_s;
  uint16_t time_ms;
  rtc_get_time_ms(&time_s, &time_ms);
  return (((uint64_t)time_s) * 1000 + time_ms);
}
