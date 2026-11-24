/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/gpio.h>
#include <pbl/drivers/i2c.h>
#include <pbl/drivers/vibe.h>
#include <pbl/drivers/vibe/drv2604.h>
#include <pbl/logging/logging.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>
#include <pbl/util/size.h>

#include <dt-bindings/input/ti-drv260x.h>

PBL_LOG_MODULE_DECLARE(driver_vibe, CONFIG_DRIVER_VIBE_LOG_LEVEL);

#define DRV2604_STATUS                      0x00
#define DRV2604_STATUS_OC_DETECT            PBL_BIT(0)
#define DRV2604_STATUS_OVER_TEMP            PBL_BIT(1)
#define DRV2604_MODE                        0x01
#define DRV2604_MODE_MODE_MASK              PBL_GENMASK(2, 0)
#define DRV2604_MODE_TRIGGER                PBL_FIELD_PREP(DRV2604_MODE_MODE_MASK, 0x0)
#define DRV2604_MODE_RTP                    PBL_FIELD_PREP(DRV2604_MODE_MODE_MASK, 0x5)
#define DRV2604_MODE_AUTOCAL                PBL_FIELD_PREP(DRV2604_MODE_MODE_MASK, 0x7)
#define DRV2604_MODE_STANDBY                PBL_BIT(6)
#define DRV2604_RTP_INPUT                   0x02
#define DRV2604_GO                          0x0C
#define DRV2604_GO_GO                       PBL_BIT(0)
#define DRV2604_RATED_VOLTAGE               0x16
#define DRV2604_OD_CLAMP                    0x17
#define DRV2604_A_CAL_COMP                  0x18
#define DRV2604_A_CAL_BEMF                  0x19
#define DRV2604_FBCTL                       0x1A
#define DRV2604_FBCTL_LRA                   PBL_BIT(7)
#define DRV2604_FBCTL_FB_BRAKE_FACTOR_MASK  PBL_GENMASK(6, 4)
#define DRV2604_FBCTL_FB_BRAKE_FACTOR(n)    PBL_FIELD_PREP(DRV2604_FBCTL_FB_BRAKE_FACTOR_MASK, (n))
#define DRV2604_FBCTL_LOOP_GAIN_MASK        PBL_GENMASK(3, 2)
#define DRV2604_FBCTL_LOOP_GAIN(n)          PBL_FIELD_PREP(DRV2604_FBCTL_LOOP_GAIN_MASK, (n))
#define DRV2604_FBCTL_BEMF_GAIN_MASK        PBL_GENMASK(1, 0)
#define DRV2604_FBCTL_BEMF_GAIN(n)          PBL_FIELD_PREP(DRV2604_FBCTL_BEMF_GAIN_MASK, (n))
#define DRV2604_CONTROL1                    0x1B
#define DRV2604_CONTROL1_STARTUP_BOOST      PBL_BIT(7)
#define DRV2604_CONTROL1_DRIVE_TIME_MASK    PBL_GENMASK(4, 0)
#define DRV2604_CONTROL1_DRIVE_TIME(n)      PBL_FIELD_PREP(DRV2604_CONTROL1_DRIVE_TIME_MASK, (n))
#define DRV2604_CONTROL2                    0x1C
#define DRV2604_CONTROL2_BIDIR_INPUT        PBL_BIT(7)
#define DRV2604_CONTROL2_BRAKE_STABILIZER   PBL_BIT(6)
#define DRV2604_CONTROL2_SAMPLE_TIME_MASK   PBL_GENMASK(5, 4)
#define DRV2604_CONTROL2_SAMPLE_TIME(n)     PBL_FIELD_PREP(DRV2604_CONTROL2_SAMPLE_TIME_MASK, (n))
#define DRV2604_CONTROL2_BLANKING_TIME_MASK PBL_GENMASK(3, 2)
#define DRV2604_CONTROL2_BLANKING_TIME(n)   PBL_FIELD_PREP(DRV2604_CONTROL2_BLANKING_TIME_MASK, (n))
#define DRV2604_CONTROL2_IDISS_TIME_MASK    PBL_GENMASK(1, 0)
#define DRV2604_CONTROL2_IDISS_TIME(n)      PBL_FIELD_PREP(DRV2604_CONTROL2_IDISS_TIME_MASK, (n))
#define DRV2604_CONTROL3                    0x1D
#define DRV2604_CONTROL3_NG_THRESH_DEFAULT  0x80
#define DRV2604_CONTROL3_LRA_OPEN_LOOP      PBL_BIT(0)
#define DRV2604_CONTROL4                    0x1E
#define DRV2604_CONTROL4_AUTO_CAL_TIME_MASK PBL_GENMASK(5, 4)
#define DRV2604_CONTROL4_AUTO_CAL_TIME(n)   PBL_FIELD_PREP(DRV2604_CONTROL4_AUTO_CAL_TIME_MASK, (n))

#define DRV2604_FBCTL_CFG \
  (DRV2604_FBCTL_FB_BRAKE_FACTOR(2) | DRV2604_FBCTL_LOOP_GAIN(2) | DRV2604_FBCTL_BEMF_GAIN(2))
#define DRV2604_CONTROL1_CFG \
  (DRV2604_CONTROL1_STARTUP_BOOST | DRV2604_CONTROL1_DRIVE_TIME(0x10 /* 2.1 ms */))
#define DRV2604_CONTROL2_CFG                                             \
  (DRV2604_CONTROL2_BIDIR_INPUT | DRV2604_CONTROL2_BRAKE_STABILIZER |    \
   DRV2604_CONTROL2_SAMPLE_TIME(3) | DRV2604_CONTROL2_BLANKING_TIME(1) | \
   DRV2604_CONTROL2_IDISS_TIME(1))

static const struct pbl_drv2604 *prv_get(const struct pbl_vibe *vibe) {
  return PBL_CONTAINER_OF(vibe, const struct pbl_drv2604, vibe);
}

static int prv_read_register(const struct pbl_drv2604 *dev, uint8_t reg, uint8_t *val) {
  int ret;

  ret = pbl_i2c_reg_read_byte(&dev->i2c, reg, val);

  return ret;
}

static int prv_write_register(const struct pbl_drv2604 *dev, uint8_t reg, uint8_t val) {
  const uint8_t buf[2] = {reg, val};
  int ret;

  ret = pbl_i2c_write(&dev->i2c, buf, sizeof(buf));

  return ret;
}

static int prv_write_registers(const struct pbl_drv2604 *dev, const uint8_t (*regs)[2],
                               size_t count) {
  int ret;

  for (size_t i = 0; i < count; i++) {
    ret = prv_write_register(dev, regs[i][0], regs[i][1]);
    if (ret != 0) {
      PBL_LOG_ERR("Could not write register 0x%02x (%d)", regs[i][0], ret);
      return ret;
    }
  }

  return 0;
}

static bool prv_is_lra(const struct pbl_drv2604 *dev) {
  return dev->mode != DRV260X_ERM_MODE;
}

static uint8_t prv_fbctl(const struct pbl_drv2604 *dev) {
  return (prv_is_lra(dev) ? DRV2604_FBCTL_LRA : 0U) | DRV2604_FBCTL_CFG;
}

static uint8_t prv_control3(const struct pbl_drv2604 *dev) {
  return DRV2604_CONTROL3_NG_THRESH_DEFAULT |
         ((prv_is_lra(dev) && dev->lra_open_loop) ? DRV2604_CONTROL3_LRA_OPEN_LOOP : 0U);
}

static int prv_init(const struct pbl_vibe *vibe) {
  const struct pbl_drv2604 *dev = prv_get(vibe);
  const uint8_t config[][2] = {
    {DRV2604_MODE, DRV2604_MODE_TRIGGER},
    {DRV2604_FBCTL, prv_fbctl(dev)},
    {DRV2604_RATED_VOLTAGE, dev->rated_voltage},
    {DRV2604_OD_CLAMP, dev->od_clamp},
    {DRV2604_A_CAL_COMP, 0x0D},
    {DRV2604_A_CAL_BEMF, 0x80},
    {DRV2604_CONTROL1, DRV2604_CONTROL1_CFG},
    {DRV2604_CONTROL2, DRV2604_CONTROL2_CFG},
    {DRV2604_CONTROL3, prv_control3(dev)},
    {DRV2604_MODE, DRV2604_MODE_STANDBY | DRV2604_MODE_TRIGGER},
  };
  uint8_t status;
  int ret;

  ret = pbl_gpio_configure(&dev->enable, PBL_GPIO_OUTPUT_ACTIVE);
  if (ret != 0) {
    return ret;
  }

  ret = prv_read_register(dev, DRV2604_STATUS, &status);
  if (ret != 0) {
    PBL_LOG_ERR("Could not read the status (%d)", ret);
    (void)pbl_gpio_set(&dev->enable, false);
    return -ENODEV;
  }

  ret = prv_write_registers(dev, config, PBL_ARRAY_SIZE(config));

  // Disabling the chip keeps its registers
  (void)pbl_gpio_set(&dev->enable, false);

  return ret;
}

// The output stops by itself on overcurrent or overtemperature: report it rather than vibrate
// nothing
static int prv_check_faults(const struct pbl_drv2604 *dev) {
  uint8_t status;
  int ret;

  ret = prv_read_register(dev, DRV2604_STATUS, &status);
  if (ret != 0) {
    return ret;
  }

  if ((status & (DRV2604_STATUS_OC_DETECT | DRV2604_STATUS_OVER_TEMP)) != 0U) {
    PBL_LOG_ERR("Output fault (status 0x%02x)", status);
    return -EIO;
  }

  return 0;
}

// RTP input is signed (BIDIR_INPUT), so negative strengths brake
static int prv_start(const struct pbl_vibe *vibe, int8_t strength) {
  const struct pbl_drv2604 *dev = prv_get(vibe);
  int ret;

  ret = pbl_gpio_set(&dev->enable, true);
  if (ret != 0) {
    return ret;
  }

  ret = prv_write_register(dev, DRV2604_MODE, DRV2604_MODE_RTP);
  if (ret != 0) {
    return ret;
  }

  ret = prv_write_register(dev, DRV2604_RTP_INPUT, (uint8_t)(strength * 0x7F / 100));
  if (ret == 0) {
    ret = prv_check_faults(dev);
  }

  if (ret != 0) {
    (void)prv_write_register(dev, DRV2604_MODE, DRV2604_MODE_STANDBY | DRV2604_MODE_RTP);
    (void)pbl_gpio_set(&dev->enable, false);
  }

  return ret;
}

static int prv_stop(const struct pbl_vibe *vibe) {
  const struct pbl_drv2604 *dev = prv_get(vibe);
  int ret;

  // Standby too, in case the enable line is not connected
  ret = prv_write_register(dev, DRV2604_MODE, DRV2604_MODE_STANDBY | DRV2604_MODE_RTP);

  (void)pbl_gpio_set(&dev->enable, false);

  return ret;
}

// The result stays in the chip, so there is nothing to persist. Calibration tracks the LRA's
// resonance, which an open-loop drive does not use.
static int prv_calibrate(const struct pbl_vibe *vibe, uint8_t *cali) {
  const struct pbl_drv2604 *dev = prv_get(vibe);
  const uint8_t autocal[][2] = {
    {DRV2604_MODE, DRV2604_MODE_AUTOCAL},
    {DRV2604_FBCTL,
     DRV2604_FBCTL_LRA | DRV2604_FBCTL_FB_BRAKE_FACTOR(2) | DRV2604_FBCTL_LOOP_GAIN(2)},
    {DRV2604_RATED_VOLTAGE, dev->rated_voltage},
    {DRV2604_OD_CLAMP, dev->od_clamp},
    {DRV2604_CONTROL1, DRV2604_CONTROL1_CFG},
    {DRV2604_CONTROL2, DRV2604_CONTROL2_CFG},
    {DRV2604_CONTROL4, DRV2604_CONTROL4_AUTO_CAL_TIME(3)},
    {DRV2604_GO, DRV2604_GO_GO},
  };

  if (!prv_is_lra(dev) || dev->lra_open_loop) {
    return -ENOTSUP;
  }

  return prv_write_registers(dev, autocal, PBL_ARRAY_SIZE(autocal));
}

const struct pbl_vibe_ops pbl_drv2604_ops = {
  .init = prv_init,
  .start = prv_start,
  .stop = prv_stop,
  .calibrate = prv_calibrate,
};

#include PBL_DT_INSTANCES(ti_drv2604)
