/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

//! Addresses outside the caller's memory, for the fuzzer to pass as arguments.
struct syscall_fuzz_targets {
  //! Kernel buffer that no syscall may change, see sys_syscall_fuzz_check_canary().
  uintptr_t canary;
  size_t canary_size;
  uintptr_t kernel_stack;
  uintptr_t syscall_stack;
  uintptr_t worker_ram;
  uintptr_t flash;
  uintptr_t app_ram_start;
  uintptr_t app_ram_end;
};

//! Called, unprivileged, instead of returning from a rejected syscall.
typedef void (*syscall_fuzz_reject_fn)(void);

//! While armed, a syscall the calling thread makes resumes it at @p on_reject
//! when the syscall rejects its arguments, instead of killing the process,
//! on the stack the syscall was made from. nullptr disarms.
void sys_syscall_fuzz_arm(syscall_fuzz_reject_fn on_reject);

void sys_syscall_fuzz_get_targets(struct syscall_fuzz_targets *targets);

//! @return whether the canary is intact. It is restored either way.
bool sys_syscall_fuzz_check_canary(void);

//! Called when a syscall fails: where to resume the caller, or nullptr.
syscall_fuzz_reject_fn syscall_fuzz_take_failure(void);
