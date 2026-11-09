/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/regulator/fixed.h>
#include <pbl/kernel/delay.h>
#include <pbl/util/misc.h>

static const struct pbl_regulator_fixed *prv_fixed(const struct pbl_regulator *reg) {
  return container_of(reg, const struct pbl_regulator_fixed, reg);
}

static int prv_set(const struct pbl_regulator_fixed *fixed, bool on) {
  return pbl_gpio_set_raw(&fixed->enable, fixed->active_high == on);
}

static int prv_init(const struct pbl_regulator *reg) {
  const struct pbl_regulator_fixed *fixed = prv_fixed(reg);
  int ret;

  if (pbl_device_init(&fixed->enable.port->dev) != 0) {
    return -ENODEV;
  }

  ret = pbl_gpio_configure(&fixed->enable,
                           fixed->active_high ? PBL_GPIO_OUTPUT_LOW : PBL_GPIO_OUTPUT_HIGH);
  if (ret != 0) {
    return ret;
  }

  return 0;
}

static int prv_enable(const struct pbl_regulator *reg) {
  const struct pbl_regulator_fixed *fixed = prv_fixed(reg);
  int ret;

  ret = prv_set(fixed, true);
  if (ret != 0) {
    return ret;
  }

  if (fixed->startup_delay_us != 0U) {
    pbl_busy_wait_us(fixed->startup_delay_us);
  }

  return 0;
}

static int prv_disable(const struct pbl_regulator *reg) {
  return prv_set(prv_fixed(reg), false);
}

const struct pbl_regulator_ops pbl_regulator_fixed_ops = {
  .init = prv_init,
  .enable = prv_enable,
  .disable = prv_disable,
};

#include PBL_DT_INSTANCES(regulator_fixed)
