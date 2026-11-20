/* SPDX-FileCopyrightText: 2025 Matthew Wardrop */
/* SPDX-FileCopyrightText: 2025 Bob Wei */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/i2c.h>
#include <pbl/drivers/sensor.h>
#include <pbl/drivers/sensor/mmc5603nj/mmc5603nj.h>
#include <pbl/drivers/sensor/mmc5603nj/registers.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/time.h>
#include <pbl/logging/logging.h>
#include <pbl/util/byteorder.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>
#include <pbl/util/size.h>

PBL_LOG_MODULE_DECLARE(driver_sensor, CONFIG_DRIVER_SENSOR_LOG_LEVEL);

#define MMC5603NJ_RATE_HZ_MAX 255U

#define MMC5603NJ_FRAME_SIZE 6U

// Single measurement time at the 6.6 ms bandwidth, with margin
#define MMC5603NJ_MEAS_TIMEOUT_MS 20U

// Unsigned, offset by 2^15, 1024 counts per gauss: 100000 / 1024 nT per count
#define NT_NUM  100000
#define NT_DEN  1024
#define NT_BIAS (-(32768 / NT_DEN) * NT_NUM)

static const struct pbl_sensor_chan s_chans[] = {
  PBL_SENSOR_CHAN(PBL_SENSOR_MAGN, PBL_SENSOR_AXIS_X, 0, PBL_SENSOR_FMT_U16_BE, NT_NUM, NT_DEN,
                  NT_BIAS),
  PBL_SENSOR_CHAN(PBL_SENSOR_MAGN, PBL_SENSOR_AXIS_Y, 2, PBL_SENSOR_FMT_U16_BE, NT_NUM, NT_DEN,
                  NT_BIAS),
  PBL_SENSOR_CHAN(PBL_SENSOR_MAGN, PBL_SENSOR_AXIS_Z, 4, PBL_SENSOR_FMT_U16_BE, NT_NUM, NT_DEN,
                  NT_BIAS),
};

static const struct pbl_mmc5603nj *s_dev;

static int prv_read(uint8_t reg, uint8_t *data, uint32_t len) {
  int ret;

  ret = pbl_i2c_burst_read(&s_dev->i2c, reg, data, len);
  if (ret != 0) {
    PBL_LOG_ERR("Could not read register 0x%02x (%d)", reg, ret);
  }

  return ret;
}

static int prv_write(uint8_t reg, uint8_t val) {
  int ret;

  ret = pbl_i2c_reg_write_byte(&s_dev->i2c, reg, val);
  if (ret != 0) {
    PBL_LOG_ERR("Could not write register 0x%02x (%d)", reg, ret);
  }

  return ret;
}

static int prv_is_data_ready(bool *ready) {
  uint8_t status;
  int ret;

  ret = prv_read(MMC5603NJ_REG_STATUS1, &status, 1);
  if (ret != 0) {
    return ret;
  }

  *ready = (status & MMC5603NJ_STATUS1_MEAS_M_DONE_MASK) != 0U;

  return 0;
}

static int prv_read_sample(uint8_t *frame) {
  return prv_read(MMC5603NJ_REG_XOUT0, frame, MMC5603NJ_FRAME_SIZE);
}

static int prv_reset(void) {
  int ret;

  ret = prv_write(MMC5603NJ_REG_CTRL1,
                  PBL_FIELD_PREP(MMC5603NJ_CTRL1_BANDWIDTH_MASK, MMC5603NJ_CTRL1_BANDWIDTH_6ms6) |
                      MMC5603NJ_CTRL1_SW_RESET);
  if (ret != 0) {
    return ret;
  }

  pbl_sleep_ms(MMC5603NJ_SW_RESET_DELAY_MS);

  ret = prv_write(MMC5603NJ_REG_CTRL0, MMC5603NJ_CTRL0_DO_SET);
  if (ret != 0) {
    return ret;
  }

  pbl_sleep_ms(MMC5603NJ_SET_DELAY_MS);

  ret = prv_write(MMC5603NJ_REG_CTRL0, MMC5603NJ_CTRL0_DO_RESET);
  if (ret != 0) {
    return ret;
  }

  pbl_sleep_ms(MMC5603NJ_SET_DELAY_MS);

  return 0;
}

static int prv_set_rate_hw(uint8_t rate_hz) {
  uint8_t discard[MMC5603NJ_FRAME_SIZE];
  bool ready;
  int ret;

  // Leave continuous mode
  ret = prv_write(MMC5603NJ_REG_CTRL2, 0x00);
  if (ret != 0) {
    return ret;
  }

  // Clear any pending data-ready flag
  if (prv_is_data_ready(&ready) == 0 && ready) {
    (void)prv_read_sample(discard);
  }

  if (rate_hz == 0U) {
    return 0;
  }

  ret = prv_write(MMC5603NJ_REG_ODR, rate_hz);
  if (ret != 0) {
    return ret;
  }

  // Recompute the measurement period
  ret = prv_write(MMC5603NJ_REG_CTRL0, MMC5603NJ_CTRL0_AUTO_SR_EN | MMC5603NJ_CTRL0_CMM_FREQ_EN);
  if (ret != 0) {
    return ret;
  }

  return prv_write(MMC5603NJ_REG_CTRL2, PBL_FIELD_PREP(MMC5603NJ_CTRL2_AUTOSET_PRD_MASK,
                                                       MMC5603NJ_CTRL2_AUTOSET_PRD_100) |
                                            MMC5603NJ_CTRL2_PRD_SET_EN | MMC5603NJ_CTRL2_CMM_EN);
}

static pbl_tick_t prv_poll_period_ticks(uint8_t rate_hz) {
  return pbl_ms_to_ticks(PBL_DIV_ROUND_UP(1000U, rate_hz));
}

static void prv_poll_fn(struct pbl_work *work) {
  struct pbl_mmc5603nj_state *state = s_dev->state;
  uint8_t *frame;
  bool ready;
  int ret;

  pbl_sensor_lock(&s_dev->sensor);

  if (state->rate_hz == 0U) {
    goto out;
  }

  ret = prv_is_data_ready(&ready);
  if (ret == 0 && !ready) {
    goto out;
  }

  frame = pbl_sensor_buf_alloc(&s_dev->sensor, nullptr);
  if (frame == nullptr) {
    goto out;
  }

  if (ret == 0) {
    ret = prv_read_sample(frame);
  }

  if (ret != 0) {
    pbl_sensor_buf_free(&s_dev->sensor, frame);
    PBL_LOG_WRN("Read failed (%d), restarting sampling", ret);
    if (prv_set_rate_hw(0U) == 0) {
      (void)prv_set_rate_hw(state->rate_hz);
    }
    goto out;
  }

  pbl_sensor_report(&s_dev->sensor, &(const struct pbl_sensor_batch){
                                      .data = frame,
                                      .chans = s_chans,
                                      .num_chans = PBL_ARRAY_SIZE(s_chans),
                                      .stride = MMC5603NJ_FRAME_SIZE,
                                      .num_frames = 1U,
                                      .timestamp_us = pbl_uptime_us(),
                                      .interval_us = 1000000U / state->rate_hz,
                                    });

out:
  // Rate changes hold the lock too
  if (state->rate_hz != 0U && !pbl_delayed_work_is_pending(&state->poll_timer)) {
    pbl_delayed_work_schedule_next(&state->poll_timer, prv_poll_period_ticks(state->rate_hz));
  }

  pbl_sensor_unlock(&s_dev->sensor);
}

static int prv_init(const struct pbl_sensor *sensor) {
  uint8_t whoami;
  int ret;

  s_dev = PBL_CONTAINER_OF(sensor, const struct pbl_mmc5603nj, sensor);
  pbl_delayed_work_init(&s_dev->state->poll_timer, prv_poll_fn);

  ret = prv_read(MMC5603NJ_REG_WHO_AM_I, &whoami, 1);
  if (ret != 0) {
    return -ENODEV;
  }

  if (whoami != MMC5603NJ_WHO_AM_I_VALUE) {
    PBL_LOG_ERR("Unexpected id: 0x%02x!=0x%02x", whoami, MMC5603NJ_WHO_AM_I_VALUE);
    return -ENODEV;
  }

  ret = prv_reset();
  if (ret != 0) {
    PBL_LOG_ERR("Could not reset (%d)", ret);
    return ret;
  }

  return 0;
}

static int prv_set_interval(int32_t *interval_us) {
  struct pbl_mmc5603nj_state *state = s_dev->state;
  uint8_t rate_hz = 0U;
  int ret;

  if (*interval_us < 0) {
    return -EINVAL;
  }

  if (*interval_us > 0) {
    rate_hz = (uint8_t)PBL_CLAMP(PBL_DIV_ROUND_UP(1000000U, (uint32_t)*interval_us), 1U,
                                 MMC5603NJ_RATE_HZ_MAX);
  }

  if (rate_hz != state->rate_hz) {
    PBL_LOG_DBG("Setting sample rate to %u Hz", rate_hz);

    ret = prv_set_rate_hw(rate_hz);
    if (ret != 0) {
      return ret;
    }

    state->rate_hz = rate_hz;

    if (rate_hz == 0U) {
      pbl_delayed_work_cancel(&state->poll_timer);
    } else {
      pbl_delayed_work_reschedule(&pbl_sys_workq_high, &state->poll_timer,
                                  PBL_TICKS(prv_poll_period_ticks(rate_hz)));
    }
  }

  *interval_us = (rate_hz == 0U) ? 0 : (int32_t)(1000000U / rate_hz);

  return 0;
}

static int prv_attr_set(const struct pbl_sensor *sensor, enum pbl_sensor_type type,
                        enum pbl_sensor_attr attr, int32_t *val) {
  if (attr != PBL_SENSOR_ATTR_INTERVAL) {
    return -ENOTSUP;
  }

  return prv_set_interval(val);
}

static int prv_attr_get(const struct pbl_sensor *sensor, enum pbl_sensor_type type,
                        enum pbl_sensor_attr attr, int32_t *val) {
  uint8_t rate_hz = s_dev->state->rate_hz;

  if (attr != PBL_SENSOR_ATTR_INTERVAL) {
    return -ENOTSUP;
  }

  *val = (rate_hz == 0U) ? 0 : (int32_t)(1000000U / rate_hz);

  return 0;
}

static int prv_measure(void) {
  bool ready = false;
  int ret;

  // Same polarity as the periodic SET of continuous mode
  ret = prv_write(MMC5603NJ_REG_CTRL0, MMC5603NJ_CTRL0_DO_SET);
  if (ret != 0) {
    return ret;
  }

  pbl_sleep_ms(MMC5603NJ_SET_DELAY_MS);

  ret = prv_write(MMC5603NJ_REG_CTRL0, MMC5603NJ_CTRL0_TAKE_MEAS_M);
  if (ret != 0) {
    return ret;
  }

  return PBL_POLL_SLEEP_MS(prv_is_data_ready(&ready) != 0 || ready, 1, MMC5603NJ_MEAS_TIMEOUT_MS);
}

static int prv_sensor_read(const struct pbl_sensor *sensor, enum pbl_sensor_type type,
                           struct pbl_sensor_frame *frame) {
  int ret;

  if (s_dev->state->rate_hz == 0U) {
    ret = prv_measure();
    if (ret != 0) {
      return ret;
    }
  }

  ret = prv_read_sample(frame->data);
  if (ret != 0) {
    return ret;
  }

  frame->chans = s_chans;
  frame->num_chans = PBL_ARRAY_SIZE(s_chans);
  frame->timestamp_us = pbl_uptime_us();

  return 0;
}

const struct pbl_sensor_ops pbl_mmc5603nj_ops = {
  .init = prv_init,
  .read = prv_sensor_read,
  .attr_set = prv_attr_set,
  .attr_get = prv_attr_get,
};

#include PBL_DT_INSTANCES(memsic_mmc5603)
