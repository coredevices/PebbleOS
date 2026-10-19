/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/syscall.h>

#include <kernel/pebble_tasks.h>

//! Any function defined with this macro will be privileged.
//! Privileges are raised upon entry to the syscall, and dropped
//! once the syscall is exited (unless the caller was originally privileged).
#define DEFINE_SYSCALL(retType, funcName, ...) \
  PRV_DEFINE_SYSCALL(".syscall_text.", retType, funcName, __VA_ARGS__)

//! Call this from privileged mode whenever a syscall did something wrong. This will kick out the
//! misbehaving app.
[[noreturn]] void syscall_failed(void);

//! Call this from privileged mode when entering a syscall to ensure that provided
//! pointers are in the app's memory space, rather than in the kernel. If the buffer is not,
//! syscall_failed is called.
void syscall_assert_userspace_buffer(const void *buf, size_t num_bytes);

//! Copy @p num_bytes from app memory at @p src to kernel memory at @p dst.
//! @return 0, or -EFAULT if @p src is not in the app's memory space.
int syscall_copy_from_user(void *dst, const void *src, size_t num_bytes);

//! Copy @p num_bytes from kernel memory at @p src to app memory at @p dst.
//! @return 0, or -EFAULT if @p dst is not in the app's memory space.
int syscall_copy_to_user(void *dst, const void *src, size_t num_bytes);

struct LogBinaryMessage;

//! Like syscall_assert_userspace_buffer(), for a binary log message: also checks that its message
//! and filename are terminated within the caller's memory.
void syscall_assert_userspace_log_message(const struct LogBinaryMessage *log_message);

//! @return The length of the NUL-terminated string at @p str, or -EFAULT if it
//! is longer than @p max or does not end within the app's memory space.
int syscall_user_strnlen(const char *str, size_t max);

//! PRIVILEGE_WAS_ELEVATED:
//! @return True if the most recent syscall originated from userspace, resulting in a privilege
//! escalation. It can only be called from a function created with DEFINE_SYSCALL, or from a
//! PBL_ALWAYS_INLINE helper of one; the firmware build checks this.
#if defined(UNITTEST)

// TODO: really implement privilege escalation in unit tests. See PBL-9688
#define PRV_DEFINE_SYSCALL(section, retType, funcName, ...) retType funcName(__VA_ARGS__)

#if UNITTEST_WITH_SYSCALL_PRIVILEGES
bool arch_syscall_returns_to_user(void *ret_addr);
#define PRIVILEGE_WAS_ELEVATED (arch_syscall_returns_to_user(PBL_RETURN_ADDRESS(0)))
#else
#define PRIVILEGE_WAS_ELEVATED (0)
#endif

#elifdef CONFIG_USERSPACE

#define PRV_DEFINE_SYSCALL     ARCH_SYSCALL_DEFINE
#define PRIVILEGE_WAS_ELEVATED ARCH_SYSCALL_FROM_USER()

#else

// A single privilege level: syscalls are plain calls.
#define PRV_DEFINE_SYSCALL(section, retType, funcName, ...) retType funcName(__VA_ARGS__)
#define PRIVILEGE_WAS_ELEVATED                              (0)

#endif
