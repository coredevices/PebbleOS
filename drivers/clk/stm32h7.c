/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/clk/stm32h7.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/mmio.h>
#include <pbl/kernel/time.h>
#include <pbl/util/bits.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>
#include <pbl/util/size.h>
#include <pbl/util/units.h>

#include <dt-bindings/clock/stm32h7-clks.h>
#include <hardware/stm32h7_pwr.h>
#include <hardware/stm32h7_rcc.h>

// The driver numbers its clocks as the binding does
static_assert(STM32H7_CLK_SYS_D1CPRE == SYS_D1CPRE && STM32H7_CLK_HCLK == HCLK &&
              STM32H7_CLK_PCLK1 == PCLK1 && STM32H7_CLK_PCLK2 == PCLK2 &&
              STM32H7_CLK_PCLK3 == PCLK3 && STM32H7_CLK_PCLK4 == PCLK4);
static_assert(STM32H7_CLK_HSE_CK == HSE_CK && STM32H7_CLK_LSE_CK == LSE_CK &&
              STM32H7_CLK_HSI_CK == HSI_CK && STM32H7_CLK_CSI_CK == CSI_CK &&
              STM32H7_CLK_SYS_CK == SYS_CK && STM32H7_CLK_LSI_CK == LSI_CK);
static_assert(STM32H7_CLK_GPIOA_CK == GPIOA_CK && STM32H7_CLK_GPIOK_CK == GPIOK_CK &&
              STM32H7_CLK_USART1_CK == USART1_CK && STM32H7_CLK_UART8_CK == UART8_CK &&
              STM32H7_CLK_LPTIM1_CK == LPTIM1_CK && STM32H7_CLK_LPTIM2_CK == LPTIM2_CK);
static_assert(STM32H7_CLK_PLLSRC == PLLSRC && STM32H7_CLK_PLL1_P == PLL1_P &&
              STM32H7_CLK_PLL1_Q == PLL1_Q && STM32H7_CLK_PLL1_R == PLL1_R &&
              STM32H7_CLK_PLL3_R == PLL3_R && STM32H7_CLK_PLL3_R == STM32H7_CLK_PLL1_P + 8);
static_assert(STM32H7_CLK_SYSCFG_CK == SYSCFG_CK);
static_assert(STM32H7_CLK_RC48_CK == RC48_CK && STM32H7_CLK_RNG_CK == RNG_CK);
static_assert(STM32H7_CLK_CRC_CK == CRC_CK);
static_assert(STM32H7_CLK_QUADSPI_CK == QUADSPI_CK);
static_assert(STM32H7_CLK_DMA1_CK == DMA1_CK && STM32H7_CLK_DMA2_CK == DMA2_CK);
static_assert(STM32H7_CLK_ADC3_CK == ADC3_CK);
static_assert(STM32H7_CLK_RTC_CK == RTC_CK && STM32H7_CLK_RTCAPB_CK == RTCAPB_CK);
static_assert(STM32H7_CLK_USB1OTG_CK == USB1OTG_CK && STM32H7_CLK_USB1ULPI_CK == USB1ULPI_CK);
static_assert(STM32H7_CLK_HSEM_CK == HSEM_CK && STM32H7_CLK_WWDG2_CK == WWDG2_CK);
static_assert(STM32H7_CLK_FMC_CK == FMC_CK);
static_assert(STM32H7_CLK_DMA2D_CK == DMA2D_CK);
static_assert(STM32H7_CLK_DSI_CK == DSI_CK && STM32H7_CLK_LTDC_CK == LTDC_CK);
static_assert(STM32H7_CLK_I2C4_CK == I2C4_CK);
static_assert(STM32H7_CLK_SAI1_CK == SAI1_CK);
static_assert(STM32H7_CLK_BDMA_CK == BDMA_CK && STM32H7_CLK_SAI4A_CK == SAI4A_CK);
static_assert(PBL_CLK_STM32H7_NUM_CLKS == LPUART1_CK + 1);

// The system clock goes to PLL1 P once PLL1 starts
static bool s_sys_on_pll1;

#define MHZ(n) ((uint64_t)(n) * 1000000ULL * PBL_MILLIHZ_PER_HZ)

// The core supply settles within a few hundred microseconds
#define ACTVOSRDY_TIMEOUT_US 10000U
// The HSE crystal starts within a few milliseconds, the PLL within a few hundred microseconds
#define HSERDY_TIMEOUT_US 100000U
#define PLLRDY_TIMEOUT_US 10000U
// The LSI oscillator starts within tens of microseconds, the LSE crystal within 2 s
#define OSCRDY_TIMEOUT_US 1000U
#define LSERDY_TIMEOUT_US 2000000U

// Maximums at VOS1
#define CPU_MAX_MHZ  MHZ(400)
#define HCLK_MAX_MHZ MHZ(240)
#define PCLK_MAX_MHZ MHZ(120)

// PLL reference and wide-range VCO limits
#define PLL_REF_MIN_MHZ MHZ(2)
#define PLL_REF_MAX_MHZ MHZ(16)
#define PLL_VCO_MIN_MHZ MHZ(192)
#define PLL_VCO_MAX_MHZ MHZ(836)
#define PLL_DIVM_MAX    63U
#define PLL_DIVN_MIN    4U
#define PLL_DIVN_MAX    512U
#define PLL_DIVPQR_MAX  128U

// An enable bit of a peripheral clock
struct gate {
  uint16_t reg;
  uint8_t bit;
  uint8_t parent;
  // Bit in D3AMR of a D3 peripheral working in Stop, which keeps its clocks while the CPU that
  // enabled it is stopped
  bool autonomous;
  uint8_t amen;
};

#define GATE(_reg, _bit, _parent) \
  {.reg = STM32H7_RCC_##_reg##_OFFSET, .bit = (_bit), .parent = STM32H7_CLK_##_parent}

#define GATE_AUTONOMOUS(_reg, _bit, _parent, _amen) \
  {.reg = STM32H7_RCC_##_reg##_OFFSET,              \
   .bit = (_bit),                                   \
   .parent = STM32H7_CLK_##_parent,                 \
   .autonomous = true,                              \
   .amen = (_amen)}

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
  [STM32H7_CLK_CRC_CK] = GATE(AHB4ENR, 19, HCLK),
  [STM32H7_CLK_DMA1_CK] = GATE(AHB1ENR, 0, HCLK),
  [STM32H7_CLK_DMA2_CK] = GATE(AHB1ENR, 1, HCLK),
  [STM32H7_CLK_ADC3_CK] = GATE(AHB4ENR, 24, HCLK),
  [STM32H7_CLK_USB1OTG_CK] = GATE(AHB1ENR, 25, HCLK),
  [STM32H7_CLK_USB1ULPI_CK] = GATE(AHB1ENR, 26, HCLK),
  [STM32H7_CLK_HSEM_CK] = GATE(AHB4ENR, 25, HCLK),
  [STM32H7_CLK_WWDG2_CK] = GATE(APB1LENR, 11, PCLK1),
  [STM32H7_CLK_BDMA_CK] = GATE(AHB4ENR, 21, HCLK),
  [STM32H7_CLK_DMA2D_CK] = GATE(AHB3ENR, 4, HCLK),
  [STM32H7_CLK_FMC_CK] = GATE(AHB3ENR, 12, HCLK),
  [STM32H7_CLK_LPTIM1_CK] = GATE(APB1LENR, 9, PCLK1),
  [STM32H7_CLK_LPTIM2_CK] = GATE_AUTONOMOUS(APB4ENR, 9, PCLK4, 9),
  [STM32H7_CLK_SYSCFG_CK] = GATE(APB4ENR, 1, PCLK4),
  [STM32H7_CLK_RTCAPB_CK] = GATE(APB4ENR, 16, PCLK4),
  [STM32H7_CLK_I2C4_CK] = GATE(APB4ENR, 7, PCLK4),
  [STM32H7_CLK_RNG_CK] = GATE(AHB2ENR, 6, RC48_CK),
  [STM32H7_CLK_QUADSPI_CK] = GATE(AHB3ENR, 14, HCLK),
  [STM32H7_CLK_LTDC_CK] = GATE(APB3ENR, 3, PLL3_R),
  [STM32H7_CLK_DSI_CK] = GATE(APB3ENR, 4, PCLK3),
  [STM32H7_CLK_USART1_CK] = GATE(APB2ENR, 4, PCLK2),
  [STM32H7_CLK_SAI1_CK] = GATE(APB2ENR, 22, PLL1_Q),
  [STM32H7_CLK_SAI4A_CK] = GATE(APB4ENR, 21, PLL1_Q),
  [STM32H7_CLK_UART8_CK] = GATE(APB1LENR, 31, PCLK1),
};

static const struct pbl_clk_stm32h7 *prv_rcc(const struct pbl_clk_controller *ctrl) {
  return PBL_CONTAINER_OF(ctrl, const struct pbl_clk_stm32h7, ctrl);
}

static inline uint32_t prv_getreg(const struct pbl_clk_stm32h7 *rcc, uint32_t offset) {
  return pbl_readl(rcc->base + offset);
}

static inline void prv_modify(const struct pbl_clk_stm32h7 *rcc, uint32_t offset, uint32_t mask,
                              uint32_t value) {
  pbl_modifyl(mask, PBL_FIELD_PREP(mask, value), rcc->base + offset);
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

// The HPRE and D1CPRE field of a division, or -EINVAL
static int prv_ahb_pre(uint32_t div) {
  static const uint16_t s_divs[] = {2, 4, 8, 16, 64, 128, 256, 512};

  if (div == 1U) {
    return 0;
  }
  for (uint32_t i = 0U; i < PBL_ARRAY_SIZE(s_divs); i++) {
    if (s_divs[i] == div) {
      return (int)(RCC_AHB_PRE_DIVIDED | i);
    }
  }

  return -EINVAL;
}

// The PPRE field of a division, or -EINVAL
static int prv_apb_pre(uint32_t div) {
  for (uint32_t i = 0U; i < 4U; i++) {
    if (div == (2U << i)) {
      return (int)(RCC_APB_PRE_DIVIDED | i);
    }
  }

  return div == 1U ? 0 : -EINVAL;
}

static int prv_hse_on(const struct pbl_clk_stm32h7 *rcc) {
  pbl_modifyl(0U, RCC_CR_HSEON, rcc->base + STM32H7_RCC_CR_OFFSET);

  return PBL_POLL_BUSY_US((prv_getreg(rcc, STM32H7_RCC_CR_OFFSET) & RCC_CR_HSERDY) != 0U, 10U,
                          HSERDY_TIMEOUT_US);
}

// Turns an oscillator on, at bit on of a register, and waits for its ready bit
static int prv_osc_on(const struct pbl_clk_stm32h7 *rcc, uint32_t offset, uint32_t on,
                      uint32_t rdy) {
  pbl_modifyl(0U, on, rcc->base + offset);

  return PBL_POLL_BUSY_US((prv_getreg(rcc, offset) & rdy) != 0U, 1U, OSCRDY_TIMEOUT_US);
}

// The LSE is in the backup domain, written with its protection lifted. It stays on.
static int prv_lse_on(const struct pbl_clk_stm32h7 *rcc) {
  pbl_modifyl(0U, PWR_CR1_DBP, rcc->pwr + STM32H7_PWR_CR1_OFFSET);
  pbl_modifyl(0U, RCC_BDCR_LSEON, rcc->base + STM32H7_RCC_BDCR_OFFSET);

  return PBL_POLL_BUSY_US((prv_getreg(rcc, STM32H7_RCC_BDCR_OFFSET) & RCC_BDCR_LSERDY) != 0U, 100U,
                          LSERDY_TIMEOUT_US);
}

// The RTC clock is in the backup domain too
static int prv_rtc_on(const struct pbl_clk_stm32h7 *rcc, bool on) {
  pbl_modifyl(0U, PWR_CR1_DBP, rcc->pwr + STM32H7_PWR_CR1_OFFSET);
  pbl_modifyl(on ? 0U : RCC_BDCR_RTCEN, on ? RCC_BDCR_RTCEN : 0U,
              rcc->base + STM32H7_RCC_BDCR_OFFSET);

  return 0;
}

// PLL outputs follow each other, P, Q and R of PLL1, then of PLL2 and PLL3
static bool prv_is_pll(uint32_t id) {
  return id >= STM32H7_CLK_PLL1_P && id <= STM32H7_CLK_PLL3_R;
}

static uint32_t prv_pll(uint32_t id) {
  return (id - STM32H7_CLK_PLL1_P) / 3U;
}

static uint32_t prv_pll_out_mask(uint32_t id) {
  static const uint32_t s_masks[] = {RCC_PLLDIVR_P_MASK, RCC_PLLDIVR_Q_MASK, RCC_PLLDIVR_R_MASK};

  return s_masks[(id - STM32H7_CLK_PLL1_P) % 3U];
}

static bool prv_pll_on(const struct pbl_clk_stm32h7 *rcc, uint32_t pll) {
  return (prv_getreg(rcc, STM32H7_RCC_CR_OFFSET) & RCC_CR_PLLON(pll)) != 0U;
}

static bool prv_any_pll_on(const struct pbl_clk_stm32h7 *rcc) {
  return prv_pll_on(rcc, 0U) || prv_pll_on(rcc, 1U) || prv_pll_on(rcc, 2U);
}

// The Cortex-M7 runs on the D1 clock, the Cortex-M4 on HCLK
#ifdef CONFIG_CPU_CORTEX_M4
#define CORE_CLK STM32H7_CLK_HCLK
#else
#define CORE_CLK STM32H7_CLK_SYS_D1CPRE
#endif

static void prv_update_core_clock(const struct pbl_clk_controller *ctrl) {
  uint64_t rate_mhz;

  if (pbl_clk_get_rate(&(struct pbl_clk)PBL_CLK(ctrl, CORE_CLK), &rate_mhz) == 0) {
    pbl_cycles_per_sec_set((uint32_t)(rate_mhz / PBL_MILLIHZ_PER_HZ));
  }
}

static int prv_enable(const struct pbl_clk_controller *ctrl, uint32_t id) {
  const struct pbl_clk_stm32h7 *rcc = prv_rcc(ctrl);

  switch (id) {
    case STM32H7_CLK_HSE_CK:
      return prv_hse_on(rcc);
    case STM32H7_CLK_RC48_CK:
      return prv_osc_on(rcc, STM32H7_RCC_CR_OFFSET, RCC_CR_HSI48ON, RCC_CR_HSI48RDY);
    case STM32H7_CLK_LSI_CK:
      return prv_osc_on(rcc, STM32H7_RCC_CSR_OFFSET, RCC_CSR_LSION, RCC_CSR_LSIRDY);
    case STM32H7_CLK_LSE_CK:
      return prv_lse_on(rcc);
    case STM32H7_CLK_RTC_CK:
      return prv_rtc_on(rcc, true);
    default:
      break;
  }

  if (!prv_is_gate(id)) {
    return 0;
  }

  if (s_gates[id].autonomous) {
    pbl_modifyl(0U, PBL_BIT(s_gates[id].amen), rcc->base + STM32H7_RCC_D3AMR_OFFSET);
  }
  pbl_modifyl(0U, PBL_BIT(s_gates[id].bit), rcc->base + s_gates[id].reg);
  // The clock runs two cycles after the write; the read back waits for them
  (void)prv_getreg(rcc, s_gates[id].reg);

  return 0;
}

static int prv_disable(const struct pbl_clk_controller *ctrl, uint32_t id) {
  const struct pbl_clk_stm32h7 *rcc = prv_rcc(ctrl);

  switch (id) {
    case STM32H7_CLK_RC48_CK:
      pbl_modifyl(RCC_CR_HSI48ON, 0U, rcc->base + STM32H7_RCC_CR_OFFSET);
      return 0;
    case STM32H7_CLK_LSI_CK:
      pbl_modifyl(RCC_CSR_LSION, 0U, rcc->base + STM32H7_RCC_CSR_OFFSET);
      return 0;
    case STM32H7_CLK_RTC_CK:
      return prv_rtc_on(rcc, false);
    default:
      break;
  }

  if (prv_is_gate(id)) {
    pbl_modifyl(PBL_BIT(s_gates[id].bit), 0U, rcc->base + s_gates[id].reg);
    if (s_gates[id].autonomous) {
      pbl_modifyl(PBL_BIT(s_gates[id].amen), 0U, rcc->base + STM32H7_RCC_D3AMR_OFFSET);
    }
  }

  return 0;
}

// LPTIM1 and LPTIM2 take the same parents, their bus clock being PCLK1 or PCLK4
static int prv_get_lptim_parent(const struct pbl_clk_controller *ctrl, uint32_t pclk, uint32_t sel,
                                struct pbl_clk *parent) {
  switch (sel) {
    case RCC_LPTIMSEL_PCLK:
      *parent = (struct pbl_clk)PBL_CLK(ctrl, pclk);
      return 0;
    case RCC_LPTIMSEL_PLL2_P:
      *parent = (struct pbl_clk)PBL_CLK(ctrl, STM32H7_CLK_PLL2_P);
      return 0;
    case RCC_LPTIMSEL_PLL3_R:
      *parent = (struct pbl_clk)PBL_CLK(ctrl, STM32H7_CLK_PLL3_R);
      return 0;
    case RCC_LPTIMSEL_LSE:
      *parent = (struct pbl_clk)PBL_CLK(ctrl, STM32H7_CLK_LSE_CK);
      return 0;
    case RCC_LPTIMSEL_LSI:
      *parent = (struct pbl_clk)PBL_CLK(ctrl, STM32H7_CLK_LSI_CK);
      return 0;
    default:
      return -ENOTSUP;
  }
}

static int prv_get_sai_parent(const struct pbl_clk_controller *ctrl, uint32_t sel,
                              struct pbl_clk *parent) {
  switch (sel) {
    case RCC_SAISEL_PLL1Q:
      *parent = (struct pbl_clk)PBL_CLK(ctrl, STM32H7_CLK_PLL1_Q);
      return 0;
    case RCC_SAISEL_PLL2P:
      *parent = (struct pbl_clk)PBL_CLK(ctrl, STM32H7_CLK_PLL2_P);
      return 0;
    case RCC_SAISEL_PLL3P:
      *parent = (struct pbl_clk)PBL_CLK(ctrl, STM32H7_CLK_PLL3_P);
      return 0;
    default:
      return -ENOENT;
  }
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
    case STM32H7_CLK_RC48_CK:
    case STM32H7_CLK_LSI_CK:
      return -ENOENT;
    case STM32H7_CLK_PLLSRC:
      switch (
          PBL_FIELD_GET(RCC_PLLCKSELR_PLLSRC_MASK, prv_getreg(rcc, STM32H7_RCC_PLLCKSELR_OFFSET))) {
        case RCC_PLLCKSELR_PLLSRC_HSI:
          *parent = (struct pbl_clk)PBL_CLK(ctrl, STM32H7_CLK_HSI_CK);
          return 0;
        case RCC_PLLCKSELR_PLLSRC_CSI:
          *parent = (struct pbl_clk)PBL_CLK(ctrl, STM32H7_CLK_CSI_CK);
          return 0;
        case RCC_PLLCKSELR_PLLSRC_HSE:
          *parent = (struct pbl_clk)PBL_CLK(ctrl, STM32H7_CLK_HSE_CK);
          return 0;
        default:
          return -ENOENT;
      }
    case STM32H7_CLK_PLL1_P ... STM32H7_CLK_PLL3_R:
      *parent = (struct pbl_clk)PBL_CLK(ctrl, STM32H7_CLK_PLLSRC);
      return 0;
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
          *parent = (struct pbl_clk)PBL_CLK(ctrl, STM32H7_CLK_PLL1_P);
          return 0;
      }
    case STM32H7_CLK_SYS_D1CPRE:
      *parent = (struct pbl_clk)PBL_CLK(ctrl, STM32H7_CLK_SYS_CK);
      return 0;
    case STM32H7_CLK_HCLK:
      *parent = (struct pbl_clk)PBL_CLK(ctrl, STM32H7_CLK_SYS_D1CPRE);
      return 0;
    case STM32H7_CLK_PCLK1:
    case STM32H7_CLK_PCLK2:
    case STM32H7_CLK_PCLK3:
    case STM32H7_CLK_PCLK4:
      *parent = (struct pbl_clk)PBL_CLK(ctrl, STM32H7_CLK_HCLK);
      return 0;
    case STM32H7_CLK_USART1_CK:
    case STM32H7_CLK_UART8_CK:
      // USART1 and USART6 on PCLK2, the others on PCLK1
      sel = prv_getreg(rcc, STM32H7_RCC_D2CCIP2R_OFFSET);
      sel = (id == STM32H7_CLK_USART1_CK) ? PBL_FIELD_GET(RCC_D2CCIP2R_USART16SEL_MASK, sel)
                                          : PBL_FIELD_GET(RCC_D2CCIP2R_USART234578SEL_MASK, sel);
      switch (sel) {
        case RCC_D2CCIP2R_USARTSEL_PCLK:
          *parent = (struct pbl_clk)PBL_CLK(
              ctrl, (id == STM32H7_CLK_USART1_CK) ? STM32H7_CLK_PCLK2 : STM32H7_CLK_PCLK1);
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
    case STM32H7_CLK_FMC_CK:
      switch (PBL_FIELD_GET(RCC_D1CCIPR_FMCSEL_MASK, prv_getreg(rcc, STM32H7_RCC_D1CCIPR_OFFSET))) {
        case RCC_D1CCIPR_FMCSEL_HCLK:
          *parent = (struct pbl_clk)PBL_CLK(ctrl, STM32H7_CLK_HCLK);
          return 0;
        case RCC_D1CCIPR_FMCSEL_PLL1_Q:
          *parent = (struct pbl_clk)PBL_CLK(ctrl, STM32H7_CLK_PLL1_Q);
          return 0;
        default:
          return -ENOTSUP;
      }
    case STM32H7_CLK_RNG_CK:
      switch (
          PBL_FIELD_GET(RCC_D2CCIP2R_RNGSEL_MASK, prv_getreg(rcc, STM32H7_RCC_D2CCIP2R_OFFSET))) {
        case RCC_D2CCIP2R_RNGSEL_HSI48:
          *parent = (struct pbl_clk)PBL_CLK(ctrl, STM32H7_CLK_RC48_CK);
          return 0;
        case RCC_D2CCIP2R_RNGSEL_PLL1_Q:
          *parent = (struct pbl_clk)PBL_CLK(ctrl, STM32H7_CLK_PLL1_Q);
          return 0;
        case RCC_D2CCIP2R_RNGSEL_LSE:
          *parent = (struct pbl_clk)PBL_CLK(ctrl, STM32H7_CLK_LSE_CK);
          return 0;
        default:
          *parent = (struct pbl_clk)PBL_CLK(ctrl, STM32H7_CLK_LSI_CK);
          return 0;
      }
    case STM32H7_CLK_RTC_CK:
      switch (PBL_FIELD_GET(RCC_BDCR_RTCSEL_MASK, prv_getreg(rcc, STM32H7_RCC_BDCR_OFFSET))) {
        case RCC_BDCR_RTCSEL_LSE:
          *parent = (struct pbl_clk)PBL_CLK(ctrl, STM32H7_CLK_LSE_CK);
          return 0;
        case RCC_BDCR_RTCSEL_LSI:
          *parent = (struct pbl_clk)PBL_CLK(ctrl, STM32H7_CLK_LSI_CK);
          return 0;
        default:
          return -ENOENT;
      }
    case STM32H7_CLK_I2C4_CK:
      switch (
          PBL_FIELD_GET(RCC_D3CCIPR_I2C4SEL_MASK, prv_getreg(rcc, STM32H7_RCC_D3CCIPR_OFFSET))) {
        case RCC_D3CCIPR_I2C4SEL_PCLK:
          *parent = (struct pbl_clk)PBL_CLK(ctrl, STM32H7_CLK_PCLK4);
          return 0;
        case RCC_D3CCIPR_I2C4SEL_PLL3R:
          *parent = (struct pbl_clk)PBL_CLK(ctrl, STM32H7_CLK_PLL3_R);
          return 0;
        case RCC_D3CCIPR_I2C4SEL_HSI:
          *parent = (struct pbl_clk)PBL_CLK(ctrl, STM32H7_CLK_HSI_CK);
          return 0;
        default:
          *parent = (struct pbl_clk)PBL_CLK(ctrl, STM32H7_CLK_CSI_CK);
          return 0;
      }
    case STM32H7_CLK_SAI1_CK:
      return prv_get_sai_parent(
          ctrl,
          PBL_FIELD_GET(RCC_D2CCIP1R_SAI1SEL_MASK, prv_getreg(rcc, STM32H7_RCC_D2CCIP1R_OFFSET)),
          parent);
    case STM32H7_CLK_SAI4A_CK:
      return prv_get_sai_parent(
          ctrl,
          PBL_FIELD_GET(RCC_D3CCIPR_SAI4ASEL_MASK, prv_getreg(rcc, STM32H7_RCC_D3CCIPR_OFFSET)),
          parent);
    case STM32H7_CLK_LPTIM1_CK:
      return prv_get_lptim_parent(
          ctrl, STM32H7_CLK_PCLK1,
          PBL_FIELD_GET(RCC_D2CCIP2R_LPTIM1SEL_MASK, prv_getreg(rcc, STM32H7_RCC_D2CCIP2R_OFFSET)),
          parent);
    case STM32H7_CLK_LPTIM2_CK:
      return prv_get_lptim_parent(
          ctrl, STM32H7_CLK_PCLK4,
          PBL_FIELD_GET(RCC_D3CCIPR_LPTIM2SEL_MASK, prv_getreg(rcc, STM32H7_RCC_D3CCIPR_OFFSET)),
          parent);
    case STM32H7_CLK_USB1OTG_CK:
      switch (
          PBL_FIELD_GET(RCC_D2CCIP2R_USBSEL_MASK, prv_getreg(rcc, STM32H7_RCC_D2CCIP2R_OFFSET))) {
        case RCC_D2CCIP2R_USBSEL_PLL1_Q:
          *parent = (struct pbl_clk)PBL_CLK(ctrl, STM32H7_CLK_PLL1_Q);
          return 0;
        case RCC_D2CCIP2R_USBSEL_PLL3_Q:
          *parent = (struct pbl_clk)PBL_CLK(ctrl, STM32H7_CLK_PLL3_Q);
          return 0;
        case RCC_D2CCIP2R_USBSEL_HSI48:
          *parent = (struct pbl_clk)PBL_CLK(ctrl, STM32H7_CLK_RC48_CK);
          return 0;
        default:
          return -ENOENT;
      }
    default:
      if (!prv_is_gate(id)) {
        return -EINVAL;
      }
      *parent = (struct pbl_clk)PBL_CLK(ctrl, s_gates[id].parent);
      return 0;
  }
}

static int prv_get_rate(const struct pbl_clk_controller *ctrl, uint32_t id, uint64_t parent_mhz,
                        uint64_t *rate_mhz) {
  const struct pbl_clk_stm32h7 *rcc = prv_rcc(ctrl);
  uint32_t d1cfgr = prv_getreg(rcc, STM32H7_RCC_D1CFGR_OFFSET);
  uint32_t d2cfgr = prv_getreg(rcc, STM32H7_RCC_D2CFGR_OFFSET);
  uint32_t d3cfgr = prv_getreg(rcc, STM32H7_RCC_D3CFGR_OFFSET);

  switch (id) {
    case STM32H7_CLK_HSI_CK:
      *rate_mhz = (STM32H7_HSI_HZ * PBL_MILLIHZ_PER_HZ) >>
                  PBL_FIELD_GET(RCC_CR_HSIDIV_MASK, prv_getreg(rcc, STM32H7_RCC_CR_OFFSET));
      return 0;
    case STM32H7_CLK_CSI_CK:
      *rate_mhz = STM32H7_CSI_HZ * PBL_MILLIHZ_PER_HZ;
      return 0;
    case STM32H7_CLK_RC48_CK:
      *rate_mhz = STM32H7_HSI48_HZ * PBL_MILLIHZ_PER_HZ;
      return 0;
    case STM32H7_CLK_LSI_CK:
      *rate_mhz = STM32H7_LSI_HZ * PBL_MILLIHZ_PER_HZ;
      return 0;
    case STM32H7_CLK_PLL1_P ... STM32H7_CLK_PLL3_R: {
      uint32_t pll = prv_pll(id);
      uint32_t divm = PBL_FIELD_GET(RCC_PLLCKSELR_DIVM_MASK(pll),
                                    prv_getreg(rcc, STM32H7_RCC_PLLCKSELR_OFFSET));
      uint32_t divr = prv_getreg(rcc, STM32H7_RCC_PLLDIVR_OFFSET(pll));

      if (!prv_pll_on(rcc, pll) || divm == 0U) {
        *rate_mhz = 0U;
        return 0;
      }
      *rate_mhz = parent_mhz / divm * (PBL_FIELD_GET(RCC_PLLDIVR_N_MASK, divr) + 1U) /
                  (PBL_FIELD_GET(prv_pll_out_mask(id), divr) + 1U);
      return 0;
    }
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
    case STM32H7_CLK_PCLK3:
      *rate_mhz = parent_mhz / prv_apb_div(PBL_FIELD_GET(RCC_D1CFGR_D1PPRE_MASK, d1cfgr));
      return 0;
    case STM32H7_CLK_PCLK4:
      *rate_mhz = parent_mhz / prv_apb_div(PBL_FIELD_GET(RCC_D3CFGR_D3PPRE_MASK, d3cfgr));
      return 0;
    default:
      *rate_mhz = parent_mhz;
      return 0;
  }
}

static int prv_set_pllsrc(const struct pbl_clk_controller *ctrl, const struct pbl_clk *parent) {
  const struct pbl_clk_stm32h7 *rcc = prv_rcc(ctrl);
  uint32_t sel;

  if (parent->ctrl != ctrl) {
    return -EINVAL;
  }

  switch (parent->id) {
    case STM32H7_CLK_HSI_CK:
      sel = RCC_PLLCKSELR_PLLSRC_HSI;
      break;
    case STM32H7_CLK_CSI_CK:
      sel = RCC_PLLCKSELR_PLLSRC_CSI;
      break;
    case STM32H7_CLK_HSE_CK:
      sel = RCC_PLLCKSELR_PLLSRC_HSE;
      break;
    default:
      return -EINVAL;
  }

  // Every PLL takes it, so it changes only while they are off
  if (prv_any_pll_on(rcc)) {
    return PBL_FIELD_GET(RCC_PLLCKSELR_PLLSRC_MASK,
                         prv_getreg(rcc, STM32H7_RCC_PLLCKSELR_OFFSET)) == sel
               ? 0
               : -EBUSY;
  }

  if (sel == RCC_PLLCKSELR_PLLSRC_HSE) {
    int ret = prv_hse_on(rcc);

    if (ret != 0) {
      return ret;
    }
  }

  prv_modify(rcc, STM32H7_RCC_PLLCKSELR_OFFSET, RCC_PLLCKSELR_PLLSRC_MASK, sel);

  return 0;
}

// Divides the system clock down, so that the bus clocks stay within their maximums at its rate
static void prv_limit_buses(const struct pbl_clk_stm32h7 *rcc, uint64_t sys_mhz) {
  uint32_t d1cfgr = prv_getreg(rcc, STM32H7_RCC_D1CFGR_OFFSET);
  uint64_t cpu_mhz = sys_mhz / prv_ahb_div(PBL_FIELD_GET(RCC_D1CFGR_D1CPRE_MASK, d1cfgr));
  uint64_t hclk_mhz = cpu_mhz / prv_ahb_div(PBL_FIELD_GET(RCC_D1CFGR_HPRE_MASK, d1cfgr));
  uint32_t hdiv = 1U;
  uint32_t pdiv = 1U;

  while (hclk_mhz > HCLK_MAX_MHZ * hdiv && hdiv < 512U) {
    hdiv *= 2U;
  }
  if (hdiv > 1U) {
    prv_modify(
        rcc, STM32H7_RCC_D1CFGR_OFFSET, RCC_D1CFGR_HPRE_MASK,
        (uint32_t)prv_ahb_pre(prv_ahb_div(PBL_FIELD_GET(RCC_D1CFGR_HPRE_MASK, d1cfgr)) * hdiv));
    hclk_mhz /= hdiv;
  }

  while (hclk_mhz > PCLK_MAX_MHZ * pdiv && pdiv < 16U) {
    pdiv *= 2U;
  }
  if (pdiv > 1U) {
    uint32_t pre = (uint32_t)prv_apb_pre(pdiv);

    prv_modify(rcc, STM32H7_RCC_D1CFGR_OFFSET, RCC_D1CFGR_D1PPRE_MASK, pre);
    prv_modify(rcc, STM32H7_RCC_D2CFGR_OFFSET, RCC_D2CFGR_D2PPRE1_MASK, pre);
    prv_modify(rcc, STM32H7_RCC_D2CFGR_OFFSET, RCC_D2CFGR_D2PPRE2_MASK, pre);
    prv_modify(rcc, STM32H7_RCC_D3CFGR_OFFSET, RCC_D3CFGR_D3PPRE_MASK, pre);
  }
}

static int prv_set_sys_parent(const struct pbl_clk_controller *ctrl, const struct pbl_clk *parent) {
  const struct pbl_clk_stm32h7 *rcc = prv_rcc(ctrl);
  uint64_t rate_mhz;
  uint32_t sw;
  int ret;

  if (parent->ctrl != ctrl) {
    return -EINVAL;
  }

  switch (parent->id) {
    case STM32H7_CLK_HSI_CK:
      sw = RCC_CFGR_SW_HSI;
      break;
    case STM32H7_CLK_CSI_CK:
      sw = RCC_CFGR_SW_CSI;
      break;
    case STM32H7_CLK_HSE_CK:
      sw = RCC_CFGR_SW_HSE;
      ret = prv_hse_on(rcc);
      if (ret != 0) {
        return ret;
      }
      break;
    case STM32H7_CLK_PLL1_P:
      sw = RCC_CFGR_SW_PLL1;
      // Assigned parents come before assigned rates: the rate of PLL1 P starts it, then switches
      if ((prv_getreg(rcc, STM32H7_RCC_CR_OFFSET) & RCC_CR_PLLRDY(0U)) == 0U) {
        s_sys_on_pll1 = true;
        return 0;
      }
      break;
    default:
      return -EINVAL;
  }

  ret = pbl_clk_get_rate(parent, &rate_mhz);
  if (ret != 0) {
    return ret;
  }
  if (rate_mhz / prv_ahb_div(PBL_FIELD_GET(RCC_D1CFGR_D1CPRE_MASK,
                                           prv_getreg(rcc, STM32H7_RCC_D1CFGR_OFFSET))) >
      CPU_MAX_MHZ) {
    return -EINVAL;
  }

  prv_limit_buses(rcc, rate_mhz);
  prv_modify(rcc, STM32H7_RCC_CFGR_OFFSET, RCC_CFGR_SW_MASK, sw);
  while (PBL_FIELD_GET(RCC_CFGR_SWS_MASK, prv_getreg(rcc, STM32H7_RCC_CFGR_OFFSET)) != sw) {
  }
  prv_update_core_clock(ctrl);

  return 0;
}

// LPTIM1 and LPTIM2 take the same parents, their bus clock being PCLK1 or PCLK4
static int prv_set_lptim_parent(const struct pbl_clk_controller *ctrl, uint32_t offset,
                                uint32_t mask, uint32_t pclk, const struct pbl_clk *parent) {
  uint32_t sel;

  if (parent->ctrl != ctrl) {
    return -EINVAL;
  }

  switch (parent->id) {
    case STM32H7_CLK_PLL2_P:
      sel = RCC_LPTIMSEL_PLL2_P;
      break;
    case STM32H7_CLK_PLL3_R:
      sel = RCC_LPTIMSEL_PLL3_R;
      break;
    case STM32H7_CLK_LSE_CK:
      sel = RCC_LPTIMSEL_LSE;
      break;
    case STM32H7_CLK_LSI_CK:
      sel = RCC_LPTIMSEL_LSI;
      break;
    default:
      if (parent->id != pclk) {
        return -EINVAL;
      }
      sel = RCC_LPTIMSEL_PCLK;
      break;
  }

  prv_modify(prv_rcc(ctrl), offset, mask, sel);

  return 0;
}

// The selection only changes after a reset of the backup domain, which would stop the LSE and
// lose the time: it is set once, from the first boot
static int prv_set_rtc_parent(const struct pbl_clk_controller *ctrl, const struct pbl_clk *parent) {
  const struct pbl_clk_stm32h7 *rcc = prv_rcc(ctrl);
  uint32_t cur = PBL_FIELD_GET(RCC_BDCR_RTCSEL_MASK, prv_getreg(rcc, STM32H7_RCC_BDCR_OFFSET));
  uint32_t sel;

  if (parent->ctrl != ctrl) {
    return -EINVAL;
  }

  switch (parent->id) {
    case STM32H7_CLK_LSE_CK:
      sel = RCC_BDCR_RTCSEL_LSE;
      break;
    case STM32H7_CLK_LSI_CK:
      sel = RCC_BDCR_RTCSEL_LSI;
      break;
    default:
      return -EINVAL;
  }

  if (cur == sel) {
    return 0;
  }

  if (cur != RCC_BDCR_RTCSEL_NONE) {
    return -EBUSY;
  }

  pbl_modifyl(0U, PWR_CR1_DBP, rcc->pwr + STM32H7_PWR_CR1_OFFSET);
  prv_modify(rcc, STM32H7_RCC_BDCR_OFFSET, RCC_BDCR_RTCSEL_MASK, sel);

  return 0;
}

static int prv_set_usb_parent(const struct pbl_clk_controller *ctrl, const struct pbl_clk *parent) {
  uint32_t sel;

  if (parent->ctrl != ctrl) {
    return -EINVAL;
  }

  switch (parent->id) {
    case STM32H7_CLK_PLL1_Q:
      sel = RCC_D2CCIP2R_USBSEL_PLL1_Q;
      break;
    case STM32H7_CLK_PLL3_Q:
      sel = RCC_D2CCIP2R_USBSEL_PLL3_Q;
      break;
    case STM32H7_CLK_RC48_CK:
      sel = RCC_D2CCIP2R_USBSEL_HSI48;
      break;
    default:
      return -EINVAL;
  }

  prv_modify(prv_rcc(ctrl), STM32H7_RCC_D2CCIP2R_OFFSET, RCC_D2CCIP2R_USBSEL_MASK, sel);

  return 0;
}

static int prv_set_sai_parent(const struct pbl_clk_controller *ctrl, uint32_t offset, uint32_t mask,
                              const struct pbl_clk *parent) {
  uint32_t sel;

  if (parent->ctrl != ctrl) {
    return -EINVAL;
  }

  switch (parent->id) {
    case STM32H7_CLK_PLL1_Q:
      sel = RCC_SAISEL_PLL1Q;
      break;
    case STM32H7_CLK_PLL2_P:
      sel = RCC_SAISEL_PLL2P;
      break;
    case STM32H7_CLK_PLL3_P:
      sel = RCC_SAISEL_PLL3P;
      break;
    default:
      return -EINVAL;
  }

  prv_modify(prv_rcc(ctrl), offset, mask, sel);

  return 0;
}

static int prv_set_parent(const struct pbl_clk_controller *ctrl, uint32_t id,
                          const struct pbl_clk *parent) {
  switch (id) {
    case STM32H7_CLK_RTC_CK:
      return prv_set_rtc_parent(ctrl, parent);
    case STM32H7_CLK_SAI1_CK:
      return prv_set_sai_parent(ctrl, STM32H7_RCC_D2CCIP1R_OFFSET, RCC_D2CCIP1R_SAI1SEL_MASK,
                                parent);
    case STM32H7_CLK_SAI4A_CK:
      return prv_set_sai_parent(ctrl, STM32H7_RCC_D3CCIPR_OFFSET, RCC_D3CCIPR_SAI4ASEL_MASK,
                                parent);
    case STM32H7_CLK_LPTIM1_CK:
      return prv_set_lptim_parent(ctrl, STM32H7_RCC_D2CCIP2R_OFFSET, RCC_D2CCIP2R_LPTIM1SEL_MASK,
                                  STM32H7_CLK_PCLK1, parent);
    case STM32H7_CLK_LPTIM2_CK:
      return prv_set_lptim_parent(ctrl, STM32H7_RCC_D3CCIPR_OFFSET, RCC_D3CCIPR_LPTIM2SEL_MASK,
                                  STM32H7_CLK_PCLK4, parent);
    case STM32H7_CLK_USB1OTG_CK:
      return prv_set_usb_parent(ctrl, parent);
    case STM32H7_CLK_PLLSRC:
      return prv_set_pllsrc(ctrl, parent);
    case STM32H7_CLK_SYS_CK:
      return prv_set_sys_parent(ctrl, parent);
    default:
      return -EINVAL;
  }
}

// Configures a PLL for one of its outputs, with the first reference within limits and the other
// outputs at the same rate, then starts it. P of PLL1 only divides by even numbers. PLL1 must not
// run the system clock.
static int prv_pll_set_rate(const struct pbl_clk_stm32h7 *rcc, uint32_t id, uint64_t rate_mhz,
                            uint64_t src_mhz) {
  uint32_t pll = prv_pll(id);
  uint32_t step = id == STM32H7_CLK_PLL1_P ? 2U : 1U;
  uint32_t rge;

  if (src_mhz == 0U || rate_mhz == 0U) {
    return -EINVAL;
  }

  if (pll == 0U && PBL_FIELD_GET(RCC_CFGR_SWS_MASK, prv_getreg(rcc, STM32H7_RCC_CFGR_OFFSET)) ==
                       RCC_CFGR_SW_PLL1) {
    return -EBUSY;
  }

  for (uint32_t div = step; div <= PLL_DIVPQR_MAX; div += step) {
    uint64_t vco_mhz = rate_mhz * div;

    if (vco_mhz < PLL_VCO_MIN_MHZ || vco_mhz > PLL_VCO_MAX_MHZ) {
      continue;
    }

    for (uint32_t m = 1U; m <= PLL_DIVM_MAX; m++) {
      uint64_t ref_mhz = src_mhz / m;
      uint32_t n;

      if (src_mhz % m != 0U || ref_mhz < PLL_REF_MIN_MHZ || ref_mhz > PLL_REF_MAX_MHZ ||
          vco_mhz % ref_mhz != 0U) {
        continue;
      }
      n = (uint32_t)(vco_mhz / ref_mhz);
      if (n < PLL_DIVN_MIN || n > PLL_DIVN_MAX) {
        continue;
      }

      if (ref_mhz < MHZ(4)) {
        rge = RCC_PLLCFGR_RGE_2_4MHZ;
      } else if (ref_mhz < MHZ(8)) {
        rge = RCC_PLLCFGR_RGE_4_8MHZ;
      } else {
        rge = RCC_PLLCFGR_RGE_8_16MHZ;
      }

      pbl_modifyl(RCC_CR_PLLON(pll), 0U, rcc->base + STM32H7_RCC_CR_OFFSET);
      while ((prv_getreg(rcc, STM32H7_RCC_CR_OFFSET) & RCC_CR_PLLRDY(pll)) != 0U) {
      }

      prv_modify(rcc, STM32H7_RCC_PLLCKSELR_OFFSET, RCC_PLLCKSELR_DIVM_MASK(pll), m);
      pbl_modifyl(RCC_PLLCFGR_FRACEN(pll) | RCC_PLLCFGR_VCOSEL(pll) | RCC_PLLCFGR_RGE_MASK(pll) |
                      RCC_PLLCFGR_DIVEN_ALL(pll),
                  PBL_FIELD_PREP(RCC_PLLCFGR_RGE_MASK(pll), rge) | RCC_PLLCFGR_DIVEN_ALL(pll),
                  rcc->base + STM32H7_RCC_PLLCFGR_OFFSET);
      pbl_writel(PBL_FIELD_PREP(RCC_PLLDIVR_N_MASK, n - 1U) |
                     PBL_FIELD_PREP(RCC_PLLDIVR_P_MASK, div - 1U) |
                     PBL_FIELD_PREP(RCC_PLLDIVR_Q_MASK, div - 1U) |
                     PBL_FIELD_PREP(RCC_PLLDIVR_R_MASK, div - 1U),
                 rcc->base + STM32H7_RCC_PLLDIVR_OFFSET(pll));

      pbl_modifyl(0U, RCC_CR_PLLON(pll), rcc->base + STM32H7_RCC_CR_OFFSET);

      return PBL_POLL_BUSY_US((prv_getreg(rcc, STM32H7_RCC_CR_OFFSET) & RCC_CR_PLLRDY(pll)) != 0U,
                              10U, PLLRDY_TIMEOUT_US);
    }
  }

  return -EINVAL;
}

static int prv_set_rate(const struct pbl_clk_controller *ctrl, uint32_t id, uint64_t rate_mhz,
                        uint64_t parent_mhz) {
  const struct pbl_clk_stm32h7 *rcc = prv_rcc(ctrl);
  uint32_t div;
  int pre;

  if (prv_is_pll(id)) {
    int ret = prv_pll_set_rate(rcc, id, rate_mhz, parent_mhz);

    if (ret == 0 && id == STM32H7_CLK_PLL1_P && s_sys_on_pll1) {
      s_sys_on_pll1 = false;
      ret = prv_set_sys_parent(ctrl, &(struct pbl_clk)PBL_CLK(ctrl, STM32H7_CLK_PLL1_P));
    }
    return ret;
  }

  if (rate_mhz == 0U || parent_mhz % rate_mhz != 0U) {
    return -EINVAL;
  }
  div = (uint32_t)(parent_mhz / rate_mhz);

  switch (id) {
    case STM32H7_CLK_SYS_D1CPRE:
    case STM32H7_CLK_HCLK:
      pre = prv_ahb_pre(div);
      if (pre < 0 || rate_mhz > (id == STM32H7_CLK_HCLK ? HCLK_MAX_MHZ : CPU_MAX_MHZ)) {
        return -EINVAL;
      }
      prv_modify(rcc, STM32H7_RCC_D1CFGR_OFFSET,
                 id == STM32H7_CLK_HCLK ? RCC_D1CFGR_HPRE_MASK : RCC_D1CFGR_D1CPRE_MASK,
                 (uint32_t)pre);
      prv_update_core_clock(ctrl);
      return 0;
    case STM32H7_CLK_PCLK1:
    case STM32H7_CLK_PCLK2:
    case STM32H7_CLK_PCLK3:
    case STM32H7_CLK_PCLK4:
      pre = prv_apb_pre(div);
      if (pre < 0 || rate_mhz > PCLK_MAX_MHZ) {
        return -EINVAL;
      }
      if (id == STM32H7_CLK_PCLK3) {
        prv_modify(rcc, STM32H7_RCC_D1CFGR_OFFSET, RCC_D1CFGR_D1PPRE_MASK, (uint32_t)pre);
      } else if (id == STM32H7_CLK_PCLK4) {
        prv_modify(rcc, STM32H7_RCC_D3CFGR_OFFSET, RCC_D3CFGR_D3PPRE_MASK, (uint32_t)pre);
      } else {
        prv_modify(rcc, STM32H7_RCC_D2CFGR_OFFSET,
                   id == STM32H7_CLK_PCLK1 ? RCC_D2CFGR_D2PPRE1_MASK : RCC_D2CFGR_D2PPRE2_MASK,
                   (uint32_t)pre);
      }
      return 0;
    default:
      return -EINVAL;
  }
}

uint32_t pbl_clk_stm32h7_reset_flags(const struct pbl_clk_controller *ctrl, bool clear) {
  const struct pbl_clk_stm32h7 *rcc = prv_rcc(ctrl);
  uint32_t rsr = prv_getreg(rcc, STM32H7_RCC_RSR_OFFSET);

  if (clear) {
    pbl_modifyl(0U, RCC_RSR_RMVF, rcc->base + STM32H7_RCC_RSR_OFFSET);
  }

  return rsr;
}

const struct pbl_clk_controller_ops pbl_clk_stm32h7_ops = {
  .enable = prv_enable,
  .disable = prv_disable,
  .get_rate = prv_get_rate,
  .get_parent = prv_get_parent,
  .set_parent = prv_set_parent,
  .set_rate = prv_set_rate,
};

#ifdef CONFIG_CPU_CORTEX_M4
// The Cortex-M7 set up the supply and the clock tree before starting this core
int pbl_clk_stm32h7_init(const struct pbl_device *dev) {
  const struct pbl_clk_stm32h7 *rcc = PBL_CONTAINER_OF(dev, const struct pbl_clk_stm32h7, ctrl.dev);

  prv_update_core_clock(&rcc->ctrl);

  return pbl_clk_assign(rcc->assigned);
}
#else
// The supply configuration takes once per power-on: a different one written before stays
int pbl_clk_stm32h7_init(const struct pbl_device *dev) {
  const struct pbl_clk_stm32h7 *rcc = PBL_CONTAINER_OF(dev, const struct pbl_clk_stm32h7, ctrl.dev);
  uintptr_t cr3 = rcc->pwr + STM32H7_PWR_CR3_OFFSET;
  int ret;

  pbl_modifyl(PWR_CR3_SUPPLY_MASK, rcc->supply, cr3);
  if ((pbl_readl(cr3) & PWR_CR3_SUPPLY_MASK) != rcc->supply) {
    return -EIO;
  }

  ret = PBL_POLL_BUSY_US((pbl_readl(rcc->pwr + STM32H7_PWR_CSR1_OFFSET) & PWR_CSR1_ACTVOSRDY) != 0U,
                         10U, ACTVOSRDY_TIMEOUT_US);
  if (ret != 0) {
    return ret;
  }

  // Any rate runs at VOS1
  pbl_modifyl(PWR_D3CR_VOS_MASK, PBL_FIELD_PREP(PWR_D3CR_VOS_MASK, PWR_D3CR_VOS_SCALE1),
              rcc->pwr + STM32H7_PWR_D3CR_OFFSET);
  ret = PBL_POLL_BUSY_US((pbl_readl(rcc->pwr + STM32H7_PWR_D3CR_OFFSET) & PWR_D3CR_VOSRDY) != 0U,
                         10U, ACTVOSRDY_TIMEOUT_US);
  if (ret != 0) {
    return ret;
  }

  return pbl_clk_assign(rcc->assigned);
}
#endif

#include PBL_DT_INSTANCES(st_stm32h743_rcc_clk)
