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
#include <pbl/util/misc.h>

PBL_LOG_MODULE_DEFINE(driver_pressure_bmp390, CONFIG_DRIVER_PRESSURE_LOG_LEVEL);

#define BMP390_CHIP_ID              0x00
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

static int prv_read_status(const struct pbl_bmp390 *dev, uint8_t *status) {
  return pbl_i2c_reg_read_byte(&dev->i2c, BMP390_STATUS, status);
}

static int prv_init(const struct pbl_pressure *pressure) {
  const struct pbl_bmp390 *dev = prv_get(pressure);
  uint8_t raw[BMP390_CALIB_LEN];
  uint8_t val = 0U;
  int ret;

  ret = pbl_i2c_reg_read_byte(&dev->i2c, BMP390_CHIP_ID, &val);
  if (ret != 0 || val != BMP390_CHIP_ID_VAL) {
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

  pbl_bmp390_parse_calib(raw, dev->calib);

  // Reset leaves the IIR filter off and the sensor in sleep mode
  return pbl_i2c_reg_write_byte(&dev->i2c, BMP390_OSR,
                                PBL_FIELD_PREP(BMP390_OSR_P_MASK, OSR_P_LOG2) |
                                    PBL_FIELD_PREP(BMP390_OSR_T_MASK, OSR_T_LOG2));
}

static int prv_read(const struct pbl_pressure *pressure, uint32_t *pa, int32_t *millicelsius) {
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

  pbl_bmp390_compensate(
      dev->calib, (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16),
      (uint32_t)data[3] | ((uint32_t)data[4] << 8) | ((uint32_t)data[5] << 16), pa, millicelsius);

  return 0;
}

const struct pbl_pressure_ops pbl_bmp390_ops = {
  .init = prv_init,
  .read = prv_read,
};

#include PBL_DT_INSTANCES(bosch_bmp390)
