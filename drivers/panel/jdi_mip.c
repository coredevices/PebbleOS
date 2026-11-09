/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
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

  if (jdi->state->powered) {
    ret = pbl_regulator_disable(jdi->vddp);
    if (ret != 0) {
      return ret;
    }

    pbl_busy_wait_us(POWER_SEQ_DELAY_US);

    ret = pbl_regulator_disable(jdi->vlcd);
    if (ret != 0) {
      (void)pbl_regulator_enable(jdi->vddp);
      return ret;
    }

    jdi->state->powered = false;
  }

  jdi->state->off_ms = pbl_uptime_ms();

  return 0;
}

int pbl_panel_jdi_mip_init(const struct pbl_device *dev) {
  const struct pbl_panel_jdi_mip *jdi =
      container_of(dev, const struct pbl_panel_jdi_mip, panel.dev);

  if (pbl_device_init(&jdi->vddp->dev) != 0 || pbl_device_init(&jdi->vlcd->dev) != 0 ||
      pbl_device_init(&jdi->vcom.ctrl->dev) != 0) {
    return -ENODEV;
  }

  jdi->state->off_ms = pbl_uptime_ms();

  return 0;
}

static int prv_prepare(const struct pbl_panel *panel) {
  const struct pbl_panel_jdi_mip *jdi = prv_jdi(panel);
  uint64_t off = pbl_uptime_ms() - jdi->state->off_ms;
  int ret;

  if (jdi->state->powered) {
    return 0;
  }

  if (off < OFF_TIME_MS) {
    pbl_busy_wait_us((OFF_TIME_MS - (uint32_t)off) * 1000U);
  }

  ret = pbl_regulator_enable(jdi->vlcd);
  if (ret != 0) {
    return ret;
  }

  pbl_busy_wait_us(POWER_SEQ_DELAY_US);

  ret = pbl_regulator_enable(jdi->vddp);
  if (ret != 0) {
    (void)pbl_regulator_disable(jdi->vlcd);
    return ret;
  }

  jdi->state->powered = true;

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

#include PBL_DT_INSTANCES(jdi_lpm015m135a)
#include PBL_DT_INSTANCES(sharp_ls013b7dd02)
