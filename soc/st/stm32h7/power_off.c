/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/idle.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/kernel/reboot.h>
#include <pbl/kernel/section.h>
#include <pbl/util/bits.h>

#include <devicetree/regs.h>
#include <hardware/stm32h7_exti.h>
#include <hardware/stm32h7_pwr.h>

#define PWR(reg)  (PBL_DT_REG_PWR + STM32H7_PWR_##reg##_OFFSET)
#define EXTI(reg) (PBL_DT_REG_EXTI + STM32H7_EXTI_##reg##_OFFSET)

// Only the wakeup pins the drivers armed, on the Cortex-M7, wake the system: the timers and the
// other lines of this core that wake it from Stop are masked
static void prv_mask_wakeups(uint32_t wkup) {
  pbl_writel(0U, EXTI(CORE_IMR1));
  pbl_writel(0U, EXTI(CORE_EMR1));
  pbl_writel(wkup, EXTI(CORE_IMR2));
  pbl_writel(0U, EXTI(CORE_EMR2));
  pbl_writel(0U, EXTI(CORE_IMR3));
  pbl_writel(0U, EXTI(CORE_EMR3));
  pbl_writel(UINT32_MAX, EXTI(CORE_PR1));
  pbl_writel(UINT32_MAX, EXTI(CORE_PR2));
  pbl_writel(UINT32_MAX, EXTI(CORE_PR3));
}

#ifdef CONFIG_SOC_STM32H7_CM4

// From RAM: once this core stops, the domain of the Cortex-M7 goes down with the flash, and a
// wakeup would have nothing to run
[[noreturn]] static PBL_SECTION_RAM void prv_stop(void) {
  while (true) {
    pbl_cpu_idle_deep();
  }
}

// Asked by the Cortex-M7: Standby for every domain, as far as this core is concerned, with
// nothing to wake it
[[noreturn]] void pbl_soc_power_off(void) {
  (void)pbl_irq_disable_all();

  prv_mask_wakeups(0U);
  pbl_modifyl(PWR_CPUCR_RUN_D3, PWR_CPUCR_PDDS_D1 | PWR_CPUCR_PDDS_D2 | PWR_CPUCR_PDDS_D3,
              PWR(CORECR));

  prv_stop();
}

#else

// Standby for every domain, with only the wakeup pins drivers armed to wake the Cortex-M7. Only
// the backup domain, with the RTC, keeps running; the SoC starts again from reset.
[[noreturn]] void pbl_soc_power_off(void) {
  (void)pbl_irq_disable_all();

  prv_mask_wakeups(EXTI_LINES2_WKUP);
  pbl_modifyl(0U, PWR_CPUCR_PDDS_D1 | PWR_CPUCR_PDDS_D2 | PWR_CPUCR_PDDS_D3 | PWR_CPUCR_CSSF,
              PWR(CPUCR));

  while (true) {
    pbl_cpu_idle_deep();
  }
}

#endif
