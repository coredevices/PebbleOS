/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/logging/logging.h>

#include <syscall/syscall_internal.h>

// A native build runs everything at one privilege level: syscalls are plain
// calls and every buffer is trusted.

extern void sys_app_fault(uint32_t lr);

[[noreturn]] void syscall_failed(void) {
  PBL_LOG_WRN("Bad syscall!");
  sys_app_fault((uint32_t)(uintptr_t)PBL_RETURN_ADDRESS(0));
  for (;;) {
  }
}

void syscall_assert_userspace_buffer(const void *buf, size_t num_bytes) {
}

bool syscall_internal_check_return_address(void *ret_addr) {
  return false;
}

const MpuRegion *syscall_get_stack_guard_region(PebbleTask task) {
  return nullptr;
}

uint16_t syscall_app_stack_free_bytes(void) {
  return 0xFFFF;
}

uint16_t syscall_worker_stack_free_bytes(void) {
  return 0xFFFF;
}
