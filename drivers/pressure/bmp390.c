/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/i2c.h>
#include <pbl/drivers/pressure.h>
#include <pbl/drivers/pressure/bmp390.h>
#include <pbl/kernel/delay.h>
#include <pbl/logging/logging.h>
#include <pbl/util/bits.h>
#include <pbl/util/byteorder.h>
#include <pbl/util/misc.h>

PBL_LOG_MODULE_DEFINE(driver_pressure_bmp390, CONFIG_DRIVER_PRESSURE_LOG_LEVEL);

#define BMP390_CHIP_ID              0x00
#define BMP380_CHIP_ID_VAL          0x50
#define BMP390_CHIP_ID_VAL          0x60
#define BMP390_ERR_REG              0x02
#define BMP390_STATUS               0x03
#define BMP390_STATUS_CMD_RDY       PBL_BIT(4)
#define BMP390_STATUS_DRDY_PRESS    PBL_BIT(5)
#define BMP390_STATUS_DRDY_TEMP     PBL_BIT(6)
#define BMP390_DATA                 0x04
#define BMP390_PWR_CTRL             0x1B
#define BMP390_PWR_CTRL_PRESS_EN    PBL_BIT(0)
#define BMP390_PWR_CTRL_TEMP_EN     PBL_BIT(1)
#define BMP390_PWR_CTRL_MODE_MASK   PBL_GENMASK(5, 4)
#define BMP390_PWR_CTRL_MODE_FORCED 0x1U
#define BMP390_OSR                  0x1C
#define BMP390_OSR_P_MASK           PBL_GENMASK(2, 0)
#define BMP390_OSR_T_MASK           PBL_GENMASK(5, 3)
#define BMP390_CONFIG               0x1F
#define BMP390_CALIB                0x31
#define BMP390_CALIB_LEN            21U
#define BMP390_CMD                  0x7E
#define BMP390_CMD_SOFTRESET        0xB6

// 8x pressure, 1x temperature: the "high resolution" setting of the datasheet
#define OSR_P_LOG2 3U
#define OSR_T_LOG2 0U

// Datasheet 3.9.2: 234 + (392 + 2^osr_p * 2020) + (163 + 2^osr_t * 2020) us
#define CONVERSION_US         (234U + 392U + (2020U << OSR_P_LOG2) + 163U + (2020U << OSR_T_LOG2))
#define CONVERSION_TIMEOUT_MS 50U

#define STARTUP_MS     2U
#define CMD_TIMEOUT_MS 10U

static const struct pbl_bmp390 *prv_get(const struct pbl_pressure *pressure) {
  return PBL_CONTAINER_OF(pressure, const struct pbl_bmp390, pressure);
}

static void prv_parse_calib(const uint8_t raw[BMP390_CALIB_LEN], struct pbl_bmp390_calib *calib) {
  calib->par_t1 = pbl_get_le16(&raw[0]);
  calib->par_t2 = pbl_get_le16(&raw[2]);
  calib->par_t3 = (int8_t)raw[4];
  calib->par_p1 = (int16_t)pbl_get_le16(&raw[5]);
  calib->par_p2 = (int16_t)pbl_get_le16(&raw[7]);
  calib->par_p3 = (int8_t)raw[9];
  calib->par_p4 = (int8_t)raw[10];
  calib->par_p5 = pbl_get_le16(&raw[11]);
  calib->par_p6 = pbl_get_le16(&raw[13]);
  calib->par_p7 = (int8_t)raw[15];
  calib->par_p8 = (int8_t)raw[16];
  calib->par_p9 = (int16_t)pbl_get_le16(&raw[17]);
  calib->par_p10 = (int8_t)raw[19];
  calib->par_p11 = (int8_t)raw[20];
}

// Datasheet compensation (BMP390 datasheet, section 8.4 to 8.6) in single precision, which the
// FPU runs. The temperature only serves the compensation. The terms stay far below the 2^24 integer
// limit where it matters (raw_temp - t1).
static uint32_t prv_compensate(const struct pbl_bmp390_calib *c, uint32_t raw_press,
                               uint32_t raw_temp) {
  const float t2 = c->par_t2 / 1073741824.0f;
  const float t3 = c->par_t3 / 281474976710656.0f;
  const float p1 = (c->par_p1 - 16384) / 1048576.0f;
  const float p2 = (c->par_p2 - 16384) / 536870912.0f;
  const float p3 = c->par_p3 / 4294967296.0f;
  const float p4 = c->par_p4 / 137438953472.0f;
  const float p5 = c->par_p5 * 8.0f;
  const float p6 = c->par_p6 / 64.0f;
  const float p7 = c->par_p7 / 256.0f;
  const float p8 = c->par_p8 / 32768.0f;
  const float p9 = c->par_p9 / 281474976710656.0f;
  const float p10 = c->par_p10 / 281474976710656.0f;
  const float p11 = c->par_p11 / 36893488147419103232.0f;
  const float d = (float)((int32_t)raw_temp - (int32_t)(c->par_t1 * 256U));
  const float t = d * t2 + d * d * t3;
  const float up = (float)raw_press;
  const float offset = p5 + t * (p6 + t * (p7 + t * p8));
  const float sensitivity = p1 + t * (p2 + t * (p3 + t * p4));
  const float p = offset + up * sensitivity + up * up * (p9 + t * p10) + up * up * up * p11;

  return p > 0.0f ? (uint32_t)(p + 0.5f) : 0U;
}

static int prv_read_status(const struct pbl_bmp390 *dev, uint8_t *status) {
  return pbl_i2c_reg_read_byte(&dev->i2c, BMP390_STATUS, status);
}

static int prv_init(const struct pbl_pressure *pressure) {
  const struct pbl_bmp390 *dev = prv_get(pressure);
  uint8_t raw[BMP390_CALIB_LEN];
  uint8_t val = 0U;
  int ret;

  ret = pbl_i2c_reg_read_byte(&dev->i2c, BMP390_CHIP_ID, &val);
  if (ret != 0 || (val != BMP380_CHIP_ID_VAL && val != BMP390_CHIP_ID_VAL)) {
    PBL_LOG_ERR("Unexpected chip ID 0x%02x (%d)", val, ret);
    return -ENODEV;
  }

  ret = pbl_i2c_reg_write_byte(&dev->i2c, BMP390_CMD, BMP390_CMD_SOFTRESET);
  if (ret != 0) {
    return ret;
  }

  pbl_sleep_ms(STARTUP_MS);

  ret = PBL_POLL_SLEEP_MS(prv_read_status(dev, &val) == 0 && (val & BMP390_STATUS_CMD_RDY) != 0U, 1,
                          CMD_TIMEOUT_MS);
  if (ret != 0) {
    PBL_LOG_ERR("Not ready after reset");
    return ret;
  }

  ret = pbl_i2c_burst_read(&dev->i2c, BMP390_CALIB, raw, sizeof(raw));
  if (ret != 0) {
    return ret;
  }

  prv_parse_calib(raw, dev->calib);

  // Reset leaves the IIR filter off and the sensor in sleep mode
  return pbl_i2c_reg_write_byte(&dev->i2c, BMP390_OSR,
                                PBL_FIELD_PREP(BMP390_OSR_P_MASK, OSR_P_LOG2) |
                                    PBL_FIELD_PREP(BMP390_OSR_T_MASK, OSR_T_LOG2));
}

static int prv_read(const struct pbl_pressure *pressure, uint32_t *pa) {
  const struct pbl_bmp390 *dev = prv_get(pressure);
  const uint8_t drdy = BMP390_STATUS_DRDY_PRESS | BMP390_STATUS_DRDY_TEMP;
  uint8_t data[6];
  uint8_t status = 0U;
  uint8_t err;
  int ret;

  // Reading the data clears ready bits a failed read may have left set
  ret = pbl_i2c_burst_read(&dev->i2c, BMP390_DATA, data, sizeof(data));
  if (ret != 0) {
    return ret;
  }

  // Forced mode runs one conversion, then the sensor goes back to sleep
  ret = pbl_i2c_reg_write_byte(
      &dev->i2c, BMP390_PWR_CTRL,
      BMP390_PWR_CTRL_PRESS_EN | BMP390_PWR_CTRL_TEMP_EN |
          PBL_FIELD_PREP(BMP390_PWR_CTRL_MODE_MASK, BMP390_PWR_CTRL_MODE_FORCED));
  if (ret != 0) {
    return ret;
  }

  pbl_sleep_ms((CONVERSION_US + 999U) / 1000U);

  // Reading the data registers clears the ready bits
  ret = PBL_POLL_SLEEP_MS(prv_read_status(dev, &status) == 0 && (status & drdy) == drdy, 1,
                          CONVERSION_TIMEOUT_MS);
  if (ret != 0) {
    if (pbl_i2c_reg_read_byte(&dev->i2c, BMP390_ERR_REG, &err) == 0) {
      PBL_LOG_ERR("Conversion timed out, status 0x%02x err 0x%02x", status, err);
    }
    return ret;
  }

  ret = pbl_i2c_burst_read(&dev->i2c, BMP390_DATA, data, sizeof(data));
  if (ret != 0) {
    return ret;
  }

  *pa = prv_compensate(dev->calib, pbl_get_le24(&data[0]), pbl_get_le24(&data[3]));

  return 0;
}

const struct pbl_pressure_ops pbl_bmp390_ops = {
  .init = prv_init,
  .read = prv_read,
};

#include PBL_DT_INSTANCES(bosch_bmp380)
