/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <kernel/util/stack_info.h>
#include <syscall/syscall_define.h>

PBL_SYSCALL(uint32_t, sys_stack_free_bytes) {
  return stack_free_bytes();
}
