/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/i2c.h>
#include <pbl/drivers/led.h>
#include <pbl/drivers/led/aw2016.h>
#include <pbl/kernel/delay.h>
#include <pbl/logging/logging.h>
#include <pbl/util/misc.h>
#include <pbl/util/size.h>

PBL_LOG_MODULE_DECLARE(driver_led, CONFIG_DRIVER_LED_LOG_LEVEL);

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

// Not in datasheet: measured empirically
#define AW2016_STARTUP_MS 1U

static const struct pbl_aw2016 *prv_get(const struct pbl_led *led) {
  return PBL_CONTAINER_OF(led, const struct pbl_aw2016, led);
}

static int prv_read_register(const struct pbl_aw2016 *dev, uint8_t reg, uint8_t *val) {
  int ret;

  ret = pbl_i2c_reg_read_byte(&dev->i2c, reg, val);

  return ret;
}

static int prv_write_register(const struct pbl_aw2016 *dev, uint8_t reg, uint8_t val) {
  int ret;

  ret = pbl_i2c_reg_write_byte(&dev->i2c, reg, val);
  if (ret != 0) {
    PBL_LOG_ERR("Write of 0x%02x to 0x%02x failed (%d)", val, reg, ret);
  }

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

static const uint8_t s_config[][2] = {
  {AW2016_REG_GCR1, AW2016_REG_GCR1_CHGDIS_DIS | AW2016_REG_GCR1_CHIPEN_EN},
  {AW2016_REG_GCR2, AW2016_REG_GCR2_IMAX_30MA},
  {AW2016_REG_LCTR, AW2016_REG_LCTR_EXP_LINEAR | AW2016_REG_LCTR_LE3_EN | AW2016_REG_LCTR_LE2_EN |
                        AW2016_REG_LCTR_LE1_EN},
  {AW2016_REG_LCFG1, AW2016_REG_LCFG1_CUR_MAX},
  {AW2016_REG_LCFG2, AW2016_REG_LCFG2_CUR_MAX},
  {AW2016_REG_LCFG3, AW2016_REG_LCFG3_CUR_MAX},
};

static void prv_color_regs(uint8_t regs[3][2], uint32_t rgb_color, uint8_t brightness) {
  regs[0][0] = AW2016_REG_PWM1;
  regs[0][1] = ((rgb_color >> 16) & 0xFFU) * brightness / PBL_LED_BRIGHTNESS_MAX;
  regs[1][0] = AW2016_REG_PWM2;
  regs[1][1] = ((rgb_color >> 8) & 0xFFU) * brightness / PBL_LED_BRIGHTNESS_MAX;
  regs[2][0] = AW2016_REG_PWM3;
  regs[2][1] = (rgb_color & 0xFFU) * brightness / PBL_LED_BRIGHTNESS_MAX;
}

static int prv_enable(const struct pbl_aw2016 *dev) {
  int ret;

  ret = prv_write_register(dev, s_config[0][0], s_config[0][1]);
  if (ret != 0) {
    return ret;
  }

  // Only GCR1 is writable until the oscillator runs, and writes are still acknowledged
  pbl_sleep_ms(AW2016_STARTUP_MS);

  return prv_write_registers(dev, &s_config[1], PBL_ARRAY_SIZE(s_config) - 1U);
}

static int prv_write_color(const struct pbl_aw2016 *dev, uint32_t rgb_color, uint8_t brightness) {
  uint8_t regs[3][2];

  prv_color_regs(regs, rgb_color, brightness);

  return prv_write_registers(dev, regs, PBL_ARRAY_SIZE(regs));
}

// Tells a chip that lost its configuration apart from a fault downstream of it
static void prv_verify_registers(const struct pbl_aw2016 *dev, const uint8_t (*regs)[2],
                                 size_t count) {
  for (size_t i = 0; i < count; i++) {
    uint8_t val = 0U;
    int ret;

    ret = prv_read_register(dev, regs[i][0], &val);
    if (ret != 0) {
      PBL_LOG_ERR("Read of 0x%02x failed (%d)", regs[i][0], ret);
    } else if (val != regs[i][1]) {
      PBL_LOG_WRN("0x%02x reads 0x%02x, wrote 0x%02x", regs[i][0], val, regs[i][1]);
    }
  }
}

static void prv_verify(const struct pbl_aw2016 *dev, uint32_t rgb_color, uint8_t brightness) {
  uint8_t regs[3][2];

  prv_color_regs(regs, rgb_color, brightness);

  prv_verify_registers(dev, s_config, PBL_ARRAY_SIZE(s_config));
  prv_verify_registers(dev, regs, PBL_ARRAY_SIZE(regs));
}

int pbl_aw2016_init(const struct pbl_device *d) {
  const struct pbl_aw2016 *dev = prv_get(PBL_CONTAINER_OF(d, const struct pbl_led, dev));
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

static int prv_set(const struct pbl_led *led, uint8_t brightness, uint32_t rgb_color) {
  const struct pbl_aw2016 *dev = prv_get(led);
  int ret;

  if (brightness == 0U) {
    ret = prv_write_register(dev, AW2016_REG_GCR1,
                             AW2016_REG_GCR1_CHGDIS_DIS | AW2016_REG_GCR1_CHIPEN_DIS);
    if (ret == 0) {
      dev->state->enabled = false;
    }
    return ret;
  }

  if (dev->state->enabled) {
    return prv_write_color(dev, rgb_color, brightness);
  }

  ret = prv_enable(dev);
  if (ret != 0) {
    return ret;
  }
  dev->state->enabled = true;

  ret = prv_write_color(dev, rgb_color, brightness);
  if (ret == 0) {
    prv_verify(dev, rgb_color, brightness);
  }

  return ret;
}

static int prv_refresh(const struct pbl_led *led, uint8_t brightness, uint32_t rgb_color) {
  prv_get(led)->state->enabled = false;

  return prv_set(led, brightness, rgb_color);
}

const struct pbl_led_ops pbl_aw2016_ops = {
  .set = prv_set,
  .refresh = prv_refresh,
};

#include PBL_DT_INSTANCES(awinic_aw2016)
