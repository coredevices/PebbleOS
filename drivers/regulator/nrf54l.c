/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/regulator.h>
#include <pbl/kernel/mmio.h>
#include <pbl/logging/logging.h>
#include <pbl/util/misc.h>

#include <dt-bindings/regulator/nordic,nrf54l15-regulators.h>
#include <hardware/nrf54l_oscillators.h>

PBL_LOG_MODULE_DEFINE(driver_regulator_nrf54l, CONFIG_DRIVER_REGULATOR_LOG_LEVEL);

struct pbl_regulator_nrf54l {
  struct pbl_regulator reg;
  uintptr_t base;
  uint8_t mode;
};

static const struct pbl_regulator_nrf54l *prv_vreg(const struct pbl_regulator *reg) {
  return PBL_CONTAINER_OF(reg, const struct pbl_regulator_nrf54l, reg);
}

// The DC/DC converter only runs with its inductor, which the SoC detects while VREGMAIN is on its
// LDO. Across a reset other than power-on, the converter keeps running.
static int prv_init(const struct pbl_regulator *reg) {
  const struct pbl_regulator_nrf54l *vreg = prv_vreg(reg);
  const uintptr_t dcdcen = vreg->base + NRF54L_REGULATORS_VREGMAIN_DCDCEN_OFFSET;
  const bool running = (pbl_readl(dcdcen) & REGULATORS_VREGMAIN_DCDCEN_EN) != 0U;

  if (vreg->mode != NRF54L15_REGULATOR_MODE_DCDC) {
    pbl_writel(0U, dcdcen);
    return 0;
  }

  if (!running && (pbl_readl(vreg->base + NRF54L_REGULATORS_VREGMAIN_INDUCTORDET_OFFSET) &
                   REGULATORS_VREGMAIN_INDUCTORDET_DETECTED) == 0U) {
    PBL_LOG_WRN("No DC/DC inductor, VREGMAIN stays on its LDO");
    return 0;
  }

  pbl_writel(REGULATORS_VREGMAIN_DCDCEN_EN, dcdcen);

  return 0;
}

static int prv_enable(const struct pbl_regulator *reg) {
  return 0;
}

static int prv_disable(const struct pbl_regulator *reg) {
  return -ENOTSUP;
}

static const struct pbl_regulator_ops s_ops = {
  .init = prv_init,
  .enable = prv_enable,
  .disable = prv_disable,
};

#define PBL_REGULATOR_NRF54L_DEFINE(sym, _name, _base, _mode, _always_on)                    \
  PBL_REGULATOR_STATE_DEFINE(sym);                                                           \
  PBL_DEVICE_INSTANCE(sym, struct pbl_regulator_nrf54l, struct pbl_regulator, reg) {         \
    .reg = PBL_REGULATOR_INIT(sym, _name, nullptr, &s_ops, 0U, _always_on), .base = (_base), \
    .mode = (_mode),                                                                         \
  }

#include PBL_DT_INSTANCES(nordic_nrf54l15_oscillators)
