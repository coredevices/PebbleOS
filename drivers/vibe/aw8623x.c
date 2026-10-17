/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/gpio.h>
#include <pbl/drivers/i2c.h>
#include <pbl/drivers/vibe.h>
#include <pbl/drivers/vibe/aw8623x.h>
#include <pbl/kernel/delay.h>
#include <pbl/logging/logging.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>
#include <pbl/util/size.h>

PBL_LOG_MODULE_DEFINE(driver_vibe_aw8623x, CONFIG_DRIVER_VIBE_LOG_LEVEL);

#define AW8623X_PLAYCFG3                0x08U
#define AW8623X_PLAYCFG3_BRK_EN         PBL_BIT(2)
#define AW8623X_PLAYCFG3_PLAY_MODE_MASK PBL_GENMASK(1, 0)
#define AW8623X_PLAYCFG3_PLAY_MODE_CONT 0x2U

#define AW8623X_PLAYCFG4      0x09U
#define AW8623X_PLAYCFG4_STOP PBL_BIT(1)
#define AW8623X_PLAYCFG4_GO   PBL_BIT(0)

#define AW8623X_CONTCFG1               0x17U
#define AW8623X_CONTCFG1_EDGE_FRE_NONE 0x0U
#define AW8623X_CONTCFG1_SIN_MODE_COS  PBL_BIT(4)

#define AW8623X_CONTCFG2               0x18U
#define AW8623X_CONTCFG2_CONF_F0(freq) (24000U / (freq))

#define AW8623X_CONTCFG3                 0x19U
#define AW8623X_CONTCFG3_DRV_WIDTH(freq) (48000U / (freq))

#define AW8623X_CONTCFG6               0x1CU
#define AW8623X_CONTCFG6_TRACK_EN      PBL_BIT(7)
#define AW8623X_CONTCFG6_DRV1_LVL_MASK PBL_GENMASK(6, 0)

#define AW8623X_CONTCFG7               0x1DU
#define AW8623X_CONTCFG7_DRV2_LVL_MASK PBL_GENMASK(6, 0)
#define AW8623X_CONTCFG7_DRV2_LVL_MAX  0x7FU

#define AW8623X_CONTCFG8               0x1EU
#define AW8623X_CONTCFG8_DRV1_TIME_MAX 0xFFU

#define AW8623X_CONTCFG9               0x1FU
#define AW8623X_CONTCFG9_DRV2_TIME_MAX 0xFFU

#define AW8623X_GLBRD5               0x3FU
#define AW8623X_GLBRD5_STATE_MASK    PBL_GENMASK(3, 0)
#define AW8623X_GLBRD5_STATE_STANDBY 0x00U

#define AW8623X_SYSCTRL2         0x46U
#define AW8623X_SYSCTRL2_STANDBY PBL_BIT(6)

#define AW8623X_VBATCTRL              0x4EU
#define AW8623X_VBATCTRL_VBAT_MODE_HW PBL_BIT(6)

#define AW8623X_IDH          0x57U
#define AW8623X_IDH_CHIPID_H 0x23U

#define AW8623X_IDL          0x69U
#define AW8623X_IDL_CHIPID_L 0x40U

#define AW8623X_PWR_OFF_TIME_MS 2
#define AW8623X_PWR_ON_TIME_MS  3
#define AW8623X_STOP_RETRIES    40
#define AW8623X_STOP_POLL_MS    2

#define AW8623X_LRA_F0_HZ 235U

static const struct pbl_aw8623x *prv_get(const struct pbl_vibe *vibe) {
  return container_of(vibe, const struct pbl_aw8623x, vibe);
}

static int prv_read_register(const struct pbl_aw8623x *dev, uint8_t reg, uint8_t *val) {
  int ret;

  pbl_i2c_use(&dev->i2c);
  ret = pbl_i2c_read_register_block(&dev->i2c, reg, 1, val);
  pbl_i2c_release(&dev->i2c);

  return ret;
}

static int prv_write_register(const struct pbl_aw8623x *dev, uint8_t reg, uint8_t val) {
  int ret;

  pbl_i2c_use(&dev->i2c);
  ret = pbl_i2c_write_register_block(&dev->i2c, reg, 1, &val);
  pbl_i2c_release(&dev->i2c);

  return ret;
}

static int prv_set_standby(const struct pbl_aw8623x *dev, bool standby) {
  uint8_t val;
  int ret;

  ret = prv_read_register(dev, AW8623X_SYSCTRL2, &val);
  if (ret != 0) {
    return ret;
  }

  val = (val & ~AW8623X_SYSCTRL2_STANDBY) | PBL_FIELD_PREP(AW8623X_SYSCTRL2_STANDBY, standby);

  return prv_write_register(dev, AW8623X_SYSCTRL2, val);
}

static int prv_wait_for_standby(const struct pbl_aw8623x *dev) {
  uint8_t val;
  int ret;

  for (unsigned int i = 0; i < AW8623X_STOP_RETRIES; i++) {
    ret = prv_read_register(dev, AW8623X_GLBRD5, &val);
    if (ret != 0) {
      return ret;
    }

    if (PBL_FIELD_GET(AW8623X_GLBRD5_STATE_MASK, val) == AW8623X_GLBRD5_STATE_STANDBY) {
      return 0;
    }

    pbl_sleep_ms(AW8623X_STOP_POLL_MS);
  }

  return -ETIMEDOUT;
}

static int prv_set_strength(const struct pbl_aw8623x *dev, uint8_t strength) {
  const uint8_t scale = ((uint16_t)strength * AW8623X_CONTCFG7_DRV2_LVL_MAX) / 100U;
  int ret;

  ret = prv_write_register(
      dev, AW8623X_CONTCFG6,
      PBL_FIELD_PREP(AW8623X_CONTCFG6_DRV1_LVL_MASK, scale) | AW8623X_CONTCFG6_TRACK_EN);
  if (ret != 0) {
    return ret;
  }

  return prv_write_register(dev, AW8623X_CONTCFG7,
                            PBL_FIELD_PREP(AW8623X_CONTCFG7_DRV2_LVL_MASK, scale));
}

static int prv_init(const struct pbl_vibe *vibe) {
  const struct pbl_aw8623x *dev = prv_get(vibe);
  uint8_t val;
  int ret;

  ret = pbl_gpio_configure(&dev->reset, PBL_GPIO_OUTPUT_ACTIVE);
  if (ret != 0) {
    return ret;
  }

  pbl_sleep_ms(AW8623X_PWR_OFF_TIME_MS);
  (void)pbl_gpio_set(&dev->reset, false);
  pbl_sleep_ms(AW8623X_PWR_ON_TIME_MS);

  ret = prv_read_register(dev, AW8623X_IDH, &val);
  if (ret != 0 || val != AW8623X_IDH_CHIPID_H) {
    PBL_LOG_ERR("Could not read the chip ID high byte (%d)", ret);
    return -ENODEV;
  }

  ret = prv_read_register(dev, AW8623X_IDL, &val);
  if (ret != 0 || val != AW8623X_IDL_CHIPID_L) {
    PBL_LOG_ERR("Could not read the chip ID low byte (%d)", ret);
    return -ENODEV;
  }

  // F0 detection disables automatic braking; normal playback only needs tracking.
  const uint8_t config[][2] = {
    {AW8623X_CONTCFG1, AW8623X_CONTCFG1_EDGE_FRE_NONE | AW8623X_CONTCFG1_SIN_MODE_COS},
    {AW8623X_CONTCFG2, AW8623X_CONTCFG2_CONF_F0(AW8623X_LRA_F0_HZ)},
    {AW8623X_CONTCFG3, AW8623X_CONTCFG3_DRV_WIDTH(AW8623X_LRA_F0_HZ)},
    {AW8623X_PLAYCFG3, AW8623X_PLAYCFG3_BRK_EN | PBL_FIELD_PREP(AW8623X_PLAYCFG3_PLAY_MODE_MASK,
                                                                AW8623X_PLAYCFG3_PLAY_MODE_CONT)},
    {AW8623X_VBATCTRL, AW8623X_VBATCTRL_VBAT_MODE_HW},
  };

  for (size_t i = 0; i < ARRAY_LENGTH(config); i++) {
    ret = prv_write_register(dev, config[i][0], config[i][1]);
    if (ret != 0) {
      PBL_LOG_ERR("Could not write register 0x%02x (%d)", config[i][0], ret);
      return ret;
    }
  }

  ret = prv_set_strength(dev, dev->state->strength);
  if (ret != 0) {
    PBL_LOG_ERR("Could not set the strength (%d)", ret);
  }

  return ret;
}

static int prv_start(const struct pbl_vibe *vibe, int8_t strength) {
  const struct pbl_aw8623x *dev = prv_get(vibe);
  int ret;

  dev->state->strength = (strength < 0) ? -strength : strength;

  ret = prv_set_strength(dev, dev->state->strength);
  if (ret != 0) {
    return ret;
  }

  // A failed stop may have left forced standby enabled
  ret = prv_set_standby(dev, false);
  if (ret != 0) {
    return ret;
  }

  ret = prv_write_register(dev, AW8623X_CONTCFG8, AW8623X_CONTCFG8_DRV1_TIME_MAX);
  if (ret != 0) {
    return ret;
  }

  ret = prv_write_register(dev, AW8623X_CONTCFG9, AW8623X_CONTCFG9_DRV2_TIME_MAX);
  if (ret != 0) {
    return ret;
  }

  return prv_write_register(dev, AW8623X_PLAYCFG4, AW8623X_PLAYCFG4_GO);
}

static int prv_stop(const struct pbl_vibe *vibe) {
  const struct pbl_aw8623x *dev = prv_get(vibe);
  int ret;

  ret = prv_write_register(dev, AW8623X_PLAYCFG4, AW8623X_PLAYCFG4_STOP);
  if (ret == 0) {
    ret = prv_wait_for_standby(dev);
  }
  if (ret == 0) {
    return 0;
  }

  PBL_LOG_WRN("Playback stop failed (%d), forcing standby", ret);

  ret = prv_set_standby(dev, true);
  if (ret == 0) {
    ret = prv_wait_for_standby(dev);
  }
  if (ret == 0) {
    return 0;
  }

  PBL_LOG_ERR("Could not confirm the playback stop (%d), resetting the chip", ret);

  return prv_init(vibe);
}

const struct pbl_vibe_ops pbl_aw8623x_ops = {
  .init = prv_init,
  .start = prv_start,
  .stop = prv_stop,
};
