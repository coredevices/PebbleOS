/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/touch/ft6236.h>
#include <pbl/kernel/delay.h>
#include <pbl/logging/logging.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>

PBL_LOG_MODULE_DECLARE(driver_touch, CONFIG_DRIVER_TOUCH_LOG_LEVEL);

#define FT6236_RESET_MS     5U
#define FT6236_POR_DELAY_MS 80U

#define FT6236_TD_STATUS_REG   0x02U
#define FT6236_TD_STATUS_MASK  PBL_GENMASK(3, 0)
#define FT6236_POS_H_MASK      PBL_GENMASK(3, 0)
#define FT6236_G_MODE_REG      0xA4U
#define FT6236_G_MODE_TRIGGER  0x01U
#define FT6236_PMODE_REG       0xA5U
#define FT6236_PMODE_HIBERNATE 0x03U
#define FT6236_CHIP_ID_REG     0xA3U
#define FT6236_FW_VERSION_REG  0xA6U

// TD_STATUS, then XH, XL, YH and YL of the first contact
#define FT6236_DATA_SIZE 5U

static inline const struct pbl_ft6236 *prv_from_touch(const struct pbl_touch *touch) {
  return PBL_CONTAINER_OF(touch, const struct pbl_ft6236, touch);
}

static int prv_read(const struct pbl_ft6236 *ft, uint8_t reg, uint8_t *buf, uint32_t len) {
  int ret;

  ret = pbl_i2c_write(&ft->i2c, &reg, sizeof(reg));
  if (ret != 0) {
    return ret;
  }

  return pbl_i2c_read(&ft->i2c, buf, len);
}

static int prv_write(const struct pbl_ft6236 *ft, uint8_t reg, uint8_t val) {
  const uint8_t buf[] = {reg, val};

  return pbl_i2c_write(&ft->i2c, buf, sizeof(buf));
}

static int prv_hw_reset(const struct pbl_ft6236 *ft) {
  int ret;

  ret = pbl_gpio_set(&ft->reset, true);
  if (ret != 0) {
    return ret;
  }

  pbl_sleep_ms(FT6236_RESET_MS);

  ret = pbl_gpio_set(&ft->reset, false);
  if (ret != 0) {
    return ret;
  }

  pbl_sleep_ms(FT6236_POR_DELAY_MS);

  return 0;
}

static int prv_wake(const struct pbl_ft6236 *ft) {
  int ret;

  ret = prv_hw_reset(ft);
  if (ret != 0) {
    return ret;
  }

  ret = prv_write(ft, FT6236_G_MODE_REG, FT6236_G_MODE_TRIGGER);
  if (ret != 0) {
    return ret;
  }

  return pbl_intc_enable(&ft->irq);
}

static void prv_work(struct pbl_work *work) {
  struct pbl_ft6236_state *state = PBL_CONTAINER_OF(work, struct pbl_ft6236_state, work);
  const struct pbl_ft6236 *ft = state->ft;
  uint8_t data[FT6236_DATA_SIZE];
  uint16_t x;
  uint16_t y;
  bool down;
  int ret;

  pbl_touch_lock(&ft->touch);

  if (!state->enabled) {
    pbl_touch_unlock(&ft->touch);
    return;
  }

  ret = prv_read(ft, FT6236_TD_STATUS_REG, data, sizeof(data));
  if (ret != 0) {
    PBL_LOG_ERR("Read failed (%d), resetting", ret);
    (void)pbl_intc_disable(&ft->irq);
    ret = prv_wake(ft);
    if (ret != 0) {
      PBL_LOG_ERR("Reset failed (%d)", ret);
    }
    pbl_touch_unlock(&ft->touch);
    pbl_touch_report(&ft->touch, false, 0U, 0U);
    return;
  }

  pbl_touch_unlock(&ft->touch);

  down = PBL_FIELD_GET(FT6236_TD_STATUS_MASK, data[0]) != 0U;
  x = (PBL_FIELD_GET(FT6236_POS_H_MASK, data[1]) << 8) | data[2];
  y = (PBL_FIELD_GET(FT6236_POS_H_MASK, data[3]) << 8) | data[4];

  pbl_touch_report(&ft->touch, down, x, y);
}

static void prv_irq_handler(void *arg) {
  const struct pbl_ft6236 *ft = arg;

  pbl_work_submit_boosted(&pbl_sys_workq, &ft->state->work);
}

static int prv_sleep(const struct pbl_ft6236 *ft) {
  int ret;

  ft->state->enabled = false;

  ret = pbl_intc_disable(&ft->irq);
  if (ret != 0) {
    return ret;
  }

  return prv_write(ft, FT6236_PMODE_REG, FT6236_PMODE_HIBERNATE);
}

static int prv_init(const struct pbl_touch *touch) {
  const struct pbl_ft6236 *ft = prv_from_touch(touch);
  struct pbl_ft6236_state *state = ft->state;
  uint8_t id[4];
  int ret;

  state->ft = ft;
  pbl_work_init(&state->work, prv_work);

  ret = pbl_device_init(&ft->reset.port->dev);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_gpio_configure(&ft->reset, PBL_GPIO_OUTPUT_ACTIVE);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_pinctrl_select_state(&ft->pinctrl, PBL_PINCTRL_STATE_DEFAULT);
  if (ret != 0 && ret != -ENOENT) {
    return ret;
  }

  ret = pbl_device_init(&ft->irq.intc->dev);
  if (ret != 0) {
    return ret;
  }

  ret = prv_hw_reset(ft);
  if (ret != 0) {
    return ret;
  }

  // Chip ID, then firmware version three registers on
  ret = prv_read(ft, FT6236_CHIP_ID_REG, id, sizeof(id));
  if (ret != 0) {
    return -ENODEV;
  }

  PBL_LOG_DBG("Chip ID 0x%02X, firmware 0x%02X", id[0],
              id[FT6236_FW_VERSION_REG - FT6236_CHIP_ID_REG]);

  ret = pbl_intc_request(&ft->irq, prv_irq_handler, (void *)ft);
  if (ret != 0) {
    return ret;
  }

  return prv_sleep(ft);
}

static int prv_enable(const struct pbl_touch *touch) {
  const struct pbl_ft6236 *ft = prv_from_touch(touch);
  int ret;

  ret = prv_wake(ft);
  if (ret != 0) {
    return ret;
  }

  ft->state->enabled = true;

  return 0;
}

static int prv_disable(const struct pbl_touch *touch) {
  return prv_sleep(prv_from_touch(touch));
}

const struct pbl_touch_ops pbl_ft6236_ops = {
  .init = prv_init,
  .enable = prv_enable,
  .disable = prv_disable,
};

#include PBL_DT_INSTANCES(focaltech_ft6236)
