/* SPDX-FileCopyrightText: 2025 Matthew Wardrop */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/accel.h"
#include "drivers/i2c.h"
#include "drivers/exti.h"
#include "drivers/rtc.h"
#include "drivers/vibe.h"
#include "kernel/util/sleep.h"
#include "pbl/services/regular_timer.h"
#include "pbl/services/vibe_pattern.h"
#include "system/logging.h"
#include "util/math.h"
#include "lsm6dso_reg.h"

#include "lsm6dso.h"
#include "lsm6dso_core.h"

PBL_LOG_MODULE_DEFINE(driver_accel_lsm6dso, CONFIG_DRIVER_IMU_LOG_LEVEL);

// Forward declaration of private functions defined below public functions
static void prv_lsm6dso_init(void);
static void prv_lsm6dso_chase_target_state(void);
static void prv_lsm6dso_configure_interrupts(void);
static void prv_lsm6dso_configure_double_tap(bool enable);
static void prv_lsm6dso_configure_shake(bool enable, bool sensitivity_high);
static void prv_lsm6dso_interrupt_handler(bool *should_context_switch);
static void prv_lsm6dso_process_interrupts(void);
static void prv_lsm6dso_interrupt_watchdog_callback(void *data);
static bool prv_lsm6dso_force_reinit(void);
static bool prv_is_vibing(void);
typedef struct {
  lsm6dso_odr_xl_t odr;
  lsm6dso_xl_hm_mode_t power_mode;
  uint32_t interval_us;
} odr_xl_interval_t;
static odr_xl_interval_t prv_get_odr_for_interval(uint32_t interval_us);
static int32_t prv_lsm6dso_set_sampling_interval(uint32_t interval_us);
static void prv_lsm6dso_read_samples(void);
static uint8_t prv_lsm6dso_read_sample(AccelDriverSample *data);
static void prv_note_new_sample(const AccelDriverSample *sample);
static void prv_note_new_sample_mg(int16_t x_mg, int16_t y_mg, int16_t z_mg);
static uint32_t prv_compute_age_ms(uint64_t now_ms, uint64_t then_ms);
typedef enum {
  X_AXIS = 0,
  Y_AXIS = 1,
  Z_AXIS = 2,
} axis_t;
static int16_t prv_get_axis_projection_mg(axis_t axis, int16_t *raw_vector);
static uint64_t prv_get_timestamp_ms(void);

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
static uint32_t s_tap_threshold = BOARD_CONFIG_ACCEL.accel_config.double_tap_threshold / 1250;
static uint32_t s_last_vibe_detected = 0;

// User-configured sensitivity percentage (0-100), where 100 = most sensitive
// Default to 100% (maximum sensitivity) to maintain current behavior
static uint8_t s_user_sensitivity_percent = 100;

// Error tracking and recovery (I2C health lives in the shared core)
static int16_t s_last_sample_mg[3] = {0};
static uint64_t s_last_sample_timestamp_ms = 0;
// Interrupt activity instrumentation so we can spot when the sensor stops firing INT1.
static uint64_t s_last_interrupt_ms = 0;
static uint64_t s_last_wake_event_ms = 0;
static uint64_t s_last_double_tap_ms = 0;
static uint32_t s_interrupt_count = 0;
static uint32_t s_wake_event_count = 0;
static uint32_t s_double_tap_event_count = 0;
static uint32_t s_watchdog_recovery_attempts = 0;

// Interrupt watchdog timer
static RegularTimerInfo s_interrupt_watchdog_timer = {
  .cb = prv_lsm6dso_interrupt_watchdog_callback,
  .cb_data = NULL
};

// watch rotation
static bool s_rotated_180 = false;

// Maximum allowed sampling interval for tap detection (i.e., slowest rate, in microseconds)
#define LSM6DSO_TAP_DETECTION_MAX_INTERVAL_US 2398

// Delay after detecting a vibe before shake/tap interrupts should be processed again
#define LSM6DSO_VIBE_COOLDOWN_MS 50

// Error recovery thresholds and watchdog timeouts
#define LSM6DSO_MAX_CONSECUTIVE_FAILURES 3
#define LSM6DSO_INTERRUPT_GAP_LOG_THRESHOLD_MS 3000
#define LSM6DSO_INTERRUPT_WATCHDOG_MS 10000 //run watchdog every 10 seconds
#define LSM6DSO_INTERRUPT_WATCHDOG_TIMEOUT_MS 5000  // but count as failure if no interrupt in 5 seconds
#define LSM6DSO_INTERRUPT_WATCHDOG_MS_NO_SAMPLES 600000 //if no samples are requested, every 10 minutes is fine

// LSM6DSO configuration entrypoints

void accel_init(void) {
  // Initialize the LSM6DSO sensor to a powered down state.
  prv_lsm6dso_init();
}

void accel_power_up(void) {
  s_lsm6dso_enabled = true;
  prv_lsm6dso_chase_target_state();
}

void accel_power_down(void) {
  PBL_LOG_DBG("LSM6DSO: Powering down accelerometer");
  s_lsm6dso_enabled = false;
  prv_lsm6dso_chase_target_state();
}

// accel.h implementation

uint32_t accel_set_sampling_interval(uint32_t interval_us) {
  PBL_LOG_DBG("LSM6DSO: Requesting update of sampling interval to %lu us",
          interval_us);
  s_lsm6dso_state_target.sampling_interval_us = interval_us;
  prv_lsm6dso_chase_target_state();
  return s_lsm6dso_state.sampling_interval_us;
}

uint32_t accel_get_sampling_interval(void) { return s_lsm6dso_state.sampling_interval_us; }

void accel_set_num_samples(uint32_t num_samples) {
  PBL_LOG_DBG("LSM6DSO: Setting number of samples to %lu", num_samples);
  s_lsm6dso_state_target.num_samples = num_samples;
  prv_lsm6dso_chase_target_state();
}

int accel_peek(AccelDriverSample *data) { return prv_lsm6dso_read_sample(data); }

void accel_enable_shake_detection(bool on) {
  PBL_LOG_DBG("LSM6DSO: %s shake detection.", on ? "Enabling" : "Disabling");
  s_lsm6dso_state_target.shake_detection_enabled = on;
  prv_lsm6dso_chase_target_state();
}

bool accel_get_shake_detection_enabled(void) { return s_lsm6dso_state.shake_detection_enabled; }

void accel_enable_double_tap_detection(bool on) {
  PBL_LOG_DBG("LSM6DSO: %s double tap detection.", on ? "Enabling" : "Disabling");
  s_lsm6dso_state_target.double_tap_detection_enabled = on;
  prv_lsm6dso_chase_target_state();
}

bool accel_get_double_tap_detection_enabled(void) {
  return s_lsm6dso_state.double_tap_detection_enabled;
}

void accel_set_shake_sensitivity_high(bool sensitivity_high) {
  PBL_LOG_DBG("LSM6DSO: Setting shake sensitivity to %s.",
          sensitivity_high ? "high" : "normal");
  s_lsm6dso_state_target.shake_sensitivity_high = sensitivity_high;
  prv_lsm6dso_chase_target_state();
}

void accel_set_shake_sensitivity_percent(uint8_t percent) {
  if (percent > 100) {
    percent = 100; // Clamp to max
  }
  
  s_user_sensitivity_percent = percent;
  
  // Reconfigure shake detection if it's currently enabled
  if (s_lsm6dso_state.shake_detection_enabled) {
    prv_lsm6dso_configure_shake(true, s_lsm6dso_state.shake_sensitivity_high);
  }
  
  PBL_LOG_INFO("LSM6DSO: User sensitivity set to %u percent", percent);
}

// Initialization

//! Initialize the LSM6DSO sensor and configure it to a powered down state.
//! This function should be called once at system startup to prepare the sensor.
static void prv_lsm6dso_init(void) {
  if (s_lsm6dso_initialized) {
    return;
  }

  // Chip-level initialization is shared with the gyro half
  if (!lsm6dso_core_init()) {
    return;
  }

  // Configure interrupts
  // Note that we only configure on interrupt pin for now, since not all devices
  // have enough channels for two (and it is not in any case neccessary).

  exti_configure_pin(BOARD_CONFIG_ACCEL.accel_ints[0], ExtiTrigger_Rising,
                     prv_lsm6dso_interrupt_handler);

  s_lsm6dso_initialized = true;
  PBL_LOG_DBG("LSM6DSO: Initialization complete");
}

//! Synchronize the LSM6DSO state with the desired target state.
static void prv_lsm6dso_chase_target_state(void) {
  if (!s_lsm6dso_initialized) {
    PBL_LOG_ERR("LSM6DSO: Cannot chase target state before initialization");
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
      PBL_LOG_DBG("LSM6DSO: Stopping accelerometer");
      lsm6dso_xl_data_rate_set(&lsm6dso_ctx, LSM6DSO_XL_ODR_OFF);
      s_lsm6dso_running = false;
      s_lsm6dso_state = (lsm6dso_state_t){0};
      prv_lsm6dso_configure_interrupts();
      // Stop the interrupt watchdog when sensor is stopped
      regular_timer_remove_callback(&s_interrupt_watchdog_timer);
    }
    return;
  } else if (!s_lsm6dso_running) {
    s_lsm6dso_running = true;
    update_interrupts = true;
    // Start the interrupt watchdog when sensor starts running
    regular_timer_add_multisecond_callback(&s_interrupt_watchdog_timer, 
                                  LSM6DSO_INTERRUPT_WATCHDOG_MS / 1000);
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

  // Update sampling interval. Ensure ODR is enabled when event-only features are active.
  if (update_interrupts ||
      s_lsm6dso_state_target.sampling_interval_us != s_lsm6dso_state.sampling_interval_us) {
    uint32_t requested_interval = s_lsm6dso_state_target.sampling_interval_us;

    // If double-tap is enabled, we must run fast enough regardless of data subscribers.
    if (s_lsm6dso_state_target.double_tap_detection_enabled) {
      if (requested_interval == 0) {
        requested_interval = LSM6DSO_TAP_DETECTION_MAX_INTERVAL_US; // ~417 Hz ceiling
      } else {
        requested_interval = MIN(requested_interval, LSM6DSO_TAP_DETECTION_MAX_INTERVAL_US);
      }
    }

    // If shake detection is enabled (any-motion wake), make sure ODR is not OFF.
    // Choose a conservative, low-power ODR suitable for motion detection when nothing else requests data.
    if (s_lsm6dso_state_target.shake_detection_enabled && requested_interval == 0) {
      // 52 Hz is a good compromise for responsiveness vs. power on this part.
      requested_interval = 19231; // ~52 Hz
    }

    s_lsm6dso_state.sampling_interval_us = prv_lsm6dso_set_sampling_interval(requested_interval);
  }

  // Update interrupts if necessary
  if (update_interrupts) {
    prv_lsm6dso_configure_interrupts();
  }

  // Note: Do NOT reset target state here as it creates a race condition
  // where new target changes during this function execution could be lost.
  // Instead, only sync the fields that were actually processed.

  PBL_LOG_DBG("LSM6DSO: Reached target state: sampling_interval_us=%lu, num_samples=%lu, "
          "shake_detection_enabled=%d, shake_high_sensitivity=%d, double_tap_detection_enabled=%d",
          s_lsm6dso_state.sampling_interval_us, s_lsm6dso_state.num_samples,
          s_lsm6dso_state.shake_detection_enabled, s_lsm6dso_state.shake_sensitivity_high,
          s_lsm6dso_state.double_tap_detection_enabled);
}

static void prv_lsm6dso_configure_interrupts(void) {
  // Disable interrupts during configuration to prevent race conditions
  // and ensure atomic configuration updates

  bool should_enable_interrupts = s_lsm6dso_enabled &&
      (s_lsm6dso_state.num_samples || s_lsm6dso_state.shake_detection_enabled ||
       s_lsm6dso_state.double_tap_detection_enabled);

  // Always disable interrupts first to ensure clean state
  exti_disable(BOARD_CONFIG_ACCEL.accel_ints[0]);

  if (!should_enable_interrupts) {
    // Also disable all interrupt sources in the sensor to prevent phantom interrupts
    lsm6dso_pin_int1_route_t int1_routes = {0}; // All disabled
    if (lsm6dso_pin_int1_route_set(&lsm6dso_ctx, int1_routes)) {
      PBL_LOG_ERR("LSM6DSO: Failed to disable INT1 routes while turning off sensor");
    }
    return;
  }

  bool routing_configured = true;

  lsm6dso_pin_int1_route_t int1_routes = {0};
  bool use_fifo = s_lsm6dso_state.num_samples > 1;  // batching requested

  // Configure FIFO first, then set up interrupt routing
  if (use_fifo) {
    lsm6dso_core_fifo_request_xl(s_lsm6dso_state.num_samples,
                                 s_lsm6dso_state.sampling_interval_us);
    int1_routes.fifo_th = 1;  // watermark interrupt
    int1_routes.fifo_ovr = 1; // Enable overflow interrupt to prevent lockup
    int1_routes.drdy_xl = 0;
  } else {
    lsm6dso_core_fifo_request_xl(0, 0);
    int1_routes.drdy_xl = s_lsm6dso_state.num_samples > 0;  // single-sample mode
    int1_routes.fifo_th = 0;
    int1_routes.fifo_ovr = 0;
  }

  int1_routes.double_tap = s_lsm6dso_state.double_tap_detection_enabled;
  int1_routes.wake_up = s_lsm6dso_state.shake_detection_enabled;  // use wake-up (any-motion)

  // Configure interrupt routing atomically
  if (lsm6dso_pin_int1_route_set(&lsm6dso_ctx, int1_routes)) {
    PBL_LOG_ERR("LSM6DSO: Failed to configure INT1 routes; re-enabling external interrupt");
    routing_configured = false;
  } else {
    // Clear any pending interrupt sources before enabling external interrupt
    lsm6dso_all_sources_t all_sources;
    if (lsm6dso_all_sources_get(&lsm6dso_ctx, &all_sources)) {
      PBL_LOG_WRN("LSM6DSO: Failed to clear pending interrupt sources after routing update");
    }
  }

  // Always re-enable the external interrupt so we do not lose future INT1 edges
  exti_enable(BOARD_CONFIG_ACCEL.accel_ints[0]);

  if (!routing_configured) {
    PBL_LOG_WRN("LSM6DSO: INT1 routing not updated; external interrupt left enabled for recovery");
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

  // Threshold calculation:
  // - Board config provides Low (15) and High (64) thresholds
  // - sensitivity_high flag indicates stationary mode (use low threshold for any movement)
  // - s_user_sensitivity_percent (0-100) controls normal mode threshold
  //   * 100% = most sensitive = use Low threshold (15)
  //   * 50% = medium = interpolate between Low and High (~40)
  //   * 0% = least sensitive = use High threshold (64)
  
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
  if (raw > 63) raw = 63;  // lsm6dso wk_ths is 6 bits
  if (raw < 2) raw = 2;     // Avoid noise storms with very low thresholds
  
  lsm6dso_wkup_threshold_set(&lsm6dso_ctx, (uint8_t)raw);
  
  PBL_LOG_DBG("LSM6DSO: Shake threshold set to %lu (sensitivity_high=%d, user_percent=%u)", 
          raw, sensitivity_high, s_user_sensitivity_percent);
}

static void prv_lsm6dso_interrupt_handler(bool *should_context_switch) {
  // Offload processing to a worker. The LSM6DSO can miss events if interrupts
  // are ignored due to pending flags, so it is important to process them
  // quickly. The actual clearing of the interrupt flags will happen in the
  // worker via an I2C transaction.
  accel_offload_work_from_isr(prv_lsm6dso_process_interrupts, should_context_switch);
}

static void prv_lsm6dso_process_interrupts(void) {
  const uint64_t now_ms = prv_get_timestamp_ms();
  const uint64_t previous_interrupt_ms = s_last_interrupt_ms;
  s_last_interrupt_ms = now_ms;
  s_interrupt_count++;
  s_watchdog_recovery_attempts = 0;

  uint32_t gap_ms = 0;
  if (previous_interrupt_ms == 0) {
    PBL_LOG_INFO("LSM6DSO: First INT1 service (count=%lu)",
            (unsigned long)s_interrupt_count);
  } else {
    uint64_t raw_gap_ms = now_ms - previous_interrupt_ms;
    gap_ms = (raw_gap_ms > UINT32_MAX) ? UINT32_MAX : (uint32_t)raw_gap_ms;
    if (gap_ms >= LSM6DSO_INTERRUPT_GAP_LOG_THRESHOLD_MS) {
      PBL_LOG_INFO("LSM6DSO: INT1 gap %lu ms (count=%lu wake=%lu tap=%lu)",
              (unsigned long)gap_ms, (unsigned long)s_interrupt_count,
              (unsigned long)s_wake_event_count, (unsigned long)s_double_tap_event_count);
    }
  }

  // Read and clear interrupt sources atomically to prevent loss
  lsm6dso_all_sources_t all_sources;
  
  // Multiple attempts to read interrupt sources in case of transient I2C issues
  int read_attempts = 0;
  const int max_read_attempts = 2;
  
  do {
    if (lsm6dso_all_sources_get(&lsm6dso_ctx, &all_sources) == 0) {
      break; // Success
    }
    read_attempts++;
    if (read_attempts < max_read_attempts) {
      // Brief delay and retry - this prevents losing interrupts due to transient I2C glitches
      psleep(1);
    }
  } while (read_attempts < max_read_attempts);
  
  if (read_attempts >= max_read_attempts) {
    PBL_LOG_ERR("LSM6DSO: Failed to read interrupt sources after retries");
    Lsm6dsoCoreHealth *health = lsm6dso_core_health();
    health->consecutive_errors++;
    if (health->consecutive_errors >= LSM6DSO_MAX_CONSECUTIVE_FAILURES) {
      health->sensor_health_ok = false;
      PBL_LOG_WRN("LSM6DSO: Interrupt processing failed, sensor health degraded");
    }
    return;
  }

  // Reset failure count on successful read
  lsm6dso_core_health()->consecutive_errors = 0;

  // Prevent FIFO overflow by proper watermark management
  // FIFO overflow causes the sensor to stop generating interrupts
  if (all_sources.fifo_ovr || all_sources.fifo_full) {
    lsm6dso_core_fifo_recover();

    // Force re-enable of external interrupt to ensure it's active
    exti_disable(BOARD_CONFIG_ACCEL.accel_ints[0]);
    psleep(1);
    exti_enable(BOARD_CONFIG_ACCEL.accel_ints[0]);
  }

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
    s_double_tap_event_count++;
    s_last_double_tap_ms = now_ms;
    PBL_LOG_DBG("LSM6DSO: Double tap interrupt triggered");
    // Handle double tap detection
    axis_t axis;
    if (all_sources.tap_x) {
      axis = X_AXIS;
    } else if (all_sources.tap_y) {
      axis = Y_AXIS;
    } else if (all_sources.tap_z) {
      axis = Z_AXIS;
    } else {
      PBL_LOG_DBG("LSM6DSO: No tap axis detected");
      return;  // No valid tap detected
    }

    uint8_t axis_offset = BOARD_CONFIG_ACCEL.accel_config.axes_offsets[axis];
    uint8_t axis_direction = (BOARD_CONFIG_ACCEL.accel_config.axes_inverts[axis] ? -1 : 1) *
                             (all_sources.tap_sign ? -1 : 1);

    PBL_LOG_DBG("LSM6DSO: Double tap interrupt triggered; axis=%d, direction=%d",
            axis_offset, axis_direction);
    accel_cb_double_tap_detected(axis_offset, axis_direction);
  }

  // Wake-up (any-motion) event -> treat as shake. Axis & direction derived from wake_up_src.
  if (s_lsm6dso_state.shake_detection_enabled && all_sources.wake_up) {
    s_wake_event_count++;
    s_last_wake_event_ms = now_ms;
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
        int16_t mg_x = prv_get_axis_projection_mg(X_AXIS, accel_raw);
        int16_t mg_y = prv_get_axis_projection_mg(Y_AXIS, accel_raw);
        int16_t mg_z = prv_get_axis_projection_mg(Z_AXIS, accel_raw);
        prv_note_new_sample_mg(mg_x, mg_y, mg_z);
      }
      PBL_LOG_DBG("LSM6DSO: Shake detected; axis=%d, direction=%lu", axis, direction);
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

static bool prv_lsm6dso_force_reinit(void) {
  PBL_LOG_WRN("LSM6DSO: Performing forced sensor reinitialization");

  // Stop the watchdog timer before clearing state to prevent double-registration
  regular_timer_remove_callback(&s_interrupt_watchdog_timer);

  // Prevent spurious edges while the device is reconfigured
  exti_disable(BOARD_CONFIG_ACCEL.accel_ints[0]);

  s_lsm6dso_initialized = false;
  s_lsm6dso_running = false;

  if (!lsm6dso_core_reinit()) {
    PBL_LOG_ERR("LSM6DSO: Forced reinit failed; sensor still unresponsive");
    return false;
  }

  prv_lsm6dso_init();
  if (!s_lsm6dso_initialized) {
    PBL_LOG_ERR("LSM6DSO: Forced reinit failed; sensor still unresponsive");
    return false;
  }

  s_lsm6dso_state = (lsm6dso_state_t){0};

  prv_lsm6dso_chase_target_state();

#if defined(CONFIG_GYRO_LSM6DSO)
  // The chip reset wiped the gyro half's configuration too
  lsm6dso_gyro_handle_core_reinit();
#endif

  return s_lsm6dso_running;
}

static void prv_lsm6dso_interrupt_watchdog_callback(void *data) {
  PBL_LOG_DBG("LSM6DSO: Watchdog callback running");
  
  // Check if interrupts have stopped for too long
  const uint64_t now_ms = prv_get_timestamp_ms();
  const uint32_t interrupt_age_ms = prv_compute_age_ms(now_ms, s_last_interrupt_ms);
  
  PBL_LOG_DBG("LSM6DSO: Interrupt age: %" PRIu32 " ms", interrupt_age_ms);

  if ((interrupt_age_ms >= LSM6DSO_INTERRUPT_WATCHDOG_TIMEOUT_MS && s_lsm6dso_state.num_samples > 0) ||
      (interrupt_age_ms >= LSM6DSO_INTERRUPT_WATCHDOG_MS_NO_SAMPLES && s_lsm6dso_state.num_samples == 0)) {
    PBL_LOG_WRN("LSM6DSO: Interrupt watchdog triggered - no interrupts for %" PRIu32 " ms, count=%lu; forcing reinit",
            interrupt_age_ms, (unsigned long)s_interrupt_count);
    // Mark sensor as unhealthy
    lsm6dso_core_health()->sensor_health_ok = false;

    if (!s_lsm6dso_running) {
      return;
    }

    // Always escalate to forced reinitialization on watchdog expiry
    if (prv_lsm6dso_force_reinit()) {
      lsm6dso_core_health()->sensor_health_ok = true;
      // Reset interrupt timestamp and count to avoid repeated watchdog triggers
      s_last_interrupt_ms = now_ms;
      s_interrupt_count = 0;
      if (s_lsm6dso_running) {
        prv_lsm6dso_configure_interrupts();
      }
    } else {
      PBL_LOG_ERR("LSM6DSO: Forced sensor reinitialization failed");
    }
  }
}

// Sampling interval configuration

static odr_xl_interval_t prv_get_odr_for_interval(uint32_t interval_us) {
  if (interval_us >= 625000) return (odr_xl_interval_t){LSM6DSO_XL_ODR_1Hz6, LSM6DSO_ULTRA_LOW_POWER_MD, 625000};
  if (interval_us >= 80000) return (odr_xl_interval_t){LSM6DSO_XL_ODR_12Hz5, LSM6DSO_ULTRA_LOW_POWER_MD, 80000};
  if (interval_us >= 38462) return (odr_xl_interval_t){LSM6DSO_XL_ODR_26Hz, LSM6DSO_ULTRA_LOW_POWER_MD, 38462};
  if (interval_us >= 19231) return (odr_xl_interval_t){LSM6DSO_XL_ODR_52Hz, LSM6DSO_ULTRA_LOW_POWER_MD, 19231};
  if (interval_us >= 9615) return (odr_xl_interval_t){LSM6DSO_XL_ODR_104Hz, LSM6DSO_LOW_NORMAL_POWER_MD, 9615};
  if (interval_us >= 4808) return (odr_xl_interval_t){LSM6DSO_XL_ODR_208Hz, LSM6DSO_LOW_NORMAL_POWER_MD, 4808};
  if (interval_us >= 2398) return (odr_xl_interval_t){LSM6DSO_XL_ODR_417Hz, LSM6DSO_HIGH_PERFORMANCE_MD, 2398};
  if (interval_us >= 1200) return (odr_xl_interval_t){LSM6DSO_XL_ODR_833Hz, LSM6DSO_HIGH_PERFORMANCE_MD, 1200};
  if (interval_us >= 600) return (odr_xl_interval_t){LSM6DSO_XL_ODR_1667Hz, LSM6DSO_HIGH_PERFORMANCE_MD, 600};
  if (interval_us >= 300) return (odr_xl_interval_t){LSM6DSO_XL_ODR_3333Hz, LSM6DSO_HIGH_PERFORMANCE_MD, 300};
  return (odr_xl_interval_t){LSM6DSO_XL_ODR_6667Hz, LSM6DSO_HIGH_PERFORMANCE_MD, 150};
}

static int32_t prv_lsm6dso_set_sampling_interval(uint32_t interval_us) {
  if (!s_lsm6dso_initialized) {
    PBL_LOG_ERR("LSM6DSO: Not initialized, cannot set sampling interval");
    return -1;
  }

  if (s_lsm6dso_state.double_tap_detection_enabled) {
    interval_us = MIN(interval_us, LSM6DSO_TAP_DETECTION_MAX_INTERVAL_US);
  }

  // Ensure sufficient ODR for wake-up (shake) detection even without data subscribers.
  // Use ~52Hz as a practical minimum for responsive any-motion events.
  if (s_lsm6dso_state.shake_detection_enabled) {
    interval_us = MIN(interval_us, 19231); // ~52 Hz
  }

  odr_xl_interval_t odr_interval = prv_get_odr_for_interval(interval_us);

  lsm6dso_odr_xl_t old_odr;
  if (lsm6dso_xl_data_rate_get(&lsm6dso_ctx, &old_odr) != 0) {
    PBL_LOG_ERR("LSM6DSO: failed to read old ODR");
    return -1;
  }

  lsm6dso_xl_hm_mode_t old_power_mode;
  if (lsm6dso_xl_power_mode_get(&lsm6dso_ctx, &old_power_mode) != 0) {
    PBL_LOG_ERR("LSM6DSO: failed to read old power mode");
    return -1;
  }

  lsm6dso_xl_hm_mode_t new_power_mode = odr_interval.power_mode;
#if defined(CONFIG_GYRO_LSM6DSO)
  // The accelerometer must not use ultra-low-power mode while the gyro is
  // active (LSM6DSO datasheet section 6.2.1)
  if (lsm6dso_gyro_is_active() && new_power_mode == LSM6DSO_ULTRA_LOW_POWER_MD) {
    new_power_mode = LSM6DSO_LOW_NORMAL_POWER_MD;
  }
#endif

  if (old_odr == odr_interval.odr && old_power_mode == new_power_mode) {
    PBL_LOG_DBG("LSM6DSO: we were already in that sampling mode, so we're good");
    return odr_interval.interval_us;
  }

  if (old_power_mode != new_power_mode) {
    // Section 6.2.1: you have to power down the accel before switching ULP
    // mode
    if (lsm6dso_xl_data_rate_set(&lsm6dso_ctx, LSM6DSO_XL_ODR_OFF) != 0) {
      PBL_LOG_ERR("LSM6DSO: failed to power off before changing power mode");
      return -1;
    }

    if (lsm6dso_xl_power_mode_set(&lsm6dso_ctx, new_power_mode) != 0) {
      PBL_LOG_ERR("LSM6DSO: failed to set power mode");
      return -1;
    }

    PBL_LOG_DBG("LSM6DSO: switched to accelerometer power mode lsm6dso_xl_hm_mode_t = %d", new_power_mode);
  }

  if (lsm6dso_xl_data_rate_set(&lsm6dso_ctx, odr_interval.odr) != 0) {
    PBL_LOG_ERR("LSM6DSO: Failed to set ODR");
    return -1;
  }

  // Wait for ODR change to take effect (LSM6DSO needs time to stabilize)
  if (odr_interval.odr != LSM6DSO_XL_ODR_OFF) {
    psleep(10); // Allow time for ODR change to stabilize
  }

  PBL_LOG_DBG("LSM6DSO: Set sampling interval to %lu us (requested %lu us)",
          odr_interval.interval_us, interval_us);
  return odr_interval.interval_us;
}

// Accelerometer sample reading (and reporting)

static void prv_lsm6dso_read_samples(void) {
  if (s_lsm6dso_state.num_samples <= 1 || !lsm6dso_core_fifo_in_use()) {
    // Single sample path
    AccelDriverSample sample;
    prv_lsm6dso_read_sample(&sample);
    return;
  }

  // Drain the shared FIFO; accel records come back through
  // lsm6dso_accel_handle_fifo_record() below
  lsm6dso_core_fifo_drain();
}

void lsm6dso_accel_handle_fifo_record(const uint8_t data[6], uint64_t timestamp_us) {
  int16_t raw_vector[3];
  raw_vector[0] = (int16_t)((data[1] << 8) | data[0]);
  raw_vector[1] = (int16_t)((data[3] << 8) | data[2]);
  raw_vector[2] = (int16_t)((data[5] << 8) | data[4]);

  AccelDriverSample sample = {0};
  sample.x = prv_get_axis_projection_mg(X_AXIS, raw_vector);
  sample.y = prv_get_axis_projection_mg(Y_AXIS, raw_vector);
  sample.z = prv_get_axis_projection_mg(Z_AXIS, raw_vector);
  sample.timestamp_us = timestamp_us;
  accel_cb_new_sample(&sample);
  prv_note_new_sample(&sample);
}

void lsm6dso_accel_gyro_state_changed(void) {
  // Re-apply the sampling interval so the power mode is re-evaluated against
  // the gyro's new state (ULP mode is unavailable while the gyro is active)
  if (s_lsm6dso_running && s_lsm6dso_state.sampling_interval_us > 0) {
    prv_lsm6dso_set_sampling_interval(s_lsm6dso_state.sampling_interval_us);
  }
}

static uint8_t prv_lsm6dso_read_sample(AccelDriverSample *data) {
  if (!s_lsm6dso_initialized) {
    PBL_LOG_ERR("LSM6DSO: Not initialized, cannot read sample");
    return -1;
  }

  // TODO: Handle case when accelerometer is not enabled or running (by briefly
  // enabling it.

  int16_t accel_raw[3];
  if (lsm6dso_acceleration_raw_get(&lsm6dso_ctx, accel_raw) != 0) {
    PBL_LOG_ERR("LSM6DSO: Failed to read accelerometer data");
    return -1;
  }

  data->x = s_rotated_180 ? prv_get_axis_projection_mg(X_AXIS, accel_raw) * -1
                          : prv_get_axis_projection_mg(X_AXIS, accel_raw);
  data->y = s_rotated_180 ? prv_get_axis_projection_mg(Y_AXIS, accel_raw) * -1
                          : prv_get_axis_projection_mg(Y_AXIS, accel_raw);
  data->z = prv_get_axis_projection_mg(Z_AXIS, accel_raw);
  data->timestamp_us = prv_get_timestamp_ms() * 1000;

  prv_note_new_sample(data);

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

static uint32_t prv_compute_age_ms(uint64_t now_ms, uint64_t then_ms) {
  if (then_ms == 0) {
    return UINT32_MAX;
  }

  if (now_ms <= then_ms) {
    return 0;
  }

  uint64_t delta = now_ms - then_ms;
  if (delta > UINT32_MAX) {
    return UINT32_MAX;
  }

  return (uint32_t)delta;
}


void lsm6dso_get_diagnostics(Lsm6dsoDiagnostics *diagnostics) {
  if (!diagnostics) {
    return;
  }

  *diagnostics = (Lsm6dsoDiagnostics){0};

  diagnostics->last_sample_mg[0] = s_last_sample_mg[0];
  diagnostics->last_sample_mg[1] = s_last_sample_mg[1];
  diagnostics->last_sample_mg[2] = s_last_sample_mg[2];

  const Lsm6dsoCoreHealth *health = lsm6dso_core_health();
  const uint64_t now_ms = prv_get_timestamp_ms();
  diagnostics->last_sample_age_ms = prv_compute_age_ms(now_ms, s_last_sample_timestamp_ms);
  diagnostics->last_successful_read_age_ms =
      prv_compute_age_ms(now_ms, (uint64_t)health->last_successful_read_ms);
  diagnostics->last_interrupt_age_ms = prv_compute_age_ms(now_ms, s_last_interrupt_ms);
  diagnostics->last_wake_event_age_ms = prv_compute_age_ms(now_ms, s_last_wake_event_ms);
  diagnostics->last_double_tap_age_ms = prv_compute_age_ms(now_ms, s_last_double_tap_ms);

  diagnostics->i2c_error_count = health->i2c_error_count;
  diagnostics->consecutive_error_count = health->consecutive_errors;
  diagnostics->interrupt_count = s_interrupt_count;
  diagnostics->wake_event_count = s_wake_event_count;
  diagnostics->double_tap_event_count = s_double_tap_event_count;

  uint32_t flags = 0;
  if (s_lsm6dso_initialized) {
    flags |= LSM6DSO_STATE_FLAG_INITIALIZED;
  }
  if (s_lsm6dso_enabled) {
    flags |= LSM6DSO_STATE_FLAG_ENABLED;
  }
  if (s_lsm6dso_running) {
    flags |= LSM6DSO_STATE_FLAG_RUNNING;
  }
  if (health->sensor_health_ok) {
    flags |= LSM6DSO_STATE_FLAG_HEALTH_OK;
  }
  if (s_last_sample_timestamp_ms != 0) {
    flags |= LSM6DSO_STATE_FLAG_SAMPLE_VALID;
  }

  diagnostics->state_flags = flags;
}

void accel_set_rotated(bool rotated) {
  s_rotated_180 = rotated;
}