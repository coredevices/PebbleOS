/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <pbl/kernel/compiler.h>
#include <pbl/util/listify.h>

#include <kernel.h>

extern uint8_t _estack[];

void Reset_Handler(void);

void arch_irq_spurious(void);
void arch_irq_spurious(void) {
  // An exception or IRQ with no handler; IPSR holds its number.
  KERNEL_ASSERT(false);
}

PBL_WEAK PBL_ALIAS("arch_irq_spurious") void NMI_Handler(void);
PBL_WEAK PBL_ALIAS("arch_irq_spurious") void HardFault_Handler(void);
PBL_WEAK PBL_ALIAS("arch_irq_spurious") void MemManage_Handler(void);
PBL_WEAK PBL_ALIAS("arch_irq_spurious") void BusFault_Handler(void);
PBL_WEAK PBL_ALIAS("arch_irq_spurious") void UsageFault_Handler(void);
PBL_WEAK PBL_ALIAS("arch_irq_spurious") void DebugMon_Handler(void);
void SVC_Handler(void);
void PendSV_Handler(void);
void SysTick_Handler(void);

#define ARCH_VECTOR_DECLARE(n) PBL_WEAK PBL_ALIAS("arch_irq_spurious") void pbl_isr_##n(void);
PBL_LISTIFY(CONFIG_NUM_IRQS, ARCH_VECTOR_DECLARE)

PBL_EXTERNALLY_VISIBLE PBL_SECTION(".isr_vector") const void *const arch_vector_table[] = {
  _estack,
  Reset_Handler,
  NMI_Handler,
  HardFault_Handler,
  MemManage_Handler,
  BusFault_Handler,
  UsageFault_Handler,
  0,
  0,
  0,
  0,
  SVC_Handler,
  DebugMon_Handler,
  0,
  PendSV_Handler,
  SysTick_Handler,
#define ARCH_VECTOR_ENTRY(n) pbl_isr_##n,
  PBL_LISTIFY(CONFIG_NUM_IRQS, ARCH_VECTOR_ENTRY)
};
