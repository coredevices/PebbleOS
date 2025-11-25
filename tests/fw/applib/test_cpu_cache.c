/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/cpu_cache.h"
#include "mcu/cache.h"

#include "clar.h"

// Fakes
//////////////////////////////////////////////////////////
static uintptr_t s_user_start, s_user_size;
static size_t s_flush_size, s_invalidate_size;
static uintptr_t s_flush_addr, s_invalidate_addr;

typedef enum {
  UserSpaceBuffer_Unchecked,
  UserSpaceBuffer_Valid,
  UserSpaceBuffer_Invalid,
  UserSpaceBuffer_NotRun,
} UserSpaceBufferValidity;
static UserSpaceBufferValidity s_addr_result;

uint32_t dcache_line_size(void) {
  return cl_mock_type(size_t);
}

uint32_t icache_line_size(void) {
  return cl_mock_type(size_t);
}

bool dcache_is_enabled(void) {
  return cl_mock_type(bool);
}

bool icache_is_enabled(void) {
  return cl_mock_type(bool);
}

void icache_invalidate(void *addr, size_t size) {
  s_invalidate_size = size;
  s_invalidate_addr = (uintptr_t)addr;
}

void dcache_flush(const void *addr, size_t size) {
  s_flush_size = size;
  s_flush_addr = (uintptr_t)addr;
}

bool syscall_internal_check_return_address(void * ret_addr) {
  s_addr_result = UserSpaceBuffer_Unchecked;
  return cl_mock_type(bool);
}

void syscall_assert_userspace_buffer(const void* buf, size_t num_bytes) {
  uintptr_t addr = (uintptr_t)buf;
  uintptr_t end = addr + num_bytes - 1;
  if ((addr >= s_user_start) && (addr < (s_user_start + s_user_size)) &&
      (end >= s_user_start) && (end < (s_user_start + s_user_size))) {
    s_addr_result = UserSpaceBuffer_Valid;
  } else {
    s_addr_result = UserSpaceBuffer_Invalid;
  }
}

// Tests
////////////////////////////////////

void test_cpu_cache__alignment(void) {
  cl_will_return(icache_is_enabled, true);
  cl_will_return(dcache_is_enabled, true);

  cl_will_return(icache_line_size, 8);
  cl_will_return(dcache_line_size, 16);

  cl_will_return(syscall_internal_check_return_address, true);
  s_user_start = 0x00;
  s_user_size = 0x20;

  memory_cache_flush((void*)0x0F, 0x2);

  cl_assert_equal_i(s_flush_addr, 0x00);
  cl_assert_equal_i(s_flush_size, 0x20);
  cl_assert_equal_i(s_invalidate_addr, 0x00);
  cl_assert_equal_i(s_invalidate_size, 0x20);

  cl_assert_equal_i(s_addr_result, UserSpaceBuffer_Valid);
}

void test_cpu_cache__userspace_fail_from_size(void) {
  cl_will_return(icache_is_enabled, true);
  cl_will_return(dcache_is_enabled, true);

  cl_will_return(icache_line_size, 1);
  cl_will_return(dcache_line_size, 1);

  cl_will_return(syscall_internal_check_return_address, true);
  s_user_start = 0x00;
  s_user_size = 0x20;

  memory_cache_flush((void*)0x1F, 0x2);

  cl_assert_equal_i(s_flush_addr, 0x1F);
  cl_assert_equal_i(s_flush_size, 0x02);
  cl_assert_equal_i(s_invalidate_addr, 0x1F);
  cl_assert_equal_i(s_invalidate_size, 0x02);

  cl_assert_equal_i(s_addr_result, UserSpaceBuffer_Invalid);
}

void test_cpu_cache__userspace_fail_from_addr(void) {
  cl_will_return(icache_is_enabled, true);
  cl_will_return(dcache_is_enabled, true);

  cl_will_return(icache_line_size, 1);
  cl_will_return(dcache_line_size, 1);

  cl_will_return(syscall_internal_check_return_address, true);
  s_user_start = 0x00;
  s_user_size = 0x20;

  memory_cache_flush((void*)0x20, 0x1);

  cl_assert_equal_i(s_flush_addr, 0x20);
  cl_assert_equal_i(s_flush_size, 0x01);
  cl_assert_equal_i(s_invalidate_addr, 0x20);
  cl_assert_equal_i(s_invalidate_size, 0x01);

  cl_assert_equal_i(s_addr_result, UserSpaceBuffer_Invalid);
}

void test_cpu_cache__userspace_aligned_fail(void) {
  cl_will_return(icache_is_enabled, true);
  cl_will_return(dcache_is_enabled, true);

  cl_will_return(icache_line_size, 8);
  cl_will_return(dcache_line_size, 8);

  cl_will_return(syscall_internal_check_return_address, true);
  s_user_start = 0x24;
  s_user_size = 0x20;

  memory_cache_flush((void*)0x26, 0x2);

  cl_assert_equal_i(s_flush_addr, 0x20);
  cl_assert_equal_i(s_flush_size, 0x08);
  cl_assert_equal_i(s_invalidate_addr, 0x20);
  cl_assert_equal_i(s_invalidate_size, 0x08);

  cl_assert_equal_i(s_addr_result, UserSpaceBuffer_Invalid);
}

void test_cpu_cache__userspace_ignore(void) {
  cl_will_return(icache_is_enabled, true);
  cl_will_return(dcache_is_enabled, true);

  cl_will_return(icache_line_size, 1);
  cl_will_return(dcache_line_size, 1);

  cl_will_return(syscall_internal_check_return_address, false);
  s_user_start = 0x00;
  s_user_size = 0x04;

  memory_cache_flush((void*)0x00, 0x10);

  cl_assert_equal_i(s_flush_addr, 0x00);
  cl_assert_equal_i(s_flush_size, 0x10);
  cl_assert_equal_i(s_invalidate_addr, 0x00);
  cl_assert_equal_i(s_invalidate_size, 0x10);

  cl_assert_equal_i(s_addr_result, UserSpaceBuffer_Unchecked);
}

void test_cpu_cache__without_icache(void) {
  cl_will_return(icache_is_enabled, false);
  cl_will_return(dcache_is_enabled, true);

  cl_will_return(icache_line_size, 1);
  cl_will_return(dcache_line_size, 1);

  s_invalidate_addr = s_flush_addr = 0xAA55;
  s_invalidate_size = s_flush_size = 0x55AA;

  cl_will_return(syscall_internal_check_return_address, true);
  s_user_start = 0x00;
  s_user_size = 0x20;

  memory_cache_flush((void*)0x20, 0x1);

  cl_assert_equal_i(s_flush_addr, 0x20);
  cl_assert_equal_i(s_flush_size, 0x01);
  cl_assert_equal_i(s_invalidate_addr, 0xAA55);
  cl_assert_equal_i(s_invalidate_size, 0x55AA);

  cl_assert_equal_i(s_addr_result, UserSpaceBuffer_Invalid);
}

void test_cpu_cache__without_dcache(void) {
  cl_will_return(icache_is_enabled, true);
  cl_will_return(dcache_is_enabled, false);

  cl_will_return(icache_line_size, 1);
  cl_will_return(dcache_line_size, 1);

  s_invalidate_addr = s_flush_addr = 0xAA55;
  s_invalidate_size = s_flush_size = 0x55AA;

  cl_will_return(syscall_internal_check_return_address, true);
  s_user_start = 0x00;
  s_user_size = 0x20;

  memory_cache_flush((void*)0x20, 0x1);

  cl_assert_equal_i(s_flush_addr, 0xAA55);
  cl_assert_equal_i(s_flush_size, 0x55AA);
  cl_assert_equal_i(s_invalidate_addr, 0x20);
  cl_assert_equal_i(s_invalidate_size, 0x01);

  cl_assert_equal_i(s_addr_result, UserSpaceBuffer_Invalid);
}

void test_cpu_cache__without_cache(void) {
  cl_will_return(icache_is_enabled, false);
  cl_will_return(dcache_is_enabled, false);

  cl_will_return(icache_line_size, 1);
  cl_will_return(dcache_line_size, 1);

  s_invalidate_addr = s_flush_addr = 0xAA55;
  s_invalidate_size = s_flush_size = 0x55AA;

  cl_will_return(syscall_internal_check_return_address, true);
  s_user_start = 0x00;
  s_user_size = 0x20;

  memory_cache_flush((void*)0x20, 0x1);

  cl_assert_equal_i(s_flush_addr, 0xAA55);
  cl_assert_equal_i(s_flush_size, 0x55AA);
  cl_assert_equal_i(s_invalidate_addr, 0xAA55);
  cl_assert_equal_i(s_invalidate_size, 0x55AA);

  cl_assert_equal_i(s_addr_result, UserSpaceBuffer_Invalid);
}
