/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/kernel/compiler.h>

#include <cmsis_core.h>

//! Defines the syscall stub @p name, placed in .syscall_text, and opens the
//! body of __@p name, which the stub runs privileged.
#define ARCH_SYSCALL_DEFINE(ret, name, ...)                             \
  ret PBL_NAKED PBL_SECTION(".syscall_text." #name) name(__VA_ARGS__) { \
    __asm volatile(                                                     \
        "push { lr } \n"                                                \
        "bl arch_syscall_enter \n"                                      \
        "svc 2 \n"                                                      \
        "b __" #name "\n");                                             \
  }                                                                     \
  ret PBL_EXTERNALLY_VISIBLE PBL_USED __##name(__VA_ARGS__)

//! True in the body of a syscall stub called from unprivileged code. Only valid
//! in the body itself or a PBL_ALWAYS_INLINE helper of it.
#define ARCH_SYSCALL_FROM_USER() arch_syscall_returns_to_user(PBL_RETURN_ADDRESS(0))

static inline bool arch_is_user_context(void) {
  return __get_IPSR() == 0 && (__get_CONTROL() & CONTROL_nPRIV_Msk) != 0;
}

void arch_syscall_enter(void);
bool arch_syscall_returns_to_user(void *ret_addr);

//! Stack pointer the syscall in progress was made from.
uintptr_t arch_syscall_caller_sp(void);
