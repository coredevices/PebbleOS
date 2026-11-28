/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <devicetree/regs.h>
#include <soc_riscv.h>

//! Line the SoC raises to switch threads, at the first level above the threads'.
#define RISCV_SWITCH_IRQ SOC_RISCV_SWITCH_IRQ

//! CLIC registers, one word per line: pending, enable, attributes and control, a byte each.
#define CLICINT(n)      (PBL_DT_REG_CLIC + 0x1000U + ((uint32_t)(n) * 4U))
#define CLICINT_IP      (1U << 0)
#define CLICINT_IE      (1U << 8)
#define CLICINT_CTL_POS 24U

//! What a switched-out thread keeps below the frame the core stacked.
#define RISCV_CALLEE_SIZE 16

#ifndef __ASSEMBLER__

#include <stdint.h>

extern uint32_t arch_riscv_started;

//! Frame the core stacks on an interrupt, lowest address first. Bit 0 of mepc tells that the core
//! moved sp 4 bytes further down first, to keep the frame 8-byte aligned.
struct arch_riscv_irq_frame {
  uint32_t ra;
  uint32_t t2;
  uint32_t t1;
  uint32_t t0;
  uint32_t a5;
  uint32_t a4;
  uint32_t a3;
  uint32_t a2;
  uint32_t a1;
  uint32_t a0;
  uint32_t mcause;
  uint32_t mepc;
};

//! Context of a switched-out thread, with the mepc and mcause it returns with.
struct arch_riscv_context {
  uint32_t s0;
  uint32_t s1;
  uint32_t mepc;
  uint32_t mcause;
  struct arch_riscv_irq_frame frame;
};

static_assert(sizeof(struct arch_riscv_context) - sizeof(struct arch_riscv_irq_frame) ==
              RISCV_CALLEE_SIZE);

#endif
