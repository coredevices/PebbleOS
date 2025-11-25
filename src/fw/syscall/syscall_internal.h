/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/attributes.h"

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

//! Any function defined with this macro will be privileged.
//! Privileges are raised upon entry to the syscall, and dropped
//! once the syscall is exited (unless the caller was originally privileged).
#define DEFINE_SYSCALL(retType, funcName, ...) \
  retType NAKED_FUNC SECTION(".syscall_text." #funcName) funcName(__VA_ARGS__) { \
    __asm volatile (\
      "push { lr } \n" \
      "bl syscall_internal_maybe_skip_privilege \n" \
      "svc 2 \n" \
      "b __" #funcName "\n" \
    );\
  }\
  EXTERNALLY_VISIBLE retType USED __##funcName(__VA_ARGS__)

//! Useful function for checking syscall privileges.
//! @return True if the most recent syscall originated from userspace, resulting in a privilege escalation.
//! It can only be called from a function created with DEFINE_SYSCALL
#define PRIVILEGE_WAS_ELEVATED (syscall_internal_check_return_address(__builtin_return_address(0)))

//! Check if ret_addr points at the drop_privilege code
bool syscall_internal_check_return_address(void * ret_addr);

//! Call this from privileged mode whenever a syscall did something wrong. This will kick out the misbehaving app.
NORETURN syscall_failed(void);

//! Call this from privileged mode when entering a syscall to ensure that provided
//! pointers are in the app's memory space, rather than in the kernel. If the buffer is not,
//! syscall_failed is called.
void syscall_assert_userspace_buffer(const void* buf, size_t num_bytes);

// Used to implement DEFINE_SYSCALL
void syscall_internal_maybe_skip_privilege(void);

// Test overrides.
// TODO: really implement privilege escalation in unit tests. See PBL-9688
#if defined(UNITTEST)

# undef DEFINE_SYSCALL
# define DEFINE_SYSCALL(retType, funcName, ...) \
    retType funcName(__VA_ARGS__)

# if !UNITTEST_WITH_SYSCALL_PRIVILEGES
#  undef PRIVILEGE_WAS_ELEVATED
#  define PRIVILEGE_WAS_ELEVATED (0)
# endif

#endif
