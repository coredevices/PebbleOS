/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "cores.h"

#include <pbl/kernel/delay.h>
#include <pbl/kernel/idle.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/kernel/reboot.h>
#include <pbl/kernel/section.h>
#include <pbl/util/bits.h>

#include <devicetree/regs.h>
#include <hardware/stm32h7_exti.h>
#include <hardware/stm32h7_hsem.h>
#include <hardware/stm32h7_pwr.h>
#include <hardware/stm32h7_rcc.h>

#define PWR(reg)    (PBL_DT_REG_PWR + STM32H7_PWR_##reg##_OFFSET)
#define EXTI(reg)   (PBL_DT_REG_EXTI + STM32H7_EXTI_##reg##_OFFSET)
#define HSEM(reg)   (PBL_DT_REG_HSEM + STM32H7_HSEM_##reg##_OFFSET)
#define HSEM_R(n)   (PBL_DT_REG_HSEM + STM32H7_HSEM_R_OFFSET(n))
#define HSEM_RLR(n) (PBL_DT_REG_HSEM + STM32H7_HSEM_RLR_OFFSET(n))
#define RCC(reg)    (PBL_DT_REG_RCC + STM32H7_RCC_##reg##_OFFSET)

// Long enough for the Cortex-M4 to run its power-off handlers
#define CM4_ACK_TIMEOUT_US 200000U

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

// From RAM, as the flash, in the domain of the Cortex-M7, goes down once it has the
// acknowledgement: taking the semaphore with a read tells the Cortex-M7 this core is done
[[noreturn]] static PBL_SECTION_RAM void prv_ack_and_stop(void) {
  (void)pbl_readl(HSEM_RLR(STM32H7_CM4_SEM_ACK));

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

  prv_ack_and_stop();
}

#else

// A running Cortex-M4 would keep its domain out of Standby, and run from the flash once its
// domain is down: release the semaphore it watches, which runs its power-off, and wait until it
// takes the one that tells it is done
static void prv_stop_cm4(void) {
  const uint32_t acked = HSEM_R_LOCK | PBL_FIELD_PREP(HSEM_R_COREID_MASK, HSEM_COREID_CM4);

  if ((pbl_readl(RCC(GCR)) & RCC_GCR_BOOT_C2) == 0U) {
    return;
  }

  pbl_modifyl(0U, RCC_AHB4ENR_HSEMEN, RCC(AHB4ENR));
  (void)pbl_readl(HSEM_RLR(STM32H7_CM4_SEM_POWER_OFF));
  pbl_writel(PBL_FIELD_PREP(HSEM_R_COREID_MASK, HSEM_COREID_CM7),
             HSEM_R(STM32H7_CM4_SEM_POWER_OFF));

  for (uint32_t us = 0U; us < CM4_ACK_TIMEOUT_US; us += 10U) {
    if ((pbl_readl(HSEM_R(STM32H7_CM4_SEM_ACK)) & (HSEM_R_LOCK | HSEM_R_COREID_MASK)) == acked) {
      return;
    }
    pbl_busy_wait_us(10U);
  }
}

// Standby for every domain, with only the wakeup pins drivers armed to wake the Cortex-M7. Only
// the backup domain, with the RTC, keeps running; the SoC starts again from reset.
[[noreturn]] void pbl_soc_power_off(void) {
  (void)pbl_irq_disable_all();

  prv_stop_cm4();
  prv_mask_wakeups(EXTI_LINES2_WKUP);
  pbl_modifyl(0U, PWR_CPUCR_PDDS_D1 | PWR_CPUCR_PDDS_D2 | PWR_CPUCR_PDDS_D3 | PWR_CPUCR_CSSF,
              PWR(CPUCR));

  while (true) {
    pbl_cpu_idle_deep();
  }
}

#endif
