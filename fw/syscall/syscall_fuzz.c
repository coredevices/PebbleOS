/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "syscall_fuzz.h"

#include <string.h>

#include <pbl/kernel/syscall.h>
#include <pbl/kernel/thread.h>

#include <kernel/memory_layout.h>

#define CANARY_BYTE 0xa5

extern const uint32_t __kernel_main_stack_start__[];
extern const uint32_t __WORKER_RAM__[];
extern const uint32_t __FLASH_start__[];

static syscall_fuzz_reject_fn s_on_reject;
static uint32_t s_armed_thread;
static uint8_t s_canary[64];

syscall_fuzz_reject_fn syscall_fuzz_take_failure(void) {
  if (s_on_reject == nullptr || pbl_thread_id(pbl_thread_current()) != s_armed_thread) {
    return nullptr;
  }
  return s_on_reject;
}

PBL_SYSCALL(void, sys_syscall_fuzz_arm, (VAL, syscall_fuzz_reject_fn, on_reject)) {
  s_on_reject = on_reject;
  s_armed_thread = pbl_thread_id(pbl_thread_current());
  memset(s_canary, CANARY_BYTE, sizeof(s_canary));
}

PBL_SYSCALL(void, sys_syscall_fuzz_get_targets, (OUT, struct syscall_fuzz_targets *, targets)) {
  const MpuRegion *app = memory_layout_get_app_region();
  *targets = (struct syscall_fuzz_targets){
    .canary = (uintptr_t)s_canary,
    .canary_size = sizeof(s_canary),
    .kernel_stack = (uintptr_t)__kernel_main_stack_start__,
    .syscall_stack = (uintptr_t)pbl_thread_current()->syscall_stack,
    .worker_ram = (uintptr_t)__WORKER_RAM__,
    .flash = (uintptr_t)__FLASH_start__,
    .app_ram_start = app->base_address,
    .app_ram_end = app->base_address + app->size,
  };
}

PBL_SYSCALL(bool, sys_syscall_fuzz_check_canary) {
  for (size_t i = 0; i < sizeof(s_canary); i++) {
    if (s_canary[i] != CANARY_BYTE) {
      memset(s_canary, CANARY_BYTE, sizeof(s_canary));
      return false;
    }
  }
  return true;
}
