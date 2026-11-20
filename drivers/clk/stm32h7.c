/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/clk/stm32h7.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>
#include <pbl/util/size.h>

#include <dt-bindings/clock/stm32h7-clks.h>
#include <hardware/stm32h7_pwr.h>
#include <hardware/stm32h7_rcc.h>

// The driver numbers its clocks as the binding does
static_assert(STM32H7_CLK_SYS_D1CPRE == SYS_D1CPRE && STM32H7_CLK_HCLK == HCLK &&
              STM32H7_CLK_PCLK1 == PCLK1 && STM32H7_CLK_PCLK2 == PCLK2 &&
              STM32H7_CLK_PCLK4 == PCLK4);
static_assert(STM32H7_CLK_HSE_CK == HSE_CK && STM32H7_CLK_LSE_CK == LSE_CK &&
              STM32H7_CLK_HSI_CK == HSI_CK && STM32H7_CLK_CSI_CK == CSI_CK &&
              STM32H7_CLK_SYS_CK == SYS_CK);
static_assert(STM32H7_CLK_GPIOA_CK == GPIOA_CK && STM32H7_CLK_GPIOK_CK == GPIOK_CK &&
              STM32H7_CLK_TIM5_CK == TIM5_CK && STM32H7_CLK_SYSCFG_CK == SYSCFG_CK &&
              STM32H7_CLK_USART1_CK == USART1_CK);
static_assert(PBL_CLK_STM32H7_NUM_CLKS == LPUART1_CK + 1);

#define MHZ_PER_HZ 1000ULL

// The core supply settles within a few hundred microseconds
#define ACTVOSRDY_TIMEOUT_US 10000U

// An enable bit of a peripheral clock
struct gate {
  uint16_t reg;
  uint8_t bit;
  uint8_t parent;
};

#define GATE(_reg, _bit, _parent) \
  {.reg = STM32H7_RCC_##_reg##_OFFSET, .bit = (_bit), .parent = STM32H7_CLK_##_parent}

static const struct gate s_gates[PBL_CLK_STM32H7_NUM_CLKS] = {
  [STM32H7_CLK_GPIOA_CK] = GATE(AHB4ENR, 0, HCLK),
  [STM32H7_CLK_GPIOB_CK] = GATE(AHB4ENR, 1, HCLK),
  [STM32H7_CLK_GPIOC_CK] = GATE(AHB4ENR, 2, HCLK),
  [STM32H7_CLK_GPIOD_CK] = GATE(AHB4ENR, 3, HCLK),
  [STM32H7_CLK_GPIOE_CK] = GATE(AHB4ENR, 4, HCLK),
  [STM32H7_CLK_GPIOF_CK] = GATE(AHB4ENR, 5, HCLK),
  [STM32H7_CLK_GPIOG_CK] = GATE(AHB4ENR, 6, HCLK),
  [STM32H7_CLK_GPIOH_CK] = GATE(AHB4ENR, 7, HCLK),
  [STM32H7_CLK_GPIOI_CK] = GATE(AHB4ENR, 8, HCLK),
  [STM32H7_CLK_GPIOJ_CK] = GATE(AHB4ENR, 9, HCLK),
  [STM32H7_CLK_GPIOK_CK] = GATE(AHB4ENR, 10, HCLK),
  [STM32H7_CLK_TIM5_CK] = GATE(APB1LENR, 3, PCLK1),
  [STM32H7_CLK_SYSCFG_CK] = GATE(APB4ENR, 1, PCLK4),
  [STM32H7_CLK_USART1_CK] = GATE(APB2ENR, 4, PCLK2),
};

static const struct pbl_clk_stm32h7 *prv_rcc(const struct pbl_clk_controller *ctrl) {
  return PBL_CONTAINER_OF(ctrl, const struct pbl_clk_stm32h7, ctrl);
}

static inline uint32_t prv_getreg(const struct pbl_clk_stm32h7 *rcc, uint32_t offset) {
  return pbl_readl(rcc->base + offset);
}

static bool prv_is_gate(uint32_t id) {
  return id < PBL_ARRAY_SIZE(s_gates) && s_gates[id].reg != 0U;
}

// HPRE and D1CPRE: 0xxx divides by 1, 1000 to 1111 by 2 to 512, skipping 32
static uint32_t prv_ahb_div(uint32_t pre) {
  if ((pre & RCC_AHB_PRE_DIVIDED) == 0U) {
    return 1U;
  }

  pre &= ~RCC_AHB_PRE_DIVIDED;

  return 1U << (pre + (pre >= 4U ? 2U : 1U));
}

// PPRE: 0xx divides by 1, 100 to 111 by 2 to 16
static uint32_t prv_apb_div(uint32_t pre) {
  if ((pre & RCC_APB_PRE_DIVIDED) == 0U) {
    return 1U;
  }

  return 1U << ((pre & ~RCC_APB_PRE_DIVIDED) + 1U);
}

static int prv_enable(const struct pbl_clk_controller *ctrl, uint32_t id) {
  const struct pbl_clk_stm32h7 *rcc = prv_rcc(ctrl);

  if (!prv_is_gate(id)) {
    return 0;
  }

  pbl_modifyl(0U, PBL_BIT(s_gates[id].bit), rcc->base + s_gates[id].reg);
  // The clock runs two cycles after the write; the read back waits for them
  (void)prv_getreg(rcc, s_gates[id].reg);

  return 0;
}

static int prv_disable(const struct pbl_clk_controller *ctrl, uint32_t id) {
  const struct pbl_clk_stm32h7 *rcc = prv_rcc(ctrl);

  if (prv_is_gate(id)) {
    pbl_modifyl(PBL_BIT(s_gates[id].bit), 0U, rcc->base + s_gates[id].reg);
  }

  return 0;
}

static int prv_get_parent(const struct pbl_clk_controller *ctrl, uint32_t id,
                          struct pbl_clk *parent) {
  const struct pbl_clk_stm32h7 *rcc = prv_rcc(ctrl);
  uint32_t sel;

  switch (id) {
    case STM32H7_CLK_HSE_CK:
      *parent = rcc->hse;
      return 0;
    case STM32H7_CLK_LSE_CK:
      *parent = rcc->lse;
      return 0;
    case STM32H7_CLK_HSI_CK:
    case STM32H7_CLK_CSI_CK:
      return -ENOENT;
    case STM32H7_CLK_SYS_CK:
      switch (PBL_FIELD_GET(RCC_CFGR_SWS_MASK, prv_getreg(rcc, STM32H7_RCC_CFGR_OFFSET))) {
        case RCC_CFGR_SW_HSI:
          *parent = (struct pbl_clk)PBL_CLK(ctrl, STM32H7_CLK_HSI_CK);
          return 0;
        case RCC_CFGR_SW_CSI:
          *parent = (struct pbl_clk)PBL_CLK(ctrl, STM32H7_CLK_CSI_CK);
          return 0;
        case RCC_CFGR_SW_HSE:
          *parent = (struct pbl_clk)PBL_CLK(ctrl, STM32H7_CLK_HSE_CK);
          return 0;
        default:
          return -ENOTSUP;
      }
    case STM32H7_CLK_SYS_D1CPRE:
      *parent = (struct pbl_clk)PBL_CLK(ctrl, STM32H7_CLK_SYS_CK);
      return 0;
    case STM32H7_CLK_HCLK:
      *parent = (struct pbl_clk)PBL_CLK(ctrl, STM32H7_CLK_SYS_D1CPRE);
      return 0;
    case STM32H7_CLK_PCLK1:
    case STM32H7_CLK_PCLK2:
    case STM32H7_CLK_PCLK4:
      *parent = (struct pbl_clk)PBL_CLK(ctrl, STM32H7_CLK_HCLK);
      return 0;
    case STM32H7_CLK_USART1_CK:
      sel =
          PBL_FIELD_GET(RCC_D2CCIP2R_USART16SEL_MASK, prv_getreg(rcc, STM32H7_RCC_D2CCIP2R_OFFSET));
      switch (sel) {
        case RCC_D2CCIP2R_USARTSEL_PCLK:
          *parent = (struct pbl_clk)PBL_CLK(ctrl, STM32H7_CLK_PCLK2);
          return 0;
        case RCC_D2CCIP2R_USARTSEL_HSI_KER:
          *parent = (struct pbl_clk)PBL_CLK(ctrl, STM32H7_CLK_HSI_CK);
          return 0;
        case RCC_D2CCIP2R_USARTSEL_CSI_KER:
          *parent = (struct pbl_clk)PBL_CLK(ctrl, STM32H7_CLK_CSI_CK);
          return 0;
        case RCC_D2CCIP2R_USARTSEL_LSE:
          *parent = (struct pbl_clk)PBL_CLK(ctrl, STM32H7_CLK_LSE_CK);
          return 0;
        default:
          return -ENOTSUP;
      }
    default:
      if (!prv_is_gate(id)) {
        return -EINVAL;
      }
      *parent = (struct pbl_clk)PBL_CLK(ctrl, s_gates[id].parent);
      return 0;
  }
}

// Timers run at twice an APB clock divided down, or with TIMPRE four times up to HCLK
static uint64_t prv_timer_rate(const struct pbl_clk_stm32h7 *rcc, uint32_t div, uint64_t pclk_mhz) {
  if ((prv_getreg(rcc, STM32H7_RCC_CFGR_OFFSET) & RCC_CFGR_TIMPRE) != 0U) {
    return div <= 4U ? pclk_mhz * div : pclk_mhz * 4U;
  }

  return div == 1U ? pclk_mhz : pclk_mhz * 2U;
}

static int prv_get_rate(const struct pbl_clk_controller *ctrl, uint32_t id, uint64_t parent_mhz,
                        uint64_t *rate_mhz) {
  const struct pbl_clk_stm32h7 *rcc = prv_rcc(ctrl);
  uint32_t d1cfgr = prv_getreg(rcc, STM32H7_RCC_D1CFGR_OFFSET);
  uint32_t d2cfgr = prv_getreg(rcc, STM32H7_RCC_D2CFGR_OFFSET);
  uint32_t d3cfgr = prv_getreg(rcc, STM32H7_RCC_D3CFGR_OFFSET);

  switch (id) {
    case STM32H7_CLK_HSI_CK:
      *rate_mhz = (STM32H7_HSI_HZ * MHZ_PER_HZ) >>
                  PBL_FIELD_GET(RCC_CR_HSIDIV_MASK, prv_getreg(rcc, STM32H7_RCC_CR_OFFSET));
      return 0;
    case STM32H7_CLK_CSI_CK:
      *rate_mhz = STM32H7_CSI_HZ * MHZ_PER_HZ;
      return 0;
    case STM32H7_CLK_SYS_D1CPRE:
      *rate_mhz = parent_mhz / prv_ahb_div(PBL_FIELD_GET(RCC_D1CFGR_D1CPRE_MASK, d1cfgr));
      return 0;
    case STM32H7_CLK_HCLK:
      *rate_mhz = parent_mhz / prv_ahb_div(PBL_FIELD_GET(RCC_D1CFGR_HPRE_MASK, d1cfgr));
      return 0;
    case STM32H7_CLK_PCLK1:
      *rate_mhz = parent_mhz / prv_apb_div(PBL_FIELD_GET(RCC_D2CFGR_D2PPRE1_MASK, d2cfgr));
      return 0;
    case STM32H7_CLK_PCLK2:
      *rate_mhz = parent_mhz / prv_apb_div(PBL_FIELD_GET(RCC_D2CFGR_D2PPRE2_MASK, d2cfgr));
      return 0;
    case STM32H7_CLK_PCLK4:
      *rate_mhz = parent_mhz / prv_apb_div(PBL_FIELD_GET(RCC_D3CFGR_D3PPRE_MASK, d3cfgr));
      return 0;
    case STM32H7_CLK_TIM5_CK:
      *rate_mhz = prv_timer_rate(rcc, prv_apb_div(PBL_FIELD_GET(RCC_D2CFGR_D2PPRE1_MASK, d2cfgr)),
                                 parent_mhz);
      return 0;
    default:
      *rate_mhz = parent_mhz;
      return 0;
  }
}

const struct pbl_clk_controller_ops pbl_clk_stm32h7_ops = {
  .enable = prv_enable,
  .disable = prv_disable,
  .get_rate = prv_get_rate,
  .get_parent = prv_get_parent,
};

// The configuration takes once per power-on: a different one written before stays
int pbl_clk_stm32h7_init(const struct pbl_device *dev) {
  const struct pbl_clk_stm32h7 *rcc = PBL_CONTAINER_OF(dev, const struct pbl_clk_stm32h7, ctrl.dev);
  uintptr_t cr3 = rcc->pwr + STM32H7_PWR_CR3_OFFSET;

  pbl_modifyl(PWR_CR3_SUPPLY_MASK, rcc->supply, cr3);
  if ((pbl_readl(cr3) & PWR_CR3_SUPPLY_MASK) != rcc->supply) {
    return -EIO;
  }

  return PBL_POLL_BUSY_US(
      (pbl_readl(rcc->pwr + STM32H7_PWR_CSR1_OFFSET) & PWR_CSR1_ACTVOSRDY) != 0U, 10U,
      ACTVOSRDY_TIMEOUT_US);
}

#include PBL_DT_INSTANCES(st_stm32h743_rcc)
