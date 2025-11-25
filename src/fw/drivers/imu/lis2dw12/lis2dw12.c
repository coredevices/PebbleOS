/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "board/board.h"
#include "drivers/accel.h"
#include "drivers/i2c.h"
#include "drivers/exti.h"
#include "kernel/util/sleep.h"
#include "system/logging.h"
#include "drivers/vibe.h"
#include "services/common/vibe_pattern.h"
#include "system/passert.h"
#include "lis2dw12_reg.h"

// Error recovery thresholds and watchdog timeouts
#define LIS2DW12_MAX_CONSECUTIVE_FAILURES 3
#define LIS2DW12_INTERRUPT_GAP_LOG_THRESHOLD_MS 3000
#define LIS2DW12_FIFO_MAX_WATERMARK 32
// Delay after detecting a vibe before shake/tap interrupts should be processed again
#define LIS2DW12_VIBE_COOLDOWN_MS (50)
#define LIS2DW12_REG_OPS_WAIT_TIME_MS (1)

static int32_t prv_lis2dw12_write(void* handler, uint8_t reg, const uint8_t* data, uint16_t len);
static int32_t prv_lis2dw12_read(void* handler, uint8_t reg, uint8_t* data, uint16_t len);

static stmdev_ctx_t lis2dw12_ctx = {
  .read_reg = prv_lis2dw12_read,
  .write_reg = prv_lis2dw12_write,
};

typedef struct {
  lis2dw12_odr_t odr;
  uint32_t interval_thr;
  uint32_t interval;
} lis2dw12_sample_rate_t;

static const lis2dw12_sample_rate_t s_lis2dw12_sample_rates[] = {
  {LIS2DW12_XL_ODR_1Hz6_LP_ONLY, 200000, 625000},
  {LIS2DW12_XL_ODR_12Hz5, 60000, 80000},
  {LIS2DW12_XL_ODR_25Hz, 30000, 40000},
  {LIS2DW12_XL_ODR_50Hz, 15000, 20000},
  {LIS2DW12_XL_ODR_100Hz, 7500, 10000},
  {LIS2DW12_XL_ODR_200Hz, 3750, 5000},
  {LIS2DW12_XL_ODR_400Hz, 1875, 2500},
  {LIS2DW12_XL_ODR_800Hz, 937, 1250},
  {LIS2DW12_XL_ODR_1k6Hz, 468, 625},
};

static bool s_lis2dw12_enabled = true;
static bool s_lis2dw12_running = false;
static bool s_fifo_in_use = false;  // true when we have enabled FIFO batching
static uint32_t s_last_vibe_detected = 0;
// User-configured sensitivity percentage (0-100), where 100 = most sensitive
// Default to 100% (maximum sensitivity) to maintain current behavior
static uint8_t s_user_sensitivity_percent = 100;
// Error tracking and recovery
static uint32_t s_consecutive_errors = 0;
static bool s_sensor_health_ok = true;
static int16_t s_last_sample_mg[3] = {0};
static uint64_t s_last_sample_timestamp_ms = 0;
// Interrupt activity instrumentation so we can spot when the sensor stops firing INT1.
static uint64_t s_last_interrupt_ms = 0;
static uint64_t s_last_wake_event_ms = 0;
static uint64_t s_last_double_tap_ms = 0;
static uint32_t s_interrupt_count = 0;
static uint32_t s_wake_event_count = 0;
static uint32_t s_double_tap_event_count = 0;

typedef enum {
  X_AXIS = 0,
  Y_AXIS = 1,
  Z_AXIS = 2,
} axis_t;

typedef struct {
  uint32_t sampling_interval_us;
  uint32_t num_samples;
  bool shake_detection_enabled;
  bool shake_sensitivity_high;
  bool double_tap_detection_enabled;
} lis2dw12_state_t;
lis2dw12_state_t s_lis2dw12_state = {0};
lis2dw12_state_t s_lis2dw12_state_target = {0};
static int32_t prv_lis2dw12_write(void* handler, uint8_t reg, const uint8_t* data, uint16_t len) {
  i2c_use(I2C_LSM2DW12);
  uint16_t data_len = len + sizeof(reg);
  uint8_t data_w[data_len];
  data_w[0] = reg;
  memcpy(data_w+1, data, len);
  bool rv = i2c_write_block(I2C_LSM2DW12, data_len, data_w);
  i2c_release(I2C_LSM2DW12);

  return !rv;
}

static int32_t prv_lis2dw12_read(void* handler, uint8_t reg, uint8_t* data, uint16_t len) {
  i2c_use(I2C_LSM2DW12);
  bool rv = i2c_write_block(I2C_LSM2DW12, sizeof(reg), &reg);
  if (rv) {
    rv = i2c_read_block(I2C_LSM2DW12, len, data);
  }
  i2c_release(I2C_LSM2DW12);

  return !rv;
}
static uint64_t prv_get_timestamp_ms(void) {
  time_t time_s;
  uint16_t time_ms;
  rtc_get_time_ms(&time_s, &time_ms);
  return (((uint64_t)time_s) * 1000 + time_ms);
}

static bool prv_is_vibing(void) {
  if (vibes_get_vibe_strength() != VIBE_STRENGTH_OFF) {
    s_last_vibe_detected = prv_get_timestamp_ms();
    return true;
  }
  if (s_last_vibe_detected > 0) {
    if (prv_get_timestamp_ms() - s_last_vibe_detected < LIS2DW12_VIBE_COOLDOWN_MS) {
      return true;
    } else {
      s_last_vibe_detected = 0;  // reset if cooldown expired
    }
  }
  return false;
}

static void prv_note_new_sample(const AccelDriverSample *sample) {
  if (!sample) {
    return;
  }

  s_last_sample_mg[0] = sample->x;
  s_last_sample_mg[1] = sample->y;
  s_last_sample_mg[2] = sample->z;

  if (sample->timestamp_us != 0) {
    s_last_sample_timestamp_ms = sample->timestamp_us / 1000ULL;
  } else {
    s_last_sample_timestamp_ms = prv_get_timestamp_ms();
  }
}

static void prv_note_new_sample_mg(int16_t x_mg, int16_t y_mg, int16_t z_mg) {
  AccelDriverSample sample = {
      .x = x_mg,
      .y = y_mg,
      .z = z_mg,
      .timestamp_us = prv_get_timestamp_ms() * 1000ULL,
  };
  prv_note_new_sample(&sample);
}

static int16_t prv_get_axis_projection_mg(axis_t axis, int16_t *raw_vector) {
  uint8_t axis_offset = BOARD_CONFIG_ACCEL.accel_config.axes_offsets[axis];
  int axis_direction = BOARD_CONFIG_ACCEL.accel_config.axes_inverts[axis] ? -1 : 1;

  return lis2dw12_from_fs2_lp1_to_mg(raw_vector[axis_offset] * axis_direction);
}

static uint8_t prv_lis2dw12_read_sample(AccelDriverSample *data) {
  if (!s_lis2dw12_enabled) {
    PBL_LOG(LOG_LEVEL_ERROR, "LIS2DW12: Not enabled, cannot read sample");
    return -1;
  }  

  int16_t accel_raw[3];
  if (lis2dw12_acceleration_raw_get(&lis2dw12_ctx, accel_raw) != 0) {
    PBL_LOG(LOG_LEVEL_ERROR, "LIS2DW12: Failed to read accelerometer data");
    return -1;
  }

  data->x = prv_get_axis_projection_mg(X_AXIS, accel_raw);
  data->y = prv_get_axis_projection_mg(Y_AXIS, accel_raw);
  data->z = prv_get_axis_projection_mg(Z_AXIS, accel_raw);
  data->timestamp_us = prv_get_timestamp_ms() * 1000;

  prv_note_new_sample(data);

  if (s_lis2dw12_state.num_samples > 0) { 
    accel_cb_new_sample(data);
  }

  return 0;
}

// Accelerometer sample reading (and reporting)
static void prv_lis2dw12_read_samples(void) {
  if (s_lis2dw12_state.num_samples <= 1 || !s_fifo_in_use) {
    // Single sample path
    AccelDriverSample sample;
    prv_lis2dw12_read_sample(&sample);
    return;
  }

  // Drain FIFO
  uint8_t fifo_level = 0;
  if (lis2dw12_fifo_data_level_get(&lis2dw12_ctx, &fifo_level) != 0) {
    PBL_LOG(LOG_LEVEL_ERROR, "LIS2DW12: Failed to read FIFO level");
    // Reset FIFO on communication error
    lis2dw12_fifo_mode_set(&lis2dw12_ctx, LIS2DW12_BYPASS_MODE);
    if (s_fifo_in_use) {
      lis2dw12_fifo_mode_set(&lis2dw12_ctx, LIS2DW12_BYPASS_TO_STREAM_MODE);
    }
    return;
  }
  if (fifo_level == 0) {
    return;  // nothing to do
  }

  // Prevent infinite loops on stuck FIFO
  if (fifo_level > LIS2DW12_FIFO_MAX_WATERMARK) {
    PBL_LOG(LOG_LEVEL_ERROR, "LIS2DW12: FIFO level too high (%u), resetting", fifo_level);
    // Reset FIFO on communication error
    lis2dw12_fifo_mode_set(&lis2dw12_ctx, LIS2DW12_BYPASS_MODE);
    if (s_fifo_in_use) {
      lis2dw12_fifo_mode_set(&lis2dw12_ctx, LIS2DW12_BYPASS_TO_STREAM_MODE);
    }
    return;
  }

  const uint64_t now_us = prv_get_timestamp_ms() * 1000ULL;
  const uint32_t interval_us = s_lis2dw12_state.sampling_interval_us ?: 1000;  // avoid div by zero

  for (uint16_t i = 0; i < fifo_level; ++i) {
    AccelDriverSample sample = {0};
    prv_lis2dw12_read_sample(&sample);

    // Approximate timestamp: assume fifo_level contiguous samples ending now
    uint32_t sample_index_from_end = (fifo_level - 1) - i;  // 0 for newest
    sample.timestamp_us = now_us - (sample_index_from_end * (uint64_t)interval_us);
    accel_cb_new_sample(&sample);
    prv_note_new_sample(&sample);
  }
}

static void prv_lis2dw12_process_interrupts(void) {
  const uint64_t now_ms = prv_get_timestamp_ms();
  const uint64_t previous_interrupt_ms = s_last_interrupt_ms;
  s_last_interrupt_ms = now_ms;
  s_interrupt_count++;

  uint32_t gap_ms = 0;
  if (previous_interrupt_ms == 0) {
    PBL_LOG(LOG_LEVEL_INFO, "LIS2DW12: First INT1 service (count=%lu)",
            (unsigned long)s_interrupt_count);
  } else {
    uint64_t raw_gap_ms = now_ms - previous_interrupt_ms;
    gap_ms = (raw_gap_ms > UINT32_MAX) ? UINT32_MAX : (uint32_t)raw_gap_ms;
    if (gap_ms >= LIS2DW12_INTERRUPT_GAP_LOG_THRESHOLD_MS) {
      PBL_LOG(LOG_LEVEL_INFO,
              "LIS2DW12: INT1 gap %lu ms (count=%lu wake=%lu tap=%lu)",
              (unsigned long)gap_ms, (unsigned long)s_interrupt_count,
              (unsigned long)s_wake_event_count, (unsigned long)s_double_tap_event_count);
    }
  }

  // Read and clear interrupt sources atomically to prevent loss
  lis2dw12_all_sources_t all_sources;
  
  // Multiple attempts to read interrupt sources in case of transient I2C issues
  int read_attempts = 0;
  const int max_read_attempts = 2;
  
  do {
    if (lis2dw12_all_sources_get(&lis2dw12_ctx, &all_sources) == 0) {
      break; // Success
    }
    read_attempts++;
    if (read_attempts < max_read_attempts) {
      // Brief delay and retry - this prevents losing interrupts due to transient I2C glitches
      psleep(LIS2DW12_REG_OPS_WAIT_TIME_MS);
    }
  } while (read_attempts < max_read_attempts);
  
  if (read_attempts >= max_read_attempts) {
    PBL_LOG(LOG_LEVEL_ERROR, "LIS2DW12: Failed to read interrupt sources after retries");
    s_consecutive_errors++;
    if (s_consecutive_errors >= LIS2DW12_MAX_CONSECUTIVE_FAILURES) {
      s_sensor_health_ok = false;
      PBL_LOG(LOG_LEVEL_WARNING, "LIS2DW12: Interrupt processing failed, sensor health degraded");
    }
    return;
  }
  
  // Reset failure count on successful read
  s_consecutive_errors = 0;

  if (all_sources.status_dup.ovr) {
    PBL_LOG(LOG_LEVEL_WARNING, "LIS2DW12: FIFO overflow/full detected, clearing FIFO");
    
    // Properly clear FIFO without losing configuration
    uint8_t current_watermark;
    
    // Save current FIFO configuration
    lis2dw12_fifo_watermark_get(&lis2dw12_ctx, &current_watermark);
    
    // Reset FIFO to bypass mode
    lis2dw12_fifo_mode_set(&lis2dw12_ctx, LIS2DW12_BYPASS_MODE);
    
    // Wait for FIFO to actually clear
    psleep(LIS2DW12_REG_OPS_WAIT_TIME_MS);
    
    // Clear all interrupt sources after FIFO reset to ensure clean state
    lis2dw12_all_sources_t all_sources;
	  lis2dw12_all_sources_get(&lis2dw12_ctx, &all_sources);

    // Restore FIFO configuration if it was enabled
    if (s_fifo_in_use) {
      // Reduce watermark by half to prevent future overflow
      uint8_t reduced_watermark = current_watermark / 2;
      if (reduced_watermark == 0) reduced_watermark = 1;
      
      lis2dw12_fifo_watermark_set(&lis2dw12_ctx, reduced_watermark);
      lis2dw12_fifo_mode_set(&lis2dw12_ctx, LIS2DW12_BYPASS_TO_STREAM_MODE);

      PBL_LOG(LOG_LEVEL_INFO, "LIS2DW12: Reduced FIFO watermark from %u to %u to prevent future overflow",
              current_watermark, reduced_watermark);
    }

    // Force re-enable of external interrupt to ensure it's active
    exti_disable(BOARD_CONFIG_ACCEL.accel_ints[0]);
    psleep(LIS2DW12_REG_OPS_WAIT_TIME_MS);
    exti_enable(BOARD_CONFIG_ACCEL.accel_ints[0]);
  }

  // Collect accelerometer samples if requested
  if (s_lis2dw12_state.num_samples > 0 && (all_sources.status_dup.drdy || all_sources.status_dup.ovr)) {
    prv_lis2dw12_read_samples();
  }

  // If currently vibing, any additional events should be ignored (they are 
  // likely spurious).
  if (prv_is_vibing()) {
    return;
  }

  // Process double tap events
  if (all_sources.tap_src.double_tap) {
    s_double_tap_event_count++;
    s_last_double_tap_ms = now_ms;
    PBL_LOG(LOG_LEVEL_DEBUG, "LIS2DW12: Double tap interrupt triggered");
    // Handle double tap detection
    axis_t axis;
    if (all_sources.tap_src.x_tap) {
      axis = X_AXIS;
    } else if (all_sources.tap_src.y_tap) {
      axis = Y_AXIS;
    } else if (all_sources.tap_src.z_tap) {
      axis = Z_AXIS;
    } else {
      PBL_LOG(LOG_LEVEL_DEBUG, "LIS2DW12: No tap axis detected");
      return;  // No valid tap detected
    }

    uint8_t axis_offset = BOARD_CONFIG_ACCEL.accel_config.axes_offsets[axis];
    uint8_t axis_direction = (BOARD_CONFIG_ACCEL.accel_config.axes_inverts[axis] ? -1 : 1) *
                             (all_sources.tap_src.tap_sign ? -1 : 1);

    PBL_LOG(LOG_LEVEL_DEBUG, "LIS2DW12: Double tap interrupt triggered; axis=%d, direction=%d",
            axis_offset, axis_direction);
    accel_cb_double_tap_detected(axis_offset, axis_direction);
  }

  // Wake-up (any-motion) event -> treat as shake. Axis & direction derived from wake_up_src.
  if (s_lis2dw12_state.shake_detection_enabled && all_sources.wake_up_src.wu_ia) {
    s_wake_event_count++;
    s_last_wake_event_ms = now_ms;
    
    // Use wake_up_src already read by all_sources_get - don't re-read the register
    // as that can clear flags and cause race conditions with pulsed interrupts
    IMUCoordinateAxis axis = AXIS_X;
    int32_t direction = 1;
    
    // Determine which axis triggered from already-read wake_up_src
    const AccelConfig *cfg = &BOARD_CONFIG_ACCEL.accel_config;
    if (all_sources.wake_up_src.x_wu) {
      axis = AXIS_X;
    } else if (all_sources.wake_up_src.y_wu) {
      axis = AXIS_Y;
    } else if (all_sources.wake_up_src.z_wu) {
      axis = AXIS_Z;
    }
    
    // Use last known sample for direction instead of reading new data during interrupt
    // Reading acceleration data here can interfere with FIFO operation
    int16_t val = s_last_sample_mg[cfg->axes_offsets[axis]];
    bool invert = cfg->axes_inverts[axis];
    direction = (val >= 0 ? 1 : -1) * (invert ? -1 : 1);
    
    PBL_LOG(LOG_LEVEL_DEBUG, "LIS2DW12: Shake detected; axis=%d, direction=%lu", axis, direction);
    accel_cb_shake_detected(axis, direction);

    // The wake-up interrupt is latched - if the wake condition is still true (device still
    // moving above threshold), the flag immediately re-latches after reading, keeping INT1
    // asserted high and preventing new edge interrupts. To fix this, we temporarily disable
    // wake-up interrupt routing, clear the sources, then re-enable. This forces a clean reset.
    if (s_fifo_in_use) {
      // Disable wake-up interrupt routing temporarily
      lis2dw12_ctrl4_int1_pad_ctrl_t int1_routes;
      lis2dw12_pin_int1_route_get(&lis2dw12_ctx, &int1_routes);
      uint8_t saved_wu = int1_routes.int1_wu;
      int1_routes.int1_wu = 0;
      lis2dw12_pin_int1_route_set(&lis2dw12_ctx, &int1_routes);

      // Clear interrupt sources while wake-up is disabled
      lis2dw12_all_sources_t clear_sources;
      lis2dw12_all_sources_get(&lis2dw12_ctx, &clear_sources);

      // Re-enable wake-up interrupt routing
      int1_routes.int1_wu = saved_wu;
      lis2dw12_pin_int1_route_set(&lis2dw12_ctx, &int1_routes);
    }
  }
}

static void prv_lis2dw12_interrupt_handler(bool *should_context_switch) {
  // Offload processing to a worker. The LIS2DW12 can miss events if interrupts
  // are ignored due to pending flags, so it is important to process them
  // quickly. The actual clearing of the interrupt flags will happen in the
  // worker via an I2C transaction.
  accel_offload_work_from_isr(prv_lis2dw12_process_interrupts, should_context_switch);
}

static void prv_lis2dw12_configure_fifo(bool enable) {
  // Always (re)program watermark and batch rates when enabling or already enabled,
  // but only flip FIFO mode when the enabled/disabled state changes.
  if (enable) {
    // Proper FIFO watermark calculation to prevent overflow
    // Setting watermark too high can cause overflow and sensor lockup
    
    uint32_t watermark = s_lis2dw12_state.num_samples;
    
    // Set watermark to 50% of requested samples to prevent overflow
    // This provides more buffer for timing variations and prevents lockup
    watermark = watermark / 2;
    if (watermark == 0) watermark = 1;  // minimum
    if (watermark > LIS2DW12_FIFO_MAX_WATERMARK) watermark = LIS2DW12_FIFO_MAX_WATERMARK;
    
    PBL_LOG(LOG_LEVEL_DEBUG, "LIS2DW12: Setting FIFO watermark to %lu (requested %lu samples)", 
            watermark, s_lis2dw12_state.num_samples);
    
    if (lis2dw12_fifo_watermark_set(&lis2dw12_ctx, (uint8_t)watermark)) {
      PBL_LOG(LOG_LEVEL_ERROR, "LIS2DW12: Failed to set FIFO watermark");
    }

    // Always clear and re-enable FIFO to ensure clean state after configuration changes.
    // This is critical when watermark changes while FIFO is already enabled, as stale
    // samples in the FIFO can prevent new watermark interrupts from being generated.
    // For example, if FIFO has 25 samples and watermark is lowered to 3, the sensor
    // won't generate an interrupt because the FIFO already exceeds the watermark.
    lis2dw12_fifo_mode_set(&lis2dw12_ctx, LIS2DW12_BYPASS_MODE);
    psleep(LIS2DW12_REG_OPS_WAIT_TIME_MS); // Allow time for FIFO to clear

    // Put FIFO in bypass-to-stream mode instead of pure stream mode.
    // Stream mode continuously fills the FIFO even during interrupt processing,
    // which can cause overflow races and interfere with wake-up (shake) detection.
    // Bypass-to-stream provides cleaner state transitions and prevents overflow loops.
    if (lis2dw12_fifo_mode_set(&lis2dw12_ctx, LIS2DW12_BYPASS_TO_STREAM_MODE)) {
      PBL_LOG(LOG_LEVEL_ERROR, "LIS2DW12: Failed to enable FIFO bypass-to-stream mode");
    }
  } else {
    if (s_fifo_in_use) {
      // Disable batching & return to bypass
      if (lis2dw12_fifo_mode_set(&lis2dw12_ctx, LIS2DW12_BYPASS_MODE)) {
        PBL_LOG(LOG_LEVEL_ERROR, "LIS2DW12: Failed to disable FIFO");
      }
    }
  }

  s_fifo_in_use = enable;
  PBL_LOG(LOG_LEVEL_DEBUG, "LIS2DW12: FIFO %s (wm=%lu)", enable ? "enabled" : "disabled",
          (unsigned long)s_lis2dw12_state.num_samples);
}

void prv_lis2dw12_configure_double_tap(bool enable) {
  if (enable) {
    // Enable tap detection on all axes
    lis2dw12_tap_detection_on_x_set(&lis2dw12_ctx, PROPERTY_ENABLE);
    lis2dw12_tap_detection_on_y_set(&lis2dw12_ctx, PROPERTY_ENABLE);
    lis2dw12_tap_detection_on_z_set(&lis2dw12_ctx, PROPERTY_ENABLE);

    // Configure tap timing
    uint8_t tap_shock = BOARD_CONFIG_ACCEL.accel_config.tap_shock;
    uint8_t tap_quiet = BOARD_CONFIG_ACCEL.accel_config.tap_quiet;
    uint8_t tap_dur = BOARD_CONFIG_ACCEL.accel_config.tap_dur;

    lis2dw12_tap_shock_set(&lis2dw12_ctx, tap_shock);  // Shock duration
    lis2dw12_tap_quiet_set(&lis2dw12_ctx, tap_quiet);  // Quiet period
    lis2dw12_tap_dur_set(&lis2dw12_ctx, tap_dur);      // Double tap window

    // Enable double tap recognition
    lis2dw12_tap_mode_set(&lis2dw12_ctx, LIS2DW12_BOTH_SINGLE_DOUBLE);
  } else {
    // Disable tap detection
    lis2dw12_tap_detection_on_x_set(&lis2dw12_ctx, PROPERTY_DISABLE);
    lis2dw12_tap_detection_on_y_set(&lis2dw12_ctx, PROPERTY_DISABLE);
    lis2dw12_tap_detection_on_z_set(&lis2dw12_ctx, PROPERTY_DISABLE);
  }
}

// Configure wake-up (any-motion) for shake detection using wake-up threshold & duration.
static void prv_lis2dw12_configure_shake(bool enable, bool sensitivity_high) {
  if (!enable) {
    // Disable wake-up related routing by clearing threshold
    lis2dw12_wkup_threshold_set(&lis2dw12_ctx, 0);
    return;
  }

  // Duration: increase a bit to reduce spurious triggers
  lis2dw12_wkup_dur_set(&lis2dw12_ctx, sensitivity_high ? 0 : 1);

  // Threshold calculation:
  // - Board config provides Low and High thresholds
  // - sensitivity_high flag indicates stationary mode (use low threshold for any movement)
  // - s_user_sensitivity_percent (0-100) controls normal mode threshold
  //   * 100% = most sensitive = use Low threshold
  //   * 50% = medium = interpolate between Low and High
  //   * 0% = least sensitive = use High threshold
  
  uint32_t raw_high = BOARD_CONFIG_ACCEL.accel_config.shake_thresholds[AccelThresholdHigh];
  uint32_t raw_low = BOARD_CONFIG_ACCEL.accel_config.shake_thresholds[AccelThresholdLow];
  uint32_t raw;
  
  if (sensitivity_high) {
    // Stationary mode: always use low threshold for maximum sensitivity
    raw = raw_low;
  } else {
    // Normal mode: interpolate based on user preference
    // Invert the percentage: 100% sensitive = low threshold, 0% sensitive = high threshold
    uint32_t inverted_percent = 100 - s_user_sensitivity_percent;
    raw = raw_low + ((raw_high - raw_low) * inverted_percent) / 100;
  }
  
  // Clamp to valid range
  if (raw > 63) raw = 63;  // lis2dw12 wk_ths is 6 bits
  if (raw < 2) raw = 2;     // Avoid noise storms with very low thresholds
  
  lis2dw12_wkup_threshold_set(&lis2dw12_ctx, (uint8_t)raw);
  
  PBL_LOG(LOG_LEVEL_DEBUG, "LIS2DW12: Shake threshold set to %lu (sensitivity_high=%d, user_percent=%u)", 
          raw, sensitivity_high, s_user_sensitivity_percent);
}

static uint32_t prv_lis2dw12_set_sampling_interval(uint32_t target_interval) {
  for (uint8_t i=0 ; i<sizeof(s_lis2dw12_sample_rates)/sizeof(lis2dw12_sample_rate_t); i++) {
    if (target_interval >= s_lis2dw12_sample_rates[i].interval_thr) {
      lis2dw12_odr_t odr = s_lis2dw12_sample_rates[i].odr;
      lis2dw12_data_rate_set(&lis2dw12_ctx, odr);
      return s_lis2dw12_sample_rates[i].interval;
    }
  }

  PBL_LOG(LOG_LEVEL_ERROR, "LIS2DW12 can not get a suitable odr");
  return UINT32_MAX;
}

static void prv_lis2dw12_configure_interrupts(void) {
  // Disable interrupts during configuration to prevent race conditions
  // and ensure atomic configuration updates

  bool should_enable_interrupts = s_lis2dw12_enabled &&
      (s_lis2dw12_state.num_samples || s_lis2dw12_state.shake_detection_enabled ||
       s_lis2dw12_state.double_tap_detection_enabled);

  // Always disable interrupts first to ensure clean state
  exti_disable(BOARD_CONFIG_ACCEL.accel_ints[0]);

  if (!should_enable_interrupts) {
    // Also disable all interrupt sources in the sensor to prevent phantom interrupts
    lis2dw12_ctrl4_int1_pad_ctrl_t route = {0};
    if (lis2dw12_pin_int1_route_set(&lis2dw12_ctx, &route)) {
      PBL_LOG(LOG_LEVEL_ERROR, "LIS2DW12: Failed to disable INT1 routes while turning off sensor");
    }
    return;
  }

  bool routing_configured = true;

  lis2dw12_ctrl4_int1_pad_ctrl_t int1_routes = {0};
  bool use_fifo = s_lis2dw12_state.num_samples > 1;  // batching requested

  // Configure FIFO first, then set up interrupt routing
  if (use_fifo) {
    prv_lis2dw12_configure_fifo(true);
    int1_routes.int1_diff5 = 1;
    int1_routes.int1_fth = 1;
    int1_routes.int1_drdy = 0;
  } else {
    prv_lis2dw12_configure_fifo(false);
    int1_routes.int1_diff5 = 0;
    int1_routes.int1_fth = 0;
    int1_routes.int1_drdy = s_lis2dw12_state.num_samples > 0;  // single-sample mode;
  }

  int1_routes.int1_tap = s_lis2dw12_state.double_tap_detection_enabled;
  int1_routes.int1_wu = s_lis2dw12_state.shake_detection_enabled;  // use wake-up (any-motion)

  // Configure interrupt routing atomically
  if (lis2dw12_pin_int1_route_set(&lis2dw12_ctx, &int1_routes) != 0) {
    PBL_LOG(LOG_LEVEL_ERROR, "LIS2DW12: Failed to configure INT1 routes; re-enabling external interrupt");
    routing_configured = false;
  } else {
    // Allow time for interrupt routing configuration to take effect.
    // The LIS2DW12 needs a brief delay after CTRL4/CTRL7 register writes
    // before the wake-up interrupt logic is fully operational.
    psleep(LIS2DW12_REG_OPS_WAIT_TIME_MS);
    
    // Clear any pending interrupt sources before enabling external interrupt
    lis2dw12_all_sources_t all_sources;
    if (lis2dw12_all_sources_get(&lis2dw12_ctx, &all_sources)) {
      PBL_LOG(LOG_LEVEL_WARNING, "LIS2DW12: Failed to clear pending interrupt sources after routing update");
    }
  }

  // Always re-enable the external interrupt so we do not lose future INT1 edges
  exti_enable(BOARD_CONFIG_ACCEL.accel_ints[0]);

  if (!routing_configured) {
    PBL_LOG(LOG_LEVEL_WARNING, "LIS2DW12: INT1 routing not updated; external interrupt left enabled for recovery");
  }
}

// default odr off
void lis2dw12_init(void) {
  uint8_t id;
  int32_t ret = lis2dw12_device_id_get(&lis2dw12_ctx, &id);
  if (ret || LIS2DW12_ID != id) {
    PBL_LOG(LOG_LEVEL_ERROR, "Failed to get LIS2DW12 chip ID");
    return;
  }
  
  /* Restore default configuration */
  ret = lis2dw12_reset_set(&lis2dw12_ctx, PROPERTY_ENABLE);
  uint8_t rst;
  int reset_timeout = 100; // 100ms max wait for reset
  do {  // Wait for reset to complete with timeout
    psleep(LIS2DW12_REG_OPS_WAIT_TIME_MS);
    if (lis2dw12_reset_get(&lis2dw12_ctx, &rst) != 0) {
      PBL_LOG(LOG_LEVEL_ERROR, "LIS2DW12: Failed to read reset status");
      return;
    }
    reset_timeout--;
  } while (rst && reset_timeout > 0);
  
  if (reset_timeout == 0) {
    PBL_LOG(LOG_LEVEL_ERROR, "LIS2DW12: Reset timeout - sensor may be unresponsive");
    return;
  }

  /* full scale: +/- 2g */
	if (lis2dw12_full_scale_set(&lis2dw12_ctx, LIS2DW12_2g)) {
		PBL_LOG(LOG_LEVEL_ERROR, "Failed to set accelerometer scale");
		return;
	}

  /* low power normal mode (no HP) */
	if (lis2dw12_power_mode_set(&lis2dw12_ctx, LIS2DW12_CONT_LOW_PWR_12bit)) {
		PBL_LOG(LOG_LEVEL_ERROR, "Failed to set power mode");
		return;
	}

  /* tap detection on all axis: X, Y, Z */
	if (lis2dw12_tap_detection_on_x_set(&lis2dw12_ctx, PROPERTY_ENABLE)) {
		PBL_LOG(LOG_LEVEL_ERROR, "Failed to enable tap detection on X axis");
		return;
	}

	if (lis2dw12_tap_detection_on_y_set(&lis2dw12_ctx, PROPERTY_ENABLE)) {
		PBL_LOG(LOG_LEVEL_ERROR, "Failed to enable tap detection on Y axis");
		return;
	}

	if (lis2dw12_tap_detection_on_z_set(&lis2dw12_ctx, PROPERTY_ENABLE)) {
		PBL_LOG(LOG_LEVEL_ERROR, "Failed to enable tap detection on Z axis");
		return;
	}

	/* X,Y,Z threshold: 1 * FS_XL / 2^5 = 1 * 2 / 32 = 62.5 mg */
	if (lis2dw12_tap_threshold_x_set(&lis2dw12_ctx, 1)) {
		PBL_LOG(LOG_LEVEL_ERROR, "Failed to set tap threshold on X axis");
		return;
	}

	if (lis2dw12_tap_threshold_y_set(&lis2dw12_ctx, 1)) {
		PBL_LOG(LOG_LEVEL_ERROR, "Failed to set tap threshold on Y axis");
		return;
	}

	if (lis2dw12_tap_threshold_z_set(&lis2dw12_ctx, 1)) {
		PBL_LOG(LOG_LEVEL_ERROR, "Failed to set tap threshold on Z axis");
		return;
	}

	/* shock time: 2 / ODR_XL = 2 / 26 ~= 77 ms */
	if (lis2dw12_tap_shock_set(&lis2dw12_ctx, 0)) {
		PBL_LOG(LOG_LEVEL_ERROR, "Failed to set tap shock duration");
		return;
	}

	/* quiet time: 2 / ODR_XL = 2 / 26 ~= 77 ms */
	if (lis2dw12_tap_quiet_set(&lis2dw12_ctx, 0)) {
		PBL_LOG(LOG_LEVEL_ERROR, "Failed to set tap quiet duration");
		return;
	}

  /* route single rap to INT1 */
	lis2dw12_ctrl4_int1_pad_ctrl_t route = {.int1_single_tap = 1};
	if (lis2dw12_pin_int1_route_set(&lis2dw12_ctx, &route)) {
		PBL_LOG(LOG_LEVEL_ERROR, "Failed to route interrupt");
		return;
	}

  /* data rate: default off */
	if (lis2dw12_data_rate_set(&lis2dw12_ctx, LIS2DW12_XL_ODR_OFF)) {
		PBL_LOG(LOG_LEVEL_ERROR, "Failed to set accelerometer data rate");
		return;
	}

  exti_configure_pin(BOARD_CONFIG_ACCEL.accel_ints[0], ExtiTrigger_Rising,
    prv_lis2dw12_interrupt_handler);

  if (lis2dw12_data_ready_mode_set(&lis2dw12_ctx, LIS2DW12_DRDY_PULSED)) {
    PBL_LOG(LOG_LEVEL_ERROR, "Failed to set accelerometer data ready mode");
		return;
  }
  if (lis2dw12_int_notification_set(&lis2dw12_ctx, LIS2DW12_INT_PULSED)) {
    PBL_LOG(LOG_LEVEL_ERROR, "Failed to set accelerometer int notification mode");
		return;
  }
}

//! Synchronize the LIS2DW12 state with the desired target state.
static void prv_lis2dw12_chase_target_state(void) {
  bool update_interrupts = false;

  // Check whether we should be spinning up the accelerometer
  bool should_be_running = s_lis2dw12_state_target.sampling_interval_us > 0 ||
                           s_lis2dw12_state_target.num_samples > 0 ||
                           s_lis2dw12_state_target.shake_detection_enabled ||
                           s_lis2dw12_state_target.double_tap_detection_enabled;

  if (!should_be_running || !s_lis2dw12_enabled) {
    if (s_lis2dw12_running) {
      PBL_LOG(LOG_LEVEL_DEBUG, "LIS2DW12: Stopping accelerometer");
      lis2dw12_data_rate_set(&lis2dw12_ctx, LIS2DW12_XL_ODR_OFF);
      s_lis2dw12_running = false;
      s_lis2dw12_state = (lis2dw12_state_t){0};
      prv_lis2dw12_configure_interrupts();
    }
    return;
  } else if (!s_lis2dw12_running) {
    s_lis2dw12_running = true;
    update_interrupts = true;
  }

  // Update number of samples
  if (s_lis2dw12_state_target.num_samples != s_lis2dw12_state.num_samples) {
    s_lis2dw12_state.num_samples = s_lis2dw12_state_target.num_samples;
    update_interrupts = true;
  }

  // Update shake detection
  if (s_lis2dw12_state_target.shake_detection_enabled != s_lis2dw12_state.shake_detection_enabled ||
      s_lis2dw12_state_target.shake_sensitivity_high != s_lis2dw12_state.shake_sensitivity_high) {
    s_lis2dw12_state.shake_detection_enabled = s_lis2dw12_state_target.shake_detection_enabled;
    s_lis2dw12_state.shake_sensitivity_high = s_lis2dw12_state_target.shake_sensitivity_high;
    prv_lis2dw12_configure_shake(s_lis2dw12_state.shake_detection_enabled,
                                s_lis2dw12_state.shake_sensitivity_high);
    update_interrupts = true;
  }

  // Update double tap detection
  if (s_lis2dw12_state_target.double_tap_detection_enabled !=
      s_lis2dw12_state.double_tap_detection_enabled) {
    prv_lis2dw12_configure_double_tap(s_lis2dw12_state_target.double_tap_detection_enabled);
    s_lis2dw12_state.double_tap_detection_enabled =
        s_lis2dw12_state_target.double_tap_detection_enabled;
    update_interrupts = true;
  }

  // Update sampling interval. Ensure ODR is enabled when event-only features are active.
  if (update_interrupts ||
      s_lis2dw12_state_target.sampling_interval_us != s_lis2dw12_state.sampling_interval_us) {
    uint32_t requested_interval = s_lis2dw12_state_target.sampling_interval_us;

    //default ODR to 100Hz
    if(requested_interval == 0) requested_interval = 10 * 1000;
    s_lis2dw12_state.sampling_interval_us = prv_lis2dw12_set_sampling_interval(requested_interval);
  }

  // Update interrupts if necessary
  if (update_interrupts) {
    prv_lis2dw12_configure_interrupts();
  }

  // Note: Do NOT reset target state here as it creates a race condition
  // where new target changes during this function execution could be lost.
  // Instead, only sync the fields that were actually processed.

  PBL_LOG(LOG_LEVEL_DEBUG,
          "LIS2DW12: Reached target state: sampling_interval_us=%lu, num_samples=%lu, "
          "shake_detection_enabled=%d, shake_high_sensitivity=%d, double_tap_detection_enabled=%d",
          s_lis2dw12_state.sampling_interval_us, s_lis2dw12_state.num_samples,
          s_lis2dw12_state.shake_detection_enabled, s_lis2dw12_state.shake_sensitivity_high,
          s_lis2dw12_state.double_tap_detection_enabled);
}

void lis2dw12_power_up(void) {
  s_lis2dw12_enabled = true;
  prv_lis2dw12_chase_target_state();
}

void lis2dw12_power_down(void) {
  PBL_LOG(LOG_LEVEL_DEBUG, "LIS2DW12: Powering down accelerometer");
  s_lis2dw12_enabled = false;
  prv_lis2dw12_chase_target_state();
}

uint32_t accel_set_sampling_interval(uint32_t interval_us) {
  PBL_LOG(LOG_LEVEL_DEBUG, "LIS2DW12: Requesting update of sampling interval to %lu us",
          interval_us);
  s_lis2dw12_state_target.sampling_interval_us = interval_us;
  prv_lis2dw12_chase_target_state();
  return s_lis2dw12_state.sampling_interval_us;
}

uint32_t accel_get_sampling_interval(void) { return s_lis2dw12_state.sampling_interval_us; }
 
void accel_set_num_samples(uint32_t num_samples) {
  PBL_LOG(LOG_LEVEL_DEBUG, "LIS2DW12: Setting number of samples to %lu", num_samples);
  s_lis2dw12_state_target.num_samples = num_samples;
  prv_lis2dw12_chase_target_state();
}

int accel_peek(AccelDriverSample *data) { return prv_lis2dw12_read_sample(data); }

void accel_enable_shake_detection(bool on) {
  PBL_LOG(LOG_LEVEL_DEBUG, "LIS2DW12: %s shake detection.", on ? "Enabling" : "Disabling");
  s_lis2dw12_state_target.shake_detection_enabled = on;
    prv_lis2dw12_chase_target_state();

}

bool accel_get_shake_detection_enabled(void) { return s_lis2dw12_state.shake_detection_enabled; }

void accel_enable_double_tap_detection(bool on) {
  PBL_LOG(LOG_LEVEL_DEBUG, "LIS2DW12: %s double tap detection.", on ? "Enabling" : "Disabling");
  s_lis2dw12_state_target.double_tap_detection_enabled = on;
  prv_lis2dw12_chase_target_state();
}

bool accel_get_double_tap_detection_enabled(void) {
  return s_lis2dw12_state.double_tap_detection_enabled;
}

void accel_set_shake_sensitivity_high(bool sensitivity_high) {
  PBL_LOG(LOG_LEVEL_DEBUG, "LIS2DW12: Setting shake sensitivity to %s.",
          sensitivity_high ? "high" : "normal");
  s_lis2dw12_state_target.shake_sensitivity_high = sensitivity_high;
  prv_lis2dw12_chase_target_state();
}

void accel_set_shake_sensitivity_percent(uint8_t percent) {
  if (percent > 100) {
    percent = 100; // Clamp to max
  }
  
  s_user_sensitivity_percent = percent;
  
  // Reconfigure shake detection if it's currently enabled
  if (s_lis2dw12_state.shake_detection_enabled) {
    prv_lis2dw12_configure_shake(true, s_lis2dw12_state.shake_sensitivity_high);
  }
  
  PBL_LOG(LOG_LEVEL_INFO, "LIS2DW12: User sensitivity set to %u percent", percent);
}

bool accel_run_selftest(void) {
  //TODO: implement selftest function
  return true;
}
