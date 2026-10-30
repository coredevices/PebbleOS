/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <pbl/kernel/assert.h>
#include <pbl/kernel/compiler.h>
#include <pbl/kernel/syscall.h>

PBL_WEAK bool s_syscall_did_fail;

PBL_WEAK void stubs_syscall_init(void) {
  s_syscall_did_fail = false;
}

PBL_WEAK [[noreturn]] void pbl_syscall_fail(void) {
  s_syscall_did_fail = true;
  printf("Warning: Syscall failed!\n");

  // Use cl_assert_panic() if you want to catch this getting hit.
  PBL_ASSERTN(false);
}

PBL_WEAK void pbl_syscall_verify_user(const void *buf, size_t num_bytes) {
  if (!buf) {
    pbl_syscall_fail();
  }
  // TODO: What else can we check here?
  return;
}

PBL_WEAK int pbl_user_copy_from(void *dst, const void *src, size_t num_bytes) {
  if (!src) {
    return -EFAULT;
  }
  memcpy(dst, src, num_bytes);
  return 0;
}

PBL_WEAK int pbl_user_copy_to(void *dst, const void *src, size_t num_bytes) {
  if (!dst) {
    return -EFAULT;
  }
  memcpy(dst, src, num_bytes);
  return 0;
}

PBL_WEAK int pbl_user_strnlen(const char *str, size_t max) {
  if (!str) {
    return -EFAULT;
  }
  const char *nul = memchr(str, '\0', (max == SIZE_MAX) ? SIZE_MAX : max + 1);
  return (nul == nullptr) ? -EFAULT : (int)(nul - str);
}

PBL_WEAK void pbl_syscall_copy_out(struct pbl_syscall_copy_out *out) {
  if (out->dst != nullptr && pbl_user_copy_to(out->dst, out->src, out->num_bytes) < 0) {
    pbl_syscall_fail();
  }
}
