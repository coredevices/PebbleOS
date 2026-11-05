/* SPDX-FileCopyrightText: 2025 Core Devices */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/als.h>
#include <pbl/drivers/ambient/opt3001.h>
#include <pbl/drivers/i2c.h>
#include <pbl/logging/logging.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>

PBL_LOG_MODULE_DEFINE(driver_ambient_opt3001, CONFIG_DRIVER_AMBIENT_LOG_LEVEL);

#define OPT3001_RESULT                 0x00
#define OPT3001_RESULT_EXPONENT_MASK   PBL_GENMASK(15, 12)
#define OPT3001_RESULT_MANTISSA_MASK   PBL_GENMASK(11, 0)
#define OPT3001_CONFIG                 0x01
#define OPT3001_CONFIG_RANGE_MASK      PBL_GENMASK(15, 12)
#define OPT3001_CONFIG_RANGE_AUTO      0xCU
#define OPT3001_CONFIG_CONVTIME_800MS  PBL_BIT(11)
#define OPT3001_CONFIG_MODE_MASK       PBL_GENMASK(10, 9)
#define OPT3001_CONFIG_MODE_SINGLESHOT 0x1U
#define OPT3001_CONFIG_MODE_CONTINUOUS 0x3U
#define OPT3001_MFGID                  0x7E
#define OPT3001_MFGID_VAL              0x5449 /* "TI" */
#define OPT3001_DEVID                  0x7F
#define OPT3001_DEVID_VAL              0x3001

static const struct pbl_opt3001 *prv_get(const struct pbl_als *als) {
  return container_of(als, const struct pbl_opt3001, als);
}

static int prv_read_register(const struct pbl_opt3001 *dev, uint8_t reg, uint16_t *val) {
  uint8_t buf[2];
  int ret;

  ret = pbl_i2c_burst_read(&dev->i2c, reg, buf, sizeof(buf));
  if (ret != 0) {
    return ret;
  }

  *val = (uint16_t)((buf[0] << 8) | buf[1]);

  return 0;
}

static int prv_write_register(const struct pbl_opt3001 *dev, uint8_t reg, uint16_t val) {
  const uint8_t buf[2] = {val >> 8, val & 0xFFU};
  int ret;

  ret = pbl_i2c_burst_write(&dev->i2c, reg, buf, sizeof(buf));

  return ret;
}

static int prv_configure(const struct pbl_opt3001 *dev, uint8_t mode) {
  return prv_write_register(dev, OPT3001_CONFIG,
                            PBL_FIELD_PREP(OPT3001_CONFIG_RANGE_MASK, OPT3001_CONFIG_RANGE_AUTO) |
                                PBL_FIELD_PREP(OPT3001_CONFIG_CONVTIME_800MS, 0) |
                                PBL_FIELD_PREP(OPT3001_CONFIG_MODE_MASK, mode));
}

static int prv_init(const struct pbl_als *als) {
  const struct pbl_opt3001 *dev = prv_get(als);
  uint16_t mfgid;
  uint16_t devid;
  int ret;

  ret = prv_read_register(dev, OPT3001_MFGID, &mfgid);
  if (ret != 0) {
    PBL_LOG_ERR("Could not read the manufacturer ID (%d)", ret);
    return -ENODEV;
  }

  ret = prv_read_register(dev, OPT3001_DEVID, &devid);
  if (ret != 0) {
    PBL_LOG_ERR("Could not read the device ID (%d)", ret);
    return -ENODEV;
  }

  if (mfgid != OPT3001_MFGID_VAL || devid != OPT3001_DEVID_VAL) {
    PBL_LOG_ERR("Unexpected manufacturer/device ID: 0x%04x/0x%04x", mfgid, devid);
    return -ENODEV;
  }

  if (dev->always_on) {
    ret = prv_configure(dev, OPT3001_CONFIG_MODE_CONTINUOUS);
    if (ret != 0) {
      PBL_LOG_ERR("Could not start continuous conversions (%d)", ret);
      return ret;
    }
  }

  return 0;
}

static int prv_read(const struct pbl_als *als, uint32_t *level) {
  const struct pbl_opt3001 *dev = prv_get(als);
  uint16_t result;
  int ret;

  if (!dev->always_on) {
    ret = prv_configure(dev, OPT3001_CONFIG_MODE_SINGLESHOT);
    if (ret != 0) {
      return ret;
    }
  }

  ret = prv_read_register(dev, OPT3001_RESULT, &result);
  if (ret != 0) {
    return ret;
  }

  *level = (uint32_t)PBL_FIELD_GET(OPT3001_RESULT_MANTISSA_MASK, result)
           << PBL_FIELD_GET(OPT3001_RESULT_EXPONENT_MASK, result);

  return 0;
}

const struct pbl_als_ops pbl_opt3001_ops = {
  .init = prv_init,
  .read = prv_read,
};
