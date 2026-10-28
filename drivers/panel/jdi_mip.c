/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/panel/jdi_mip.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/time.h>
#include <pbl/util/misc.h>

#define POWER_SEQ_DELAY_US 11000U
#define OFF_TIME_MS        500U

static const struct pbl_panel_jdi_mip *prv_jdi(const struct pbl_panel *panel) {
  return container_of(panel, const struct pbl_panel_jdi_mip, panel);
}

static int prv_power_off(const struct pbl_panel_jdi_mip *jdi) {
  int ret;

  ret = pbl_gpio_set(&jdi->vddp, false);
  if (ret != 0) {
    return ret;
  }

  pbl_busy_wait_us(POWER_SEQ_DELAY_US);

  ret = pbl_gpio_set(&jdi->vlcd, false);
  if (ret != 0) {
    return ret;
  }

  jdi->state->off_ms = pbl_uptime_ms();

  return 0;
}

int pbl_panel_jdi_mip_init(const struct pbl_device *dev) {
  const struct pbl_panel_jdi_mip *jdi =
      container_of(dev, const struct pbl_panel_jdi_mip, panel.dev);
  int ret;

  if (pbl_device_init(&jdi->vddp.port->dev) != 0 || pbl_device_init(&jdi->vlcd.port->dev) != 0 ||
      pbl_device_init(&jdi->vcom.ctrl->dev) != 0) {
    return -ENODEV;
  }

  ret = pbl_gpio_configure(&jdi->vddp, PBL_GPIO_OUTPUT_INACTIVE);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_gpio_configure(&jdi->vlcd, PBL_GPIO_OUTPUT_INACTIVE);
  if (ret != 0) {
    return ret;
  }

  jdi->state->off_ms = pbl_uptime_ms();

  return 0;
}

static int prv_prepare(const struct pbl_panel *panel) {
  const struct pbl_panel_jdi_mip *jdi = prv_jdi(panel);
  uint64_t off = pbl_uptime_ms() - jdi->state->off_ms;
  int ret;

  if (off < OFF_TIME_MS) {
    pbl_busy_wait_us((OFF_TIME_MS - (uint32_t)off) * 1000U);
  }

  ret = pbl_gpio_set(&jdi->vlcd, true);
  if (ret != 0) {
    return ret;
  }

  pbl_busy_wait_us(POWER_SEQ_DELAY_US);

  ret = pbl_gpio_set(&jdi->vddp, true);
  if (ret != 0) {
    return ret;
  }

  pbl_busy_wait_us(POWER_SEQ_DELAY_US);

  return 0;
}

static int prv_enable(const struct pbl_panel *panel) {
  const struct pbl_panel_jdi_mip *jdi = prv_jdi(panel);
  struct pbl_pwm_state vcom;
  int ret;

  pbl_pwm_init_state(&jdi->vcom, &vcom);
  ret = pbl_pwm_set_relative_duty_cycle(&vcom, 1U, 2U);
  if (ret != 0) {
    return ret;
  }

  vcom.enabled = true;

  return pbl_pwm_apply(&jdi->vcom, &vcom);
}

static int prv_disable(const struct pbl_panel *panel) {
  return pbl_pwm_disable(&prv_jdi(panel)->vcom);
}

static int prv_unprepare(const struct pbl_panel *panel) {
  pbl_busy_wait_us(POWER_SEQ_DELAY_US);

  return prv_power_off(prv_jdi(panel));
}

const struct pbl_panel_ops pbl_panel_jdi_mip_ops = {
  .prepare = prv_prepare,
  .enable = prv_enable,
  .disable = prv_disable,
  .unprepare = prv_unprepare,
};
