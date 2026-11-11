/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <applib/cpu_cache.h>
#include <clar.h>

// Fakes
//////////////////////////////////////////////////////////

static uintptr_t s_user_start, s_user_size;

typedef enum {
  UserSpaceBuffer_Unchecked,
  UserSpaceBuffer_Valid,
  UserSpaceBuffer_Invalid,
} UserSpaceBufferValidity;

static UserSpaceBufferValidity s_addr_result;

bool arch_syscall_returns_to_user(void *ret_addr) {
  s_addr_result = UserSpaceBuffer_Unchecked;
  return cl_mock_type(bool);
}

void pbl_syscall_verify_user(const void *buf, size_t num_bytes) {
  uintptr_t addr = (uintptr_t)buf;
  uintptr_t end = addr + num_bytes - 1;

  if ((addr >= s_user_start) && (addr < (s_user_start + s_user_size)) && (end >= s_user_start) &&
      (end < (s_user_start + s_user_size))) {
    s_addr_result = UserSpaceBuffer_Valid;
  } else {
    s_addr_result = UserSpaceBuffer_Invalid;
  }
}

// Tests
////////////////////////////////////

void test_cpu_cache__userspace_valid(void) {
  cl_will_return(arch_syscall_returns_to_user, true);
  s_user_start = 0x00;
  s_user_size = 0x20;

  memory_cache_flush((void *)0x0F, 0x2);

  cl_assert_equal_i(s_addr_result, UserSpaceBuffer_Valid);
}

void test_cpu_cache__userspace_fail_from_size(void) {
  cl_will_return(arch_syscall_returns_to_user, true);
  s_user_start = 0x00;
  s_user_size = 0x20;

  memory_cache_flush((void *)0x1F, 0x2);

  cl_assert_equal_i(s_addr_result, UserSpaceBuffer_Invalid);
}

void test_cpu_cache__userspace_fail_from_addr(void) {
  cl_will_return(arch_syscall_returns_to_user, true);
  s_user_start = 0x00;
  s_user_size = 0x20;

  memory_cache_flush((void *)0x20, 0x1);

  cl_assert_equal_i(s_addr_result, UserSpaceBuffer_Invalid);
}

void test_cpu_cache__kernel_unchecked(void) {
  cl_will_return(arch_syscall_returns_to_user, false);
  s_user_start = 0x00;
  s_user_size = 0x04;

  memory_cache_flush((void *)0x00, 0x10);

  cl_assert_equal_i(s_addr_result, UserSpaceBuffer_Unchecked);
}
