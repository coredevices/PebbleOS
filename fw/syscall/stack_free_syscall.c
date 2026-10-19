/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/syscall.h>

#include <kernel/util/stack_info.h>

PBL_SYSCALL(uint32_t, sys_stack_free_bytes) {
  return stack_free_bytes();
}
