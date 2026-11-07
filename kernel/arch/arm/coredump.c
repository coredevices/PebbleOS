/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdbool.h>
#include <stdint.h>

#include <pbl/coredump/arch.h>
#include <pbl/coredump/coredump.h>
#include <pbl/kernel/compiler.h>

#include <cmsis_core.h>

#define RETURNS_TO_PSP(exc_return) (((exc_return) & 0x4) == 0x4)

#define XPSR_STACK_PADDING 0x200u

#define REG_UNKNOWN 0xa5a5a5a5u

#define EXC_RETURN_NO_FP 0x10u

#define FRAME_WORDS    8
#define FP_FRAME_WORDS 18

#define IPSR_MASK        0x1ffu
#define IPSR_HARD_FAULT  3u
#define IPSR_USAGE_FAULT 6u

// CFSR causes that mean the faulting code's registers never made it to its stack.
#define CFSR_STACKING_ERRORS ((1u << 4) | (1u << 12) | (1u << 20)) // MSTKERR, STKERR, STKOF

extern uint8_t __RAM_start__;
extern uint8_t __RAM_size__;

PBL_COREDUMP_REGION_DEFINE(ram, &__RAM_start__, (size_t)&__RAM_size__);
PBL_COREDUMP_REGION_DEFINE(nvic_iser, (const void *)&NVIC->ISER, sizeof(NVIC->ISER));
PBL_COREDUMP_REGION_DEFINE(nvic_ispr, (const void *)&NVIC->ISPR, sizeof(NVIC->ISPR));
PBL_COREDUMP_REGION_DEFINE(nvic_iabr, (const void *)&NVIC->IABR, sizeof(NVIC->IABR));

/*
 * Written by NMI_Handler, whose assembly depends on the layout. Tools can find it through the
 * s_saved_registers symbol. sp is the MSP and lr the EXC_RETURN value at handler entry; pc is the
 * handler's own address.
 */
static struct PBL_PACKED PBL_ALIGNED(4) {
  uint32_t core_reg[PBL_THREAD_REG_COUNT];
  struct pbl_coredump_extra_regs extra_reg;
} s_saved_registers;

/*
 * Written by PBL_ARCH_COREDUMP_FAULT_ENTRY on every fault, whose assembly depends on the layout.
 * Recovered faults leave it behind, so it only means something if the dump preempted a fault
 * handler, which s_in_fault_handler tells.
 */
PBL_USED struct {
  uint32_t r4_r11[8];
  uint32_t exc_return;
  uint32_t msp;
  uint32_t psp;
  uint32_t cfsr;
} arch_coredump_fault;

static bool s_in_fault_handler;

void arch_coredump_trigger(void) {
  SCB->ICSR = SCB_ICSR_NMIPENDSET_Msk;
  __DSB();
  __ISB();
}

void PBL_NAKED NMI_Handler(void) {
  // Save what isn't stacked on exception entry before any C code can clobber it.
  __asm volatile(
      "  ldr r0, =%[s_saved_registers]\n"
      "  stmia r0!, {r4-r11}          \n"
      "  str sp, [r0, #4]!            \n" // sp, skipping r12
      "  str lr, [r0, #4]!            \n" // lr
      "  mrs r1, xpsr                 \n"
      "  mrs r2, msp                  \n"
      "  mrs r3, psp                  \n"
      "  adds r0, #8                  \n" // skip pc
      "  stmia r0!, {r1-r3}           \n" // xpsr, msp, psp
      "  b arch_coredump_handler_c    \n"
      :
      : [s_saved_registers] "i"(&s_saved_registers.core_reg[PBL_THREAD_REG_R4])
      : "r0", "r1", "r2", "r3", "cc");
}

PBL_EXTERNALLY_VISIBLE void arch_coredump_handler_c(void) {
  uint32_t *process_sp = (uint32_t *)(RETURNS_TO_PSP(s_saved_registers.core_reg[PBL_THREAD_REG_LR])
                                          ? s_saved_registers.extra_reg.psp
                                          : s_saved_registers.extra_reg.msp);
  s_saved_registers.core_reg[PBL_THREAD_REG_R0] = process_sp[0];
  s_saved_registers.core_reg[PBL_THREAD_REG_R1] = process_sp[1];
  s_saved_registers.core_reg[PBL_THREAD_REG_R2] = process_sp[2];
  s_saved_registers.core_reg[PBL_THREAD_REG_R3] = process_sp[3];
  s_saved_registers.core_reg[PBL_THREAD_REG_R12] = process_sp[4];
  // Make it look like the processor had halted at the start of the handler.
  s_saved_registers.core_reg[PBL_THREAD_REG_PC] = (uint32_t)NMI_Handler;
  s_saved_registers.extra_reg.primask = __get_PRIMASK();
  s_saved_registers.extra_reg.basepri = __get_BASEPRI();
  s_saved_registers.extra_reg.faultmask = __get_FAULTMASK();
  s_saved_registers.extra_reg.control = __get_CONTROL();

  if (!RETURNS_TO_PSP(s_saved_registers.core_reg[PBL_THREAD_REG_LR])) {
    const uint32_t exception = process_sp[7] & IPSR_MASK;

    s_in_fault_handler = exception >= IPSR_HARD_FAULT && exception <= IPSR_USAGE_FAULT;
  }

  pbl_coredump_arch_handler();
}

static void prv_fault_regs(uint32_t regs[PBL_THREAD_REG_COUNT]) {
  const uint32_t exc_return = arch_coredump_fault.exc_return;
  const uint32_t sp =
      RETURNS_TO_PSP(exc_return) ? arch_coredump_fault.psp : arch_coredump_fault.msp;

  for (int i = 0; i < PBL_THREAD_REG_COUNT; i++) {
    regs[i] = REG_UNKNOWN;
  }
  for (int i = 0; i < 8; i++) {
    regs[PBL_THREAD_REG_R4 + i] = arch_coredump_fault.r4_r11[i];
  }
  regs[PBL_THREAD_REG_SP] = sp;

  if (arch_coredump_fault.cfsr & CFSR_STACKING_ERRORS) {
    return;
  }

  const uint32_t *frame = (const uint32_t *)sp;
  regs[PBL_THREAD_REG_R0] = frame[0];
  regs[PBL_THREAD_REG_R1] = frame[1];
  regs[PBL_THREAD_REG_R2] = frame[2];
  regs[PBL_THREAD_REG_R3] = frame[3];
  regs[PBL_THREAD_REG_R12] = frame[4];
  regs[PBL_THREAD_REG_LR] = frame[5];
  regs[PBL_THREAD_REG_PC] = frame[6];
  regs[PBL_THREAD_REG_XPSR] = frame[7];

  uint32_t words = FRAME_WORDS;
  if (!(exc_return & EXC_RETURN_NO_FP)) {
    words += FP_FRAME_WORDS;
  }
  if (frame[7] & XPSR_STACK_PADDING) {
    words++;
  }
  regs[PBL_THREAD_REG_SP] = (uint32_t)&frame[words];
}

void arch_coredump_thread_regs(uint32_t regs[PBL_THREAD_REG_COUNT]) {
  if (RETURNS_TO_PSP(s_saved_registers.core_reg[PBL_THREAD_REG_LR])) {
    for (int i = 0; i < PBL_THREAD_REG_COUNT; i++) {
      regs[i] = s_saved_registers.core_reg[i];
    }
    regs[PBL_THREAD_REG_SP] = s_saved_registers.extra_reg.psp;
    return;
  }

  if (s_in_fault_handler && RETURNS_TO_PSP(arch_coredump_fault.exc_return)) {
    prv_fault_regs(regs);
    return;
  }

  // The thread was interrupted by an exception: only what is stacked on the PSP is known, and SP
  // is off if FP registers were stacked, which only the outermost EXC_RETURN can tell.
  for (int i = 0; i < PBL_THREAD_REG_COUNT; i++) {
    regs[i] = REG_UNKNOWN;
  }
  const uint32_t *sp = (const uint32_t *)s_saved_registers.extra_reg.psp;
  regs[PBL_THREAD_REG_R0] = sp[0];
  regs[PBL_THREAD_REG_R1] = sp[1];
  regs[PBL_THREAD_REG_R2] = sp[2];
  regs[PBL_THREAD_REG_R3] = sp[3];
  regs[PBL_THREAD_REG_R12] = sp[4];
  regs[PBL_THREAD_REG_LR] = sp[5];
  regs[PBL_THREAD_REG_PC] = sp[6];
  regs[PBL_THREAD_REG_XPSR] = sp[7];
  regs[PBL_THREAD_REG_SP] = (uint32_t)&sp[(sp[7] & XPSR_STACK_PADDING) ? 9 : 8];
}

bool arch_coredump_exception_regs(uint32_t regs[PBL_THREAD_REG_COUNT]) {
  if (RETURNS_TO_PSP(s_saved_registers.core_reg[PBL_THREAD_REG_LR])) {
    return false;
  }
  if (s_in_fault_handler && !RETURNS_TO_PSP(arch_coredump_fault.exc_return)) {
    prv_fault_regs(regs);
    return true;
  }
  for (int i = 0; i < PBL_THREAD_REG_COUNT; i++) {
    regs[i] = s_saved_registers.core_reg[i];
  }
  return true;
}

const struct pbl_coredump_extra_regs *arch_coredump_extra_regs(void) {
  return &s_saved_registers.extra_reg;
}
