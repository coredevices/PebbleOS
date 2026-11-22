/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/delay.h>
#include <pbl/kernel/idle.h>
#include <pbl/kernel/mmio.h>
#include <pbl/kernel/time.h>
#include <pbl/soc/st/idle.h>
#include <pbl/util/bits.h>
#include <pbl/util/size.h>

#ifdef CONFIG_RAM
#include <pbl/drivers/ram.h>
#endif

#include <devicetree/idle_states.h>
#include <devicetree/regs.h>
#include <hardware/stm32h7_pwr.h>
#include <hardware/stm32h7_rcc.h>

#define RCC(reg) (PBL_DT_REG_RCC + STM32H7_RCC_##reg##_OFFSET)
#define PWR(reg) (PBL_DT_REG_PWR + STM32H7_PWR_##reg##_OFFSET)

// The HSE crystal starts within a few milliseconds, the PLLs within a few hundred microseconds
#define OSC_TIMEOUT_US 100000U

// What Stop turns off and the CPU turns back on: the HSE, the HSI48 and the PLLs
#define RCC_CR_STOP_OFF \
  (RCC_CR_HSEON | RCC_CR_HSI48ON | RCC_CR_PLLON(0U) | RCC_CR_PLLON(1U) | RCC_CR_PLLON(2U))

static void prv_on(uint32_t on, uint32_t rdy) {
  pbl_modifyl(0U, on, RCC(CR));
  (void)PBL_POLL_BUSY_US((pbl_readl(RCC(CR)) & rdy) == rdy, 1U, OSC_TIMEOUT_US);
}

// Back from Stop the CPU runs on the HSI: the oscillators and PLLs that ran come back, then the
// system clock, once the core supply is back at its level. Those still running are left as they
// are.
static void prv_clocks_restore(uint32_t cr, uint32_t sw) {
  uint32_t rdy = 0U;

  if ((cr & RCC_CR_HSEON) != 0U) {
    prv_on(RCC_CR_HSEON, RCC_CR_HSERDY);
  }

  if ((cr & RCC_CR_HSI48ON) != 0U) {
    rdy |= RCC_CR_HSI48RDY;
  }
  for (uint32_t pll = 0U; pll < 3U; pll++) {
    if ((cr & RCC_CR_PLLON(pll)) != 0U) {
      rdy |= RCC_CR_PLLRDY(pll);
    }
  }
  prv_on(cr & (RCC_CR_HSI48ON | RCC_CR_PLLON(0U) | RCC_CR_PLLON(1U) | RCC_CR_PLLON(2U)), rdy);

  (void)PBL_POLL_BUSY_US((pbl_readl(PWR(D3CR)) & PWR_D3CR_VOSRDY) != 0U, 1U, OSC_TIMEOUT_US);

  pbl_modifyl(RCC_CFGR_SW_MASK, PBL_FIELD_PREP(RCC_CFGR_SW_MASK, sw), RCC(CFGR));
  while (PBL_FIELD_GET(RCC_CFGR_SWS_MASK, pbl_readl(RCC(CFGR))) != sw) {
  }
}

static void prv_enter_stop(void) {
  uint32_t cr = pbl_readl(RCC(CR)) & RCC_CR_STOP_OFF;
  uint32_t sw = PBL_FIELD_GET(RCC_CFGR_SW_MASK, pbl_readl(RCC(CFGR)));

#ifdef CONFIG_RAM
  pbl_ram_suspend(EXTRAM);
#endif

  // A domain goes to Standby only when every core asks for it: the Cortex-M7 asks for Stop, but
  // when powering off, and the Cortex-M4 leaves the choice to it. The regulator and the flash go
  // to low power.
#ifdef CONFIG_SOC_STM32H7_CM4
  pbl_modifyl(PWR_CPUCR_RUN_D3, PWR_CPUCR_PDDS_D1 | PWR_CPUCR_PDDS_D2 | PWR_CPUCR_PDDS_D3,
              PWR(CORECR));
#else
  pbl_modifyl(PWR_CPUCR_PDDS_D1 | PWR_CPUCR_PDDS_D2 | PWR_CPUCR_PDDS_D3 | PWR_CPUCR_RUN_D3, 0U,
              PWR(CORECR));
#endif
  pbl_modifyl(0U, PWR_CR1_LPDS | PWR_CR1_FLPS, PWR(CR1));

  pbl_cpu_idle_deep();

  // The clocks only stop with the whole system, when the other core is stopped too
  prv_clocks_restore(cr, sw);

#ifdef CONFIG_RAM
  pbl_ram_resume(EXTRAM);
#endif
}

const struct pbl_idle_state pbl_soc_idle_states[] = {
  [SOC_STM32_IDLE_WFI] = {.name = "wfi", .min_ticks = 1, .exit_ticks = 0, .enter = pbl_cpu_idle},
#ifdef CONFIG_TIMER_STM32_LPTIM
  [SOC_STM32_IDLE_STOP] = {
    .name = "stop",
    .min_ticks = PBL_US_TO_TICKS_CEIL(PBL_DT_IDLE_STATE_STOP_MIN_RESIDENCY_US),
    .exit_ticks = PBL_US_TO_TICKS_CEIL(PBL_DT_IDLE_STATE_STOP_EXIT_LATENCY_US),
    .enter = prv_enter_stop,
  },
#endif
};
const size_t pbl_soc_num_idle_states = PBL_ARRAY_SIZE(pbl_soc_idle_states);
