/* SPDX-FileCopyrightText: 2026 Dave Bortz */
/* SPDX-License-Identifier: Apache-2.0 */

#include "lsm6dso_core.h"

#include "drivers/accel.h"
#include "drivers/gyro.h"
#include "drivers/i2c.h"
#include "drivers/rtc.h"
#include "kernel/util/sleep.h"
#include "system/logging.h"
#include "util/math.h"

#include <string.h>

PBL_LOG_MODULE_DEFINE(driver_lsm6dso_core, CONFIG_DRIVER_IMU_LOG_LEVEL);

// Maximum FIFO watermark supported by hardware (diff_fifo is 10 bits -> 0..1023)
#define LSM6DSO_CORE_FIFO_MAX_WATERMARK 1023

#define LSM6DSO_CORE_MAX_CONSECUTIVE_FAILURES 3

static bool s_core_initialized = false;

static Lsm6dsoCoreHealth s_health = {
  .sensor_health_ok = true,
};

// Per-half FIFO batching requests
typedef struct {
  uint32_t num_samples;
  uint32_t interval_us;
} Lsm6dsoFifoRequest;

static Lsm6dsoFifoRequest s_xl_request;
static Lsm6dsoFifoRequest s_gy_request;
static bool s_fifo_in_use = false;

static int32_t prv_lsm6dso_read(void *handle, uint8_t reg_addr, uint8_t *buffer,
                                uint16_t read_size);
static int32_t prv_lsm6dso_write(void *handle, uint8_t reg_addr, const uint8_t *buffer,
                                 uint16_t write_size);
static void prv_lsm6dso_mdelay(uint32_t ms);

stmdev_ctx_t lsm6dso_ctx = {
    .write_reg = prv_lsm6dso_write,
    .read_reg = prv_lsm6dso_read,
    .mdelay = prv_lsm6dso_mdelay,
};

Lsm6dsoCoreHealth *lsm6dso_core_health(void) { return &s_health; }

uint64_t lsm6dso_core_timestamp_ms(void) {
  time_t time_s;
  uint16_t time_ms;
  rtc_get_time_ms(&time_s, &time_ms);
  return (((uint64_t)time_s) * 1000 + time_ms);
}

// HAL context implementations

static int32_t prv_lsm6dso_read(void *handle, uint8_t reg_addr, uint8_t *buffer,
                                uint16_t read_size) {
  i2c_use(I2C_LSM6D);
  bool result = i2c_write_block(I2C_LSM6D, 1, &reg_addr);
  if (result) result = i2c_read_block(I2C_LSM6D, read_size, buffer);
  i2c_release(I2C_LSM6D);

  if (!result) {
    s_health.i2c_error_count++;
    s_health.consecutive_errors++;
    PBL_LOG_ERR("LSM6DSO: I2C read failed (reg=0x%02x, count=%lu)",
            reg_addr, s_health.consecutive_errors);
    if (s_health.consecutive_errors >= LSM6DSO_CORE_MAX_CONSECUTIVE_FAILURES) {
      s_health.sensor_health_ok = false;
      PBL_LOG_ERR("LSM6DSO: Sensor health degraded after %lu failures",
              s_health.consecutive_errors);
    }
    return -1;
  } else {
    s_health.consecutive_errors = 0;
    s_health.last_successful_read_ms = lsm6dso_core_timestamp_ms();
    s_health.sensor_health_ok = true;
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
    s_health.i2c_error_count++;
    s_health.consecutive_errors++;
    PBL_LOG_ERR("LSM6DSO: I2C write failed (reg=0x%02x)", reg_addr);
    return -1;
  } else {
    s_health.consecutive_errors = 0;
  }

  return 0;
}

static void prv_lsm6dso_mdelay(uint32_t ms) { psleep(ms); }

// Initialization

bool lsm6dso_core_is_initialized(void) { return s_core_initialized; }

bool lsm6dso_core_init(void) {
  if (s_core_initialized) {
    return true;
  }

  s_health = (Lsm6dsoCoreHealth){
    .sensor_health_ok = true,
  };

  // Verify sensor is present and functioning
  uint8_t whoami;
  if (lsm6dso_device_id_get(&lsm6dso_ctx, &whoami)) {
    PBL_LOG_ERR("LSM6DSO: Failed to read WHO_AM_I register");
    return false;
  }
  if (whoami != LSM6DSO_ID) {
    PBL_LOG_ERR("LSM6DSO: Sensor not detected or malfunctioning (WHO_AM_I=0x%02x, expecting 0x%02x)",
            whoami, LSM6DSO_ID);
    return false;
  }

  PBL_LOG_DBG("LSM6DSO: Sensor detected successfully (WHO_AM_I=0x%02x)", whoami);

  // Reset sensor to known state
  if (lsm6dso_reset_set(&lsm6dso_ctx, PROPERTY_ENABLE)) {
    PBL_LOG_ERR("LSM6DSO: Failed to reset sensor");
    return false;
  }
  uint8_t rst;
  int reset_timeout = 100; // 100ms max wait for reset
  do {  // Wait for reset to complete with timeout
    psleep(1);
    if (lsm6dso_reset_get(&lsm6dso_ctx, &rst) != 0) {
      PBL_LOG_ERR("LSM6DSO: Failed to read reset status");
      return false;
    }
    reset_timeout--;
  } while (rst && reset_timeout > 0);

  if (reset_timeout == 0) {
    PBL_LOG_ERR("LSM6DSO: Reset timeout - sensor may be unresponsive");
    return false;
  }

  // Disable I3C interface
  if (lsm6dso_i3c_disable_set(&lsm6dso_ctx, LSM6DSO_I3C_DISABLE)) {
    PBL_LOG_ERR("LSM6DSO: Failed to disable I3C interface");
    return false;
  }

  // Enable Block Data Update
  if (lsm6dso_block_data_update_set(&lsm6dso_ctx, PROPERTY_ENABLE)) {
    PBL_LOG_ERR("LSM6DSO: Failed to enable block data update");
    return false;
  }

  // Enable Auto Increment
  if (lsm6dso_auto_increment_set(&lsm6dso_ctx, PROPERTY_ENABLE)) {
    PBL_LOG_ERR("LSM6DSO: Failed to enable auto increment");
    return false;
  }

  // Set FIFO mode to bypass (will be reconfigured as necessary later)
  if (lsm6dso_fifo_mode_set(&lsm6dso_ctx, LSM6DSO_BYPASS_MODE)) {
    PBL_LOG_ERR("LSM6DSO: Failed to set FIFO mode to bypass");
    return false;
  }

  // Set default full scales
  if (lsm6dso_xl_full_scale_set(&lsm6dso_ctx, LSM6DSO_4g)) {
    PBL_LOG_ERR("LSM6DSO: Failed to set accelerometer full scale");
    return false;
  }
  if (lsm6dso_gy_full_scale_set(&lsm6dso_ctx, LSM6DSO_250dps)) {
    PBL_LOG_ERR("LSM6DSO: Failed to set gyroscope full scale");
    return false;
  }

  // Set output rates to zero (disabling sensors)
  if (lsm6dso_xl_data_rate_set(&lsm6dso_ctx, LSM6DSO_XL_ODR_OFF)) {
    PBL_LOG_ERR("LSM6DSO: Failed to set accelerometer ODR");
    return false;
  }
  if (lsm6dso_gy_data_rate_set(&lsm6dso_ctx, LSM6DSO_GY_ODR_OFF)) {
    PBL_LOG_ERR("LSM6DSO: Failed to set gyroscope ODR");
    return false;
  }

  // Use pulsed interrupts so that a missed edge does not suppress subsequent
  // interrupts (a single interrupt line may be shared by several sources).
  if (lsm6dso_data_ready_mode_set(&lsm6dso_ctx, LSM6DSO_DRDY_PULSED)) {
    PBL_LOG_ERR("LSM6DSO: Failed to set data ready mode");
    return false;
  }
  if (lsm6dso_int_notification_set(&lsm6dso_ctx, LSM6DSO_ALL_INT_PULSED)) {
    PBL_LOG_ERR("LSM6DSO: Failed to configure interrupt notification");
    return false;
  }

  s_core_initialized = true;
  s_health.last_successful_read_ms = lsm6dso_core_timestamp_ms();
  s_health.consecutive_errors = 0;
  PBL_LOG_DBG("LSM6DSO: Core initialization complete");
  return true;
}

bool lsm6dso_core_reinit(void) {
  s_core_initialized = false;
  s_fifo_in_use = false;
  s_health.sensor_health_ok = false;
  s_health.consecutive_errors = 0;
  return lsm6dso_core_init();
}

// Shared FIFO management

bool lsm6dso_core_fifo_in_use(void) { return s_fifo_in_use; }

// Map a sampling interval to the accelerometer FIFO batching rate enum
static lsm6dso_bdr_xl_t prv_get_fifo_batch_rate_xl(uint32_t interval_us) {
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

// Map a sampling interval to the gyroscope FIFO batching rate enum
static lsm6dso_bdr_gy_t prv_get_fifo_batch_rate_gy(uint32_t interval_us) {
  if (interval_us >= 80000) return LSM6DSO_GY_BATCHED_AT_12Hz5;
  if (interval_us >= 38462) return LSM6DSO_GY_BATCHED_AT_26Hz;
  if (interval_us >= 19231) return LSM6DSO_GY_BATCHED_AT_52Hz;
  if (interval_us >= 9615) return LSM6DSO_GY_BATCHED_AT_104Hz;
  if (interval_us >= 4808) return LSM6DSO_GY_BATCHED_AT_208Hz;
  if (interval_us >= 2398) return LSM6DSO_GY_BATCHED_AT_417Hz;
  if (interval_us >= 1200) return LSM6DSO_GY_BATCHED_AT_833Hz;
  if (interval_us >= 600) return LSM6DSO_GY_BATCHED_AT_1667Hz;
  if (interval_us >= 300) return LSM6DSO_GY_BATCHED_AT_3333Hz;
  return LSM6DSO_GY_BATCHED_AT_6667Hz;
}

static bool prv_request_batching(const Lsm6dsoFifoRequest *req) {
  return req->num_samples > 1 && req->interval_us > 0;
}

//! Records per second one side contributes to the FIFO (0 when not batching)
static uint32_t prv_records_per_second(const Lsm6dsoFifoRequest *req) {
  if (!prv_request_batching(req)) {
    return 0;
  }
  return (1000000 + req->interval_us - 1) / req->interval_us;
}

//! Program watermark and batch rates from the current per-half requests.
//! @param watermark_override when nonzero, use this watermark instead of the
//!   computed one (used by overflow recovery to back off).
static void prv_fifo_update(uint16_t watermark_override) {
  bool xl_batch = prv_request_batching(&s_xl_request);
  bool gy_batch = prv_request_batching(&s_gy_request);

  if (!xl_batch && !gy_batch) {
    if (s_fifo_in_use) {
      // Disable batching & return to bypass
      lsm6dso_fifo_xl_batch_set(&lsm6dso_ctx, LSM6DSO_XL_NOT_BATCHED);
      lsm6dso_fifo_gy_batch_set(&lsm6dso_ctx, LSM6DSO_GY_NOT_BATCHED);
      if (lsm6dso_fifo_mode_set(&lsm6dso_ctx, LSM6DSO_BYPASS_MODE)) {
        PBL_LOG_ERR("LSM6DSO: Failed to disable FIFO");
      }
    }
    s_fifo_in_use = false;
    PBL_LOG_DBG("LSM6DSO: FIFO disabled");
    return;
  }

  uint16_t watermark;
  if (watermark_override > 0) {
    watermark = watermark_override;
  } else {
    // The watermark must wake us up before either half's delivery deadline.
    // Each half wants delivery after num_samples; following the established
    // pattern we target 50% of that to leave headroom against overflow. With
    // both halves batching, the FIFO fills at the combined record rate, so
    // convert the earliest per-half deadline into a record count.
    uint32_t total_rate = prv_records_per_second(&s_xl_request) +
                          prv_records_per_second(&s_gy_request);
    uint64_t deadline_us = UINT64_MAX;
    if (xl_batch) {
      uint64_t xl_deadline =
          ((uint64_t)MAX(s_xl_request.num_samples / 2, 1)) * s_xl_request.interval_us;
      deadline_us = MIN(deadline_us, xl_deadline);
    }
    if (gy_batch) {
      uint64_t gy_deadline =
          ((uint64_t)MAX(s_gy_request.num_samples / 2, 1)) * s_gy_request.interval_us;
      deadline_us = MIN(deadline_us, gy_deadline);
    }
    uint64_t watermark64 = (deadline_us * total_rate) / 1000000;
    if (watermark64 == 0) watermark64 = 1;
    if (watermark64 > LSM6DSO_CORE_FIFO_MAX_WATERMARK) {
      watermark64 = LSM6DSO_CORE_FIFO_MAX_WATERMARK;
    }
    watermark = (uint16_t)watermark64;
  }

  PBL_LOG_DBG("LSM6DSO: Setting FIFO watermark to %u (xl=%lu@%luus gy=%lu@%luus)",
          watermark, s_xl_request.num_samples, s_xl_request.interval_us,
          s_gy_request.num_samples, s_gy_request.interval_us);

  if (lsm6dso_fifo_watermark_set(&lsm6dso_ctx, watermark)) {
    PBL_LOG_ERR("LSM6DSO: Failed to set FIFO watermark");
  }

  if (lsm6dso_fifo_xl_batch_set(&lsm6dso_ctx, xl_batch
          ? prv_get_fifo_batch_rate_xl(s_xl_request.interval_us)
          : LSM6DSO_XL_NOT_BATCHED)) {
    PBL_LOG_ERR("LSM6DSO: Failed to set accel FIFO batch rate");
  }
  if (lsm6dso_fifo_gy_batch_set(&lsm6dso_ctx, gy_batch
          ? prv_get_fifo_batch_rate_gy(s_gy_request.interval_us)
          : LSM6DSO_GY_NOT_BATCHED)) {
    PBL_LOG_ERR("LSM6DSO: Failed to set gyro FIFO batch rate");
  }

  // Always clear and re-enable FIFO to ensure clean state after configuration
  // changes. This is critical when the watermark changes while the FIFO is
  // already enabled, as stale samples in the FIFO can prevent new watermark
  // interrupts from being generated.
  lsm6dso_fifo_mode_set(&lsm6dso_ctx, LSM6DSO_BYPASS_MODE);
  psleep(1); // Allow time for FIFO to clear

  // Put FIFO in stream mode so we keep collecting samples and get periodic
  // watermark interrupts
  if (lsm6dso_fifo_mode_set(&lsm6dso_ctx, LSM6DSO_STREAM_MODE)) {
    PBL_LOG_ERR("LSM6DSO: Failed to enable FIFO stream mode");
  }

  s_fifo_in_use = true;
}

void lsm6dso_core_fifo_request_xl(uint32_t num_samples, uint32_t interval_us) {
  s_xl_request = (Lsm6dsoFifoRequest){
    .num_samples = num_samples,
    .interval_us = interval_us,
  };
  prv_fifo_update(0);
}

void lsm6dso_core_fifo_request_gy(uint32_t num_samples, uint32_t interval_us) {
  s_gy_request = (Lsm6dsoFifoRequest){
    .num_samples = num_samples,
    .interval_us = interval_us,
  };
  prv_fifo_update(0);
}

void lsm6dso_core_fifo_recover(void) {
  PBL_LOG_WRN("LSM6DSO: FIFO overflow/full detected, clearing FIFO");

  uint16_t current_watermark = 0;
  lsm6dso_fifo_watermark_get(&lsm6dso_ctx, &current_watermark);

  // Reset FIFO to bypass mode and wait for it to actually clear
  lsm6dso_fifo_mode_set(&lsm6dso_ctx, LSM6DSO_BYPASS_MODE);
  psleep(1);

  // Clear all interrupt sources after FIFO reset to ensure clean state
  lsm6dso_all_sources_t clear_sources;
  lsm6dso_all_sources_get(&lsm6dso_ctx, &clear_sources);

  if (s_fifo_in_use) {
    // Reduce watermark by half to prevent future overflow
    uint16_t reduced_watermark = current_watermark / 2;
    if (reduced_watermark == 0) reduced_watermark = 1;
    prv_fifo_update(reduced_watermark);
    PBL_LOG_INFO("LSM6DSO: Reduced FIFO watermark from %u to %u to prevent future overflow",
            current_watermark, reduced_watermark);
  }
}

void lsm6dso_core_fifo_drain(void) {
  uint16_t fifo_level = 0;
  if (lsm6dso_fifo_data_level_get(&lsm6dso_ctx, &fifo_level) != 0) {
    PBL_LOG_ERR("LSM6DSO: Failed to read FIFO level");
    // Reset FIFO on communication error
    lsm6dso_fifo_mode_set(&lsm6dso_ctx, LSM6DSO_BYPASS_MODE);
    if (s_fifo_in_use) {
      lsm6dso_fifo_mode_set(&lsm6dso_ctx, LSM6DSO_STREAM_MODE);
    }
    return;
  }
  if (fifo_level == 0) {
    return;  // nothing to do
  }

  // Prevent infinite loops on stuck FIFO
  if (fifo_level > LSM6DSO_CORE_FIFO_MAX_WATERMARK) {
    PBL_LOG_ERR("LSM6DSO: FIFO level too high (%u), resetting", fifo_level);
    lsm6dso_fifo_mode_set(&lsm6dso_ctx, LSM6DSO_BYPASS_MODE);
    if (s_fifo_in_use) {
      lsm6dso_fifo_mode_set(&lsm6dso_ctx, LSM6DSO_STREAM_MODE);
    }
    return;
  }

  const uint64_t now_us = lsm6dso_core_timestamp_ms() * 1000ULL;

  // Records from both sensors are interleaved in the FIFO. For timestamping,
  // estimate how many records belong to each half from the batch rates, then
  // assume each half's records are contiguous samples ending now. Timestamps
  // are documented as approximate.
  const uint32_t xl_rate = prv_records_per_second(&s_xl_request);
  const uint32_t gy_rate = prv_records_per_second(&s_gy_request);
  const uint32_t total_rate = xl_rate + gy_rate;
  uint32_t xl_expected = 0;
  uint32_t gy_expected = 0;
  if (total_rate > 0) {
    xl_expected = ((uint64_t)fifo_level * xl_rate) / total_rate;
    gy_expected = fifo_level - xl_expected;
  }
  const uint32_t xl_interval_us = s_xl_request.interval_us ?: 1000;  // avoid div by zero
  const uint32_t gy_interval_us = s_gy_request.interval_us ?: 1000;

  uint32_t xl_seen = 0;
  uint32_t gy_seen = 0;

  for (uint16_t i = 0; i < fifo_level; ++i) {
    uint8_t raw_bytes[7];
    if (lsm6dso_read_reg(&lsm6dso_ctx, LSM6DSO_FIFO_DATA_OUT_TAG, raw_bytes, sizeof(raw_bytes)) != 0) {
      PBL_LOG_ERR("LSM6DSO: Failed to read FIFO sample (%u/%u)", i, fifo_level);
      // Reset FIFO on communication error
      lsm6dso_fifo_mode_set(&lsm6dso_ctx, LSM6DSO_BYPASS_MODE);
      if (s_fifo_in_use) {
        lsm6dso_fifo_mode_set(&lsm6dso_ctx, LSM6DSO_STREAM_MODE);
      }
      break;
    }

    lsm6dso_fifo_tag_t tag = raw_bytes[0] >> 3;
    if (tag == LSM6DSO_XL_NC_TAG || tag == LSM6DSO_XL_NC_T_1_TAG ||
        tag == LSM6DSO_XL_NC_T_2_TAG || tag == LSM6DSO_XL_2XC_TAG ||
        tag == LSM6DSO_XL_3XC_TAG) {
      uint32_t index_from_end = (xl_expected > xl_seen) ? (xl_expected - 1 - xl_seen) : 0;
      uint64_t timestamp_us = now_us - (index_from_end * (uint64_t)xl_interval_us);
#if defined(CONFIG_ACCEL_LSM6DSO)
      lsm6dso_accel_handle_fifo_record(&raw_bytes[1], timestamp_us);
#else
      (void)timestamp_us;
#endif
      xl_seen++;
    } else if (tag == LSM6DSO_GYRO_NC_TAG) {
      uint32_t index_from_end = (gy_expected > gy_seen) ? (gy_expected - 1 - gy_seen) : 0;
      uint64_t timestamp_us = now_us - (index_from_end * (uint64_t)gy_interval_us);
#if defined(CONFIG_GYRO_LSM6DSO)
      lsm6dso_gyro_handle_fifo_record(&raw_bytes[1], timestamp_us);
#else
      (void)timestamp_us;
#endif
      gy_seen++;
    }
    // Other tags (timestamp/config/etc.) are ignored
  }
}

void lsm6dso_core_offload_work(Lsm6dsoCoreWorkCallback cb) {
#if defined(CONFIG_ACCEL_LSM6DSO)
  accel_offload_work(cb);
#else
  gyro_offload_work(cb);
#endif
}
