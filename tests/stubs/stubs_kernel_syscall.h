/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdio.h>

extern bool s_syscall_did_fail;

void stubs_syscall_init(void);

[[noreturn]] void pbl_syscall_fail(void);

#define assert_syscall_failed() cl_assert_equal_b(true, s_syscall_did_fail);

void pbl_syscall_verify_user(const void *buf, size_t num_bytes);
