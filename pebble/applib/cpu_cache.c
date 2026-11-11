/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "cpu_cache.h"

#include <pbl/kernel/cache.h>
#include <pbl/kernel/syscall.h>

PBL_SYSCALL(void, memory_cache_flush, (VAL, void *, start), (VAL, size_t, size)) {
  if (from_user) {
    pbl_syscall_verify_user(start, size);
  }

  pbl_dcache_flush_range(start, size);
  pbl_icache_invalidate_range(start, size);
}
