/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "cores.h"

#include <pbl/device.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/kernel/reboot.h>
#include <pbl/kernel/work.h>
#include <pbl/util/bits.h>

#include <devicetree/regs.h>
#include <hardware/stm32h7_hsem.h>
#include <hardware/stm32h7_rcc.h>

#define HSEM(reg)   (PBL_DT_REG_HSEM + STM32H7_HSEM_##reg##_OFFSET)
#define HSEM_R(n)   (PBL_DT_REG_HSEM + STM32H7_HSEM_R_OFFSET(n))
#define HSEM_RLR(n) (PBL_DT_REG_HSEM + STM32H7_HSEM_RLR_OFFSET(n))
#define RCC(reg)    (PBL_DT_REG_RCC + STM32H7_RCC_##reg##_OFFSET)

// The power-off of the Cortex-M7 is that of the system: this core follows it, its drivers first
static void prv_power_off(struct pbl_work *work) {
  pbl_power_off();
}

static PBL_WORK_DEFINE(s_power_off, prv_power_off);

static void prv_hsem_isr(void) {
  pbl_writel(PBL_BIT(STM32H7_CM4_SEM_POWER_OFF), HSEM(C2ICR));
  (void)pbl_work_submit(&pbl_sys_workq, &s_power_off);
}

PBL_IRQ_CONNECT(HSEM2, (1U << CONFIG_NUM_IRQ_PRIO_BITS) - 1U, prv_hsem_isr, , 0);

// The flash, in the domain of the Cortex-M7, has to stay up while this core runs from it: so does
// that domain, until this core stops too
static int prv_init(const struct pbl_device *dev) {
  pbl_modifyl(0U, RCC_AHB3ENR_FLASHEN, RCC(AHB3ENR));
  pbl_modifyl(0U, RCC_AHB4ENR_HSEMEN, RCC(AHB4ENR));

  // Left taken by an earlier power-off whose domain did not reach Standby
  pbl_writel(PBL_FIELD_PREP(HSEM_R_COREID_MASK, HSEM_COREID_CM4), HSEM_R(STM32H7_CM4_SEM_ACK));

  pbl_writel(PBL_BIT(STM32H7_CM4_SEM_POWER_OFF), HSEM(C2ICR));
  pbl_modifyl(0U, PBL_BIT(STM32H7_CM4_SEM_POWER_OFF), HSEM(C2IER));
  pbl_irq_enable(PBL_IRQN(HSEM2));

  return 0;
}

PBL_DEVICE_STATE_DEFINE(s_cm4);
static const struct pbl_device s_cm4 = PBL_DEVICE_INIT(s_cm4, "cm4", prv_init, nullptr);
PBL_DEVICE_REGISTER(s_cm4, &s_cm4);
