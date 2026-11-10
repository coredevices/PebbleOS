/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/clk/sf32lb.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>

#include <hardware/sf32lb52_hpsys_rcc.h>

#define RCC_NUM_LINES 64U

#define SYSCLK_48M_HZ 48000000U
#define DLL_MIN_HZ    24000000U
#define DLL_STEP_HZ   24000000U

struct pbl_clk_sf32lb {
  struct pbl_clk_controller ctrl;
  uintptr_t base;
};

static uintptr_t prv_base(const struct pbl_clk_controller *ctrl) {
  return container_of(ctrl, const struct pbl_clk_sf32lb, ctrl)->base;
}

static inline uintptr_t prv_reg(uintptr_t base, uint32_t offset, uint32_t id) {
  return base + offset + ((id >> 5) << 2);
}

static int prv_clk_enable(const struct pbl_clk_controller *ctrl, uint32_t id) {
  pbl_writel(PBL_BIT(id & 31U), prv_reg(prv_base(ctrl), SF32LB52_HPSYS_RCC_ESR1_OFFSET, id));

  return 0;
}

static int prv_clk_disable(const struct pbl_clk_controller *ctrl, uint32_t id) {
  pbl_writel(PBL_BIT(id & 31U), prv_reg(prv_base(ctrl), SF32LB52_HPSYS_RCC_ECR1_OFFSET, id));

  return 0;
}

static uint32_t prv_rcc_read(uintptr_t base, uint32_t offset) {
  return pbl_readl(base + offset);
}

static uint32_t prv_sysclk_hz(uintptr_t base) {
  uint32_t dll1cr;

  if (PBL_FIELD_GET(HPSYS_RCC_CSR_SEL_SYS_MASK,
                    prv_rcc_read(base, SF32LB52_HPSYS_RCC_CSR_OFFSET)) !=
      HPSYS_RCC_CSR_SEL_SYS_DLL1) {
    return SYSCLK_48M_HZ;
  }

  dll1cr = prv_rcc_read(base, SF32LB52_HPSYS_RCC_DLL1CR_OFFSET);
  if ((dll1cr & HPSYS_RCC_DLL1CR_EN) == 0U) {
    return 0U;
  }

  return DLL_MIN_HZ + PBL_FIELD_GET(HPSYS_RCC_DLL1CR_STG_MASK, dll1cr) * DLL_STEP_HZ;
}

static uint32_t prv_pclk1_hz(uintptr_t base) {
  const uint32_t cfgr = prv_rcc_read(base, SF32LB52_HPSYS_RCC_CFGR_OFFSET);
  const uint32_t hdiv = PBL_FIELD_GET(HPSYS_RCC_CFGR_HDIV_MASK, cfgr);

  return (prv_sysclk_hz(base) / ((hdiv == 0U) ? 1U : hdiv)) >>
         PBL_FIELD_GET(HPSYS_RCC_CFGR_PDIV1_MASK, cfgr);
}

static int prv_clk_get_rate(const struct pbl_clk_controller *ctrl, uint32_t id, uint64_t parent_mhz,
                            uint64_t *rate_mhz) {
  if (id != SF32LB52_HPSYS_CLK_EFUSEC) {
    return -ENOTSUP;
  }

  *rate_mhz = (uint64_t)prv_pclk1_hz(prv_base(ctrl)) * 1000U;

  return 0;
}

static const struct pbl_clk_controller_ops s_clk_ops = {
  .enable = prv_clk_enable,
  .disable = prv_clk_disable,
  .get_rate = prv_clk_get_rate,
};

#define PBL_CLK_SF32LB_DEFINE(sym, _name, _base)                                                \
  PBL_CLK_CONTROLLER_STATE_DEFINE(sym, RCC_NUM_LINES);                                          \
  PBL_DEVICE_INSTANCE(sym, struct pbl_clk_sf32lb, struct pbl_clk_controller, ctrl) {            \
    .ctrl = PBL_CLK_CONTROLLER_INIT(sym, _name, nullptr, nullptr, &s_clk_ops), .base = (_base), \
  }

#include PBL_DT_INSTANCES(sifli_sf32lb52_hpsys_rcc_clk)
