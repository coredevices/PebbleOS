/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/backlight.h>
#include <pbl/drivers/backlight/aw2016.h>
#include <pbl/drivers/i2c.h>
#include <pbl/logging/logging.h>
#include <pbl/util/misc.h>
#include <pbl/util/size.h>

PBL_LOG_MODULE_DEFINE(driver_backlight_aw2016, CONFIG_DRIVER_BACKLIGHT_LOG_LEVEL);

#define AW2016_REG_RSTR         0x00U
#define AW2016_REG_RSTR_CHIP_ID 0x09U
#define AW2016_REG_RSTR_RST     0x55U

#define AW2016_REG_GCR1            0x01U
#define AW2016_REG_GCR1_CHGDIS_DIS (1U << 1U)
#define AW2016_REG_GCR1_CHIPEN_EN  (1U << 0U)
#define AW2016_REG_GCR1_CHIPEN_DIS 0U

#define AW2016_REG_GCR2           0x04U
#define AW2016_REG_GCR2_IMAX_30MA 1U

#define AW2016_REG_LCTR            (0x30)
#define AW2016_REG_LCTR_EXP_LINEAR (1U << 3U)
#define AW2016_REG_LCTR_LE3_EN     (1U << 2U)
#define AW2016_REG_LCTR_LE2_EN     (1U << 1U)
#define AW2016_REG_LCTR_LE1_EN     (1U << 0U)

#define AW2016_REG_LCFG1         0x31U
#define AW2016_REG_LCFG1_CUR_MAX 0x0FU

#define AW2016_REG_LCFG2         0x32U
#define AW2016_REG_LCFG2_CUR_MAX 0x0FU

#define AW2016_REG_LCFG3         0x33U
#define AW2016_REG_LCFG3_CUR_MAX 0x0FU

#define AW2016_REG_PWM1 0x34U

#define AW2016_REG_PWM2 0x35U

#define AW2016_REG_PWM3 0x36U

static const struct pbl_aw2016 *prv_get(const struct pbl_backlight *bl) {
  return container_of(bl, const struct pbl_aw2016, backlight);
}

static int prv_read_register(const struct pbl_aw2016 *dev, uint8_t reg, uint8_t *val) {
  int ret;

  pbl_i2c_use(&dev->i2c);
  ret = pbl_i2c_read_register_block(&dev->i2c, reg, 1, val);
  pbl_i2c_release(&dev->i2c);

  return ret;
}

static int prv_write_register(const struct pbl_aw2016 *dev, uint8_t reg, uint8_t val) {
  int ret;

  pbl_i2c_use(&dev->i2c);
  ret = pbl_i2c_write_register_block(&dev->i2c, reg, 1, &val);
  pbl_i2c_release(&dev->i2c);

  return ret;
}

static int prv_write_registers(const struct pbl_aw2016 *dev, const uint8_t (*regs)[2],
                               size_t count) {
  int ret;

  for (size_t i = 0; i < count; i++) {
    ret = prv_write_register(dev, regs[i][0], regs[i][1]);
    if (ret != 0) {
      return ret;
    }
  }

  return 0;
}

static int prv_enable(const struct pbl_aw2016 *dev) {
  static const uint8_t s_config[][2] = {
    {AW2016_REG_GCR1, AW2016_REG_GCR1_CHGDIS_DIS | AW2016_REG_GCR1_CHIPEN_EN},
    {AW2016_REG_GCR2, AW2016_REG_GCR2_IMAX_30MA},
    {AW2016_REG_LCTR, AW2016_REG_LCTR_EXP_LINEAR | AW2016_REG_LCTR_LE3_EN | AW2016_REG_LCTR_LE2_EN |
                          AW2016_REG_LCTR_LE1_EN},
    {AW2016_REG_LCFG1, AW2016_REG_LCFG1_CUR_MAX},
    {AW2016_REG_LCFG2, AW2016_REG_LCFG2_CUR_MAX},
    {AW2016_REG_LCFG3, AW2016_REG_LCFG3_CUR_MAX},
  };

  return prv_write_registers(dev, s_config, ARRAY_LENGTH(s_config));
}

static int prv_write_color(const struct pbl_aw2016 *dev, uint32_t rgb_color, uint8_t brightness) {
  const uint8_t regs[][2] = {
    {AW2016_REG_PWM1, ((rgb_color >> 16) & 0xFFU) * brightness / PBL_BACKLIGHT_BRIGHTNESS_MAX},
    {AW2016_REG_PWM2, ((rgb_color >> 8) & 0xFFU) * brightness / PBL_BACKLIGHT_BRIGHTNESS_MAX},
    {AW2016_REG_PWM3, (rgb_color & 0xFFU) * brightness / PBL_BACKLIGHT_BRIGHTNESS_MAX},
  };

  return prv_write_registers(dev, regs, ARRAY_LENGTH(regs));
}

static int prv_init(const struct pbl_backlight *bl) {
  const struct pbl_aw2016 *dev = prv_get(bl);
  uint8_t id = 0U;
  int ret;

  ret = prv_read_register(dev, AW2016_REG_RSTR, &id);
  if (ret != 0 || id != AW2016_REG_RSTR_CHIP_ID) {
    PBL_LOG_ERR("Could not read the chip ID (%d, 0x%02x)", ret, id);
    return -ENODEV;
  }

  ret = prv_write_register(dev, AW2016_REG_RSTR, AW2016_REG_RSTR_RST);
  if (ret != 0) {
    return ret;
  }

  return prv_write_register(dev, AW2016_REG_GCR1, AW2016_REG_GCR1_CHGDIS_DIS);
}

static int prv_set_brightness(const struct pbl_backlight *bl, uint8_t brightness) {
  const struct pbl_aw2016 *dev = prv_get(bl);
  int ret;

  if (brightness == bl->state->brightness) {
    return 0;
  }

  if (brightness == 0U) {
    return prv_write_register(dev, AW2016_REG_GCR1,
                              AW2016_REG_GCR1_CHGDIS_DIS | AW2016_REG_GCR1_CHIPEN_DIS);
  }

  if (bl->state->brightness == 0U) {
    ret = prv_enable(dev);
    if (ret != 0) {
      return ret;
    }
  }

  return prv_write_color(dev, bl->state->color, brightness);
}

static int prv_refresh(const struct pbl_backlight *bl) {
  const struct pbl_aw2016 *dev = prv_get(bl);
  int ret;

  ret = prv_enable(dev);
  if (ret != 0) {
    return ret;
  }

  return prv_write_color(dev, bl->state->color, bl->state->brightness);
}

static int prv_set_color(const struct pbl_backlight *bl, uint32_t rgb_color) {
  return prv_write_color(prv_get(bl), rgb_color, bl->state->brightness);
}

const struct pbl_backlight_ops pbl_aw2016_ops = {
  .init = prv_init,
  .set_brightness = prv_set_brightness,
  .refresh = prv_refresh,
  .set_color = prv_set_color,
};
