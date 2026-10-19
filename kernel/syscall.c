/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "kernel.h"

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <pbl/kernel/syscall.h>
#include <pbl/logging/logging.h>

[[noreturn]] void pbl_syscall_fail(void) {
  const uintptr_t lr = (uintptr_t)PBL_RETURN_ADDRESS(0);

  PBL_ASSERT(!pbl_is_user_context(), "Insufficient Privileges!");
  PBL_LOG_WRN("Bad syscall!");
  pbl_kernel_syscall_fault(lr);
}

#ifdef CONFIG_USERSPACE

// Bytes of the caller's memory from @p start on. Memory below the stack
// pointer the syscall was made from is never accepted: it is the unused part
// of the caller's stack, or the guard below it.
static bool prv_user_window(uintptr_t start, size_t *avail) {
  const struct pbl_thread *t = pbl_cur;
  const uintptr_t lower = arch_syscall_caller_sp();
  if (lower < t->user_start || start < lower || start > t->user_end) {
    return false;
  }
  *avail = t->user_end - start;
  return true;
}

bool pbl_user_access_ok(const void *buf, size_t num_bytes) {
  size_t avail;
  return prv_user_window((uintptr_t)buf, &avail) && num_bytes <= avail;
}

int pbl_user_strnlen(const char *str, size_t max) {
  size_t window = 0;
  if (!prv_user_window((uintptr_t)str, &window)) {
    return -EFAULT;
  }
  if (max < window) {
    window = max + 1;
  }
  const size_t len = strnlen(str, window);
  if (len == window) {
    return -EFAULT;
  }
  return (int)len;
}

#else

// A single privilege level: every pointer is the caller's.

bool pbl_user_access_ok(const void *buf, size_t num_bytes) {
  return true;
}

int pbl_user_strnlen(const char *str, size_t max) {
  const char *nul = memchr(str, '\0', (max == SIZE_MAX) ? SIZE_MAX : max + 1);
  return (nul == nullptr) ? -EFAULT : (int)(nul - str);
}

#endif

void pbl_syscall_verify_user(const void *buf, size_t num_bytes) {
  if (!pbl_user_access_ok(buf, num_bytes)) {
    PBL_LOG_ERR("syscall failure! %p..%p is not in user memory", buf,
                (const char *)buf + num_bytes);
    pbl_syscall_fail();
  }
}

int pbl_user_copy_from(void *dst, const void *src, size_t num_bytes) {
  if (!pbl_user_access_ok(src, num_bytes)) {
    return -EFAULT;
  }
  memcpy(dst, src, num_bytes);
  return 0;
}

int pbl_user_copy_to(void *dst, const void *src, size_t num_bytes) {
  if (!pbl_user_access_ok(dst, num_bytes)) {
    return -EFAULT;
  }
  memcpy(dst, src, num_bytes);
  return 0;
}

void pbl_syscall_copy_out(struct pbl_syscall_copy_out *out) {
  if (out->dst != nullptr && pbl_user_copy_to(out->dst, out->src, out->num_bytes) < 0) {
    pbl_syscall_fail();
  }
}
