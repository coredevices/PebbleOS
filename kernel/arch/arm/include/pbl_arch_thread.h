/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#ifdef CONFIG_USERSPACE
//! State of a pbl_user_call() call in progress.
struct arch_user_call {
  uint32_t saved_r4_r11[8];
  uintptr_t caller_lr;
  uintptr_t entry_sp;
  uintptr_t outer_syscall_lr;
  uintptr_t outer_syscall_sp;
  bool active;
};
#endif

struct pbl_arch_thread {
  //! MPU region registers written on every switch to the thread: four
  //! (RBAR, RASR/RLAR) pairs for the configurable regions.
  uint32_t mpu[8];
#ifdef CONFIG_USERSPACE
  //! Where the syscall in progress returns to.
  uintptr_t syscall_lr;
  //! Stack pointer the syscall in progress was made from.
  uintptr_t syscall_sp;
  struct arch_user_call user_call;
#endif
};
