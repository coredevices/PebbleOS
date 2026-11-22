/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/regulator.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/misc.h>

#include <hardware/stm32h7_pwr.h>

// The detector reports VDD33USB ready within a few microseconds
#define USB33_TIMEOUT_US 1000U

struct pbl_regulator_stm32h7_pwr {
  struct pbl_regulator reg;
  uintptr_t base;
};

static const struct pbl_regulator_stm32h7_pwr *prv_pwr(const struct pbl_regulator *reg) {
  return PBL_CONTAINER_OF(reg, const struct pbl_regulator_stm32h7_pwr, reg);
}

// The USB transceivers run from VDD33USB, which the detector lets through once at level
static int prv_enable(const struct pbl_regulator *reg) {
  const uintptr_t cr3 = prv_pwr(reg)->base + STM32H7_PWR_CR3_OFFSET;

  pbl_modifyl(0U, PWR_CR3_USB33DEN, cr3);

  return PBL_POLL_BUSY_US((pbl_readl(cr3) & PWR_CR3_USB33RDY) != 0U, 1U, USB33_TIMEOUT_US);
}

static int prv_disable(const struct pbl_regulator *reg) {
  pbl_modifyl(PWR_CR3_USB33DEN, 0U, prv_pwr(reg)->base + STM32H7_PWR_CR3_OFFSET);

  return 0;
}

static const struct pbl_regulator_ops s_ops = {
  .enable = prv_enable,
  .disable = prv_disable,
};

#define PBL_REGULATOR_STM32H7_PWR_DEFINE(sym, _name, _base, _id, _always_on)                 \
  PBL_REGULATOR_STATE_DEFINE(sym);                                                           \
  PBL_DEVICE_INSTANCE(sym, struct pbl_regulator_stm32h7_pwr, struct pbl_regulator, reg) {    \
    .reg = PBL_REGULATOR_INIT(sym, _name, nullptr, &s_ops, 0U, _always_on), .base = (_base), \
  }

#include PBL_DT_INSTANCES(st_stm32h747_pwr)
