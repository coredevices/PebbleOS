/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/kernel/idle.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/kernel/reboot.h>
#include <pbl/kernel/section.h>
#include <pbl/soc/st/pwr.h>
#include <pbl/util/bits.h>

#include <devicetree/regs.h>
#include <hardware/stm32_gpio.h>
#include <hardware/stm32h7_exti.h>
#include <hardware/stm32h7_pwr.h>

#define PWR(reg)  (PBL_DT_REG_PWR + STM32H7_PWR_##reg##_OFFSET)
#define EXTI(reg) (PBL_DT_REG_EXTI + STM32H7_EXTI_##reg##_OFFSET)

// Only the wakeup pins armed, on the Cortex-M7, wake the system: the timers and the
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

// A pending interrupt ends WFI at once, masked or not: none may stay enabled or pending, as the
// timer's would
static void prv_irqs_off(void) {
  for (pbl_irq_t irq = 0; irq < CONFIG_NUM_IRQS; irq++) {
    pbl_irq_disable(irq);
    pbl_irq_clear_pending(irq);
  }
}

#ifdef CONFIG_CPU_CORTEX_M4

// Only the Cortex-M7 powers the system off, and wakes
int pbl_soc_stm32h7_wakeup_pin_set(uintptr_t port, uint32_t pin, bool on) {
  return -ENOTSUP;
}

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
  prv_irqs_off();

  prv_mask_wakeups(0U);
  pbl_modifyl(PWR_CPUCR_RUN_D3, PWR_CPUCR_PDDS_D1 | PWR_CPUCR_PDDS_D2 | PWR_CPUCR_PDDS_D3,
              PWR(CORECR));

  prv_stop();
}

#else

// WKUP1 to WKUP6, as port (A = 0) and pin
static const struct {
  uint8_t port;
  uint8_t pin;
} s_wkup_pins[PWR_WKUP_PINS] = {{0U, 0U}, {0U, 2U}, {8U, 8U}, {2U, 13U}, {8U, 11U}, {2U, 1U}};

// GPIO port of each wakeup pin armed, 0 when not
static uintptr_t s_wake[PWR_WKUP_PINS];

int pbl_soc_stm32h7_wakeup_pin_set(uintptr_t port, uint32_t pin, bool on) {
  for (uint32_t n = 0U; n < PWR_WKUP_PINS; n++) {
    if (s_wkup_pins[n].port == (port / STM32_GPIO_PORT_SIZE) % 16U && s_wkup_pins[n].pin == pin) {
      s_wake[n] = on ? port : 0U;
      return 0;
    }
  }

  return -ENOTSUP;
}

// Each wakeup pin armed wakes on the edge away from the level it is at, with the pull of its line
static void prv_arm_wakeup_pins(void) {
  uint32_t epr = 0U;

  for (uint32_t n = 0U; n < PWR_WKUP_PINS; n++) {
    uintptr_t port = s_wake[n];
    uint32_t pin = s_wkup_pins[n].pin;

    if (port == 0U) {
      continue;
    }

    epr |= PWR_WKUPEPR_EN(n) |
           PBL_FIELD_PREP(
               PWR_WKUPEPR_PUPD_MASK(n),
               PBL_FIELD_GET(GPIO_PUPDR_MASK(pin), pbl_readl(port + STM32_GPIO_PUPDR_OFFSET)));
    if ((pbl_readl(port + STM32_GPIO_IDR_OFFSET) & PBL_BIT(pin)) != 0U) {
      epr |= PWR_WKUPEPR_FALLING(n);
    }
  }

  pbl_writel(epr, PWR(WKUPEPR));
  pbl_writel(PWR_WKUPCR_ALL, PWR(WKUPCR));
}

// Standby for every domain, with only the wakeup pins armed to wake the Cortex-M7. Only
// the backup domain, with the RTC, keeps running; the SoC starts again from reset.
[[noreturn]] void pbl_soc_power_off(void) {
  (void)pbl_irq_disable_all();
  prv_irqs_off();
  prv_arm_wakeup_pins();

  prv_mask_wakeups(EXTI_LINES2_WKUP);
  pbl_modifyl(0U, PWR_CPUCR_PDDS_D1 | PWR_CPUCR_PDDS_D2 | PWR_CPUCR_PDDS_D3 | PWR_CPUCR_CSSF,
              PWR(CPUCR));
  // A domain goes to Standby only when every core asks for it: a Cortex-M4 that never ran, or
  // missed the doorbell, did not. One that ran has set the same bits by now.
  pbl_modifyl(0U, PWR_CPU2CR_PDDS_D1 | PWR_CPU2CR_PDDS_D2 | PWR_CPU2CR_PDDS_D3, PWR(CPU2CR));

  while (true) {
    pbl_cpu_idle_deep();
  }
}

#endif
