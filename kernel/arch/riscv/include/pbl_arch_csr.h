/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#define ARCH_CSR_MSTATUS       0x300
#define ARCH_CSR_MTVT          0x307
#define ARCH_CSR_MCOUNTINHIBIT 0x320
#define ARCH_CSR_MCYCLE        0xb00

#define ARCH_MSTATUS_MIE 0x8U

#define ARCH_MCAUSE_INTERRUPT (1U << 31)
#define ARCH_MCAUSE_MPP_M     (3U << 28)
#define ARCH_MCAUSE_MPIE      (1U << 27)
#define ARCH_MCAUSE_CODE_MSK  0xfffU

#define ARCH_MTVEC_MODE_CLIC 3U

#ifndef __ASSEMBLER__

#define ARCH_STR_(x) #x
#define ARCH_STR(x)  ARCH_STR_(x)

#define arch_csr_read(csr)                                           \
  ({                                                                 \
    unsigned long __v;                                               \
    __asm volatile("csrr %0, " ARCH_STR(csr) : "=r"(__v)::"memory"); \
    __v;                                                             \
  })

#define arch_csr_write(csr, val) \
  __asm volatile("csrw " ARCH_STR(csr) ", %0" ::"rK"((unsigned long)(val)) : "memory")

#define arch_csr_set(csr, val) \
  __asm volatile("csrs " ARCH_STR(csr) ", %0" ::"rK"((unsigned long)(val)) : "memory")

#define arch_csr_clear(csr, val) \
  __asm volatile("csrc " ARCH_STR(csr) ", %0" ::"rK"((unsigned long)(val)) : "memory")

#endif
