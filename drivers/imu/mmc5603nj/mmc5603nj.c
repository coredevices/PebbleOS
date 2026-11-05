/* SPDX-FileCopyrightText: 2025 Matthew Wardrop */
/* SPDX-FileCopyrightText: 2025 Bob Wei */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/i2c.h>
#include <pbl/drivers/imu/mmc5603nj/mmc5603nj.h>
#include <pbl/drivers/imu/mmc5603nj/registers.h>
#include <pbl/drivers/mag.h>
#include <pbl/kernel/delay.h>
#include <pbl/logging/logging.h>
#include <pbl/services/imu/units.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>

PBL_LOG_MODULE_DEFINE(driver_mag_mmc5603nj, CONFIG_DRIVER_IMU_LOG_LEVEL);

#define MMC5603NJ_RATE_HZ_MAX 255U

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

static int16_t prv_get_axis(const int16_t raw[3], IMUCoordinateAxis axis) {
  int16_t val = s_dev->axis_dir[axis] * raw[s_dev->axis_map[axis]];

  if (s_dev->state->rotated && (axis == AXIS_X || axis == AXIS_Y)) {
    val = -val;
  }

  // 1024 counts per gauss in 16-bit mode
  return (int16_t)(((int32_t)val * 1000) / 1024);
}

static int prv_read_sample(MagData *sample) {
  uint8_t data[6];
  int16_t raw[3];
  int ret;

  ret = prv_read(MMC5603NJ_REG_XOUT0, data, sizeof(data));
  if (ret != 0) {
    return ret;
  }

  // Unsigned, offset by 2^15
  for (uint8_t i = 0U; i < 3U; i++) {
    raw[i] = (int16_t)((((uint16_t)data[2U * i] << 8) | data[2U * i + 1U]) - (1U << 15));
  }

  sample->x = prv_get_axis(raw, AXIS_X);
  sample->y = prv_get_axis(raw, AXIS_Y);
  sample->z = prv_get_axis(raw, AXIS_Z);

  return 0;
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
  MagData discard;
  bool ready;
  int ret;

  // Leave continuous mode
  ret = prv_write(MMC5603NJ_REG_CTRL2, 0x00);
  if (ret != 0) {
    return ret;
  }

  // Clear any pending data-ready flag
  if (prv_is_data_ready(&ready) == 0 && ready) {
    (void)prv_read_sample(&discard);
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

static void prv_poll_cb(void *data) {
  MagData sample;
  bool ready;
  int ret;

  pbl_mag_lock(&s_dev->mag);

  if (s_dev->state->rate_hz == 0U) {
    goto out;
  }

  ret = prv_is_data_ready(&ready);
  if (ret == 0 && !ready) {
    goto out;
  }

  if (ret == 0) {
    ret = prv_read_sample(&sample);
  }

  if (ret != 0) {
    PBL_LOG_WRN("Read failed (%d), restarting sampling", ret);
    if (prv_set_rate_hw(0U) == 0) {
      (void)prv_set_rate_hw(s_dev->state->rate_hz);
    }
    goto out;
  }

  pbl_mag_report_sample(&s_dev->mag, &sample);

out:
  pbl_mag_unlock(&s_dev->mag);
}

static int prv_init(const struct pbl_mag *mag) {
  uint8_t whoami;
  int ret;

  s_dev = container_of(mag, const struct pbl_mmc5603nj, mag);

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

static int prv_set_rotated(const struct pbl_mag *mag, bool rotated) {
  s_dev->state->rotated = rotated;

  return 0;
}

static int prv_set_sampling_interval(const struct pbl_mag *mag, uint32_t interval_us,
                                     uint32_t *actual_us) {
  struct pbl_mmc5603nj_state *state = s_dev->state;
  uint8_t rate_hz = 0U;
  int ret;

  if (interval_us > 0U) {
    rate_hz = (uint8_t)CLIP(DIVIDE_CEIL(1000000U, interval_us), 1U, MMC5603NJ_RATE_HZ_MAX);
  }

  if (rate_hz != state->rate_hz) {
    PBL_LOG_DBG("Setting sample rate to %u Hz", rate_hz);

    ret = prv_set_rate_hw(rate_hz);
    if (ret != 0) {
      return ret;
    }

    state->rate_hz = rate_hz;

    if (rate_hz == 0U) {
      new_timer_stop(state->poll_timer);
    } else {
      if (state->poll_timer == TIMER_INVALID_ID) {
        state->poll_timer = new_timer_create();
        if (state->poll_timer == TIMER_INVALID_ID) {
          return -ENOMEM;
        }
      }

      new_timer_start(state->poll_timer, DIVIDE_CEIL(1000U, rate_hz), prv_poll_cb, nullptr,
                      TIMER_START_FLAG_REPEATING);
    }
  }

  *actual_us = (rate_hz == 0U) ? 0U : 1000000U / rate_hz;

  return 0;
}

static int prv_peek(const struct pbl_mag *mag, MagData *sample) {
  if (s_dev->state->rate_hz == 0U) {
    return -ENODATA;
  }

  return prv_read_sample(sample);
}

const struct pbl_mag_ops pbl_mmc5603nj_ops = {
  .init = prv_init,
  .set_rotated = prv_set_rotated,
  .set_sampling_interval = prv_set_sampling_interval,
  .peek = prv_peek,
};
