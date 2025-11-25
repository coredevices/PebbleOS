/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/pebble_tasks.h"
#include "system/passert.h"

#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>

static bool s_syscall_did_fail;

void stubs_syscall_init(void) {
  s_syscall_did_fail = false;
}

NORETURN syscall_failed(void) {
  s_syscall_did_fail = true;
  printf("Warning: Syscall failed!\n");

  // Use cl_assert_passert() if you want to catch this getting hit.
  PBL_ASSERTN(false);
}

#define assert_syscall_failed() \
  cl_assert_equal_b(true, s_syscall_did_fail);

bool syscall_made_from_userspace(void) {
  return true;
}

void syscall_assert_userspace_buffer(const void* buf, size_t num_bytes) {
  if (!buf) {
    syscall_failed();
  }
  // TODO: What else can we check here?
  return;
}

void syscall_init_context() {
}

void syscall_redirect_syscall_exit(PebbleTask task, void (*func)(void)) {
}

void syscall_reset(PebbleTask task) {
}


