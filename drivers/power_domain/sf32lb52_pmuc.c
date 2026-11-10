/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/power_domain.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>
#include <pbl/util/size.h>

#include <dt-bindings/power/sifli,sf32lb52-pmuc.h>
#include <hardware/sf32lb52_hpsys_cfg.h>
#include <hardware/sf32lb52_pmuc.h>
#include <syscfg.h>

#define NUM_DOMAINS (SF32LB52_PMUC_PD_HPSYS + 1U)

// The buck takes this long to settle on a higher voltage
#define BUCK_SETTLE_US 250U

// HPSYS LDO references of S0 and S1 on a part without factory trims
#define S0_LDO_DEFAULT 0xbU
#define S1_LDO_DEFAULT 0xdU

struct pbl_pd_sf32lb52_pmuc {
  struct pbl_power_domain_provider provider;
  uintptr_t base;
  uintptr_t cfg;
};

// The voltages and memory timings of each mode. The S modes take the LDO reference of their own
// trim, the D modes an offset from the S0 one.
struct hpsys_mode {
  uint8_t buck;
  int8_t ldo_offset;
  uint32_t ulpmcr;
};

static const struct hpsys_mode s_modes[] = {
  [SF32LB52_PMUC_HPSYS_D0] = {.buck = 0x9U, .ldo_offset = -5, .ulpmcr = 0x00100330U},
  [SF32LB52_PMUC_HPSYS_D1] = {.buck = 0xaU, .ldo_offset = -3, .ulpmcr = 0x00110331U},
  [SF32LB52_PMUC_HPSYS_S0] = {.buck = 0xdU, .ulpmcr = 0x00130213U},
  [SF32LB52_PMUC_HPSYS_S1] = {.buck = 0xfU, .ulpmcr = 0x00130213U},
};

static void prv_trims(uint8_t *s0, uint8_t *s1) {
  if (soc_sf32lb52_hpsys_ldo_trims(s0, s1) != 0) {
    *s0 = S0_LDO_DEFAULT;
    *s1 = S1_LDO_DEFAULT;
  }
}

// The clocks of the domain must already run at what the mode allows when it lowers the voltage
static int prv_set_performance_state(const struct pbl_power_domain_provider *provider, uint32_t id,
                                     uint32_t level) {
  const struct pbl_pd_sf32lb52_pmuc *pmuc =
      container_of(provider, const struct pbl_pd_sf32lb52_pmuc, provider);
  const uintptr_t syscr = pmuc->cfg + SF32LB52_HPSYS_CFG_SYSCR_OFFSET;
  const struct hpsys_mode *mode;
  uint8_t s0;
  uint8_t s1;

  if (id != SF32LB52_PMUC_PD_HPSYS || level < SF32LB52_PMUC_HPSYS_D0 ||
      level >= ARRAY_LENGTH(s_modes)) {
    return -EINVAL;
  }

  mode = &s_modes[level];
  prv_trims(&s0, &s1);

  if (level >= SF32LB52_PMUC_HPSYS_S0) {
    pbl_modifyl(PMUC_BUCK_VOUT_VOUT_MASK, PBL_FIELD_PREP(PMUC_BUCK_VOUT_VOUT_MASK, mode->buck),
                pmuc->base + SF32LB52_PMUC_BUCK_VOUT_OFFSET);
    pbl_modifyl(
        PMUC_HPSYS_VOUT_VOUT_MASK,
        PBL_FIELD_PREP(PMUC_HPSYS_VOUT_VOUT_MASK, level == SF32LB52_PMUC_HPSYS_S0 ? s0 : s1),
        pmuc->base + SF32LB52_PMUC_HPSYS_VOUT_OFFSET);
    pbl_modifyl(HPSYS_CFG_SYSCR_LDO_VSEL, 0U, syscr);
    pbl_busy_wait_us(BUCK_SETTLE_US);
    pbl_writel(mode->ulpmcr, pmuc->cfg + SF32LB52_HPSYS_CFG_ULPMCR_OFFSET);
  } else {
    pbl_modifyl(PMUC_BUCK_CR2_SET_VOUT_M_MASK,
                PBL_FIELD_PREP(PMUC_BUCK_CR2_SET_VOUT_M_MASK, mode->buck),
                pmuc->base + SF32LB52_PMUC_BUCK_CR2_OFFSET);
    pbl_modifyl(PMUC_HPSYS_LDO_VREF_MASK,
                PBL_FIELD_PREP(PMUC_HPSYS_LDO_VREF_MASK, (uint32_t)(s0 + mode->ldo_offset)),
                pmuc->base + SF32LB52_PMUC_HPSYS_LDO_OFFSET);
    pbl_writel(mode->ulpmcr, pmuc->cfg + SF32LB52_HPSYS_CFG_ULPMCR_OFFSET);
    pbl_modifyl(0U, HPSYS_CFG_SYSCR_LDO_VSEL, syscr);
  }

  return 0;
}

static const struct pbl_power_domain_provider_ops s_ops = {
  .set_performance_state = prv_set_performance_state,
};

#define PBL_PD_SF32LB52_PMUC_DEFINE(sym, _name, _base, _cfg)                              \
  PBL_POWER_DOMAIN_PROVIDER_STATE_DEFINE(sym, NUM_DOMAINS);                               \
  PBL_DEVICE_INSTANCE(sym, struct pbl_pd_sf32lb52_pmuc, struct pbl_power_domain_provider, \
                      provider) {                                                         \
    .provider = PBL_POWER_DOMAIN_PROVIDER_INIT(sym, _name, nullptr, nullptr, &s_ops),     \
    .base = (_base), .cfg = (_cfg),                                                       \
  }

#include PBL_DT_INSTANCES(sifli_sf32lb52_pmuc_power_domain)
