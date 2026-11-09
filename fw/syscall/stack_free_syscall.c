/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/debug.h>
#include <pbl/kernel/syscall.h>

PBL_SYSCALL(uint32_t, sys_stack_free_bytes) {
  return pbl_stack_free();
}
