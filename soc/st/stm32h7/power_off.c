/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/kernel/idle.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/kernel/reboot.h>
#include <pbl/soc/st/pwr.h>
#include <pbl/util/bits.h>

#include <devicetree/regs.h>
#include <hardware/stm32_gpio.h>
#include <hardware/stm32h7_exti.h>
#include <hardware/stm32h7_pwr.h>

#define PWR(reg)  (PBL_DT_REG_PWR + STM32H7_PWR_##reg##_OFFSET)
#define EXTI(reg) (PBL_DT_REG_EXTI + STM32H7_EXTI_##reg##_OFFSET)

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

// A pending interrupt ends WFI at once, masked or not: none may stay enabled or pending, as the
// timer's would
static void prv_irqs_off(void) {
  for (pbl_irq_t irq = 0; irq < CONFIG_NUM_IRQS; irq++) {
    pbl_irq_disable(irq);
    pbl_irq_clear_pending(irq);
  }
}

// Standby for every domain, of both cores, with only the wakeup pins armed to wake the
// Cortex-M7: the timers and other lines that wake it from Stop are masked. Only the backup domain,
// with the RTC, keeps running; the SoC starts again from reset.
[[noreturn]] void pbl_soc_power_off(void) {
  (void)pbl_irq_disable_all();
  prv_irqs_off();
  prv_arm_wakeup_pins();

  pbl_writel(0U, EXTI(C1IMR1));
  pbl_writel(0U, EXTI(C1EMR1));
  pbl_writel(EXTI_LINES2_WKUP, EXTI(C1IMR2));
  pbl_writel(0U, EXTI(C1EMR2));
  pbl_writel(0U, EXTI(C1IMR3));
  pbl_writel(0U, EXTI(C1EMR3));
  pbl_writel(UINT32_MAX, EXTI(C1PR1));
  pbl_writel(UINT32_MAX, EXTI(C1PR2));
  pbl_writel(UINT32_MAX, EXTI(C1PR3));

  // A running Cortex-M4 asks for Standby itself, as it idles in Stop
  pbl_modifyl(0U, PWR_CPUCR_PDDS_D1 | PWR_CPUCR_PDDS_D2 | PWR_CPUCR_PDDS_D3 | PWR_CPUCR_CSSF,
              PWR(CPUCR));

  while (true) {
    pbl_cpu_idle_deep();
  }
}
