/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

struct pbl_thread;

extern struct pbl_thread *pbl_cur;

#ifdef CONFIG_USERSPACE
//! What unprivileged code, which cannot read pbl_cur, takes for it: the last thread that ran
//! unprivileged.
extern struct pbl_thread *arch_cur_user;

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
  //! Has run unprivileged, so arch_cur_user follows it.
  bool user;
#endif
};

static inline struct pbl_thread *arch_cur_get(void) {
#ifdef CONFIG_USERSPACE
  uint32_t ipsr;
  uint32_t control;

  __asm volatile("mrs %0, ipsr\n\tmrs %1, control" : "=r"(ipsr), "=r"(control));
  if (ipsr == 0U && (control & 1U) != 0U) {
    return arch_cur_user;
  }
#endif
  return pbl_cur;
}
