/* SPDX-FileCopyrightText: 2025 Core Devices */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/als.h>
#include <pbl/drivers/ambient/w1160.h>
#include <pbl/drivers/i2c.h>
#include <pbl/drivers/rtc.h>
#include <pbl/kernel/delay.h>
#include <pbl/logging/logging.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>
#include <pbl/util/size.h>

PBL_LOG_MODULE_DEFINE(driver_ambient_w1160, CONFIG_DRIVER_AMBIENT_LOG_LEVEL);

// Registers
#define W1160_STATE_REG            0x00
#define W1160_IT_FAST1_REG         0x02
#define W1160_IT_FAST2_REG         0x03
#define W1160_SAMPLE_FAST1_REG     0x05
#define W1160_SAMPLE_FAST2_REG     0x06
#define W1160_SAMPLE_SLOW1_REG     0x07
#define W1160_SAMPLE_SLOW2_REG     0x08
#define W1160_FLAG1_REG            0x10
#define W1160_DATA1_ALS_REG        0x13
#define W1160_DATA2_ALS_REG        0x14
#define W1160_DATA_GC_REG          0x17
#define W1160_POWER_MODE_REG       0x3D
#define W1160_PDT_ID_REG           0x3E
#define W1160_FIFOCTRL1_REG        0x60
#define W1160_FIFOCTRL1_REG_VALUE  0x20
#define W1160_FIFO1_WM_LV_REG      0x61
#define W1160_FIFO2_WM_LV_REG      0x62
#define W1160_FIFOCTRL2_REG        0x63
#define W1160_FIFO_FCNT1_REG       0x64
#define W1160_FIFO_FCNT2_REG       0x65
#define W1160_FIFO_OUT_REG         0x66
#define W1160_FIFO_FLAG_REG        0x67
#define W1160_FIFOCTRL3_REG        0x68
#define W1160_FIFOCTRL4_REG        0x69
#define W1160_AGCCTRL1_REG         0x6A
#define W1160_MANUAL_GAIN_CTRL_REG 0x6B
#define W1160_AGCCTRL2_REG         0x6C
#define W1160_THD_SAT_GC_REG       0x6D
#define W1160_IT_SLOW1_REG         0x6E
#define W1160_IT_SLOW2_REG         0x6F
#define W1160_SOFT_RESET_REG       0x80
// Configuration register bits
#define W1160_ALSCTRL_REG 0xA4

#define W1160_POR_WAIT_TIME           (10) /* ms */
#define W1160_AGCCTRL1_AGC_EN         PBL_BIT(7)
#define W1160_MANUAL_GAIN_CTRL_MGC_EN PBL_BIT(0)
#define W1160_AGCCTRL2_SEL_MODE       PBL_BIT(2) /* must be 1 */
#define W1160_AGCCTRL2_12B_MODE       PBL_BIT(3)
#define W1160_ALSCTRL_SAT_EN          PBL_BIT(1)
#define W1160_ALSCTRL_DATA_FORMAT12   PBL_BIT(2) /* 1:12bit 0:16bit */
#define W1160_DATA_GC_LVL_MASK        PBL_GENMASK(7, 4)
#define W1160_SAT_GC_CONFIG           (0x0A)
#define W1160_SLOW_IT_CONFIG1         (0x03) /* 99ms ((0x3E7+1) * 99us) */
#define W1160_SLOW_IT_CONFIG2         (0xE7)
#define W1160_SLOW_ST_CONFIG1         (0x07) /* 200ms ((0x7CF+1) * 100us) */
#define W1160_SLOW_ST_CONFIG2         (0xCF)
#define W1160_STATE_SAMPLING_EN       PBL_BIT(1)
#define W1160_CHIP_ID                 (0xE5)
#define W1160_FLG_ALS_DR              PBL_BIT(7)

#define W1160_RESULT_EXPONENT_SHIFT (12)
#define W1160_RESULT_MANTISSA_MASK  (0x0FFF)

#define W1160_ALS_POLL_DELAY_MS   (5)   /* ms between data-ready polls */
#define W1160_ALS_POLL_TIMEOUT_MS (200) /* max wait for ALS data-ready */

// DATA_ALS reads zero while SAMPLING_EN=0, so the last good level is cached. The cache is served
// while suspended, and dropped when the primed window closes so the next unprimed read is fresh.
#define W1160_SETTLE_AFTER_ENABLE_MS (W1160_ALS_POLL_TIMEOUT_MS)

struct w1160_reg_val {
  uint8_t reg;
  uint8_t val;
};

static const struct w1160_reg_val s_config[] = {
  {W1160_AGCCTRL1_REG, PBL_FIELD_PREP(W1160_AGCCTRL1_AGC_EN, 0)},
  {W1160_MANUAL_GAIN_CTRL_REG, PBL_FIELD_PREP(W1160_MANUAL_GAIN_CTRL_MGC_EN, 1)},
  {W1160_DATA_GC_REG, PBL_FIELD_PREP(W1160_DATA_GC_LVL_MASK, 15)},
  {W1160_AGCCTRL2_REG,
   PBL_FIELD_PREP(W1160_AGCCTRL2_SEL_MODE, 1) | PBL_FIELD_PREP(W1160_AGCCTRL2_12B_MODE, 0)},
  {W1160_ALSCTRL_REG,
   PBL_FIELD_PREP(W1160_ALSCTRL_SAT_EN, 0) | PBL_FIELD_PREP(W1160_ALSCTRL_DATA_FORMAT12, 0)},
  {W1160_THD_SAT_GC_REG, W1160_SAT_GC_CONFIG},
  {W1160_IT_SLOW1_REG, W1160_SLOW_IT_CONFIG1},
  {W1160_IT_SLOW2_REG, W1160_SLOW_IT_CONFIG2},
  {W1160_SAMPLE_SLOW1_REG, W1160_SLOW_ST_CONFIG1},
  {W1160_SAMPLE_SLOW2_REG, W1160_SLOW_ST_CONFIG2},
};

static const struct pbl_w1160 *prv_get(const struct pbl_als *als) {
  return container_of(als, const struct pbl_w1160, als);
}

static int prv_read_register(const struct pbl_w1160 *dev, uint8_t reg, uint8_t *val) {
  int ret;

  pbl_i2c_use(&dev->i2c);
  ret = pbl_i2c_read_register_block(&dev->i2c, reg, 1, val);
  pbl_i2c_release(&dev->i2c);
  if (ret != 0) {
    PBL_LOG_ERR("Could not read register 0x%02x (%d)", reg, ret);
  }

  return ret;
}

static int prv_write_register(const struct pbl_w1160 *dev, uint8_t reg, uint8_t val) {
  int ret;

  pbl_i2c_use(&dev->i2c);
  ret = pbl_i2c_write_register_block(&dev->i2c, reg, 1, &val);
  pbl_i2c_release(&dev->i2c);
  if (ret != 0) {
    PBL_LOG_ERR("Could not write register 0x%02x (%d)", reg, ret);
  }

  return ret;
}

static int prv_read_data(const struct pbl_w1160 *dev, uint16_t *level) {
  uint8_t hi;
  uint8_t lo;
  int ret;

  ret = prv_read_register(dev, W1160_DATA1_ALS_REG, &hi);
  if (ret != 0) {
    return ret;
  }

  ret = prv_read_register(dev, W1160_DATA2_ALS_REG, &lo);
  if (ret != 0) {
    return ret;
  }

  *level = (uint16_t)((hi << 8) | lo);

  return 0;
}

// True once DATA_ALS is ready, or when the flag read fails
static bool prv_data_ready(const struct pbl_w1160 *dev, int *ret) {
  uint8_t flag;

  *ret = prv_read_register(dev, W1160_FLAG1_REG, &flag);

  return (*ret != 0) || ((flag & W1160_FLG_ALS_DR) != 0U);
}

// Wait for FLG_ALS_DR, then read DATA_ALS. Sampling must be enabled.
static int prv_read_data_polled(const struct pbl_w1160 *dev, uint16_t *level) {
  int read_ret;
  int ret;

  ret = PBL_POLL_SLEEP_MS(prv_data_ready(dev, &read_ret), W1160_ALS_POLL_DELAY_MS,
                          W1160_ALS_POLL_TIMEOUT_MS);
  if (read_ret != 0) {
    return read_ret;
  }

  if (ret != 0) {
    PBL_LOG_ERR("Data-ready timeout");
    return ret;
  }

  return prv_read_data(dev, level);
}

// Sampling has been on long enough for DATA_ALS to hold a real sample
static bool prv_sampling_has_settled(const struct pbl_w1160 *dev) {
  const RtcTicks elapsed = rtc_get_ticks() - dev->state->sampling_started_ticks;

  return elapsed >= ((RtcTicks)W1160_SETTLE_AFTER_ENABLE_MS * RTC_TICKS_HZ / 1000U);
}

static int prv_read_cached(const struct pbl_w1160 *dev, uint32_t *level, bool polled) {
  struct pbl_w1160_state *state = dev->state;
  uint16_t val;
  int ret;

  ret = polled ? prv_read_data_polled(dev, &val) : prv_read_data(dev, &val);
  if (ret != 0) {
    return ret;
  }

  state->cached_level = val;
  state->cache_valid = true;
  *level = val;

  return 0;
}

static int prv_read_oneshot(const struct pbl_w1160 *dev, uint32_t *level) {
  int ret;
  int ret_dis;

  ret = prv_write_register(dev, W1160_STATE_REG, W1160_STATE_SAMPLING_EN);
  if (ret != 0) {
    return ret;
  }

  ret = prv_read_cached(dev, level, true);

  ret_dis = prv_write_register(dev, W1160_STATE_REG, 0);
  if (ret != 0) {
    return ret;
  }

  return ret_dis;
}

static int prv_init(const struct pbl_als *als) {
  const struct pbl_w1160 *dev = prv_get(als);
  uint8_t chip_id;
  int ret;

  pbl_sleep_ms(W1160_POR_WAIT_TIME);

  ret = prv_read_register(dev, W1160_PDT_ID_REG, &chip_id);
  if (ret != 0) {
    return -ENODEV;
  }

  if (chip_id != W1160_CHIP_ID) {
    PBL_LOG_ERR("Unexpected chip ID: 0x%02x", chip_id);
    return -ENODEV;
  }

  for (size_t i = 0U; i < ARRAY_LENGTH(s_config); i++) {
    ret = prv_write_register(dev, s_config[i].reg, s_config[i].val);
    if (ret != 0) {
      return ret;
    }
  }

  return 0;
}

static int prv_read(const struct pbl_als *als, uint32_t *level) {
  const struct pbl_w1160 *dev = prv_get(als);
  struct pbl_w1160_state *state = dev->state;

  if (!state->active) {
    return prv_read_oneshot(dev, level);
  }

  if (state->sampling && prv_sampling_has_settled(dev)) {
    return prv_read_cached(dev, level, false);
  }

  if (state->cache_valid) {
    *level = state->cached_level;
    return 0;
  }

  if (!state->sampling) {
    return -ENODATA;
  }

  return prv_read_cached(dev, level, true);
}

static int prv_set_state(const struct pbl_als *als, bool active, bool sampling) {
  const struct pbl_w1160 *dev = prv_get(als);
  struct pbl_w1160_state *state = dev->state;
  int ret;

  // A new window must not reuse a sample from an earlier one-shot
  if (!state->active && active) {
    state->cache_valid = false;
  }

  if (sampling != state->sampling) {
    ret =
        prv_write_register(dev, W1160_STATE_REG, PBL_FIELD_PREP(W1160_STATE_SAMPLING_EN, sampling));
    if (ret != 0) {
      return ret;
    }

    state->sampling = sampling;
    if (sampling) {
      state->sampling_started_ticks = rtc_get_ticks();
    }
  }

  // Window closed: the next unprimed read must be fresh
  if (state->active && !active) {
    state->cache_valid = false;
  }

  state->active = active;

  return 0;
}

const struct pbl_als_ops pbl_w1160_ops = {
  .init = prv_init,
  .read = prv_read,
  .set_state = prv_set_state,
};
