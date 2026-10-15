/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <pbl/kernel/compiler.h>

#include <kernel/pebble_tasks.h>
#include <syscall/syscall_define.h>
#include <system/passert.h>

PBL_WEAK bool s_syscall_did_fail;

PBL_WEAK void stubs_syscall_init(void) {
  s_syscall_did_fail = false;
}

PBL_WEAK [[noreturn]] void syscall_failed(void) {
  s_syscall_did_fail = true;
  printf("Warning: Syscall failed!\n");

  // Use cl_assert_passert() if you want to catch this getting hit.
  PBL_ASSERTN(false);
}

PBL_WEAK bool syscall_made_from_userspace(void) {
  return true;
}

PBL_WEAK void syscall_assert_userspace_buffer(const void *buf, size_t num_bytes) {
  if (!buf) {
    syscall_failed();
  }
  // TODO: What else can we check here?
  return;
}

PBL_WEAK int syscall_copy_from_user(void *dst, const void *src, size_t num_bytes) {
  if (!src) {
    return -EFAULT;
  }
  memcpy(dst, src, num_bytes);
  return 0;
}

PBL_WEAK int syscall_copy_to_user(void *dst, const void *src, size_t num_bytes) {
  if (!dst) {
    return -EFAULT;
  }
  memcpy(dst, src, num_bytes);
  return 0;
}

PBL_WEAK int syscall_user_strnlen(const char *str, size_t max) {
  if (!str) {
    return -EFAULT;
  }
  const char *nul = memchr(str, '\0', (max == SIZE_MAX) ? SIZE_MAX : max + 1);
  return (nul == nullptr) ? -EFAULT : (int)(nul - str);
}

PBL_WEAK void syscall_copy_out(struct syscall_copy_out *out) {
  if (out->dst != nullptr && syscall_copy_to_user(out->dst, out->src, out->num_bytes) < 0) {
    syscall_failed();
  }
}

PBL_WEAK void syscall_init_context() {
}

PBL_WEAK void syscall_redirect_syscall_exit(PebbleTask task, void (*func)(void)) {
}

PBL_WEAK void syscall_reset(PebbleTask task) {
}
