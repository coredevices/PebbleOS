/* SPDX-FileCopyrightText: 2026 Dave Bortz */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/gyro.h"

#include "pbl/services/new_timer/new_timer.h"
#include "system/logging.h"
#include "util/math.h"
#include "lsm6dso_reg.h"

#include "lsm6dso.h"
#include "lsm6dso_core.h"

PBL_LOG_MODULE_DEFINE(driver_gyro_lsm6dso, CONFIG_DRIVER_IMU_LOG_LEVEL);

/*
 * Gyroscope half of the LSM6DSO driver (drivers/gyro.h implementation).
 *
 * Sample collection is FIFO-batched and timer-polled: not all boards route
 * the LSM6DSO interrupt pins (on obelix only the LIS2DW12 interrupt is
 * wired), so a periodic timer drains the FIFO at the requested batching
 * cadence. On boards where the accel half is also compiled in and
 * interrupt-driven (asterix), gyro records are additionally delivered
 * whenever the accel's FIFO watermark interrupt drains the shared FIFO.
 *
 * Power: the gyroscope is left powered down (LSM6DSO_GY_ODR_OFF, ~3 uA for
 * the whole chip if the accel is also off) unless samples have been
 * requested. The gyroscope needs ~70 ms plus 2-3 samples to produce valid
 * data after power-on (AN5192 section 3.11); samples taken during that
 * window are discarded here in the driver.
 */

// Settling time after gyro power-on before samples are valid
#define LSM6DSO_GYRO_TURN_ON_MS 70
#define LSM6DSO_GYRO_DISCARD_SAMPLES 3

// Minimum poll period; bounds timer load when a client requests per-sample
// delivery, which cannot be honored faster than this without an interrupt.
#define LSM6DSO_GYRO_MIN_POLL_MS 10

// Default sampling interval if samples are requested without a rate (~104 Hz)
#define LSM6DSO_GYRO_DEFAULT_INTERVAL_US 9615

static void prv_gyro_chase_target_state(void);

typedef struct {
  uint32_t sampling_interval_us;
  uint32_t num_samples;
} lsm6dso_gyro_state_t;

static lsm6dso_gyro_state_t s_gyro_state;
static lsm6dso_gyro_state_t s_gyro_state_target;
static bool s_gyro_enabled = true;
static bool s_gyro_running = false;

static TimerID s_gyro_poll_timer = TIMER_INVALID_ID;
static uint32_t s_gyro_poll_period_ms = 0;

// Samples with timestamps earlier than this are still settling and dropped
static uint64_t s_gyro_valid_after_ms = 0;

static GyroDriverSample s_gyro_last_sample;

typedef struct {
  lsm6dso_odr_g_t odr;
  lsm6dso_g_hm_mode_t power_mode;
  uint32_t interval_us;
} odr_g_interval_t;

static odr_g_interval_t prv_get_gyro_odr_for_interval(uint32_t interval_us) {
  // The gyroscope's slowest ODR is 12.5 Hz. High-performance mode is only
  // required above 208 Hz; below that the chip selects low-power or normal
  // mode automatically when high-performance is disabled.
  if (interval_us >= 80000) return (odr_g_interval_t){LSM6DSO_GY_ODR_12Hz5, LSM6DSO_GY_NORMAL, 80000};
  if (interval_us >= 38462) return (odr_g_interval_t){LSM6DSO_GY_ODR_26Hz, LSM6DSO_GY_NORMAL, 38462};
  if (interval_us >= 19231) return (odr_g_interval_t){LSM6DSO_GY_ODR_52Hz, LSM6DSO_GY_NORMAL, 19231};
  if (interval_us >= 9615) return (odr_g_interval_t){LSM6DSO_GY_ODR_104Hz, LSM6DSO_GY_NORMAL, 9615};
  if (interval_us >= 4808) return (odr_g_interval_t){LSM6DSO_GY_ODR_208Hz, LSM6DSO_GY_NORMAL, 4808};
  if (interval_us >= 2398) return (odr_g_interval_t){LSM6DSO_GY_ODR_417Hz, LSM6DSO_GY_HIGH_PERFORMANCE, 2398};
  if (interval_us >= 1200) return (odr_g_interval_t){LSM6DSO_GY_ODR_833Hz, LSM6DSO_GY_HIGH_PERFORMANCE, 1200};
  if (interval_us >= 600) return (odr_g_interval_t){LSM6DSO_GY_ODR_1667Hz, LSM6DSO_GY_HIGH_PERFORMANCE, 600};
  if (interval_us >= 300) return (odr_g_interval_t){LSM6DSO_GY_ODR_3333Hz, LSM6DSO_GY_HIGH_PERFORMANCE, 300};
  return (odr_g_interval_t){LSM6DSO_GY_ODR_6667Hz, LSM6DSO_GY_HIGH_PERFORMANCE, 150};
}

//! Convert a raw axis vector to millidegrees per second on a board axis.
//! At the fixed 250 dps full scale the sensitivity is 8.75 mdps/LSB (35/4).
static int32_t prv_get_axis_projection_mdps(int axis, const int16_t *raw_vector) {
  const Lsm6dsoGyroConfig *cfg = LSM6DSO_GYRO;
  int32_t raw = raw_vector[cfg->axis_map[axis]] * (int32_t)cfg->axis_dir[axis];
  return (raw * 35) / 4;
}

static void prv_notify_accel_gyro_state_changed(void) {
#if defined(CONFIG_ACCEL_LSM6DSO)
  lsm6dso_accel_gyro_state_changed();
#endif
}

static void prv_deliver_sample(const int16_t raw[3], uint64_t timestamp_us) {
  if (timestamp_us / 1000 < s_gyro_valid_after_ms) {
    return;  // still settling after power-on
  }

  GyroDriverSample sample = {
    .timestamp_us = timestamp_us,
    .x = prv_get_axis_projection_mdps(0, raw),
    .y = prv_get_axis_projection_mdps(1, raw),
    .z = prv_get_axis_projection_mdps(2, raw),
  };
  s_gyro_last_sample = sample;

  if (s_gyro_state.num_samples > 0) {
    gyro_cb_new_sample(&sample);
  }
}

void lsm6dso_gyro_handle_fifo_record(const uint8_t data[6], uint64_t timestamp_us) {
  if (!s_gyro_running) {
    return;
  }

  int16_t raw[3];
  raw[0] = (int16_t)((data[1] << 8) | data[0]);
  raw[1] = (int16_t)((data[3] << 8) | data[2]);
  raw[2] = (int16_t)((data[5] << 8) | data[4]);
  prv_deliver_sample(raw, timestamp_us);
}

bool lsm6dso_gyro_is_active(void) { return s_gyro_running; }

void lsm6dso_gyro_handle_core_reinit(void) {
  // The chip was reset; clear achieved state so the chase re-applies
  // everything the target state still asks for
  s_gyro_running = false;
  s_gyro_state = (lsm6dso_gyro_state_t){0};
  prv_gyro_chase_target_state();
}

static int prv_read_sample(GyroDriverSample *data) {
  if (!lsm6dso_core_is_initialized() || !s_gyro_running) {
    return -1;
  }

  int16_t raw[3];
  if (lsm6dso_angular_rate_raw_get(&lsm6dso_ctx, raw) != 0) {
    PBL_LOG_ERR("LSM6DSO: Failed to read gyroscope data");
    return -1;
  }

  data->timestamp_us = lsm6dso_core_timestamp_ms() * 1000ULL;
  data->x = prv_get_axis_projection_mdps(0, raw);
  data->y = prv_get_axis_projection_mdps(1, raw);
  data->z = prv_get_axis_projection_mdps(2, raw);
  return 0;
}

static void prv_gyro_poll_work(void) {
  if (!s_gyro_running || s_gyro_state.num_samples == 0) {
    return;
  }

  if (s_gyro_state.num_samples > 1 && lsm6dso_core_fifo_in_use()) {
    lsm6dso_core_fifo_drain();
    return;
  }

  // Per-sample delivery (no FIFO): poll the output registers directly
  int16_t raw[3];
  if (lsm6dso_angular_rate_raw_get(&lsm6dso_ctx, raw) != 0) {
    PBL_LOG_ERR("LSM6DSO: Failed to read gyroscope data");
    return;
  }
  prv_deliver_sample(raw, lsm6dso_core_timestamp_ms() * 1000ULL);
}

static void prv_gyro_poll_timer_cb(void *data) {
  lsm6dso_core_offload_work(prv_gyro_poll_work);
}

static void prv_gyro_update_poll_timer(void) {
  uint32_t period_ms = 0;

  if (s_gyro_running && s_gyro_state.num_samples > 0) {
    if (s_gyro_state.num_samples > 1) {
      // Drain at the FIFO watermark cadence: half the requested batch size
      period_ms = (MAX(s_gyro_state.num_samples / 2, 1) *
                   (uint64_t)s_gyro_state.sampling_interval_us) / 1000;
    } else {
      period_ms = s_gyro_state.sampling_interval_us / 1000;
    }
    period_ms = MAX(period_ms, LSM6DSO_GYRO_MIN_POLL_MS);
  }

  if (period_ms == s_gyro_poll_period_ms) {
    return;
  }
  s_gyro_poll_period_ms = period_ms;

  if (s_gyro_poll_timer == TIMER_INVALID_ID) {
    s_gyro_poll_timer = new_timer_create();
  }

  if (period_ms == 0) {
    new_timer_stop(s_gyro_poll_timer);
    return;
  }

  new_timer_start(s_gyro_poll_timer, period_ms, prv_gyro_poll_timer_cb, NULL,
                  TIMER_START_FLAG_REPEATING);
}

static void prv_gyro_chase_target_state(void) {
  if (!lsm6dso_core_is_initialized()) {
    PBL_LOG_ERR("LSM6DSO: Cannot chase gyro target state before initialization");
    return;
  }

  bool should_be_running = s_gyro_state_target.sampling_interval_us > 0 ||
                           s_gyro_state_target.num_samples > 0;

  if (!should_be_running || !s_gyro_enabled) {
    if (s_gyro_running) {
      PBL_LOG_DBG("LSM6DSO: Stopping gyroscope");
      lsm6dso_gy_data_rate_set(&lsm6dso_ctx, LSM6DSO_GY_ODR_OFF);
      s_gyro_running = false;
      s_gyro_state = (lsm6dso_gyro_state_t){0};
      lsm6dso_core_fifo_request_gy(0, 0);
      prv_gyro_update_poll_timer();
      // The accel may return to ultra-low-power mode now
      prv_notify_accel_gyro_state_changed();
    }
    return;
  }

  bool starting = !s_gyro_running;

  uint32_t requested_interval = s_gyro_state_target.sampling_interval_us;
  if (requested_interval == 0) {
    requested_interval = LSM6DSO_GYRO_DEFAULT_INTERVAL_US;
  }
  odr_g_interval_t odr_interval = prv_get_gyro_odr_for_interval(requested_interval);

  bool interval_changed = starting ||
      odr_interval.interval_us != s_gyro_state.sampling_interval_us;

  if (interval_changed) {
    if (lsm6dso_gy_power_mode_set(&lsm6dso_ctx, odr_interval.power_mode) != 0) {
      PBL_LOG_ERR("LSM6DSO: Failed to set gyroscope power mode");
      return;
    }
    if (lsm6dso_gy_data_rate_set(&lsm6dso_ctx, odr_interval.odr) != 0) {
      PBL_LOG_ERR("LSM6DSO: Failed to set gyroscope ODR");
      return;
    }
    s_gyro_state.sampling_interval_us = odr_interval.interval_us;

    if (starting) {
      // Discard samples until the gyro has settled (AN5192 section 3.11)
      uint32_t settle_ms = LSM6DSO_GYRO_TURN_ON_MS +
          (LSM6DSO_GYRO_DISCARD_SAMPLES * odr_interval.interval_us) / 1000;
      s_gyro_valid_after_ms = lsm6dso_core_timestamp_ms() + settle_ms;
    }
  }

  if (starting) {
    s_gyro_running = true;
    PBL_LOG_DBG("LSM6DSO: Starting gyroscope (interval=%lu us)",
            s_gyro_state.sampling_interval_us);
  }

  if (interval_changed || s_gyro_state_target.num_samples != s_gyro_state.num_samples) {
    s_gyro_state.num_samples = s_gyro_state_target.num_samples;
    lsm6dso_core_fifo_request_gy(
        s_gyro_state.num_samples > 1 ? s_gyro_state.num_samples : 0,
        s_gyro_state.sampling_interval_us);
  }

  prv_gyro_update_poll_timer();

  if (starting) {
    // The accel must leave ultra-low-power mode while the gyro is active
    // (datasheet section 6.2.1)
    prv_notify_accel_gyro_state_changed();
  }

  PBL_LOG_DBG("LSM6DSO: Reached gyro target state: sampling_interval_us=%lu, num_samples=%lu",
          s_gyro_state.sampling_interval_us, s_gyro_state.num_samples);
}

// gyro.h implementation

void gyro_init(void) {
  // Initialize the chip (idempotent; the accel half may have done it already
  // on boards where both halves are present). The gyro is left powered down.
  if (!lsm6dso_core_init()) {
    PBL_LOG_ERR("LSM6DSO: Gyro init failed; core initialization unsuccessful");
  }
}

void gyro_power_up(void) {
  s_gyro_enabled = true;
  prv_gyro_chase_target_state();
}

void gyro_power_down(void) {
  PBL_LOG_DBG("LSM6DSO: Powering down gyroscope");
  s_gyro_enabled = false;
  prv_gyro_chase_target_state();
}

uint32_t gyro_set_sampling_interval(uint32_t interval_us) {
  PBL_LOG_DBG("LSM6DSO: Requesting update of gyro sampling interval to %lu us", interval_us);
  s_gyro_state_target.sampling_interval_us = interval_us;
  prv_gyro_chase_target_state();
  return s_gyro_state.sampling_interval_us;
}

uint32_t gyro_get_sampling_interval(void) { return s_gyro_state.sampling_interval_us; }

void gyro_set_num_samples(uint32_t num_samples) {
  PBL_LOG_DBG("LSM6DSO: Setting gyro number of samples to %lu", num_samples);
  s_gyro_state_target.num_samples = num_samples;
  prv_gyro_chase_target_state();
}

int gyro_peek(GyroDriverSample *data) { return prv_read_sample(data); }
