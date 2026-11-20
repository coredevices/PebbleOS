/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/devicetree.h>
#include <pbl/drivers/regulator/sf32lb57_pmuc.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/misc.h>

#include <hardware/sf32lb57_pmuc.h>

#define READY_TIMEOUT_US 1000U

static const struct pbl_sf32lb57_pmuc_regulator *prv_regulator(const struct pbl_regulator *reg) {
  return PBL_CONTAINER_OF(reg, const struct pbl_sf32lb57_pmuc_regulator, reg);
}

static int prv_enable(const struct pbl_regulator *reg) {
  const uintptr_t peri_ldo = prv_regulator(reg)->base + SF32LB57_PMUC_PERI_LDO_OFFSET;

  pbl_modifyl(PMUC_PERI_LDO_LDO18_PD, PMUC_PERI_LDO_EN_LDO18, peri_ldo);

  return PBL_POLL_BUSY_US((pbl_readl(peri_ldo) & PMUC_PERI_LDO_LDO18_RDY) != 0U, 0U,
                          READY_TIMEOUT_US);
}

static int prv_disable(const struct pbl_regulator *reg) {
  const struct pbl_sf32lb57_pmuc_regulator *ldo = prv_regulator(reg);

  pbl_modifyl(PMUC_PERI_LDO_EN_LDO18 | PMUC_PERI_LDO_LDO18_PD,
              ldo->pull_down ? PMUC_PERI_LDO_LDO18_PD : 0U,
              ldo->base + SF32LB57_PMUC_PERI_LDO_OFFSET);

  return 0;
}

const struct pbl_regulator_ops pbl_sf32lb57_pmuc_regulator_ops = {
  .enable = prv_enable,
  .disable = prv_disable,
};

#include PBL_DT_INSTANCES(sifli_sf32lb57_pmuc_regulator)
