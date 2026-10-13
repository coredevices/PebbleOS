/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdio.h>

#include <kernel/pebble_tasks.h>
#include <system/passert.h>

extern bool s_syscall_did_fail;

void stubs_syscall_init(void);

[[noreturn]] void syscall_failed(void);

#define assert_syscall_failed() cl_assert_equal_b(true, s_syscall_did_fail);

bool syscall_made_from_userspace(void);

void syscall_assert_userspace_buffer(const void *buf, size_t num_bytes);

void syscall_init_context();

void syscall_redirect_syscall_exit(PebbleTask task, void (*func)(void));

void syscall_reset(PebbleTask task);
