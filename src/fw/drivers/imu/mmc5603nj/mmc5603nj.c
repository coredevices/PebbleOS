/*
 * Copyright 2025 Matthew Wardrop
 * Copyright 2025 Bob Wei
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

#include "drivers/i2c.h"
#include "drivers/mag.h"
#include "kernel/events.h"
#include "kernel/util/sleep.h"
#include "os/mutex.h"
#include "system/logging.h"
#include "system/passert.h"
#include "services/common/new_timer/new_timer.h"

#include "mmc5603nj.h"
#include "registers.h"

#define CEILING(x) ((x - (int)(x)) > 0 ? (int)(x + 1) : (int)(x))

// Forward declarations of private methods
static bool prv_mmc5603nj_read(uint8_t reg_addr, uint8_t data_len, uint8_t *data);
static bool prv_mmc5603nj_write(uint8_t reg_addr, uint8_t data);
static bool prv_mmc5603nj_write_ctrl0(mmc5603nj_internal_control_0_t *ctrl0);
static bool prv_mmc5603nj_write_ctrl1(mmc5603nj_internal_control_1_t *ctrl1);
static bool prv_mmc5603nj_write_ctrl2(mmc5603nj_internal_control_2_t *ctrl2);
static bool prv_mmc5603nj_init(void);
static bool prv_mmc5603nj_check_whoami(void);
static bool prv_mmc5603nj_reset(void);
static bool prv_mmc5603nj_set_sample_rate_hz(uint8_t rate_hz);
static void prv_configure_polling(void);
static void prv_mmc5603nj_polling_callback(void *data);
static MagReadStatus prv_mmc5603nj_get_sample(MagData *sample);
typedef enum {
  X_AXIS = 0,
  Y_AXIS = 1,
  Z_AXIS = 2,
} axis_t;
static int16_t prv_get_axis_projection(axis_t axis, int16_t *raw_vector);

// Runtime state
static bool s_initialized = false;
static int s_use_refcount = 0;
static PebbleMutex *s_mag_mutex;
static uint8_t s_sample_rate_hz = 0;
static TimerID s_polling_timer = TIMER_INVALID_ID;
static uint16_t s_polling_interval_ms = 0;
static bool s_measurement_ready = false;

// MMC5603NJ entrypoints

void mmc5603nj_init(void) {
  s_mag_mutex = mutex_create();
  if (prv_mmc5603nj_init()) {
    PBL_LOG(LOG_LEVEL_DEBUG, "MMC5603NJ: Initialization complete");
  } else {
    PBL_LOG(LOG_LEVEL_ERROR, "MMC5603NJ: Initialization failed");
  }
}

// mag.h implementation

void mag_use(void) {
  PBL_ASSERTN(s_initialized);
  mutex_lock(s_mag_mutex);
  ++s_use_refcount;
  mutex_unlock(s_mag_mutex);
}

void mag_start_sampling(void) {
  mag_use();
  mag_change_sample_rate(MagSampleRate5Hz);
}

void mag_release(void) {
  PBL_ASSERTN(s_initialized && s_use_refcount != 0);
  mutex_lock(s_mag_mutex);
  --s_use_refcount;
  if (s_use_refcount == 0) {
    prv_mmc5603nj_set_sample_rate_hz(0);
  }
  mutex_unlock(s_mag_mutex);
}

// callers responsibility to know if there is valid data to be read
MagReadStatus mag_read_data(MagData *data) {
  mutex_lock(s_mag_mutex);
  MagReadStatus rv = prv_mmc5603nj_get_sample(data);
  mutex_unlock(s_mag_mutex);
  return rv;
}

bool mag_change_sample_rate(MagSampleRate rate) {
  mutex_lock(s_mag_mutex);

  if (s_use_refcount == 0) {
    mutex_unlock(s_mag_mutex);
    return true;
  }

  uint8_t rate_hz = 0;
  switch (rate) {
    case MagSampleRate20Hz:
      rate_hz = 20;
      break;
    case MagSampleRate5Hz:
      rate_hz = 5;
      break;
    default:
      mutex_unlock(s_mag_mutex);
      return false;
  }

  bool rv = prv_mmc5603nj_set_sample_rate_hz(rate_hz);
  mutex_unlock(s_mag_mutex);
  return rv;
}

// I2C read/write helpers

static bool prv_mmc5603nj_read(uint8_t reg_addr, uint8_t data_len, uint8_t *data) {
  i2c_use(I2C_MMC5603NJ);
  bool rv = i2c_write_block(I2C_MMC5603NJ, 1, &reg_addr);
  if (!rv) {
    PBL_LOG(LOG_LEVEL_DEBUG, "MMC5603NJ: I2C write failed for register 0x%02x", reg_addr);
  }
  rv = i2c_read_block(I2C_MMC5603NJ, data_len, data);
  if (!rv) {
    PBL_LOG(LOG_LEVEL_DEBUG, "MMC5603NJ: I2C data read failed for register 0x%02x", reg_addr);
  }
  i2c_release(I2C_MMC5603NJ);
  return rv;
}

static bool prv_mmc5603nj_write(uint8_t reg_addr, uint8_t data) {
  i2c_use(I2C_MMC5603NJ);
  uint8_t d[2] = {reg_addr, data};
  bool rv = i2c_write_block(I2C_MMC5603NJ, 2, d);
  if (!rv) {
    PBL_LOG(LOG_LEVEL_DEBUG, "MMC5603NJ: I2C write failed for register 0x%02x", reg_addr);
  }
  i2c_release(I2C_MMC5603NJ);
  return rv;
}

static bool prv_mmc5603nj_write_ctrl0(mmc5603nj_internal_control_0_t *ctrl0) {
  return prv_mmc5603nj_write(MMC5603NJ_REG_INTERNAL_CONTROL_0, *(uint8_t *)ctrl0);
};
static bool prv_mmc5603nj_write_ctrl1(mmc5603nj_internal_control_1_t *ctrl1) {
  return prv_mmc5603nj_write(MMC5603NJ_REG_INTERNAL_CONTROL_1, *(uint8_t *)ctrl1);
};
static bool prv_mmc5603nj_write_ctrl2(mmc5603nj_internal_control_2_t *ctrl2) {
  return prv_mmc5603nj_write(MMC5603NJ_REG_INTERNAL_CONTROL_2, *(uint8_t *)ctrl2);
};
static bool prv_mmc5603nj_read_status1(mmc5603nj_device_status1_t *status) {
  return prv_mmc5603nj_read(MMC5603NJ_REG_STATUS1, 1, (uint8_t *)status);
}

// Initialization

static bool prv_mmc5603nj_init(void) {
  if (s_initialized) {
    return true;
  }

  if (!prv_mmc5603nj_check_whoami()) {
    PBL_LOG(LOG_LEVEL_ERROR, "MMC5603NJ: WHO_AM_I check failed. Wrong device?");
    return false;
  }

  // Reset the device
  if (!prv_mmc5603nj_reset()) {
    PBL_LOG(LOG_LEVEL_ERROR, "MMC5603NJ: Failed to reset");
    return false;
  }

  // Configure the device bandwidth
  mmc5603nj_internal_control_1_t ctrl1 = {
      .bandwidth = BANDWIDTH_6ms6,  // 6.6ms bandwidth
  };
  if (!prv_mmc5603nj_write_ctrl1(&ctrl1)) {
    return false;
  }

  s_initialized = true;
  return true;
}

// Ask the compass for a 8-bit value that's programmed into the IC at the
// factory. Useful as a sanity check to make sure everything came up properly.
bool prv_mmc5603nj_check_whoami(void) {
  uint8_t whoami = 0;
  if (!prv_mmc5603nj_read(MMC5603NJ_REG_WHO_AM_I, 1, &whoami)) {
    return false;
  }
  return (whoami == MMC5603NJ_WHO_AM_I_VALUE);
}

static bool prv_mmc5603nj_reset(void) {
  mmc5603nj_internal_control_1_t ctrl1_reset = {
      .sw_reset = 1,
  };
  if (!prv_mmc5603nj_write_ctrl1(&ctrl1_reset)) {
    PBL_LOG(LOG_LEVEL_ERROR, "MMC5603NJ: Failed to reset device.");
    return false;
  }
  psleep(MMC5603NJ_SW_RESET_DELAY_MS);

  // TODO: Not sure if this is fully necessary, given that these
  // will run automatically with the configuration in this driver.
  mmc5603nj_internal_control_0_t ctrl0 = {
      .do_set = 1,
  };
  if (!prv_mmc5603nj_write_ctrl0(&ctrl0)) {
    PBL_LOG(LOG_LEVEL_ERROR, "MMC5603NJ: Failed to reset coils.");
    return false;
  }
  psleep(MMC5603NJ_SET_DELAY_MS);
  ctrl0.do_set = 0;
  ctrl0.do_reset = 1;
  if (!prv_mmc5603nj_write_ctrl0(&ctrl0)) {
    PBL_LOG(LOG_LEVEL_ERROR, "MMC5603NJ: Failed to reset coils.");
    return false;
  }
  psleep(MMC5603NJ_SET_DELAY_MS);
  return true;
}

// Configure ODR

bool prv_mmc5603nj_set_sample_rate_hz(uint8_t rate_hz) {
  if (rate_hz == s_sample_rate_hz) {
    return true;
  }

  PBL_LOG(LOG_LEVEL_DEBUG, "MMC5603NJ: Setting sample rate to %d Hz", rate_hz);

  // Reset device runtime status (disabling continuous mode/etc)
  mmc5603nj_internal_control_2_t ctrl2 = {0};
  if (!prv_mmc5603nj_write_ctrl2(&ctrl2)) {
    return false;
  }

  // Do one final read to reset any data ready flags
  MagData discard_sample;
  prv_mmc5603nj_get_sample(&discard_sample);

  if (rate_hz > 0) {
    // Set new sampling rate
    if (!prv_mmc5603nj_write(MMC5603NJ_REG_ODR, rate_hz)) {
      PBL_LOG(LOG_LEVEL_ERROR, "MMC5603NJ: Failed to update ODR.");
    }

    // Retrigger calculation of measurements rates
    mmc5603nj_internal_control_0_t ctrl0 = {
        .auto_sr_en = true,
        .cmm_freq_en = true,
    };
    if (!prv_mmc5603nj_write_ctrl0(&ctrl0)) {
      PBL_LOG(LOG_LEVEL_ERROR, "MMC5603NJ: Failed to trigger measurement calculation update.");
    }

    // Start continuous mode
    mmc5603nj_internal_control_2_t ctrl2 = {
        .prd_set = AUTOSET_PRD_25, .prd_set_en = true, .cmm_en = true};
    if (!prv_mmc5603nj_write_ctrl2(&ctrl2)) {
      PBL_LOG(LOG_LEVEL_ERROR, "MMC5603NJ: Failed to write INTERNAL_CONTROL_2");
      mutex_unlock(s_mag_mutex);
      return false;
    }
  }

  s_sample_rate_hz = rate_hz;
  prv_configure_polling();
  return true;
}

// Configure interrupts and/or polling

static void prv_configure_polling(void) {
  // We are communicating with the MMC5603NJ chip via the I2C protocol, but
  // interrupts are only provided for via the I3C protocol. We can simulate
  // interrupts by scheduling timers that poll the device state at the rate
  // expected by the device ODR.

  uint16_t polling_interval_ms = s_sample_rate_hz == 0 ? 0 : CEILING(1000.0 / s_sample_rate_hz);

  if (s_polling_interval_ms == polling_interval_ms) {
    return;
  }
  
  if (s_polling_timer != TIMER_INVALID_ID) {
    new_timer_stop(s_polling_timer);
    new_timer_delete(s_polling_timer);
    s_polling_timer = TIMER_INVALID_ID;
  }

  if (polling_interval_ms > 0) {
    s_polling_timer = new_timer_create();
    if (s_polling_timer == TIMER_INVALID_ID) {
      PBL_LOG(LOG_LEVEL_ERROR, "MMC5603NJ: Failed to create polling timer");
      return;
    }
    new_timer_start(s_polling_timer, polling_interval_ms, prv_mmc5603nj_polling_callback, NULL,
                    TIMER_START_FLAG_REPEATING);
  }
  s_polling_interval_ms = polling_interval_ms;
}

static void prv_mmc5603nj_polling_callback(void *data) {
  if (s_use_refcount == 0 || s_sample_rate_hz == 0) {
    return;
  }

  // Check if data is ready by reading status register
  mmc5603nj_device_status1_t status;
  if (prv_mmc5603nj_read_status1(&status) && status.meas_m_done) {
    // Post event to trigger data processing
    s_measurement_ready = true;
    PebbleEvent e = {
        .type = PEBBLE_ECOMPASS_SERVICE_EVENT,
    };
    event_put(&e);
  }
}

// Samples
static MagReadStatus prv_mmc5603nj_get_sample(MagData *sample) {
  // Check if sensor enabled.
  if (s_sample_rate_hz == 0) {
    return MagReadMagOff;
  }

  // Check if data is ready
  if (!s_measurement_ready) {
    mmc5603nj_device_status1_t status;
    if (prv_mmc5603nj_read_status1(&status) && status.meas_m_done) {
      s_measurement_ready = true;
    }
  }
  if (!s_measurement_ready) {
    return MagReadCommunicationFail;
  }

  // Ready data
  uint8_t raw_data[9];
  if (!prv_mmc5603nj_read(MMC5603NJ_REG_XOUT1, sizeof(raw_data), raw_data)) {
    return MagReadCommunicationFail;
  }

  int16_t raw_vector[3];
  for (uint8_t axis = 0; axis < 3; axis++) {
    uint16_t raw_axis_value = ((uint16_t)raw_data[2 * axis] << 8) | raw_data[2 * axis + 1];
    raw_vector[axis] = (int16_t)(raw_axis_value - 32768);  // offset by 2^15 for uint -> int alignment
  }

  sample->x = prv_get_axis_projection(X_AXIS, raw_vector);
  sample->y = prv_get_axis_projection(Y_AXIS, raw_vector);
  sample->z = prv_get_axis_projection(Z_AXIS, raw_vector);

  return MagReadSuccess;
}

static int16_t prv_get_axis_projection(axis_t axis, int16_t *raw_vector) {
  uint8_t axis_offset = BOARD_CONFIG_MAG.mag_config.axes_offsets[axis];
  int8_t axis_direction = BOARD_CONFIG_MAG.mag_config.axes_inverts[axis] ? -1 : 1;

  return axis_direction * raw_vector[axis_offset];
}
