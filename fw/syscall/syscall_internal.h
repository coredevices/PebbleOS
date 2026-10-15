/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pbl/kernel/compiler.h>
#include <pbl/mcu/mpu.h>

#include <kernel/pebble_tasks.h>

//! Any function defined with this macro will be privileged.
//! Privileges are raised upon entry to the syscall, and dropped
//! once the syscall is exited (unless the caller was originally privileged).
#define DEFINE_SYSCALL(retType, funcName, ...) \
  PRV_DEFINE_SYSCALL(".syscall_text.", retType, funcName, __VA_ARGS__)

#define PRV_DEFINE_SYSCALL(section, retType, funcName, ...)                \
  retType PBL_NAKED PBL_SECTION(section #funcName) funcName(__VA_ARGS__) { \
    __asm volatile(                                                        \
        "push { lr } \n"                                                   \
        "bl syscall_internal_maybe_skip_privilege \n"                      \
        "svc 2 \n"                                                         \
        "b __" #funcName "\n");                                            \
  }                                                                        \
  retType PBL_EXTERNALLY_VISIBLE PBL_USED __##funcName(__VA_ARGS__)

//! Useful function for checking syscall privileges.
//! @return True if the most recent syscall originated from userspace, resulting in a privilege
//! escalation. It can only be called from a function created with DEFINE_SYSCALL, or from a
//! PBL_ALWAYS_INLINE helper of one; the firmware build checks this.
#define PRIVILEGE_WAS_ELEVATED (syscall_internal_check_return_address(PBL_RETURN_ADDRESS(0)))

//! Check if ret_addr points at the drop_privilege code
bool syscall_internal_check_return_address(void *ret_addr);

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

// Used to implement DEFINE_SYSCALL
void syscall_internal_maybe_skip_privilege(void);

//! Return true when @p caller_pc is the return PC from the private
//! mcu_call_unprivileged() re-entry SVC and the current task is inside
//! mcu_call_unprivileged(). This is only a predicate; it does not consume or
//! change the saved call state.
bool mcu_call_unprivileged_reentry_is_allowed(uint32_t caller_pc);

//! Handle an authorized mcu_call_unprivileged() re-entry SVC. This rechecks the
//! stacked return PC, consumes the saved call state, and redirects the exception
//! frame to mcu_call_unprivileged_resume().
bool mcu_call_unprivileged_reentry_setup(uintptr_t orig_sp, uintptr_t *lr_ptr);

//! Unused bytes on the App/Worker syscall stacks (for sizing). Returns 0xFFFF
//! when SYSCALL_PRIVILEGED_STACK is disabled.
uint16_t syscall_app_stack_free_bytes(void);
uint16_t syscall_worker_stack_free_bytes(void);

//! No-access MPU region guarding the bottom of @p task's syscall stack, or
//! NULL when the platform bounds it some other way (PSPLIM) or has none.
const MpuRegion *syscall_get_stack_guard_region(PebbleTask task);

// Test overrides.
// TODO: really implement privilege escalation in unit tests. See PBL-9688
#if defined(UNITTEST)

#undef PRV_DEFINE_SYSCALL
#define PRV_DEFINE_SYSCALL(section, retType, funcName, ...) retType funcName(__VA_ARGS__)

#if !UNITTEST_WITH_SYSCALL_PRIVILEGES
#undef PRIVILEGE_WAS_ELEVATED
#define PRIVILEGE_WAS_ELEVATED (0)
#endif

#elifdef CONFIG_ARCH_POSIX

// A native build has a single privilege level: syscalls are plain calls.
#undef PRV_DEFINE_SYSCALL
#define PRV_DEFINE_SYSCALL(section, retType, funcName, ...) retType funcName(__VA_ARGS__)

#undef PRIVILEGE_WAS_ELEVATED
#define PRIVILEGE_WAS_ELEVATED (0)

#endif
