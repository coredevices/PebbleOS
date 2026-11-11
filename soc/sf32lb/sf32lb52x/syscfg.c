/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "syscfg.h"

#include "revid.h"

#include <errno.h>
#include <stdint.h>

#include <pbl/drivers/nvmem/sf32lb52.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>

#include <devicetree/regs.h>
#include <hardware/sf32lb52_hpsys_cfg.h>
#include <hardware/sf32lb52_pmuc.h>
#include <hardware/sf32lb52_rtc.h>

#define PMUC(reg) (PBL_DT_REG_PMUC + SF32LB52_PMUC_##reg##_OFFSET)

// Factory test calibration, efuse bank 1
#define ATE_OFFSET    32U
#define ATE_SIZE      32U
#define ATE_SETTLE_US 10U

#define PID_D 4U

// Boot ROM options, RTC backup register 0
#define BOOTOPT_REG           0U
#define BOOTOPT_PD_DELAY_MASK PBL_GENMASK(11, 4)
#define BOOTOPT_PU_DELAY_MASK PBL_GENMASK(23, 12)
#define BOOTOPT_PD_DELAY_MS   100U
#define BOOTOPT_PU_DELAY_MS   10U

struct pmu_trims {
  uint8_t buck_vos_trim;
  uint8_t buck_vos_polar;
  uint8_t hpsys_ldo_vout;
  uint8_t hpsys_ldo_vout2;
  uint8_t lpsys_ldo_vout;
  uint8_t vret_trim;
  uint8_t ldo18_vref_sel;
  uint8_t vdd33_ldo2_vout;
  uint8_t vdd33_ldo3_vout;
  uint8_t aon_vos_trim;
  uint8_t aon_vos_polar;
};

static const struct pbl_nvmem_cell s_ate_cell =
    PBL_NVMEM_CELL(SF32LB52_EFUSE, ATE_OFFSET, ATE_SIZE);
static struct pmu_trims s_trims;
static bool s_trims_valid;

static bool prv_letter_series(void) {
  uint32_t rev = soc_sf32lb52_revid();

  return rev == HPSYS_CFG_IDR_REVID_A4 || rev == HPSYS_CFG_IDR_REVID_B4;
}

static uint32_t prv_pid(void) {
  return PBL_FIELD_GET(HPSYS_CFG_IDR_PID_MASK,
                       pbl_readl(PBL_DT_REG_HPSYS_CFG + SF32LB52_HPSYS_CFG_IDR_OFFSET));
}

// Byte 0 holds trims, so it is never 0 on a calibrated part
static int prv_parse(const uint8_t *d, struct pmu_trims *t) {
  const bool letter = prv_letter_series();

  if (d[0] == 0U) {
    return -ENODATA;
  }

  if (letter && prv_pid() == PID_D) {
    t->buck_vos_trim = d[20] & 0x7U;
    t->buck_vos_polar = (d[20] & 0x8U) >> 3;
    t->hpsys_ldo_vout = (d[20] & 0xF0U) >> 4;
    t->lpsys_ldo_vout = d[21] & 0xFU;
    t->vret_trim = (d[21] & 0xF0U) >> 4;
    t->hpsys_ldo_vout2 = (d[28] & 0x78U) >> 3;
  } else {
    t->buck_vos_trim = d[0] & 0x7U;
    t->buck_vos_polar = (d[0] & 0x8U) >> 3;
    t->hpsys_ldo_vout = (d[0] & 0xF0U) >> 4;
    t->lpsys_ldo_vout = d[1] & 0xFU;
    t->vret_trim = (d[1] & 0xF0U) >> 4;
    t->hpsys_ldo_vout2 = (d[13] & 0xF0U) >> 4;
  }

  if (!letter) {
    t->vdd33_ldo2_vout = (d[2] & 0xF0U) >> 4;
    t->vdd33_ldo3_vout = d[3] & 0xFU;
    t->aon_vos_trim = (d[3] & 0x70U) >> 4;
    t->aon_vos_polar = (d[3] & 0x80U) >> 7;
  }

  t->ldo18_vref_sel = d[2] & 0xFU;

  if (t->hpsys_ldo_vout == 0U || t->hpsys_ldo_vout2 == 0U) {
    return -ENODATA;
  }

  return 0;
}

static void prv_apply_trims(const struct pmu_trims *t) {
  pbl_modifyl(PMUC_BUCK_CR1_BG_BUF_VOS_POLAR | PMUC_BUCK_CR1_BG_BUF_VOS_TRIM_MASK,
              (t->buck_vos_polar != 0U ? PMUC_BUCK_CR1_BG_BUF_VOS_POLAR : 0U) |
                  PBL_FIELD_PREP(PMUC_BUCK_CR1_BG_BUF_VOS_TRIM_MASK, t->buck_vos_trim),
              PMUC(BUCK_CR1));

  pbl_modifyl(PMUC_LPSYS_VOUT_VOUT_MASK,
              PBL_FIELD_PREP(PMUC_LPSYS_VOUT_VOUT_MASK, t->lpsys_ldo_vout), PMUC(LPSYS_VOUT));

  pbl_modifyl(PMUC_VRET_CR_TRIM_MASK, PBL_FIELD_PREP(PMUC_VRET_CR_TRIM_MASK, t->vret_trim),
              PMUC(VRET_CR));

  if (prv_letter_series()) {
    pbl_modifyl(PMUC_PERI_LDO_LDO18_VREF_SEL_MASK,
                PBL_FIELD_PREP(PMUC_PERI_LDO_LDO18_VREF_SEL_MASK, t->ldo18_vref_sel),
                PMUC(PERI_LDO));
    return;
  }

  pbl_modifyl(PMUC_PERI_LDO_LDO18_VREF_SEL_MASK | PMUC_PERI_LDO_VDD33_LDO2_VOUT_MASK |
                  PMUC_PERI_LDO_VDD33_LDO3_VOUT_MASK,
              PBL_FIELD_PREP(PMUC_PERI_LDO_LDO18_VREF_SEL_MASK, t->ldo18_vref_sel) |
                  PBL_FIELD_PREP(PMUC_PERI_LDO_VDD33_LDO2_VOUT_MASK, t->vdd33_ldo2_vout) |
                  PBL_FIELD_PREP(PMUC_PERI_LDO_VDD33_LDO3_VOUT_MASK, t->vdd33_ldo3_vout),
              PMUC(PERI_LDO));

  pbl_modifyl(PMUC_AON_BG_BUF_VOS_POLAR | PMUC_AON_BG_BUF_VOS_TRIM_MASK,
              (t->aon_vos_polar != 0U ? PMUC_AON_BG_BUF_VOS_POLAR : 0U) |
                  PBL_FIELD_PREP(PMUC_AON_BG_BUF_VOS_TRIM_MASK, t->aon_vos_trim),
              PMUC(AON_BG));
}

// The bootloader leaves the flash in 4-byte address mode, which the ROM cannot read, and a reset
// can come before the flash driver takes it out: power cycle it through PA21 instead.
void soc_sf32lb52_boot_flash_delays(void) {
  const uintptr_t reg = PBL_DT_REG_RTC + SF32LB52_RTC_BKP_OFFSET(BOOTOPT_REG);

  pbl_modifyl(BOOTOPT_PD_DELAY_MASK | BOOTOPT_PU_DELAY_MASK,
              PBL_FIELD_PREP(BOOTOPT_PD_DELAY_MASK, BOOTOPT_PD_DELAY_MS) |
                  PBL_FIELD_PREP(BOOTOPT_PU_DELAY_MASK, BOOTOPT_PU_DELAY_MS),
              reg);
}

void soc_sf32lb52_pmu_init(void) {
  pbl_modifyl(PMUC_VRET_CR_VBIT_MASK, PBL_FIELD_PREP(PMUC_VRET_CR_VBIT_MASK, 1U), PMUC(VRET_CR));

  // Buck mode transitions, each given the longest settling count
  pbl_modifyl(PMUC_BUCK_CR2_M2H_CNT_MASK | PMUC_BUCK_CR2_L2M_CNT_MASK | PMUC_BUCK_CR2_L2H_CNT_MASK |
                  PMUC_BUCK_CR2_SET_VOUT_L_MASK,
              PMUC_BUCK_CR2_H2M_EN | PMUC_BUCK_CR2_M2L_EN | PMUC_BUCK_CR2_H2L_EN |
                  PMUC_BUCK_CR2_L2M_EN | PBL_FIELD_PREP(PMUC_BUCK_CR2_M2H_CNT_MASK, 7U) |
                  PBL_FIELD_PREP(PMUC_BUCK_CR2_L2M_CNT_MASK, 7U) |
                  PBL_FIELD_PREP(PMUC_BUCK_CR2_L2H_CNT_MASK, 7U) |
                  PBL_FIELD_PREP(PMUC_BUCK_CR2_SET_VOUT_L_MASK, 6U),
              PMUC(BUCK_CR2));

  // VBAT LDO at 3.3 V, lowest power-on reset threshold
  pbl_modifyl(PMUC_AON_LDO_VBAT_POR_TH_MASK | PMUC_AON_LDO_VBAT_LDO_SET_VOUT_MASK,
              PBL_FIELD_PREP(PMUC_AON_LDO_VBAT_LDO_SET_VOUT_MASK, 6U), PMUC(AON_LDO));

  // Power down on a low battery
  pbl_modifyl(0U, PMUC_WER_LOWBAT, PMUC(WER));

  // Shorter power-up delays, for faster wakeups
  pbl_modifyl(PMUC_LDO_DLY_MASK, PBL_FIELD_PREP(PMUC_LDO_DLY_MASK, 1U), PMUC(HPSYS_LDO));
  pbl_modifyl(PMUC_LDO_DLY_MASK, PBL_FIELD_PREP(PMUC_LDO_DLY_MASK, 1U), PMUC(LPSYS_LDO));
  pbl_modifyl(PMUC_SWR_DLY_MASK, PBL_FIELD_PREP(PMUC_SWR_DLY_MASK, 3U), PMUC(HPSYS_SWR));
  pbl_modifyl(PMUC_SWR_DLY_MASK, PBL_FIELD_PREP(PMUC_SWR_DLY_MASK, 3U), PMUC(LPSYS_SWR));

  // HXT48 takes 2 ms to be ready, more than the default delay
  pbl_modifyl(PMUC_HXT_CR3_DLY_MASK, PBL_FIELD_PREP(PMUC_HXT_CR3_DLY_MASK, 0x3fU), PMUC(HXT_CR3));

  pbl_writel(
      PBL_FIELD_PREP(PMUC_WKUP_CNT_PIN0_MASK, 1U) | PBL_FIELD_PREP(PMUC_WKUP_CNT_PIN1_MASK, 1U),
      PMUC(WKUP_CNT));
}

void soc_sf32lb52_syscfg_init(void) {
  uint8_t ate[ATE_SIZE];

  if (pbl_device_init(&SF32LB52_EFUSE->dev) != 0) {
    return;
  }

  pbl_busy_wait_us(ATE_SETTLE_US);

  if (pbl_nvmem_cell_read(&s_ate_cell, ate, sizeof(ate)) != 0 || prv_parse(ate, &s_trims) != 0) {
    return;
  }

  s_trims_valid = true;
  prv_apply_trims(&s_trims);
}

int soc_sf32lb52_hpsys_ldo_trims(uint8_t *s0, uint8_t *s1) {
  if (!s_trims_valid) {
    return -ENODATA;
  }

  *s0 = s_trims.hpsys_ldo_vout;
  *s1 = s_trims.hpsys_ldo_vout2;

  return 0;
}
