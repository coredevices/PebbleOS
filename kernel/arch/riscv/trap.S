/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl_arch_csr.h>

#include "riscv.h"

#if !defined(CONFIG_RISCV_HW_STACKING) || !defined(CONFIG_RISCV_CLIC)
#error "The trap entry is for cores with a CLIC that stack the caller-saved registers themselves"
#endif

/*
 * Every trap enters here. On an interrupt the core has already stacked ra, t0-t2, a0-a5 and the
 * mcause and mepc of the trap it was in, if any, where sp pointed. mret returns as mcause and mepc
 * say, then takes the registers back, the previous mcause and mepc with them. Anything else is a
 * fault.
 */
    .section .text.arch_riscv_trap, "ax"
    .balign 64
    .global arch_riscv_trap
arch_riscv_trap:
    csrr t0, mcause
    bgez t0, prv_fault
    slli t0, t0, 20
    srli t0, t0, 20
    addi t0, t0, -CONFIG_RISCV_CLIC_IRQ_OFFSET
    li t1, RISCV_SWITCH_IRQ
    beq t0, t1, prv_switch

    /* The outermost interrupt moves to the ISR stack, keeping the thread's below its frame */
    la t1, arch_riscv_isr_nesting
    lw t2, 0(t1)
    addi t2, t2, 1
    sw t2, 0(t1)
    mv a0, sp
    li t1, 1
    bne t2, t1, 1f
    la sp, _estack
1:
    addi sp, sp, -16
    sw a0, 0(sp)

    slli t0, t0, 2
    la t1, arch_riscv_irq_table
    add t1, t1, t0
    lw t1, 0(t1)
    /* More urgent levels may preempt the handler */
    csrsi mstatus, ARCH_MSTATUS_MIE
    jalr t1
    csrci mstatus, ARCH_MSTATUS_MIE

    lw sp, 0(sp)
    la t1, arch_riscv_isr_nesting
    lw t2, 0(t1)
    addi t2, t2, -1
    sw t2, 0(t1)
    mret

/*
 * Thread switch, at the first level above the threads' so only ever from a thread: s0, s1, and the mcause and
 * mepc to return with go below the frame the core stacked, which is all of the thread's context.
 */
prv_switch:
    soc_riscv_switch_clear t0
    la t0, arch_riscv_started
    lw t1, 0(t0)
    beqz t1, 2f
    addi sp, sp, -RISCV_CALLEE_SIZE
    sw s0, 0(sp)
    sw s1, 4(sp)
    csrr t1, mepc
    sw t1, 8(sp)
    csrr t1, mcause
    sw t1, 12(sp)
    la t0, pbl_cur
    lw t1, 0(t0)
    sw sp, 0(t1)
    j 3f
2:
    /* The first switch drops what booted the kernel */
    li t1, 1
    sw t1, 0(t0)
3:
    la t2, sched_next
    lw t2, 0(t2)
    la t0, pbl_cur
    sw t2, 0(t0)
    lw sp, 0(t2)
    lw s0, 0(sp)
    lw s1, 4(sp)
    lw t0, 8(sp)
    csrw mepc, t0
    lw t0, 12(sp)
    csrw mcause, t0
    addi sp, sp, RISCV_CALLEE_SIZE
    mret

prv_fault:
    la sp, _estack
    csrr a0, mcause
    csrr a1, mepc
    csrr a2, mtval
    mv a3, ra
    j arch_riscv_fault

/* Where the core starts: the first instruction of the image */
    .section .text.arch_riscv_reset, "ax"
    .global arch_riscv_reset
arch_riscv_reset:
    csrci mstatus, ARCH_MSTATUS_MIE
    la sp, _estack
    la t0, arch_riscv_trap
    ori t0, t0, ARCH_MTVEC_MODE_CLIC
    csrw mtvec, t0
    la t0, arch_riscv_vectors
    csrw ARCH_CSR_MTVT, t0
    j kernel_prep_c
