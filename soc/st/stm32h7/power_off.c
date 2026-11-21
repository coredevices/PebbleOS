/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/idle.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/kernel/reboot.h>
#include <pbl/util/bits.h>

#include <devicetree/regs.h>
#include <hardware/stm32h7_exti.h>
#include <hardware/stm32h7_pwr.h>

#define PWR(reg)  (PBL_DT_REG_PWR + STM32H7_PWR_##reg##_OFFSET)
#define EXTI(reg) (PBL_DT_REG_EXTI + STM32H7_EXTI_##reg##_OFFSET)

// Standby for every domain, of both cores, with only the wakeup pins drivers armed to wake the
// Cortex-M7: the timers and other lines that wake it from Stop are masked. Only the backup domain,
// with the RTC, keeps running; the SoC starts again from reset.
[[noreturn]] void pbl_soc_power_off(void) {
  (void)pbl_irq_disable_all();

  pbl_writel(0U, EXTI(C1IMR1));
  pbl_writel(0U, EXTI(C1EMR1));
  pbl_writel(EXTI_LINES2_WKUP, EXTI(C1IMR2));
  pbl_writel(0U, EXTI(C1EMR2));
  pbl_writel(0U, EXTI(C1IMR3));
  pbl_writel(0U, EXTI(C1EMR3));
  pbl_writel(UINT32_MAX, EXTI(C1PR1));
  pbl_writel(UINT32_MAX, EXTI(C1PR2));
  pbl_writel(UINT32_MAX, EXTI(C1PR3));

  pbl_modifyl(0U, PWR_CPUCR_PDDS_D1 | PWR_CPUCR_PDDS_D2 | PWR_CPUCR_PDDS_D3 | PWR_CPUCR_CSSF,
              PWR(CPUCR));
  pbl_modifyl(0U, PWR_CPU2CR_PDDS_D1 | PWR_CPU2CR_PDDS_D2 | PWR_CPU2CR_PDDS_D3, PWR(CPU2CR));

  while (true) {
    pbl_cpu_idle_deep();
  }
}
